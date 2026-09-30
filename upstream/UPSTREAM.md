# Upstream

`game/` and `portable/` come from **[YomotsuHisami/th09](https://github.com/YomotsuHisami/th09)**
at commit `5ea04507537350bea9410ba28594903af374d221` (2026-09-22):

| Here | Upstream |
| :--- | :--- |
| `game/` | `th09_web/cpp/game/` |
| `portable/sdl/` | `portable/sdl/` (renderer, shaders, state, miniaudio / stb headers) |
| `portable/input/` | `portable/input/` (keyboard map, touch gestures) |
| `src/Assets.*`, `src/GraphicsDevice.*`, `src/AudioDevice.*`, `src/FontDevice.*`, `src/main.cpp` | adapted from `th09_web/cpp/sdl/` |

Every change to the upstream files is in [`switch-port.patch`](switch-port.patch)
(`patch -p1` from the repository root against a fresh upstream copy). In short:

* **64-bit (AArch64)** — TH09's game code has no pointer truncation; only two
  `AnmVm` size asserts that hold for 32-bit pointers become
  `TH_LAYOUT_ASSERT` (still active on 32-bit builds).
* **Renderer** — SDL3/WebGL2 → SDL2/GLES 3 (Switch Mesa): window creation,
  `EXT_clip_control` through the GLES loader, aspect-correct pillarboxed
  present, `packed` (a reserved GLSL ES word Mesa rejects) renamed.
* Include paths flattened.

Host-side (`src/`): archive reads through stdio, SDL2 queued audio with BGM
streamed from `thbgm.dat`, SDL2_ttf text with the Switch system font as
fallback, Switch controllers as the two DirectInput pads, SD-card saves. The
browser netplay relay is not ported (its menu entry does nothing).

Game logic, timing, RNG and the replay format are untouched.

Licences: see [`THIRD-PARTY-NOTICES.txt`](THIRD-PARTY-NOTICES.txt) and
[`licenses/`](licenses/) (includes the MIT notice for the TH08 reference code
the TH09 reconstruction adapts).
