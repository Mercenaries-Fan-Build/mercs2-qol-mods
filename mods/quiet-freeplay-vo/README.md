# Quiet Freeplay VO

Mutes or throttles a class of repetitive voice-over in **Mercenaries 2: World in Flames**:

- the idle **"nag"** that fires when you're not on a mission (`Fiona.Misc.NoState01/02`,
  re-arms ~every few minutes), and
- **proximity / point-of-interest hints** as you drive around the map
  (`Fiona.Hints.01..33`).

Mission, briefing, and cinematic dialog are **left untouched**.

## How it works

Both nag and hint lines are routed through the engine's native `VO.Cue` /
`VO.CueWithoutSubtitles` Lua bindings — but so is real dialog. The game's
"this-is-freeplay-chatter" decision (the `VO.PRIORITY_SCRIPTED_FREEPLAY` priority tier)
lives in Lua and isn't visible at the native call, so this mod discriminates by the
**cue-name string**: a cue is only acted on if its name starts with one of the
configured `prefixes`.

Built on the [`m2` mod stdlib](../../sdk/):

- **MinHook `.text` detours** on `VO.Cue` / `VO.CueWithoutSubtitles` (`m2_hook`). This
  is SecuROM-safe — unlike a `.rdata` registration-slot patch, which is confirmed to
  trip anti-tamper and crash early init. MinHook's trampoline is the pass-through path
  for non-matching cues, and a code detour intercepts every call regardless of when the
  binding was registered (no startup-timing guesswork).
- **Armed by a load-progress trigger** (`m2_loadtrigger`). The detour is dormant — all
  VO passes through — until the world load reaches `arm_phase` (default "WORLD LOAD
  START"), so the shell and menus are never touched. The trigger reuses loadprobe's
  world-load ladder as its milestone vocabulary.

## Configuration — `quiet_freeplay_vo.ini`

| Key | Meaning |
| --- | --- |
| `mode` | `off` (observe only) · `throttle` (one cue per interval, default) · `silence` (drop all matching) |
| `throttle_seconds` | Minimum gap between allowed cues in `throttle` mode (default `900` = 15 min) |
| `log_all_cues` | `1` to log every cue name the game tries to play (default on) — use this to tune `prefixes` |
| `arm_phase` | loadprobe ladder phase at which filtering activates (default `10` = WORLD LOAD START; `11` = Player spawn) |
| `prefixes` | Comma/semicolon-separated cue-name prefixes that define the freeplay class |

## Tuning on your build

The cue names above are taken from the decompiled base-game scripts. They should be
correct for retail, but the surest way to verify (and to catch any cue the defaults
miss) is to **read the log**:

1. Set `mode = off` and `log_all_cues = 1`.
2. Play: idle to trigger the nag, drive near POIs to trigger hints.
3. Open `quiet_freeplay_vo.log` (next to the `.asi`) — every cue is listed, with
   `(freeplay class)` marking the ones the current `prefixes` already match.
4. Add any missed prefixes, then switch `mode` to `throttle` or `silence`.

## Build & install

```sh
make
```

Copy **both** `quiet_freeplay_vo.asi` and `quiet_freeplay_vo.ini` into `<game>/scripts/`.

## Compatibility / status

Targets the cracked retail EXE (`53,482,288` bytes, image base `0x00400000`); the
`VO.Cue` @ `0x005E9DE0` / `VO.CueWithoutSubtitles` @ `0x005E9F40` addresses and the log
stub the trigger watches are binary-specific (defined in [`sdk/m2/m2_target.h`](../../sdk/m2/m2_target.h)).

> Not yet verified on a running game — the build is clean and every primitive (MinHook
> `.text` detours, the log-stub capture, the Lua-stack readers) is ported from the
> working `pmc_bb` plugin. Confirm against `quiet_freeplay_vo.log` on first run: it logs
> whether each VO hook attached, when it armed (and at which phase), and every cue it
> sees. If cues are listed but the wrong ones match, tune `prefixes`.
