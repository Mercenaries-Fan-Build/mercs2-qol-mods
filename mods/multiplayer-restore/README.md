# multiplayer-restore (mercs2-qol-mods port)

Restores Mercenaries 2 online multiplayer by routing EA matchmaking
traffic to a private server and accepting that server's self-signed
TLS certificate. Drop-in mod for the
[mercs2-qol-mods SDK](https://github.com/Mercenaries-Fan-Build/mercs2-qol-mods).

This is a port of the multiplayer layer from **loganw234's
[Merc2Reborn / Mercenaries2](https://github.com/loganw234/Mercenaries2)**
project. The original reverse-engineering, the multiplayer hooking design,
and the FESL CA-key patch are all **loganw234's work** — this port only
strips the Lua bridge and adapts the remaining hooks to the QoL mods SDK.

## What it does

1. **DNS redirect** — hooks `ws2_32` resolvers so `*.ea.com`,
   `*.gamespy.com`, and `fesl*` resolve to the configured private
   server (default `refesl.live`).
2. **Cert blindfold** — hooks `wintrust!WinVerifyTrust` to accept the
   private server's self-signed cert blob. Local file/catalog cert
   validation is untouched.
3. **Time spoof** — pins the clock returned by Win32 + CRT time APIs
   to a date inside the served cert's validity window. Optional.
4. **FESL CA pubkey patch** *(opt-in, off by default)* — replays
   MLoader's 128-byte `.rdata` write at RVA `0x768378` so the game's
   SSL stack accepts a *self-signed* server cert chain. Off unless the
   server needs it: a server presenting a cert the game already trusts
   (e.g. the genuine EA FESL cert behind a legacy stunnel terminator)
   validates natively, and overwriting the pinned CA would break that.
   Enable with `ca_key_patch = 1` (see Config).

What it does NOT do: Lua bridge, REPL, modding hooks. Use the
upstream Merc2Fix ASI if you want those.

## Install

1. Build (see below) or drop a prebuilt `multiplayer_restore.asi`
   into your Mercenaries 2 install folder.
2. Drop `multiplayer_restore.ini` alongside it (only needed if you
   want to override the default server or disable the clock spoof).
3. Launch the game. Connect online normally — no MLoader required.

## Build

If this directory lives under `mercs2-qol-mods/mods/`:

```sh
cd mods/multiplayer-restore
make
```

Out-of-tree:

```sh
make SDK_DIR=/path/to/mercs2-qol-mods/sdk
```

Output: `multiplayer_restore.asi`.

## Config (`multiplayer_restore.ini`)

```ini
[server]
ip = refesl.live              ; hostname or dotted-quad

[compat]
spoof_clock = 1               ; 1 = spoof to 2012-06-15 (recommended)
ca_key_patch = 0              ; 1 = patch the pinned CA for a self-signed
                              ;     server; leave 0 for a game-trusted cert
```

## Status

**Builds clean against the mercs2-qol-mods SDK.** Compiles with the SDK's
MinGW toolchain via the top-level `make`, links only system DLLs (no
libgcc runtime dependency), and the hooks/INI/logging wire through the
SDK helpers (`m2_hook_attach`, `m2_ini_parse`, `m2_logf`,
`m2_module_path`) with matching signatures.

**Not yet runtime-tested in-game as this port.** The hooking model is
validated upstream: loganw234's **standalone Merc2Fix.asi** (which this
is derived from) was run against a `mercs2-securom-bypass`-patched
`Mercenaries2.exe` — all hooks armed cleanly, multiplayer worked
end-to-end, no anti-tamper trips. This port keeps that hook logic
byte-identical; what changed is only the SDK glue (Makefile, INI
callback, logging) and the CA-key patch being made opt-in. A live
in-game pass against a real FESL server is the remaining validation step.

## The CA-key patch is off by default

`FESL_CA_KEY_RVA = 0x768378` overwrites the game's pinned FESL CA pubkey
so it will trust a *self-signed* server cert. That is only the right move
when your server actually presents a self-signed cert. If the server
presents a cert the game already trusts — e.g. the **genuine EA FESL
cert** fronted by a legacy-TLS (stunnel/OpenSSL 1.0.2) terminator — the
handshake validates natively against the existing pinned CA, and the
patch would *break* it. So it ships **off** (`ca_key_patch = 0`).

If you do enable it: the offset was extracted from MLoader's dump against
the archive.org English retail build. The `mercs2-securom-bypass` tool
edits `.text` and swaps an import (`cruise.dll` → `pmc_bb.dll`, same
length) but does not resize `.rdata`, so the offset should be stable —
verify by dumping 16 bytes at `0x768378` in the live process (expect a
128-byte placeholder, mostly zeros or a repeated pattern; real data there
means the offset moved). The write keeps a brief SecuROM-unpack poll.

## Acknowledgements

- **loganw234** — original author of the
  [Merc2Reborn / Mercenaries2](https://github.com/loganw234/Mercenaries2)
  project: all the multiplayer reverse-engineering, the hook design, and
  the FESL CA-key patch this port is built on.
- **u/Kunster_** on r/MercenariesGames for ongoing collaboration
  on the Mercenaries 2 modding stack.
- The **mercs2-qol-mods** authors for the SDK this mod plugs into.
- **Tsuda Kageyu** for MinHook (vendored by the SDK, BSD-2-Clause).

## License

MIT — same as the upstream Merc2Reborn project.
