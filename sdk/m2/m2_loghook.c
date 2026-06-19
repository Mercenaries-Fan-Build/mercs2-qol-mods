#include "m2_loghook.h"
#include "m2_hook.h"
#include "m2_luastack.h"
#include "m2_target.h"
#include <windows.h>

#define MAX_LISTENERS 16

typedef struct { m2_log_listener cb; void* ud; } Listener;

static Listener g_listeners[MAX_LISTENERS];
static int g_listenerCount = 0;
static volatile LONG g_installed = 0;
static volatile LONG g_inHook = 0;
static void* g_origStub = NULL;  /* MinHook trampoline (unused no-op) */

int m2_loghook_add_listener(m2_log_listener cb, void* ud) {
    if (!cb || g_listenerCount >= MAX_LISTENERS) return 0;
    g_listeners[g_listenerCount].cb = cb;
    g_listeners[g_listenerCount].ud = ud;
    g_listenerCount++;
    return 1;
}

/* The detour. The stub is `xor eax,eax; ret`, reached by ~700 callers; only Lua
 * print/Debug.Printf pass a real lua_State*. m2_lua_join_strings rejects the rest
 * (returns -1), so non-Lua callers cost almost nothing and we behave like the
 * original no-op (return 0). We never call the trampoline (it only zeroed eax). */
static int __cdecl Hook_LogStub(void* L) {
    char msg[2048];
    int i, n;

    /* Re-entrancy guard: a listener must never cause another captured line. */
    if (InterlockedCompareExchange(&g_inHook, 1, 0) != 0) return 0;

    n = m2_lua_join_strings(L, msg, (int)sizeof(msg));
    if (n >= 1) {
        for (i = 0; i < g_listenerCount; i++) {
            g_listeners[i].cb(msg, g_listeners[i].ud);
        }
    }

    InterlockedExchange(&g_inHook, 0);
    return 0;
}

int m2_loghook_install(void) {
    if (InterlockedCompareExchange(&g_installed, 1, 0) != 0) return 1;
    if (!m2_hook_attach((void*)M2_LOG_STUB_VA, (void*)Hook_LogStub, &g_origStub)) {
        InterlockedExchange(&g_installed, 0);
        return 0;
    }
    return 1;
}
