# Guidance for Future Agents

This document captures repo-specific context and working agreements so that future automated sessions can operate safely and efficiently.

---

## Environment & Toolchain

- **Platform:** Bela embedded audio system. Code lives under `projects/instrument`. Build artifacts are generated via the Bela toolchain (`make` in project root or IDE build button).
- **Language:** C++17 (Bela default). No external build orchestrator (CMake, etc.).
- **Dependencies:**
  - Bela SDK headers (`Bela.h`, MIDI libraries, etc.).
  - Trimmed `u8g2` graphics library residing in `u8g2/`.

> **Important:** The `u8g2` directory has been pruned to support **only** the SH1106 128x64 OLED driver. Do **not** import other driver files unless explicitly requested; re-expanding the library dramatically increases compile time.

---

## Project Structure Highlights

- `render.cpp`: Bela entry points (`setup`, `render`, `cleanup`).
- `src/` & `include/`: Core modules (`ResourceManager`, `ModeManager`, `BelaInterface`, `Controller`, `Voices`, `Samplers`, `Loopers`, display contexts, utilities).
- `u8g2/`: Vendor graphics library subset. Key files:
  - `csrc/u8x8_d_sh1106_128x64.c`: Only retained display driver.
  - `csrc/u8g2_d_setup.c`: Minimal setup function used by the Linux helper.
  - `U8g2LinuxI2C.h`: Provides `U8G2_SH1106_128X64_NONAME_F_HW_I2C_LINUX`.
- `build/`: Bela-generated artifacts. Considered disposable.

---

## Hardware Assumptions

- Two SH1106-based OLEDs on I²C bus 1 (`0x3c`, `0x3d`).
- Custom control surface with buttons, pots, rotary encoders wired per `DeviceMap`.
- MIDI: Launchkey 49 MK1, exposed as ALSA ports `hw:1,0,0` (keys) and `hw:1,0,1` (controls).

If you modify pin or device mappings, update `include/DeviceMap.h` and note the change here to keep future agents aligned.

---

## Development Tips

- **Do not resurrect deleted display drivers.** If you need extra controllers, confirm with the user first.
- **Keep audio paths real-time safe.** Avoid heap allocations or heavy logging inside `processBlockwise()` functions.
- **ModeManager tests** provide a quick sanity check. Running the project on Bela and selecting `AllTest` helps validate broader changes.
- **Display updates** are queued through `DisplayContextReal`’s internal job counters; just call the provided setters (`setLines`, `setProgress`, etc.) and let the class schedule the auxiliary task (no manual flag toggles needed).
- **MIDI writes** from the controller are currently commented out. Verify hardware before re-enabling.

---

## Common Tasks & Entry Points

| Task | Where to Start |
| --- | --- |
| Add new UI state or screen | `include/Controller.h`, `src/Controller.cpp` |
| Modify control surface mapping | `include/DeviceMap.h`, `src/DeviceMap.cpp` |
| Extend sampler / looper behaviour | `src/Samplers.cpp`, `src/Loopers.cpp` |
| Touch display rendering | `src/DisplayContextReal.cpp` |
| Inject new test mode | `src/ModeManager.cpp` |

---

## Testing & Verification

- There is no automated test suite. Manual checks rely on ModeManager’s test modes.
- Building on a development host may leave stale objects in `build/`. Remove the directory if new source files are not picked up.
- When modifying MIDI code, run in an environment where ALSA devices exist or guard with connection flags to avoid hard failures.

---

## Communication

- Summaries should mention `README.md` and this file when material changes occur.
- If unexpected files appear in `build/` or other generated folders, prefer to ignore rather than delete unless instructed.
- Document significant workflow adjustments here to keep the knowledge base current.
