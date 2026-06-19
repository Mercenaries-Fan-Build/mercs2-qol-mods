/**
 * quiet_freeplay_vo.asi — Mute / throttle Mercenaries 2's freeplay nag & proximity VO
 *
 * Mercenaries 2: World in Flames plays a class of low-priority "freeplay" voice-over:
 *   - the idle "nag" (Fiona.Misc.NoState01/02) that fires when you're not on a mission,
 *   - proximity / point-of-interest hints (Fiona.Hints.01..33) as you drive around.
 *
 * Both classes are routed through the native VO.Cue / VO.CueWithoutSubtitles bindings.
 * Mission/briefing/cinematic dialog also uses those bindings, so we discriminate by the
 * cue-name string argument: only cues whose name starts with a configured prefix are
 * treated as "freeplay" and suppressed/throttled; everything else passes through.
 *
 * Technique (mirrors tools/lua_enum_asi + tools/net_hooks_asi): we redirect the
 * VO.Cue / VO.CueWithoutSubtitles function pointers in the engine's .rdata luaL_Reg
 * table to our hooks, saving the originals. On pass-through we call the saved original
 * directly (its code is untouched, so there is no trampoline and no recursion).
 *
 * Modes (quiet_freeplay_vo.ini next to the .asi):
 *   mode = off        — observe only (logs cues, suppresses nothing)
 *   mode = throttle    — allow at most one matching cue per throttle_seconds (default)
 *   mode = silence    — drop every matching cue
 *
 * Tuning: with log_all_cues = 1 the log lists every cue name VO.Cue is asked to play,
 * so you can confirm the real cue strings on your build and extend `prefixes`.
 *
 * Target: cracked retail EXE (53,482,288 bytes), image base 0x00400000. Section VAs and
 * the documented VO addresses below are binary-specific.
 *
 * Build:   make            (MinGW cross-compile)
 * Install: copy quiet_freeplay_vo.asi + quiet_freeplay_vo.ini to <game>/scripts/
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* --- Section layout (cracked retail EXE, image base 0x00400000) --- */

#define RDATA_START_VA      0x00B05000
#define RDATA_SIZE          0x000F1000
#define TEXT_START_VA       0x00401000
#define TEXT_SIZE           0x00703000

/* Documented (CERTAIN) VO binding code addresses — used only to cross-check the
 * dynamically resolved reg-table slots; the hook itself works off the reg table. */
#define VO_CUE_DOC_VA               0x005E9DE0
#define VO_CUEWITHOUTSUBS_DOC_VA    0x005E9F40

/* --- Lua 5.1 (float-number build: TValue is 8 bytes) --- */

#define LUA_TSTRING     4
#define LUA_TFUNCTION   6

/* lua_State field offsets (Lua 5.1, 32-bit): top @ +8, base @ +0x0C
 * (confirmed by tools/lua_enum_asi and tools/net_hooks_asi). */
#define LSTATE_TOP_OFF      0x08
#define LSTATE_BASE_OFF     0x0C

/* Offset from a TString GCObject pointer to the char data, 32-bit/8-byte aligned. */
#define TSTRING_DATA_OFF    16

typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State* L);

/* TValue: { Value value (4, float build); int tt } */
typedef struct { DWORD value; int tt; } TValue;

/* --- Config --- */

#define MODE_OFF        0
#define MODE_THROTTLE   1
#define MODE_SILENCE    2

#define MAX_PREFIXES        16
#define MAX_PREFIX_LEN      64

static int   g_mode           = MODE_THROTTLE;
static DWORD g_throttleMs      = 900 * 1000;   /* 15 minutes */
static int   g_logAllCues      = 1;
static char  g_prefixes[MAX_PREFIXES][MAX_PREFIX_LEN];
static int   g_prefixCount     = 0;

/* --- Globals --- */

static HMODULE g_hModule = NULL;
static HANDLE  g_logFile = INVALID_HANDLE_VALUE;
static char    g_iniPath[MAX_PATH];

static lua_CFunction g_origCue = NULL;
static lua_CFunction g_origCueNoSubs = NULL;

static volatile LONG g_lastAllowedTick = 0;
static volatile LONG g_hasAllowed = 0;

/* --- Logging --- */

static void LogInit(void) {
    char path[MAX_PATH];
    char* dot;
    GetModuleFileNameA(g_hModule, path, MAX_PATH);
    dot = strrchr(path, '.');
    if (dot) strcpy(dot, ".log");
    else strcat(path, ".log");
    g_logFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ,
                            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void Log(const char* fmt, ...) {
    char buf[1024];
    int len;
    va_list ap;
    DWORD written;
    if (g_logFile == INVALID_HANDLE_VALUE) return;
    va_start(ap, fmt);
    len = wvsprintfA(buf, fmt, ap);
    va_end(ap);
    if (len <= 0) return;
    buf[len] = '\r';
    buf[len + 1] = '\n';
    WriteFile(g_logFile, buf, len + 2, &written, NULL);
    FlushFileBuffers(g_logFile);
}

static void LogClose(void) {
    if (g_logFile != INVALID_HANDLE_VALUE) {
        CloseHandle(g_logFile);
        g_logFile = INVALID_HANDLE_VALUE;
    }
}

/* --- .ini parsing --- */

static char* TrimInPlace(char* s) {
    char* end;
    while (*s == ' ' || *s == '\t') s++;
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return s;
}

static void AddPrefix(const char* p) {
    if (g_prefixCount >= MAX_PREFIXES) return;
    if (!p || !p[0]) return;
    lstrcpynA(g_prefixes[g_prefixCount], p, MAX_PREFIX_LEN);
    g_prefixCount++;
}

static void ParsePrefixList(char* value) {
    /* comma- or semicolon-separated */
    char* tok = value;
    char* p = value;
    g_prefixCount = 0;
    for (;;) {
        if (*p == ',' || *p == ';' || *p == '\0') {
            char saved = *p;
            *p = '\0';
            AddPrefix(TrimInPlace(tok));
            if (saved == '\0') break;
            tok = p + 1;
        }
        p++;
    }
}

static void SetDefaultPrefixes(void) {
    g_prefixCount = 0;
    AddPrefix("Fiona.Misc.NoState");  /* idle nag */
    AddPrefix("Fiona.Hints.");        /* proximity / POI hints */
}

static void LoadConfig(void) {
    HANDLE hf;
    DWORD size, read;
    char* data;
    char* line;
    char* next;
    char* slash;

    /* default config */
    g_mode = MODE_THROTTLE;
    g_throttleMs = 900 * 1000;
    g_logAllCues = 1;
    SetDefaultPrefixes();

    /* ini path = <module dir>\quiet_freeplay_vo.ini */
    GetModuleFileNameA(g_hModule, g_iniPath, MAX_PATH);
    slash = strrchr(g_iniPath, '\\');
    if (!slash) slash = strrchr(g_iniPath, '/');
    if (slash) *(slash + 1) = '\0'; else g_iniPath[0] = '\0';
    strcat(g_iniPath, "quiet_freeplay_vo.ini");

    hf = CreateFileA(g_iniPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        Log("No quiet_freeplay_vo.ini found (%s); using defaults", g_iniPath);
        return;
    }
    size = GetFileSize(hf, NULL);
    if (size == INVALID_FILE_SIZE || size == 0 || size > 65536) {
        CloseHandle(hf);
        Log("ini empty/too large; using defaults");
        return;
    }
    data = (char*)HeapAlloc(GetProcessHeap(), 0, size + 1);
    if (!data) { CloseHandle(hf); return; }
    if (!ReadFile(hf, data, size, &read, NULL)) read = 0;
    data[read] = '\0';
    CloseHandle(hf);

    line = data;
    while (line && *line) {
        char* eq;
        char* key;
        char* val;
        char* semi;
        next = strchr(line, '\n');
        if (next) *next = '\0';

        /* strip inline comments and section headers */
        semi = strchr(line, ';');
        if (semi) *semi = '\0';
        line = TrimInPlace(line);
        if (line[0] == '\0' || line[0] == '[' || line[0] == '#') {
            line = next ? next + 1 : NULL;
            continue;
        }
        eq = strchr(line, '=');
        if (!eq) { line = next ? next + 1 : NULL; continue; }
        *eq = '\0';
        key = TrimInPlace(line);
        val = TrimInPlace(eq + 1);

        if (lstrcmpiA(key, "mode") == 0) {
            if (lstrcmpiA(val, "off") == 0)          g_mode = MODE_OFF;
            else if (lstrcmpiA(val, "silence") == 0)  g_mode = MODE_SILENCE;
            else                                       g_mode = MODE_THROTTLE;
        } else if (lstrcmpiA(key, "throttle_seconds") == 0) {
            int secs = 0;
            int i;
            for (i = 0; val[i] >= '0' && val[i] <= '9'; i++) secs = secs * 10 + (val[i] - '0');
            if (secs < 1) secs = 1;
            g_throttleMs = (DWORD)secs * 1000;
        } else if (lstrcmpiA(key, "log_all_cues") == 0) {
            g_logAllCues = (val[0] == '1' || lstrcmpiA(val, "true") == 0) ? 1 : 0;
        } else if (lstrcmpiA(key, "prefixes") == 0) {
            ParsePrefixList(val);
            if (g_prefixCount == 0) SetDefaultPrefixes();
        }
        line = next ? next + 1 : NULL;
    }
    HeapFree(GetProcessHeap(), 0, data);
}

/* --- .rdata reg-table resolution --- */

static BOOL IsVaInRdata(DWORD va) { return va >= RDATA_START_VA && va < RDATA_START_VA + RDATA_SIZE; }
static BOOL IsVaInText(DWORD va)  { return va >= TEXT_START_VA && va < TEXT_START_VA + TEXT_SIZE; }

static DWORD FindStringInRdata(const char* target) {
    BYTE* base = (BYTE*)RDATA_START_VA;
    size_t len = strlen(target);
    DWORD i;
    if (len == 0 || len >= RDATA_SIZE) return 0;
    for (i = 0; i + len < RDATA_SIZE; i++) {
        if (memcmp(base + i, target, len + 1) == 0) {   /* exact (incl NUL) */
            return RDATA_START_VA + i;
        }
    }
    return 0;
}

static BOOL RdataNameIsOneOf(DWORD name_va, const char* const* names) {
    int i;
    if (!IsVaInRdata(name_va)) return FALSE;
    for (i = 0; names[i]; i++) {
        if (strcmp((const char*)name_va, names[i]) == 0) return TRUE;
    }
    return FALSE;
}

/*
 * Find the {name_va, func_va} luaL_Reg slot for `name` that belongs to the VO table,
 * disambiguated by a sibling entry (Cancel / CueWithoutSubtitles / ...) within +-1 slot.
 * Returns the VA of the func_va dword (the slot to patch) and the original func.
 */
static BOOL ResolveVoRegSlot(const char* name, DWORD* outSlotVa, lua_CFunction* outFunc) {
    static const char* const siblings[] = {
        "Cancel", "CueWithoutSubtitles", "Cue", "AddSequence",
        "RemoveSequence", "SetCinematicMode", NULL
    };
    BYTE* rdata = (BYTE*)RDATA_START_VA;
    DWORD str_va = FindStringInRdata(name);
    DWORD i;
    if (!str_va) return FALSE;

    for (i = 0; i + 8 < RDATA_SIZE; i += 4) {
        DWORD nm = *(DWORD*)(rdata + i);
        DWORD fn;
        if (nm != str_va) continue;
        fn = *(DWORD*)(rdata + i + 4);
        if (!IsVaInText(fn)) continue;

        /* validate via neighbouring reg entry name (entries are 8 bytes) */
        {
            BOOL ok = FALSE;
            if (i >= 8)  ok = ok || RdataNameIsOneOf(*(DWORD*)(rdata + i - 8), siblings);
            if (i + 16 < RDATA_SIZE) ok = ok || RdataNameIsOneOf(*(DWORD*)(rdata + i + 8), siblings);
            if (!ok) continue;
        }
        *outSlotVa = RDATA_START_VA + i + 4;
        *outFunc = (lua_CFunction)fn;
        return TRUE;
    }
    return FALSE;
}

static BOOL PatchSlot(DWORD slotVa, DWORD newVal) {
    DWORD* slot = (DWORD*)slotVa;
    DWORD old;
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) return FALSE;
    *slot = newVal;
    VirtualProtect(slot, 4, old, &old);
    return TRUE;
}

/* --- read cue-name string args off the Lua stack --- */

static const char* ReadStackString(TValue* tv) {
    const char* s;
    if (!tv || IsBadReadPtr(tv, sizeof(TValue))) return NULL;
    if (tv->tt != LUA_TSTRING) return NULL;
    if (tv->value == 0) return NULL;
    s = (const char*)(tv->value + TSTRING_DATA_OFF);
    if (IsBadReadPtr((void*)s, 1)) return NULL;
    return s;
}

/* Returns the first string argument that matches a configured prefix, or the first
 * string argument at all (for logging) via outFirst. */
static const char* FindMatchingCue(lua_State* L, const char** outFirst) {
    TValue* base = *(TValue**)((BYTE*)L + LSTATE_BASE_OFF);
    TValue* top  = *(TValue**)((BYTE*)L + LSTATE_TOP_OFF);
    int nargs, i, p;
    *outFirst = NULL;
    if (!base || !top || top < base) return NULL;
    nargs = (int)(top - base);
    if (nargs > 8) nargs = 8;

    for (i = 0; i < nargs; i++) {
        const char* s = ReadStackString(base + i);
        if (!s) continue;
        if (!*outFirst) *outFirst = s;
        for (p = 0; p < g_prefixCount; p++) {
            size_t plen = strlen(g_prefixes[p]);
            if (_strnicmp(s, g_prefixes[p], plen) == 0) {
                return s;  /* matched the freeplay class */
            }
        }
    }
    return NULL;
}

/* Decide: 1 = suppress (drop), 0 = pass through. Logs as configured. */
static int DecideSuppress(lua_State* L, const char* who) {
    const char* first = NULL;
    const char* match = FindMatchingCue(L, &first);

    if (g_logAllCues && first) {
        Log("[%s] cue=\"%s\"%s", who, first, match ? "  (freeplay class)" : "");
    }
    if (!match) return 0;                 /* not our class — never touch it */
    if (g_mode == MODE_OFF) return 0;     /* observe only */
    if (g_mode == MODE_SILENCE) {
        Log("[%s] SILENCE \"%s\"", who, match);
        return 1;
    }
    /* MODE_THROTTLE: allow one per interval (global across the class) */
    {
        DWORD now = GetTickCount();
        DWORD last = (DWORD)g_lastAllowedTick;
        if (!g_hasAllowed || (now - last) >= g_throttleMs) {
            InterlockedExchange(&g_lastAllowedTick, (LONG)now);
            InterlockedExchange(&g_hasAllowed, 1);
            Log("[%s] ALLOW (throttle window open) \"%s\"", who, match);
            return 0;
        }
        Log("[%s] THROTTLE-DROP \"%s\" (%lus left)", who, match,
            (unsigned long)((g_throttleMs - (now - last)) / 1000));
        return 1;
    }
}

static int Hook_Cue(lua_State* L) {
    if (DecideSuppress(L, "Cue")) return 0;   /* drop: push nothing, callers ignore retval */
    return g_origCue ? g_origCue(L) : 0;
}

static int Hook_CueNoSubs(lua_State* L) {
    if (DecideSuppress(L, "CueNoSubs")) return 0;
    return g_origCueNoSubs ? g_origCueNoSubs(L) : 0;
}

/* --- install --- */

static void InstallHooks(void) {
    DWORD slot;
    lua_CFunction orig;

    if (ResolveVoRegSlot("Cue", &slot, &orig)) {
        g_origCue = orig;
        Log("VO.Cue reg slot @0x%08X -> func 0x%08X%s", slot, (DWORD)orig,
            ((DWORD)orig == VO_CUE_DOC_VA) ? " (matches documented addr)" : "");
        if (PatchSlot(slot, (DWORD)Hook_Cue))
            Log("VO.Cue redirected to hook");
        else
            Log("ERROR: failed to patch VO.Cue slot");
    } else {
        Log("ERROR: could not resolve VO.Cue reg slot (mod inactive for Cue)");
    }

    if (ResolveVoRegSlot("CueWithoutSubtitles", &slot, &orig)) {
        g_origCueNoSubs = orig;
        Log("VO.CueWithoutSubtitles reg slot @0x%08X -> func 0x%08X%s", slot, (DWORD)orig,
            ((DWORD)orig == VO_CUEWITHOUTSUBS_DOC_VA) ? " (matches documented addr)" : "");
        if (PatchSlot(slot, (DWORD)Hook_CueNoSubs))
            Log("VO.CueWithoutSubtitles redirected to hook");
        else
            Log("ERROR: failed to patch VO.CueWithoutSubtitles slot");
    } else {
        Log("WARN: could not resolve VO.CueWithoutSubtitles reg slot");
    }
}

static const char* ModeName(void) {
    switch (g_mode) {
        case MODE_OFF: return "off";
        case MODE_SILENCE: return "silence";
        default: return "throttle";
    }
}

static DWORD WINAPI WorkerThread(LPVOID param) {
    int i;
    (void)param;

    /* Let the engine register its Lua bindings before we redirect them. */
    Sleep(6000);

    Log("config: mode=%s throttle_seconds=%lu log_all_cues=%d prefixes=%d",
        ModeName(), (unsigned long)(g_throttleMs / 1000), g_logAllCues, g_prefixCount);
    for (i = 0; i < g_prefixCount; i++) Log("  prefix[%d] = \"%s\"", i, g_prefixes[i]);

    InstallHooks();
    Log("quiet_freeplay_vo: install complete");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hModule = (HMODULE)hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        LogInit();
        Log("quiet_freeplay_vo.asi loaded (PID %lu)", (unsigned long)GetCurrentProcessId());
        LoadConfig();
        CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        Log("quiet_freeplay_vo.asi unloaded");
        LogClose();
    }
    return TRUE;
}
