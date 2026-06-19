# Windowed Mode

Forces **Mercenaries 2: World in Flames** to run in windowed mode.

## What it does

Hooks `IDirect3D9::CreateDevice` via vtable patching and rewrites the
presentation parameters before the device is created:

- `Windowed = TRUE`
- Minimum back-buffer resolution clamped to **1280×720**
- Refresh rate reset to default, back-buffer format set to desktop format
- Swap effect coerced to `D3DSWAPEFFECT_DISCARD` if it was `FLIP` (invalid windowed)

After the device is created it applies a standard `WS_OVERLAPPEDWINDOW` style to
the focus window so the window has a title bar and borders, and repositions it.

## Build

```sh
make            # MinGW cross-compile (default)
make msvc       # native MSVC on Windows
make clean
```

## Install

Copy the built `windowed_mode.asi` into `<game>/scripts/`.

A `windowed_mode.log` is written next to the `.asi` on each launch.
