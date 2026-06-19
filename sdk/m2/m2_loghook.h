/* m2_loghook.h — shared subscription to the game's log stream.
 *
 * MinHooks the shared no-op log stub (M2_LOG_STUB_VA): every Lua print /
 * Debug.Printf / stripped subsystem log line funnels through it. Each line's
 * string arguments are joined into a message and dispatched to all registered
 * listeners. This is the SDK's single event source for "what did the game just
 * say" — m2_loadtrigger is built on top of it, and mods can listen directly.
 */
#ifndef M2_LOGHOOK_H
#define M2_LOGHOOK_H

/* Called on the game thread for each captured log line (NUL-terminated message,
 * string args tab-joined). Keep it cheap and non-reentrant — do not call back
 * into anything that itself logs. */
typedef void (*m2_log_listener)(const char* msg, void* ud);

/* Register a listener. Returns 1 on success, 0 if the table is full. Register
 * before m2_loghook_install(). */
int m2_loghook_add_listener(m2_log_listener cb, void* ud);

/* MinHook the log stub and begin dispatching. Idempotent. Returns 1 on success. */
int m2_loghook_install(void);

#endif /* M2_LOGHOOK_H */
