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
4. **FESL CA pubkey patch** *(on by default)* — replays MLoader's
   128-byte `.rdata` write at RVA `0x768378` so the game's FESL SSL
   stack trusts the server's cert. **Required for the FESL handshake:**
   that stack is a statically-linked OpenSSL that validates the server
   cert against a CA key baked into the EXE (not via `WinVerifyTrust`),
   and on the cracked EXE that key isn't EA's, so even the genuine
   OTG3-signed cert fails to validate without this. Verified live —
   with the patch off, the game resets every FESL handshake; on, it
   completes. Disable with `ca_key_patch = 0` only if you're serving a
   cert under a different CA and supplying your own key payload.

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
ca_key_patch = 1              ; 1 = patch the FESL CA key (required for the
                              ;     handshake); 0 only for a custom CA + payload
```

## Status

**Builds clean against the mercs2-qol-mods SDK.** Compiles with the SDK's
MinGW toolchain via the top-level `make`, links only system DLLs (no
libgcc runtime dependency), and the hooks/INI/logging wire through the
SDK helpers (`m2_hook_attach`, `m2_ini_parse`, `m2_logf`,
`m2_module_path`) with matching signatures.

**Validated live end-to-end** against a local FESL terminator presenting
the genuine EA cert. Confirmed in one session: the mod loads under
`pmc_bb`, all hooks arm, the `gethostbyname` redirect rewrites
`mercs2-pc.fesl.ea.com` to the server, the CA-key patch applies at
`0x768378`, and the game **completes the SSLv3/RC4 FESL handshake** and
sends its `fsys` Hello through the tunnel. With `ca_key_patch = 0` the
game reset every handshake; with it on, they completed — so the patch is
required, not optional (see below). The hooking model is also validated
upstream: loganw234's standalone Merc2Fix.asi ran end-to-end on a
`mercs2-securom-bypass` EXE; this port keeps that hook logic
byte-identical and only changes the SDK glue (Makefile, INI, logging).

## The CA-key patch is required (on by default)

`FESL_CA_KEY_RVA = 0x768378` overwrites the FESL CA pubkey the game's SSL
stack validates against. The FESL handshake uses a **statically-linked
OpenSSL** whose trust anchor is a CA key baked into the EXE's `.rdata` —
**not** `WinVerifyTrust` — so the cert blindfold (hook #2) does not cover
it. On the cracked EXE that baked-in key isn't EA's, so the server cert
fails to validate without this patch *even when it's the genuine
OTG3-signed cert*. This was confirmed live (handshake resets off / completes
on), and matches why MLoader and upstream Merc2Fix apply it unconditionally.
It ships **on** (`ca_key_patch = 1`). The clock-spoof (hook #3) handles the
cert's expiry (valid 2008–2024); the patch handles trust.

The offset was extracted from MLoader's dump against the archive.org
English retail build. The `mercs2-securom-bypass` tool edits `.text` and
swaps an import (`cruise.dll` → `pmc_bb.dll`, same length) but does not
resize `.rdata`, so it's stable on the bypass target — verified applying
cleanly there with no anti-tamper trip. If you port to a different EXE,
re-check by dumping 16 bytes at `0x768378` in the live process.

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
