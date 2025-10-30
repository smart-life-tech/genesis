/* main.cpp
   Corrected Teensy 4.1 low-latency drum sketch (AudioPlayQueue version)
   - Plays precomputed int16_t buffers from drum_buffers.h via AudioPlayQueue
   - ISR-driven piezo detection (analogReadFast in ISR)
   - FreeRTOS tasks via Arduino_FreeRTOS
   - No ESP32-specific APIs
   - Keep previous mapping and pooling logic
*/

#include <Arduino.h>
#include <Audio.h>
#include <Arduino_FreeRTOS.h> // Teensy-friendly FreeRTOS wrapper
#include <task.h>
#include <semphr.h>
#include <IntervalTimer.h>

#include "drum_buffers.h" // your generated headers; must define int16_t arrays and _len

// ----------------------- CONFIGURATION -----------------------
#define PLAYER_POOL_SIZE 6

// Physical pins (Teensy 4.1)
#define PIEZO_CENTER_PIN A0
#define PIEZO_RIM_PIN A3
#define FLEX_PIN A1
#define FSR_PIN A2

// Debug/latency toggle pins
#define PIN_LATENCY_ISR 2
#define PIN_LATENCY_PLAY 3
#define PIN_STATUS_LED 13

// ADC / sensors
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
#define FLEX_SAMPLE_INTERVAL_US 200 // 5kHz

#define AUDIO_MEMORY_BLOCKS 18
#define AUTO_PLAYER_TIMEOUT_MS 4000

#define ENABLE_LATENCY_DEBUG 1

// ----------------------- AUDIO OBJECTS ------------------------
AudioPlayQueue playerPool[PLAYER_POOL_SIZE]; // queue-style players (accept int16_t buffers)
bool playerInUse[PLAYER_POOL_SIZE];

AudioMixer4 mixer;
AudioOutputI2S i2sOut;
AudioConnection *patchPlayerToMixer[PLAYER_POOL_SIZE];
AudioConnection *patchMixerToI2S_L;
AudioConnection *patchMixerToI2S_R;
AudioControlSGTL5000 audioShield;

// ----------------------- GLOBALS ------------------------------
TaskHandle_t PlayTaskHandle = NULL;

volatile uint8_t nextPlayerIndex = 0;

volatile uint32_t lastHitMs = 0;
volatile float smoothedFlex = 0.0f;
const float flexAlpha = FLEX_SMOOTH_ALPHA;

volatile uint16_t lastPiezoCenterSample = 0;
volatile uint16_t lastPiezoRimSample = 0;

IntervalTimer piezoTimer;

// simple mutex for non-ISR shared ops - we use FreeRTOS primitives when needed
SemaphoreHandle_t xMutex = NULL;

// ----------------------- Buffer lookup struct -------------------
struct BufInfo
{
  const int16_t *buf;
  unsigned int len; // samples
};

// Forward declarations
static inline BufInfo getBufferForVariant(int velIdx, int pitchIdx, bool shortRelease);
static inline int allocatePlayer();
static inline void releasePlayer(int idx);
static inline int piezoToVelocityLayer(uint16_t piezoVal);
static inline int flexToPitchIndex(float flex);

// ----------------------- Implementations ----------------------
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

  // explicit mapping (must match drum_buffers.h symbol names)
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

static inline int allocatePlayer()
{
  for (int i = 0; i < PLAYER_POOL_SIZE; ++i)
  {
    int idx = (nextPlayerIndex + i) % PLAYER_POOL_SIZE;
    if (!playerInUse[idx])
    {
      nextPlayerIndex = (idx + 1) % PLAYER_POOL_SIZE;
      playerInUse[idx] = true;
      return idx;
    }
  }
  // no free player -> steal next
  int steal = nextPlayerIndex;
  nextPlayerIndex = (nextPlayerIndex + 1) % PLAYER_POOL_SIZE;
  playerInUse[steal] = true;
  return steal;
}

static inline void releasePlayer(int idx)
{
  if (idx < 0 || idx >= PLAYER_POOL_SIZE)
    return;
  playerInUse[idx] = false;
}

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

static inline int flexToPitchIndex(float flex)
{
  int idx = map((int)roundf(flex), FLEX_MIN, FLEX_MAX, 0, NOTE_STEPS - 1);
  if (idx < 0)
    idx = 0;
  if (idx >= NOTE_STEPS)
    idx = NOTE_STEPS - 1;
  return idx;
}

// ----------------------- ISR (piezo sampling) -------------------------
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
  if ((c > PIEZO_THRESHOLD || r > PIEZO_THRESHOLD) && (now - lastHitMs > PIEZO_DEBOUNCE_MS))
  {
    lastHitMs = now;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(PlayTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }

#if ENABLE_LATENCY_DEBUG
  digitalWriteFast(PIN_LATENCY_ISR, LOW);
#endif
}

// ----------------------- Cleanup task -------------------------------
// Frees player slot after a delay
struct CleanupArgs
{
  int playerIdx;
  uint32_t delayMs;
};

void CleanupPlayerTask(void *pv)
{
  CleanupArgs *a = (CleanupArgs *)pv;
  if (!a)
  {
    vTaskDelete(NULL);
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(a->delayMs));
  releasePlayer(a->playerIdx);
  free(a);
  vTaskDelete(NULL);
}

// ----------------------- Play Task ---------------------------------
void PlayTask(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    // Wait for ISR notification
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // Immediate quick reads
    uint16_t piezoC = analogReadFast(PIEZO_CENTER_PIN);
    uint16_t piezoR = analogReadFast(PIEZO_RIM_PIN);
    uint16_t fsr = analogReadFast(FSR_PIN);
    uint16_t flexRaw = analogReadFast(FLEX_PIN);

    // Smooth flex
    noInterrupts();
    smoothedFlex = smoothedFlex + flexAlpha * ((float)flexRaw - smoothedFlex);
    interrupts();

    bool rimHit = (piezoR > piezoC);

    int velIdx = piezoToVelocityLayer(max(piezoC, piezoR));
    int pitchIdx = flexToPitchIndex(smoothedFlex);
    bool shortRelease = (fsr > FSR_THRESHOLD);

    BufInfo bi = getBufferForVariant(velIdx, pitchIdx, shortRelease);
    if (!bi.buf || bi.len == 0)
      continue;

    int pidx = allocatePlayer();
    if (pidx < 0 || pidx >= PLAYER_POOL_SIZE)
      continue;

#if ENABLE_LATENCY_DEBUG
    digitalWriteFast(PIN_LATENCY_PLAY, HIGH);
#endif

    // Use AudioPlayQueue::play(int16_t *data, uint32_t frames)
    // Note: many AudioPlayQueue implementations expect number of frames (samples)
    // matching the earlier Karplus usage: queue.play(buf, frames)
    playerPool[pidx].play(bi.buf, bi.len);

    // Schedule cleanup task to free player slot after playback time
    const float sampleRate = 44100.0f;
    uint32_t playbackMs = (uint32_t)ceilf(((float)bi.len / sampleRate) * 1000.0f) + 40;
    CleanupArgs *args = (CleanupArgs *)malloc(sizeof(CleanupArgs));
    if (args)
    {
      args->playerIdx = pidx;
      args->delayMs = playbackMs;
      // create cleanup task
      xTaskCreate(CleanupPlayerTask, "Cleanup", 1024, args, 1, NULL);
    }
    else
    {
      // fallback: free after global timeout
      CleanupArgs *args2 = (CleanupArgs *)malloc(sizeof(CleanupArgs));
      if (args2)
      {
        args2->playerIdx = pidx;
        args2->delayMs = AUTO_PLAYER_TIMEOUT_MS;
        xTaskCreate(CleanupPlayerTask, "CleanupFB", 1024, args2, 1, NULL);
      }
    }

#if ENABLE_LATENCY_DEBUG
    digitalWriteFast(PIN_LATENCY_PLAY, LOW);
#endif
    // Continue waiting
  } // for
}

// -------------------------- Setup & Loop ------------------------------
void setup()
{
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
  Serial.println("Teensy Drum - starting...");

  analogReadResolution(ANALOG_RESOLUTION_BITS);

  AudioMemory(AUDIO_MEMORY_BLOCKS);
  audioShield.enable();
  audioShield.volume(0.9f);
  mixer.gain(0, 0.95f);

  // Connect each play queue to the mixer
  for (int i = 0; i < PLAYER_POOL_SIZE; ++i)
  {
    patchPlayerToMixer[i] = new AudioConnection(playerPool[i], 0, mixer, 0);
    playerInUse[i] = false;
  }
  patchMixerToI2S_L = new AudioConnection(mixer, 0, i2sOut, 0);
  patchMixerToI2S_R = new AudioConnection(mixer, 0, i2sOut, 1);

  // Initialize smoothing
  smoothedFlex = analogRead(FLEX_PIN);

  // Create mutex
  xMutex = xSemaphoreCreateMutex();

  // Create play task
  xTaskCreate(PlayTask, "PlayTask", 4096, NULL, tskIDLE_PRIORITY + 4, &PlayTaskHandle);

  // Start piezo sampling timer
  piezoTimer.begin(piezoISR, FLEX_SAMPLE_INTERVAL_US);

  Serial.println("Setup complete.");
}

void loop()
{
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 5000)
  {
    lastPrint = millis();
    int used = 0;
    for (int i = 0; i < PLAYER_POOL_SIZE; ++i)
      if (playerInUse[i])
        ++used;
    Serial.printf("players used=%d  smoothedFlex=%.1f\n", used, smoothedFlex);
    digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
  }
  vTaskDelay(pdMS_TO_TICKS(2000));
}
