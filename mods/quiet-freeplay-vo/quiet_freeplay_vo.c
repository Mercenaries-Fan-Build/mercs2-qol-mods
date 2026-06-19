/**
 * quiet_freeplay_vo.asi — Mute / throttle Mercenaries 2's freeplay nag & proximity VO
 *
 * Mercenaries 2: World in Flames plays a class of low-priority "freeplay" voice-over:
 *   - the idle "nag" (Fiona.Misc.NoState01/02) that fires when you're not on a mission,
 *   - proximity / point-of-interest hints (Fiona.Hints.01..33) as you drive around.
 *
 * Both are routed through the native VO.Cue / VO.CueWithoutSubtitles bindings — but so
 * is mission/briefing/cinematic dialog. We discriminate by the cue-name string: only
 * cues whose name starts with a configured prefix are suppressed/throttled.
 *
 * Built on the Mercs2 mod stdlib (sdk/m2):
 *   - MinHook .text detours on VO.Cue / VO.CueWithoutSubtitles (SecuROM-safe; a .rdata
 *     reg-slot patch trips anti-tamper).
 *   - The detour is DORMANT until armed — m2_loadtrigger arms it the moment the world
 *     load reaches "WORLD LOAD START", so the shell and menus are never touched.
 *
 * Modes (quiet_freeplay_vo.ini next to the .asi):
 *   mode = off | throttle | silence
 *
 * Install: copy quiet_freeplay_vo.asi + quiet_freeplay_vo.ini to <game>/scripts/
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "m2.h"

/* Lua C function shape (the float-build Lua 5.1 ABI; m2_luastack uses void* L). */
typedef int (__cdecl *lua_CFunction)(void* L);

/* --- Config --- */

#define MODE_OFF        0
#define MODE_THROTTLE   1
#define MODE_SILENCE    2

#define MAX_PREFIXES        16
#define MAX_PREFIX_LEN      64

static int   g_mode        = MODE_THROTTLE;
static DWORD g_throttleMs   = 900 * 1000;     /* 15 minutes */
static int   g_logAllCues   = 1;
static int   g_armPhase     = 10;             /* WORLD LOAD START */
static char  g_prefixes[MAX_PREFIXES][MAX_PREFIX_LEN];
static int   g_prefixCount  = 0;

/* --- Runtime state --- */

static HMODULE g_hModule = NULL;
static lua_CFunction g_origCue = NULL;        /* MinHook trampolines (call-original) */
static lua_CFunction g_origCueNoSubs = NULL;
static volatile LONG g_armed = 0;
static volatile LONG g_lastAllowedTick = 0;
static volatile LONG g_hasAllowed = 0;

/* --- Config parsing --- */

static void AddPrefix(const char* p) {
    if (g_prefixCount >= MAX_PREFIXES || !p || !p[0]) return;
    lstrcpynA(g_prefixes[g_prefixCount], p, MAX_PREFIX_LEN);
    g_prefixCount++;
}

static void SetDefaultPrefixes(void) {
    g_prefixCount = 0;
    AddPrefix("Fiona.Misc.NoState");  /* idle nag */
    AddPrefix("Fiona.Hints.");        /* proximity / POI hints */
}

static void ParsePrefixList(char* value) {
    char* tok = value;
    char* p = value;
    g_prefixCount = 0;
    for (;;) {
        if (*p == ',' || *p == ';' || *p == '\0') {
            char saved = *p;
            char* s;
            *p = '\0';
            s = tok;
            while (*s == ' ' || *s == '\t') s++;
            AddPrefix(s);
            if (saved == '\0') break;
            tok = p + 1;
        }
        p++;
    }
}

static void OnConfigKV(void* ud, const char* key, const char* val) {
    (void)ud;
    if (lstrcmpiA(key, "mode") == 0) {
        if (lstrcmpiA(val, "off") == 0)          g_mode = MODE_OFF;
        else if (lstrcmpiA(val, "silence") == 0)  g_mode = MODE_SILENCE;
        else                                       g_mode = MODE_THROTTLE;
    } else if (lstrcmpiA(key, "throttle_seconds") == 0) {
        int secs = m2_ini_int(val, 900);
        if (secs < 1) secs = 1;
        g_throttleMs = (DWORD)secs * 1000;
    } else if (lstrcmpiA(key, "log_all_cues") == 0) {
        g_logAllCues = m2_ini_bool(val);
    } else if (lstrcmpiA(key, "arm_phase") == 0) {
        g_armPhase = m2_ini_int(val, 10);
    } else if (lstrcmpiA(key, "prefixes") == 0) {
        char tmp[512];
        lstrcpynA(tmp, val, (int)sizeof(tmp));
        ParsePrefixList(tmp);
        if (g_prefixCount == 0) SetDefaultPrefixes();
    }
}

static void LoadConfig(void) {
    char ini[MAX_PATH];
    g_mode = MODE_THROTTLE;
    g_throttleMs = 900 * 1000;
    g_logAllCues = 1;
    g_armPhase = 10;
    SetDefaultPrefixes();
    m2_module_path(g_hModule, "quiet_freeplay_vo.ini", ini, (int)sizeof(ini));
    if (!m2_ini_parse(ini, OnConfigKV, NULL))
        m2_logf("no quiet_freeplay_vo.ini (%s); using defaults", ini);
}

/* --- Cue classification --- */

/* Returns 1 and fills `matched` if any string arg starts with a configured prefix;
 * also reports the first string arg in `first` (for logging). */
static int FindMatchingCue(void* L, char* matched, int matched_sz, char* first, int first_sz) {
    int nargs = m2_lua_nargs(L);
    int i, p, got_first = 0;
    char buf[256];
    if (matched_sz) matched[0] = '\0';
    if (first_sz) first[0] = '\0';
    if (nargs <= 0) return 0;
    for (i = 0; i < nargs; i++) {
        if (!m2_lua_arg_string(L, i, buf, (int)sizeof(buf))) continue;
        if (!got_first) { lstrcpynA(first, buf, first_sz); got_first = 1; }
        for (p = 0; p < g_prefixCount; p++) {
            int plen = (int)strlen(g_prefixes[p]);
            if (_strnicmp(buf, g_prefixes[p], plen) == 0) {
                lstrcpynA(matched, buf, matched_sz);
                return 1;
            }
        }
    }
    return 0;
}

/* 1 = drop, 0 = pass through. */
static int DecideSuppress(void* L, const char* who) {
    char matched[256], first[256];
    int hit;

    if (!g_armed) return 0;  /* dormant until world load begins */

    hit = FindMatchingCue(L, matched, sizeof(matched), first, sizeof(first));
    if (g_logAllCues && first[0])
        m2_logf("[%s] cue=\"%s\"%s", who, first, hit ? "  (freeplay class)" : "");
    if (!hit) return 0;
    if (g_mode == MODE_OFF) return 0;
    if (g_mode == MODE_SILENCE) {
        m2_logf("[%s] SILENCE \"%s\"", who, matched);
        return 1;
    }
    /* throttle: one allowed per interval across the whole class */
    {
        DWORD now = GetTickCount();
        DWORD last = (DWORD)g_lastAllowedTick;
        if (!g_hasAllowed || (now - last) >= g_throttleMs) {
            InterlockedExchange(&g_lastAllowedTick, (LONG)now);
            InterlockedExchange(&g_hasAllowed, 1);
            m2_logf("[%s] ALLOW (throttle window open) \"%s\"", who, matched);
            return 0;
        }
        m2_logf("[%s] THROTTLE-DROP \"%s\" (%lus left)", who, matched,
                (unsigned long)((g_throttleMs - (now - last)) / 1000));
        return 1;
    }
}

static int __cdecl Hook_Cue(void* L) {
    if (DecideSuppress(L, "Cue")) return 0;
    return g_origCue ? g_origCue(L) : 0;
}

static int __cdecl Hook_CueNoSubs(void* L) {
    if (DecideSuppress(L, "CueNoSubs")) return 0;
    return g_origCueNoSubs ? g_origCueNoSubs(L) : 0;
}

/* --- Load-phase arm --- */

static void OnArmPhase(int reached_idx, void* ud) {
    (void)ud;
    InterlockedExchange(&g_armed, 1);
    m2_logf("ARMED at phase %d (%s) — freeplay VO filtering active",
            reached_idx, m2_loadtrigger_phase_name(reached_idx));
}

/* --- Worker --- */

static const char* ModeName(void) {
    switch (g_mode) {
        case MODE_OFF: return "off";
        case MODE_SILENCE: return "silence";
        default: return "throttle";
    }
}

static DWORD WINAPI WorkerThread(LPVOID param) {
    int i, okCue, okNoSubs;
    (void)param;

    m2_logf("config: mode=%s throttle_seconds=%lu log_all_cues=%d arm_phase=%d (%s) prefixes=%d",
            ModeName(), (unsigned long)(g_throttleMs / 1000), g_logAllCues,
            g_armPhase, m2_loadtrigger_phase_name(g_armPhase), g_prefixCount);
    for (i = 0; i < g_prefixCount; i++) m2_logf("  prefix[%d] = \"%s\"", i, g_prefixes[i]);

    if (!m2_hook_init()) {
        m2_logf("ERROR: MinHook init failed — mod inactive");
        return 0;
    }

    /* Install VO detours dormant (they pass through until g_armed is set). */
    okCue = m2_hook_attach((void*)M2_VO_CUE_VA, (void*)Hook_Cue, (void**)&g_origCue);
    okNoSubs = m2_hook_attach((void*)M2_VO_CUEWITHOUTSUBS_VA, (void*)Hook_CueNoSubs,
                              (void**)&g_origCueNoSubs);
    m2_logf("VO.Cue hook=%s  VO.CueWithoutSubtitles hook=%s",
            okCue ? "OK" : "FAIL", okNoSubs ? "OK" : "FAIL");
    if (!okCue && !okNoSubs) {
        m2_logf("ERROR: no VO hook installed — mod inactive");
        return 0;
    }

    /* Arm when the world load reaches the configured phase (default WORLD LOAD START). */
    m2_loadtrigger_on_phase(g_armPhase, OnArmPhase, NULL);
    if (m2_loadtrigger_install())
        m2_logf("load-trigger installed (watching the game log stream)");
    else
        m2_logf("WARN: load-trigger install failed — VO filtering will stay dormant");

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hModule = (HMODULE)hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        m2_log_init(g_hModule);
        m2_logf("quiet_freeplay_vo.asi loaded (PID %lu)", (unsigned long)GetCurrentProcessId());
        LoadConfig();
        CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        m2_logf("quiet_freeplay_vo.asi unloaded");
        m2_log_close();
    }
    return TRUE;
}
