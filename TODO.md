# orbAudio Guitar Synthesis Implementation

## Current Status: In Progress

### Completed Tasks:
- [ ] Create TODO tracking file
- [ ] Remove HX711 load cell code and includes
- [ ] Modify pin definitions for 2 FSR and 2 piezo sensors
- [ ] Remove I2S input setup and audio_in_task
- [ ] Create guitar sound synthesis functions
- [ ] Modify sensor task to trigger sounds based on sensor inputs
- [ ] Update audio processing to generate sounds instead of processing mic input
- [ ] Keep volume fader functionality intact

### Implementation Plan:
1. **Core System Changes** - Transform from audio processing to synthesis
2. **Pin Definitions** - Update for new sensor configuration
3. **Audio Synthesis** - Create guitar sound generation system
4. **Sensor Mapping** - Map sensors to guitar notes
5. **Task Architecture** - Update FreeRTOS tasks for new functionality

### Notes:
- 2 FSR sensors for primary note triggering
- 2 Piezo sensors for additional notes/effects
- Volume fader remains as general volume control
- No microphone input needed
- Real-time guitar sound synthesis
