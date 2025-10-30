# orbAudio Project Issues and Solutions

This document records all past issues encountered during the development of the orbAudio guitar synthesis system and their resolutions.

## Issue 1: ARM Toolchain Installation for Teensy

### Description
Encountered issues installing the ARM toolchain required for Teensy development in the PlatformIO environment.

### Solution
Downloaded the ARM toolchain manually and added it to the system PATH. This ensured proper compilation and uploading to the Teensy board.

### Status
Resolved

---

## Issue 2: Missing ARM Cortex-M7 DSP Math Library

### Description
Build process fails with linker error: "cannot find -larm_cortexM7lfsp_math: No such file or directory". The ARM toolchain is missing the DSP math library required for Teensy 4.1 compilation.

### Solution
Resolved by adding the ARM CMSIS DSP library to PlatformIO configuration:
- Uncommented the lib_deps line in platformio.ini to include https://github.com/ARM-software/CMSIS-DSP.git#main
- Ran `pio lib install` to download and install the CMSIS DSP library
- The library provides the required arm_cortexM7lfsp_math functions for DSP operations

### Status
Resolved

---

## Issue 3: HX711 Load Cell Code Removal

### Description
The original codebase included HX711 load cell functionality that was no longer needed for the guitar synthesis system.

### Solution
Removed all HX711-related code and includes from the project. This included:
- HX711 library dependencies
- Load cell initialization code
- Weight measurement functions
- Associated pin definitions

### Status
Resolved

---

## Issue 4: Sensor Pin Definitions Update

### Description
The original system used 5 FSR sensors, but the new design required 2 FSR and 2 Piezo sensors for guitar note triggering.

### Solution
Modified pin definitions to accommodate the new sensor configuration:
- Updated from 5 FSR sensors to 2 FSR + 2 Piezo sensors
- Reassigned ADC channels accordingly
- Ensured proper mapping to guitar notes (E2, A2, D3, G3)

### Status
Resolved

---

## Issue 5: I2S Input Setup Removal

### Description
The original system processed microphone input via I2S, but the new synthesis system generates audio internally.

### Solution
Removed I2S input setup and audio_in_task from the codebase. This included:
- I2S input configuration
- Microphone processing task
- Input buffer management
- Related audio processing pipeline components

### Status
Resolved

---

## Issue 6: Guitar Sound Synthesis Implementation

### Description
Needed to implement real-time guitar sound synthesis to replace microphone processing.

### Solution
Created guitar sound synthesis functions using:
- Karplus-Strong plucked-string algorithm
- ADSR envelope shaping
- Polyphonic voice management (4 voices)
- Real-time audio generation at 44.1kHz sample rate

### Status
Resolved

---

## Issue 7: Sensor Task Modification

### Description
The sensor task needed to be updated to trigger sounds based on sensor inputs instead of processing load cell data.

### Solution
Modified the sensor task to:
- Read 4 sensors (2 FSR + 2 Piezo)
- Implement threshold-based triggering with debouncing
- Map sensors to specific guitar notes
- Allocate voices for polyphonic playback
- Scale amplitude based on sensor pressure

### Status
Resolved

---

## Issue 8: Audio Processing Update

### Description
The audio processing pipeline needed to generate synthesized sounds instead of processing microphone input.

### Solution
Updated audio processing to:
- Generate Karplus-Strong algorithm output
- Apply ADSR envelopes per voice
- Mix multiple voices for polyphony
- Output via I2S to MAX98357A amplifier
- Maintain real-time performance with FreeRTOS

### Status
Resolved

---

## Issue 9: Volume Fader Functionality Preservation

### Description
The volume fader functionality needed to be maintained while transitioning to the synthesis system.

### Solution
Kept the existing volume potentiometer control intact:
- Maintained ADC_CHANNEL_0 for volume control
- Preserved volume scaling and smoothing
- Ensured compatibility with new audio generation pipeline

### Status
Resolved

## issue 10
### description ld.exe file not found 
### soltion : adeed -larm_cortexM7lfsp_math to the unbuild so it is not compiled
- Removing unnecessary CMSIS libraries eliminated duplicate ARM macro definitions.
- Cleaning the .pio folder removed cached linker flags.
- Avoiding custom build_unflags and external includes simplified the toolchain.
---

## Notes

- All issues were resolved as part of transforming the system from a load cell/audio processing device to a sensor-triggered guitar synthesis system
- The project successfully transitioned from Teensy Audio library microphone input to internal synthesis generation
- FreeRTOS task architecture was maintained and optimized for real-time audio performance
- Sensor debouncing and voice management were implemented to ensure stable, responsive playback

## Future Considerations

- Monitor for any new issues related to polyphony limits or sensor sensitivity
- Consider adding more sophisticated sensor calibration if needed
- Evaluate performance optimizations for lower latency if required
