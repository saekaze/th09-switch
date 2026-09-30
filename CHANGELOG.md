# Changelog — Touhou 9 Switch port

## Touhou 9: Phantasmagoria of Flower View — Switch Port r2

## What's new in r2

* The NRO is now `touhou9.nro`, matching the other ports. When updating,
  delete the old `touhou09.nro` so hbmenu does not list the game twice.
* Key Config: only B, A, L/ZL, R/ZR, +, X and Y can be picked as buttons.
  The D-pad and sticks (stick clicks included) only move. Defaults are unchanged.
* Display version `1.50a-r2`.

---

## Touhou 9: Phantasmagoria of Flower View — Switch Port r1

Native Nintendo Switch homebrew port of TH09 v1.50a (`touhou9.nro`).

First release. The game logic is YomotsuHisami's TH09 C++ reconstruction,
compiled natively for AArch64; the Switch host (SDL2 + OpenGL ES 3 + libnx)
follows the TH10 port's layout and controls.

## What's in r1

* Story, Extra, Match (vs CPU) and **local 2P versus with two controllers**,
  replays, music room, rankings, endings.
* 640×480 picture pillarboxed on pure black, aspect-correct upscale.
* BGM streamed directly from `thbgm.dat` with the original loop points.
* Japanese text via MS Gothic (`msgothic.ttc`) or the Switch's built-in
  Japanese font.
* Switch buttons act as TH09's gamepad, so in-game Key Config can remap
  them; handheld touch; − (Minus) snapshots.
* hbmenu title in Japanese on 日本語 consoles, romanised elsewhere.
  Display version is `1.50a-r1`.

Not included: online versus (the web version's relay has no Switch
counterpart).

## Install

Copy to `sd:/switch/th09/` next to your legally owned game data:

```text
sd:/switch/th09/
    ├── touhou9.nro
    ├── th09.dat
    ├── thbgm.dat     # optional (BGM)
    └── msgothic.ttc  # recommended (system font is the fallback)
```

Rebuild with `./scripts/build_switch.sh` (devkitA64 + switch portlibs).

Game data is **not** included — you must own a copy of TH09 v1.50a.
