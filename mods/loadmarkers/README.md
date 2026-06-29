# loadmarkers

A minimal example mod: surface world-load progress **without enabling
`PMC_VERBOSE_LOG`**, and fire a callback the moment the player is actually
in-game.

## What it does

Subscribes to the SDK's shared log stream ([`m2_loghook`](../../sdk/m2/m2_loghook.h))
and:

1. Echoes every captured line to `loadmarkers.log`.
2. Uses [`m2_loadtrigger`](../../sdk/m2/m2_loadtrigger.h) to log
   **`>>> PLAYER IN-GAME`** when the world finishes loading (phase 20,
   `"GlobalExit - Complete"` — not the early "player spawn" at phase 11).

`m2_loghook` picks the cheapest available source: it prefers pmc_bb's live
`pmc_log_subscribe` (**pmc_bb >= 3.1.0**, in-process, no file), falls back to
tailing `pmc_blackbox.log`, and self-hooks the log stub only if pmc_bb isn't
present. In pmc_bb's default **markers-only** mode you get the ~21 world-load
milestones with no verbose-logging perf cost.

## Install

Copy `loadmarkers.asi` into `<game>/scripts/`, launch, and tail
`loadmarkers.log` (written next to the `.asi`). You'll see the load milestones
stream in, ending with `>>> PLAYER IN-GAME` once the world is up.

## Build

```sh
cd mods/loadmarkers
make
```

Output: `loadmarkers.asi`.

## Use it as a template

The two patterns are all you need to react to load state in your own mod:

```c
/* raw lines */
m2_loghook_add_listener(OnLogLine, NULL);

/* high-level "now in-game" (or any phase) */
m2_loadtrigger_on_phase(M2_PHASE_REACHED_WORLD_IDX, OnInGame, NULL);
m2_loadtrigger_install();
```
