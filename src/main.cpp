/* Drum_Teensy4_LowLatency_Full_fixed_for_Teensy.ino
   Adapted for Teensy 4.1 (Arduino/PlatformIO)

   - Uses AudioPlayQueue (int16_t buffers)
   - Uses xTaskCreate (Teensy FreeRTOS)
   - Uses analogReadFast() inside ISR
   - Uses noInterrupts()/interrupts() for small critical sections
   - Assumes 30 int16_t headers exist and "drum_buffers.h" externs them
*/

#include <Arduino.h>
#include <Audio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <drum_v0_p0_long.h>
#include <drum_v0_p0_short.h>
#include <drum_v0_p1_long.h>
#include <drum_v0_p1_short.h>
#include <drum_v0_p2_long.h>
#include <drum_v0_p2_short.h>
#include <drum_v0_p3_long.h>
#include <drum_v0_p3_short.h>
#include <drum_v0_p4_long.h>
#include <drum_v0_p4_short.h>

#include <drum_v1_p0_long.h>
#include <drum_v1_p0_short.h>
#include <drum_v1_p1_long.h>
#include <drum_v1_p1_short.h>
#include <drum_v1_p2_long.h>
#include <drum_v1_p2_short.h>
#include <drum_v1_p3_long.h>
#include <drum_v1_p3_short.h>
#include <drum_v1_p4_long.h>
#include <drum_v1_p4_short.h>

#include <drum_v2_p0_long.h>
#include <drum_v2_p0_short.h>
#include <drum_v2_p1_long.h>
#include <drum_v2_p1_short.h>
#include <drum_v2_p2_long.h>
#include <drum_v2_p2_short.h>
#include <drum_v2_p3_long.h>
#include <drum_v2_p3_short.h>
#include <drum_v2_p4_long.h>
#include <drum_v2_p4_short.h>

// Include the headers you generated earlier (int16_t arrays)
#include "drum_buffers.h"
void piezoISR();
#define analogReadFast(pin) analogRead(pin)

// ------------------- Audio objects -------------------
AudioPlayQueue queue1; // single queue object, accepts int16_t buffers
AudioMixer4 mixer;
AudioOutputI2S out;
AudioConnection patchQueueToMixer(queue1, 0, mixer, 0);
AudioConnection patchMixerToOutL(mixer, 0, out, 0);
AudioConnection patchMixerToOutR(mixer, 0, out, 1);
AudioControlSGTL5000 audioShield;

// ------------------- Configuration -------------------
#define PIEZO_CENTER_PIN A0
#define PIEZO_RIM_PIN A3
#define FLEX_PIN A1
#define FSR_PIN A2

#define PIN_LATENCY_ISR 2
#define PIN_LATENCY_PLAY 3
#define PIN_STATUS_LED 13

#define ANALOG_RESOLUTION_BITS 12
#define ADC_MAX 4095

#define PIEZO_THRESHOLD 600
#define PIEZO_DEBOUNCE_MS 20

#define FLEX_MIN 250
#define FLEX_MAX 3800
#define NOTE_STEPS 5
#define VEL_LAYERS 3

#define FSR_THRESHOLD 500
#define RELEASE_LONG_MS 900
#define RELEASE_SHORT_MS 140

#define FLEX_SMOOTH_ALPHA 0.22f
#define FLEX_SAMPLE_INTERVAL_US 200 // 5 kHz
#define PLAY_TASK_PRIORITY (configMAX_PRIORITIES - 1)

#define AUDIO_MEMORY_BLOCKS 18
#define ENABLE_LATENCY_DEBUG 1

// ------------------- Globals -------------------
IntervalTimer piezoTimer;
TaskHandle_t PlayTaskHandle = NULL;
volatile uint32_t lastHitMs = 0;
volatile float smoothedFlex = 0.0f;
const float flexAlpha = FLEX_SMOOTH_ALPHA;
volatile uint16_t lastPiezoCenterSample = 0;
volatile uint16_t lastPiezoRimSample = 0;

// mutex substitute: use noInterrupts/interrupts or a light spinlock if needed
// We'll use noInterrupts/interrupts for short critical regions.

// ------------------- Buffer descriptor -------------------
// must match the headers you generated
struct BufInfo
{
  const int16_t *buf;
  unsigned int len; // number of int16_t samples
};

// forward declaration
static inline BufInfo getBufferForVariant(int velIdx, int pitchIdx, bool shortRelease);

// clamp helper
static inline int clampi(int v, int lo, int hi)
{
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

// Convert piezo ADC reading to velocity layer 0..2
static inline int piezoToVelocityLayer(uint16_t piezoVal)
{
  if (piezoVal <= PIEZO_THRESHOLD)
    return 0;
  float normalized = (float)(piezoVal - PIEZO_THRESHOLD) / (float)(ADC_MAX - PIEZO_THRESHOLD);
  int layer = (int)floorf(normalized * (float)VEL_LAYERS);
  if (layer < 0)
    layer = 0;
  if (layer >= VEL_LAYERS)
    layer = VEL_LAYERS - 1;
  return layer;
}

// convert smoothed flex to pitch index 0..4
static inline int flexToPitchIndex(float flex)
{
  int idx = map((int)roundf(flex), FLEX_MIN, FLEX_MAX, 0, NOTE_STEPS - 1);
  idx = clampi(idx, 0, NOTE_STEPS - 1);
  return idx;
}

// ------------------- Buffer mapping implementation -------------------
// same big switch as earlier, returning pointer + length from drum_buffers.h
static inline BufInfo getBufferForVariant(int velIdx, int pitchIdx, bool shortRelease)
{
  if (velIdx < 0)
    velIdx = 0;
  if (velIdx >= VEL_LAYERS)
    velIdx = VEL_LAYERS - 1;
  if (pitchIdx < 0)
    pitchIdx = 0;
  if (pitchIdx >= NOTE_STEPS)
    pitchIdx = NOTE_STEPS - 1;

  switch (velIdx)
  {
  case 0:
    switch (pitchIdx)
    {
    case 0:
      return shortRelease ? BufInfo{drum_v0_p0_short, drum_v0_p0_short_len} : BufInfo{drum_v0_p0_long, drum_v0_p0_long_len};
    case 1:
      return shortRelease ? BufInfo{drum_v0_p1_short, drum_v0_p1_short_len} : BufInfo{drum_v0_p1_long, drum_v0_p1_long_len};
    case 2:
      return shortRelease ? BufInfo{drum_v0_p2_short, drum_v0_p2_short_len} : BufInfo{drum_v0_p2_long, drum_v0_p2_long_len};
    case 3:
      return shortRelease ? BufInfo{drum_v0_p3_short, drum_v0_p3_short_len} : BufInfo{drum_v0_p3_long, drum_v0_p3_long_len};
    default:
      return shortRelease ? BufInfo{drum_v0_p4_short, drum_v0_p4_short_len} : BufInfo{drum_v0_p4_long, drum_v0_p4_long_len};
    }
  case 1:
    switch (pitchIdx)
    {
    case 0:
      return shortRelease ? BufInfo{drum_v1_p0_short, drum_v1_p0_short_len} : BufInfo{drum_v1_p0_long, drum_v1_p0_long_len};
    case 1:
      return shortRelease ? BufInfo{drum_v1_p1_short, drum_v1_p1_short_len} : BufInfo{drum_v1_p1_long, drum_v1_p1_long_len};
    case 2:
      return shortRelease ? BufInfo{drum_v1_p2_short, drum_v1_p2_short_len} : BufInfo{drum_v1_p2_long, drum_v1_p2_long_len};
    case 3:
      return shortRelease ? BufInfo{drum_v1_p3_short, drum_v1_p3_short_len} : BufInfo{drum_v1_p3_long, drum_v1_p3_long_len};
    default:
      return shortRelease ? BufInfo{drum_v1_p4_short, drum_v1_p4_short_len} : BufInfo{drum_v1_p4_long, drum_v1_p4_long_len};
    }
  case 2:
    switch (pitchIdx)
    {
    case 0:
      return shortRelease ? BufInfo{drum_v2_p0_short, drum_v2_p0_short_len} : BufInfo{drum_v2_p0_long, drum_v2_p0_long_len};
    case 1:
      return shortRelease ? BufInfo{drum_v2_p1_short, drum_v2_p1_short_len} : BufInfo{drum_v2_p1_long, drum_v2_p1_long_len};
    case 2:
      return shortRelease ? BufInfo{drum_v2_p2_short, drum_v2_p2_short_len} : BufInfo{drum_v2_p2_long, drum_v2_p2_long_len};
    case 3:
      return shortRelease ? BufInfo{drum_v2_p3_short, drum_v2_p3_short_len} : BufInfo{drum_v2_p3_long, drum_v2_p3_long_len};
    default:
      return shortRelease ? BufInfo{drum_v2_p4_short, drum_v2_p4_short_len} : BufInfo{drum_v2_p4_long, drum_v2_p4_long_len};
    }
  default:
    return BufInfo{drum_v1_p2_long, drum_v1_p2_long_len};
  }
}

// ------------------- ISR: piezo sampling -------------------
// keep minimal and fast. Use analogReadFast() for Teensy.
void piezoISR()
{
#if ENABLE_LATENCY_DEBUG
  digitalWriteFast(PIN_LATENCY_ISR, HIGH);
#endif

  uint16_t c = analogReadFast(PIEZO_CENTER_PIN);
  uint16_t r = analogReadFast(PIEZO_RIM_PIN);
  lastPiezoCenterSample = c;
  lastPiezoRimSample = r;

  uint32_t now = millis();
  bool hitDetected = false;
  if ((c > PIEZO_THRESHOLD || r > PIEZO_THRESHOLD) && (now - lastHitMs > PIEZO_DEBOUNCE_MS))
  {
    hitDetected = true;
    lastHitMs = now;
  }

  if (hitDetected)
  {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(PlayTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }

#if ENABLE_LATENCY_DEBUG
  digitalWriteFast(PIN_LATENCY_ISR, LOW);
#endif
}

// ------------------- PlayTask -------------------
void PlayTask(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    // Wait (clears notification)
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Fast sensor snapshot (ISR may have already filled lastPiezo*)
    uint16_t piezoC = analogReadFast(PIEZO_CENTER_PIN);
    uint16_t piezoR = analogReadFast(PIEZO_RIM_PIN);
    uint16_t fsr = analogReadFast(FSR_PIN);
    uint16_t flexRaw = analogReadFast(FLEX_PIN);

    // Smooth flex: protect with interrupts disabled briefly
    noInterrupts();
    float localSmoothed = smoothedFlex;
    localSmoothed = localSmoothed + flexAlpha * ((float)flexRaw - localSmoothed);
    smoothedFlex = localSmoothed;
    interrupts();

    bool rimHit = (piezoR > piezoC);

    int velIdx = piezoToVelocityLayer(max(piezoC, piezoR));
    int pitchIdx = flexToPitchIndex(smoothedFlex);
    bool shortRelease = (fsr > FSR_THRESHOLD);

    // Lookup precomputed buffer
    BufInfo bi = getBufferForVariant(velIdx, pitchIdx, shortRelease);
    if (bi.buf == nullptr || bi.len == 0)
    {
      // no buffer
      continue;
    }

// Latency debug toggle
#if ENABLE_LATENCY_DEBUG
    digitalWriteFast(PIN_LATENCY_PLAY, HIGH);
#endif

    // play buffer via AudioPlayQueue (expects int16_t and sample count)
    // queue1.play takes pointer to int16_t data and number of samples (frames)
    // If your Audio library uses a different signature, adapt accordingly.
    queue1.play(bi.buf, bi.len);

#if ENABLE_LATENCY_DEBUG
    digitalWriteFast(PIN_LATENCY_PLAY, LOW);
#endif

    // Small yield to allow scheduler to run other tasks
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ------------------- Setup -------------------
void setup()
{
  // pins
  pinMode(PIN_LATENCY_ISR, OUTPUT);
  digitalWriteFast(PIN_LATENCY_ISR, LOW);
  pinMode(PIN_LATENCY_PLAY, OUTPUT);
  digitalWriteFast(PIN_LATENCY_PLAY, LOW);
  pinMode(PIN_STATUS_LED, OUTPUT);

  pinMode(PIEZO_CENTER_PIN, INPUT);
  pinMode(PIEZO_RIM_PIN, INPUT);
  pinMode(FLEX_PIN, INPUT);
  pinMode(FSR_PIN, INPUT);

  Serial.begin(115200);
  Serial.println("Drum_Teensy4_LowLatency_Fixed starting...");

  AudioMemory(AUDIO_MEMORY_BLOCKS);
  audioShield.enable();
  audioShield.volume(0.9f);
  mixer.gain(0, 0.95f);

  // ADC resolution
  analogReadResolution(ANALOG_RESOLUTION_BITS);
  analogReadAveraging(1);   // no averaging (faster reads)

  // initialize smoothing value to current flex reading
  smoothedFlex = analogRead(FLEX_PIN);

  // create PlayTask (highest practical priority)
  BaseType_t res = xTaskCreate(PlayTask, "PlayTask", 4096, NULL, PLAY_TASK_PRIORITY, &PlayTaskHandle);
  if (res != pdPASS)
  {
    Serial.println("ERROR: PlayTask creation failed");
    while (1)
      delay(1000);
  }

  // start piezo sampling ISR via IntervalTimer
  piezoTimer.begin(piezoISR, FLEX_SAMPLE_INTERVAL_US);

  Serial.println("Setup complete. System online.");
}

// ------------------- Loop -------------------
void loop()
{
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 5000)
  {
    lastPrint = millis();
    // minor status print
    Serial.printf("smoothedFlex=%.1f lastPiezoC=%u lastPiezoR=%u\n", smoothedFlex, lastPiezoCenterSample, lastPiezoRimSample);
  }
  vTaskDelay(pdMS_TO_TICKS(2000));
}
