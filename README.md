# orbAudio Guitar Synthesis System

A real-time guitar-like sound synthesis system for ESP32 that transforms sensor inputs into musical notes using the Karplus-Strong plucked-string algorithm.

## Overview

This system converts physical sensor inputs (2 FSR + 2 Piezo sensors) into guitar-like sounds in real-time. It replaces the original microphone processing pipeline with a sensor-triggered synthesis engine that generates realistic plucked-string sounds using the Karplus-Strong algorithm combined with ADSR envelope shaping.

## Features

- **Real-time audio synthesis** using Karplus-Strong algorithm
- **Polyphonic support** - up to 4 simultaneous notes
- **ADSR envelope shaping** for realistic attack/decay/sustain/release
- **Sensor-triggered notes** - 4 sensors mapped to different frequencies
- **Volume control** via potentiometer
- **Power control** via potentiometer
- **Debounced sensor inputs** for stable triggering
- **FreeRTOS-based** for real-time performance

## Hardware Requirements

### Sensors (4 total)
- **2 FSR (Force Sensitive Resistor) sensors** - Primary note triggers
- **2 Piezo sensors** - Additional note triggers or effects

### Audio Output
- **MAX98357A I2S amplifier** connected to:
  - I2S_BCK_OUT (Pin 26)
  - I2S_WS_OUT (Pin 25)
  - I2S_DATA_OUT (Pin 27)

### Controls
- **Volume potentiometer** - ADC_CHANNEL_0 (Pin 4)
- **Power potentiometer** - ADC_CHANNEL_2 (Pin 2)

### Pin Mapping
```
FSR1_PIN 34  -> ADC_CHANNEL_6 -> Note E2 (82.41 Hz)
FSR2_PIN 35  -> ADC_CHANNEL_7 -> Note A2 (110.00 Hz)
PIEZO1_PIN 32 -> ADC_CHANNEL_4 -> Note D3 (146.83 Hz)
PIEZO2_PIN 33 -> ADC_CHANNEL_5 -> Note G3 (196.00 Hz)
```

## Software Architecture

### Core Components

1. **Karplus-Strong Algorithm**
   - Circular buffer simulates string vibration
   - White noise burst for initial pluck
   - Decay factor controls sustain

2. **ADSR Envelope**
   - Attack: 5ms rise time
   - Decay: 50ms to sustain level
   - Sustain: 60% amplitude
   - Release: 300ms fade out

3. **Voice Management**
   - 4-voice polyphony
   - Automatic voice stealing (oldest first)
   - Independent envelope per voice

### FreeRTOS Tasks

- **sensor_task** (Core 1, Priority 4) - Reads sensors, triggers notes
- **audio_out_task** (Core 1, Priority 6) - Generates and streams audio
- **pot_task** (Core 1, Priority 3) - Handles volume/power controls

## Configuration

### Audio Parameters
```cpp
#define SAMPLE_RATE 44100          // Audio sample rate
#define DMA_BUF_LEN 128            // Buffer size (latency vs CPU trade-off)
#define KS_DECAY 0.996f           // String sustain (0.99 = longer sustain)
```

### Sensor Parameters
```cpp
#define SENSOR_THRESHOLD 1000     // ADC threshold for triggering (0-4095)
#define DEBOUNCE_MS 120          // Minimum time between triggers
```

### Envelope Parameters
```cpp
#define ADSR_ATTACK_MS 5         // Attack time in milliseconds
#define ADSR_DECAY_MS 50         // Decay time to sustain level
#define ADSR_SUSTAIN_LEVEL 0.6f  // Sustain amplitude (0.0-1.0)
#define ADSR_RELEASE_MS 300      // Release fade time
```

## Usage

1. **Power On**: Turn the power potentiometer above the midpoint
2. **Trigger Notes**: Press sensors to play notes:
   - FSR1: E2 (82.41 Hz)
   - FSR2: A2 (110.00 Hz)
   - Piezo1: D3 (146.83 Hz)
   - Piezo2: G3 (196.00 Hz)
3. **Volume Control**: Adjust volume potentiometer
4. **Multiple Notes**: Press multiple sensors simultaneously for chords

## Technical Details

### Karplus-Strong Implementation
- Each voice maintains a circular buffer of samples
- Buffer size = sample_rate / frequency
- Each sample: `new_sample = (current + next) * 0.5 * decay`
- White noise initialization simulates string pluck

### Performance
- CPU usage: ~15-25% on ESP32 (depending on polyphony)
- Latency: ~3-5ms (DMA_BUF_LEN dependent)
- Memory: ~8KB for voices + buffers

### Sensor Input
- 12-bit ADC resolution (0-4095)
- Threshold-based triggering with debouncing
- Amplitude scaling based on sensor pressure
- 50Hz polling rate for responsiveness

## Customization

### Changing Note Mappings
Modify the `SENSOR_NOTES` array:
```cpp
static const Note SENSOR_NOTES[4] = {
    {164.81f, "E3"},   // FSR1
    {220.00f, "A3"},   // FSR2
    {293.66f, "D4"},   // Piezo1
    {392.00f, "G4"}    // Piezo2
};
```

### Adjusting Sound Character
- **Brighter sound**: Increase harmonic content in synthesis
- **Longer sustain**: Increase `KS_DECAY` value
- **Faster attack**: Decrease `ADSR_ATTACK_MS`
- **More sensitive triggers**: Lower `SENSOR_THRESHOLD`

## Troubleshooting

### No Audio Output
- Check I2S pin connections
- Verify MAX98357A amplifier is powered
- Ensure power potentiometer is above threshold

### Sensors Not Triggering
- Increase `SENSOR_THRESHOLD` if too sensitive
- Decrease `SENSOR_THRESHOLD` if not sensitive enough
- Check ADC channel assignments match hardware

### Audio Glitches
- Reduce `DMA_BUF_LEN` for lower latency (may increase CPU usage)
- Increase `DMA_BUF_LEN` if crackling occurs
- Check for other tasks consuming too much CPU

## Dependencies

- ESP32 Arduino framework
- FreeRTOS
- ESP32 I2S driver
- ADC oneshot driver

## License

This implementation is part of the orbAudio project for educational and experimental purposes.

  NOTES & INSTRUCTIONS:

  1) Header generation:
     - Use the Python script I provided earlier (generate_drum_headers.py) to produce
       the pre-rendered headers for each velocity/pitch/release variant. The script
       resamples and scales the base WAV to produce the required arrays.

     - Make sure the generated arrays and the drum_buffers.h symbol names match exactly
       the names referenced in getBufferForVariant() above:
         drum_v{vel}_p{pitch}_long
         drum_v{vel}_p{pitch}_long_len
         drum_v{vel}_p{pitch}_short
         drum_v{vel}_p{pitch}_short_len

     - Example: drum_v1_p3_short is velocity layer 1, pitch index 3, short release.

  2) sampleRate:
     - The code assumes the generated headers are at 44100 Hz sample rate. If you generate
       them at another sample rate, update the sampleRate constant used for cleanup delays.

  3) Tuning thresholds:
     - PIEZO_THRESHOLD, FSR_THRESHOLD, FLEX_MIN, FLEX_MAX must be tuned experimentally.
     - For flexible calibration, you can add a serial calibration routine that prints raw ADC
       values while you strike/bend sensors; then adjust constants.

  4) Latency debugging:
     - Connect a scope or logic analyzer to PIN_LATENCY_ISR and PIN_LATENCY_PLAY. The
       delta between their rising edges is the ISR→PlayTask latency (including scheduling).
     - Typical target: < 2 ms. If you see >2 ms, try:
         * increasing PlayTask priority (already near max)
         * reducing AudioMemory blocks (but keep enough)
         * ensuring AudioPlayMemory.play() doesn't copy very large buffers (keep buffers reasonable)
         * using DMA playback via AudioPlayQueue if necessary (I can add that code)

  5) Memory:
     - Precomputing 30 buffers can consume significant flash and RAM. Each int16_t sample is 2 bytes.
       For a 0.5 sec buffer @ 44100 Hz that's ~44100 * 2 = 88 KB per buffer => 30 buffers ~ 2.6 MB.
       Teensy 4.1 has ~2 MB of RAM but larger flash; typically such arrays are stored in flash (PROGMEM)
       to save RAM. The Python generator should write arrays into flash by leaving them as const int16_t,
       which the compiler may place in flash (flash usage allowed). If arrays exceed RAM, use
       PROGMEM/ICACHE or store in external flash — but usually const arrays end up in flash (.text/.rodata)
       not RAM. Monitor memory usage in compile logs.

  6) If you need DMA-based playback (to avoid copying in player.play), say so — I will add a
     fully worked AudioPlayQueue + memcpy-to-queue solution (a bit more code but faster).

  7) Multi-pad expansion:
     - The code is single-pad (center & rim) oriented, but the architecture supports more pads by
       adding more ISR channels and mapping more buffers.

  8) Safety:
     - analogReadFast() is used inside ISR for speed. On Teensy it is implemented to be fast but
       verify your version of Teensy core supports it. If analogReadFast isn't available, consider
       using direct ADC registers or move to a high-priority polling task instead.

  9) Debugging steps:
     - If you get no sound, ensure Audio library is included and AudioMemory() is large enough.
     - If you get overloaded CPU, reduce buffer lengths produced by the Python script or reduce
       the number of velocity/pitch variants.

  10) Next steps I can do for you:
     - Add the DMA AudioPlayQueue variant (guaranteed faster write path).
     - Add an automated tool that generates drum_buffers.h from filenames.
     - Add a small web/serial UI to calibrate flex/FSR thresholds and persist to EEPROM.
     - Provide the complete Python generator (I already provided earlier — I can make a second
       version that writes all header files and a matching drum_buffers.h automatically).

  If you want me to produce the DMA playqueue version (no copying, faster), or generate the
  drum_buffers.h automatically as part of the Python tool, I’ll add that next.

  Done — you now have a full Teensy sketch, with explicit mapping of all precomputed buffers,
  ISR-driven hit detection, FreeRTOS PlayTask, smoothing, and latency test points.
*/
