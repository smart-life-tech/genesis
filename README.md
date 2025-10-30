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
