# Mercenaries 2 QoL Mods

A collection of quality-of-life mods for **Mercenaries 2: World in Flames** (PC).

These are lightweight `.asi` plugins — DLLs loaded into the game at startup by an
[ASI loader](#installation). Each mod lives in its own directory under [`mods/`](mods/)
and builds independently against a shared mod stdlib.

## Layout

- [`mods/`](mods/) — the mods, one directory each
- [`sdk/`](sdk/) — **`m2`, the Mercenaries 2 mod stdlib**: logging, INI, SecuROM-safe
  MinHook detours, safe Lua-stack reads, a shared hook on the game's log stream, and
  load-progress triggers keyed to loadprobe's world-load ladder. Mods `#include <m2.h>`
  and link it via [`sdk/sdk.mk`](sdk/sdk.mk). See the [SDK README](sdk/README.md).

## Mods

| Mod | Description |
| --- | --- |
| [windowed-mode](mods/windowed-mode/) | Forces D3D9 windowed mode (min 1280×720) by hooking `IDirect3D9::CreateDevice`. |
| [quiet-freeplay-vo](mods/quiet-freeplay-vo/) | Mutes or throttles the idle "nag" and proximity/POI voice-over (configurable via `.ini`); leaves mission dialog alone. |

## Installation

1. Install an ASI loader (e.g. [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader))
   into the game directory.
2. Drop the built `.asi` file into `<game>/scripts/`.
3. Launch the game.

Each mod writes a `<mod>.log` next to itself for troubleshooting.

## Building

You need a 32-bit MinGW cross-compiler (the game is a 32-bit process):

```sh
# macOS
brew install mingw-w64

# Ubuntu/Debian
apt install gcc-mingw-w64-i686
```

Build everything:

```sh
make
```

Or build a single mod:

```sh
make -C mods/windowed-mode
```

Built `.asi` files are written into each mod's directory and are git-ignored.

### Updating the world-load ladder

`sdk/m2/load_ladder.gen.h` is generated from loadprobe's `phases.rs` (the single source
of truth for load milestones) and committed so mods build without the loadprobe checkout.
When loadprobe's ladder changes, regenerate and verify:

```sh
make -C sdk ladder        # regenerate from the sibling ../mercenaries-game checkout
make -C sdk ladder-check  # drift guard
```

## Releases

Publishing a GitHub release triggers a CI workflow
([`.github/workflows/build-release.yml`](.github/workflows/build-release.yml)) that
cross-compiles every mod and attaches the built `.asi` files to the release as
downloadable assets. The workflow can also be run manually via *Actions →
Build and attach ASI mods to release → Run workflow*, which uploads the binaries
as a build artifact without needing a release.

## License

MIT — see [LICENSE](LICENSE).
