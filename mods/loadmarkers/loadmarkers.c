/**
 * loadmarkers.asi — show world-load progress live, no PMC_VERBOSE_LOG needed.
 *
 * A minimal example of the SDK's log stream: it subscribes to the game's log
 * (via m2_loghook, which prefers pmc_bb's live pmc_log_subscribe on pmc_bb
 * >= 3.1.0 and falls back to tailing pmc_blackbox.log), echoes each captured
 * line to loadmarkers.log, and uses m2_loadtrigger to announce the moment the
 * player is actually in-game (phase 20, "GlobalExit - Complete").
 *
 * In pmc_bb's default markers-only mode you get the ~21 world-load milestones
 * with no verbose-logging perf cost; under PMC_VERBOSE_LOG you'd see every line.
 *
 * Install: copy loadmarkers.asi to <game>/scripts/. Watch loadmarkers.log.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "m2.h"

static HMODULE g_hModule = NULL;

/* Raw passthrough: every captured log line (milestones in pmc_bb's default mode,
 * or all lines under PMC_VERBOSE_LOG). Keep it cheap — it runs on the log path. */
static void OnLogLine(const char* line, void* ud) {
    (void)ud;
    m2_logf("%s", line);
}

/* One-shot high-level event: fires when the world finishes loading and the
 * player is controllable (not the early "player spawn" at phase 11). */
static void OnInGame(int reached_idx, void* ud) {
    (void)ud;
    m2_logf(">>> PLAYER IN-GAME (phase %d: %s)",
            reached_idx, m2_loadtrigger_phase_name(reached_idx));
}

static DWORD WINAPI WorkerThread(LPVOID param) {
    (void)param;

    /* Needed only for the no-pmc_bb fallback (self-hooking the log stub); the
     * subscribe/tail paths don't use MinHook, but init defensively. */
    if (!m2_hook_init()) {
        m2_logf("WARN: MinHook init failed — load markers may be unavailable "
                "if pmc_bb isn't present");
    }

    /* Raw stream + a high-level "now in-game" trigger. Register both before the
     * install call that subscribes to the log source. */
    m2_loghook_add_listener(OnLogLine, NULL);
    m2_loadtrigger_on_phase(M2_PHASE_REACHED_WORLD_IDX, OnInGame, NULL);

    if (m2_loadtrigger_install())
        m2_logf("armed — watching the load stream (no PMC_VERBOSE_LOG needed; "
                "pmc_bb >= 3.1.0 markers-only). Waiting for the world to load.");
    else
        m2_logf("ERROR: load-trigger install failed — no log source");

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hModule = (HMODULE)hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        m2_log_init(g_hModule);
        m2_logf("loadmarkers.asi loaded (PID %lu)",
                (unsigned long)GetCurrentProcessId());
        CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        m2_log_close();
    }
    return TRUE;
}
