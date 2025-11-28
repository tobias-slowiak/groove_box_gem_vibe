# ACTUAL README WRITTEN BY ME

## Compiling and running.
The deplay_to_bela.ps1 file copies files over, compiles and runs. instructions are there in the beginning too with:

<#WARNING:
    for some reason the first make run of a session has to be run from the browser IDE
#>
<# Usage:
   (Run from a PowerShell prompt, e.g. PS C:\Users\tobia>)
   powershell -ExecutionPolicy Bypass -File "\\wsl`$\Ubuntu\home\tobi\groove_box\deploy_to_bela.ps1" [-Debug]
#>

<#Warning:
All the u8x2 files have been compiled and are there as .o files in the build folder. if the build folder is deleted, the u8g2 files need to be recompiled.
For this the u8g2 folder has to be copied to the project folder, after that it can be deleted again. i dont know why it does not compile the 
files when outside the project folder even though the folder is specified in the make parameters.
#>

## Looging when running on boot

systemd is used to log everything when running on boot. all the output is stored in /opt/Bela/logs/instrument.log so I can look at error codes/failed asserts/etc. after using it in standalone mode.s




























# RANDOM CODEX README


# Instrument Platform (Bela)

This repository contains the software stack for a custom performance instrument built around a **Bela** embedded audio platform.  
It coordinates a bespoke I²C control surface with dual OLED displays, integrates MIDI controllers, and hosts a suite of audio engines (samplers, loopers, and voice playback) that can be exercised individually or as a complete instrument.

## How to run
Look at the deploy_to_bela.ps1 file especially the first comment, it shows how to run from the powershell. stop the run by using ctrl+c or alternatively press the OFF button on the interface.

---

## High-Level Architecture

```
┌────────────┐      ┌──────────────────────┐
│  Hardware  │      │   ResourceManager    │
│            │◄────►│  (lifetime + wiring) │
│ - Buttons  │      └─────────┬────────────┘
│ - Pots     │                │
│ - Rotary   │                │
│ - OLEDs    │                │
└────┬───────┘                │
     │                        │
┌────▼───────┐       ┌────────▼────────┐
│ Bela Audio │──────►│   ModeManager   │
│  callback  │       │ (test + normal) │
└────┬───────┘       └───────┬────────┘
     │                        │
     │                        ├──► Controller (UI state machine)
     │                        ├──► Voices (sample playback + ADSR)
     │                        ├──► Samplers (record + slice)
     │                        └──► Loopers (multi-track overdub)
     │
     └──► `render.cpp` dispatches the Bela render loop into `ModeManager`
```

### Key Components

| Module | Responsibility | Relevant Files |
| --- | --- | --- |
| `ResourceManager` | Owns hardware/software singletons and Bela timing constants; performs device detection (I²C display, MIDI endpoints). | `src/ResourceManager.cpp`, `include/ResourceManager.h` |
| `ModeManager` | Chooses between normal playback and a rich set of self-test / demonstration modes (Bela interface test, display test, voices test, loopers test, samplers test, MIDI test, controller test). | `src/ModeManager.cpp`, `include/ModeManager.h` |
| `BelaInterface` | Polls the physical interface (buttons, rotary encoders, potentiometers) each audio block and queues `InterfaceMessage` events. | `src/BelaInterface.cpp`, `include/BelaInterface.h` |
| `Controller` | Drives the UI state machine, reacts to interface + MIDI events, and updates the OLED displays. | `src/Controller.cpp`, `include/Controller.h` |
| `DisplayContext*` | `DisplayContextReal` renders two SH1106 OLEDs via `u8g2` in an auxiliary Bela task; `DisplayContextFake` logs to the console when hardware is absent. | `src/DisplayContextReal.cpp`, `src/DisplayContextFake.cpp`, `include/DisplayContext*.h` |
| `Voices` | Manages up to 32 simultaneous sample voices with ADSR envelopes. | `src/Voices.cpp`, `include/Voices.h`, `include/ADSR.h` |
| `Samplers` | Records incoming audio into fixed buffers, manages slices, and exposes them for playback. | `src/Samplers.cpp`, `include/Samplers.h` |
| `Loopers` | Implements multi-track loop recording/playback using a shared two-minute buffer. | `src/Loopers.cpp`, `include/Loopers.h` |

---

## Hardware & External Dependencies

- **Bela board** with the Bela SDK/toolchain. The code is intended to build inside the Bela project tree (`projects/instrument`).
- **Control surface**  
  - Buttons, potentiometers, and rotary encoders connected according to `DeviceMap` (`include/DeviceMap.h`).  
  - Dual 128x64 OLED displays driven by the **SH1106** controller on I²C bus 1 (addresses `0x3c` and `0x3d`).
- **MIDI controllers**  
  - Novation Launchkey 49 MK1 (or compatible). MIDI device names are configured in `DeviceMap`.

**Libraries**

- Bela SDK headers and APIs.
- `u8g2` graphics library (a trimmed copy is bundled under `u8g2/`). Only the `u8x8_d_sh1106_128x64` driver is retained; other display drivers were removed to reduce compile time and footprint.

---

## Getting Started

1. **Deploy to Bela**
   - Copy this repository (or the `instrument` subfolder) into the Bela `projects` directory.
   - Inside the Bela shell, run the usual `make` or use the IDE to build the project.

2. **Hardware check**
   - Ensure the SH1106 OLED displays are wired to I²C bus 1 with addresses `0x3c` and `0x3d`.
   - Connect the control surface inputs (pins enumerated in `DeviceMap`).
   - Attach the Launchkey controller; confirm the system enumerates the MIDI ports (`hw:1,0,0` and `hw:1,0,1` by default).

3. **Run**
   - Launch the Bela project.  
   - On startup, the system probes the I²C bus; if displays are missing, it falls back to the console-based `DisplayContextFake`.
   - The top-level UI boots into the **TopMenu** mode. Use the left rotary encoder to pick a mode and press to confirm.

---

## Repository Layout

```
instrument/
├── include/              # Public headers for modules listed above
├── src/                  # Core implementation files
├── u8g2/                 # Trimmed subset of the u8g2 graphics library
│   ├── csrc/             # Minimal SH1106 driver + buffer helpers
│   ├── cppsrc/           # u8g2 C++ wrappers (unchanged)
│   └── U8g2LinuxI2C.h    # Bela/Linux binding for SH1106
├── render.cpp            # Bela entry point (setup/render/cleanup)
├── build/                # Bela build artifacts (generated)
└── README.md / AGENTS.md # Project documentation
```

> **Note:** The `build/` directory contains files produced by the Bela build system; you can safely clean it if you need a fresh build.

---

## Mode Overview & Testing

`ModeManager` exposes several modes to validate individual subsystems:

| Mode | Description | Trigger |
| --- | --- | --- |
| `TopMenu` | Menu for mode selection. Use rotary encoder 0 (left) to scroll; press to enter the highlighted mode. | Default at startup |
| `Normal` | Placeholder for the full instrument workflow (currently minimal). | Select from TopMenu |
| `AllTest` | Runs each test mode sequentially, automatically advancing when `currentTestDone` is raised. | Select from TopMenu |
| `BelaInterfaceTest` | Echoes button/pot/encoder events, prints pot readings periodically. | 10 s auto-completion |
| `DisplayContextTest` | Exercises text updates and progress bars on both displays. | 12 s auto-completion |
| `VoicesTest` | Demonstrates voice triggering, ADSR envelopes, and voice stealing. | 10 s auto-completion |
| `LoopersTest` | Records/plays back test audio into the first looper slot. Requires control-surface pots. | 10 s auto-completion |
| `SamplersTest` | Records test input into two samplers and triggers the captured slices. | 10 s auto-completion |
| `MidiTest` | Prints incoming MIDI channel messages from both key and control MIDI ports. | 5 s auto-completion |
| `ControllerTest` | Runs the `Controller` event loop (UI state machine and display updates). | 10 s auto-completion |

These modes rely on the `ResourceManager::currentTestDone` flag to progress; if you modify a test, ensure the flag is cleared when the mode restarts.

---

## Development Notes

- **Display driver**  
  The `u8g2` vendor code has been aggressively pruned—only SH1106 support remains.  
  If you need other display controllers, you must re-import the relevant driver files from upstream `u8g2`.

- **Interface messages**  
  `BelaInterface` debounces buttons and encoders and queues messages. Always drain `interface->numAvailableMessages()` in your processing loop.

- **MIDI wrappers**  
  `IMidi`, `MidiReal`, and `MidiFake` abstract Bela's MIDI API to allow headless operation. Query availability via `ResourceManager::keyMidiConnected` and `controlMidiConnected`.

- **Audio buffers**  
  - `Voices` uses `ResourceManager::getTestSample()` to obtain a demo sample.  
  - `Loopers` allocate from a single 2-minute buffer (`TOTAL_BUFFER_FRAMES`). Watch for overflows if you extend loop lengths or counts.

- **UI state machine**  
  `Controller::processBlockwise()` is the primary integration point for new UI features. Update the `UIState` enum and `setDisplay()` to expose new data to the user.

- **Threading**  
  The OLED rendering runs in a Bela auxiliary task scheduled from `DisplayContextReal::processBlockwise()`. Keep display updates lightweight and set `updateDisplayFlag` whenever you modify display contents.

---

## Contributing & Next Steps

1. **Implement the Normal instrument flow**  
   Integrate the sampler/looper backends with controller actions (currently marked TODO).
2. **MIDI feedback**  
   Re-enable looper LED updates in `Controller::updateLooperLights()` once control MIDI output is wired and tested.
3. **Sampler slicing**  
   Complete auto-slicing triggers in `Controller::processBlockwise()` and surface slice counts via `Samplers`.
4. **Documentation**  
   Keep `AGENTS.md` updated with build quirks or workflow changes—future automation relies on it.

Please maintain the code's real-time focus: avoid dynamic allocation in audio-rate paths and respect Bela's hard timing constraints.

---

## License

See individual source headers for license notices. The bundled `u8g2` subset includes its original license in `u8g2/LICENSE`.
