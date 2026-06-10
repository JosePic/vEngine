#include "system.h"

// Minimal scheduler helpers.
// Keep explicit registration. No magic.

void RegisterSystem(System *systems, int *systemCount, int maxSystems,
                    const char *name, const Query *query, SystemFn fn,
                    SystemPhase phase) {
  if (*systemCount >= maxSystems)
    return;
  System *s = &systems[(*systemCount)++];
  s->name = name;
  s->query = query;
  s->fn = fn;
  s->phase = phase;
  s->enabled = true;
}

void RunSystems(const System *systems, int systemCount, SystemPhase phase,
                EntityPool *pool, float dt, void *user) {
  for (int i = 0; i < systemCount; i++) {
    const System *s = &systems[i];
    if (!s->enabled)
      continue;
    if (s->phase != phase)
      continue;
    s->fn(pool, s->query, dt, user);
  }
}
