#pragma once

#include "query.h"

typedef enum {
  PHASE_AI = 0,
  PHASE_COMBAT = 1,
  PHASE_MOVEMENT = 2,
  PHASE_ANIMATION = 3,
  PHASE_CLEANUP = 4,
  PHASE_COUNT
} SystemPhase;

typedef void (*SystemFn)(EntityPool *pool, const Query *q, float dt, void *user);

typedef struct {
  const char *name;
  const Query *query;
  SystemFn fn;
  SystemPhase phase;
  bool enabled;
} System;

void RegisterSystem(System *systems, int *systemCount, int maxSystems,
                    const char *name, const Query *query, SystemFn fn,
                    SystemPhase phase);

void RunSystems(const System *systems, int systemCount, SystemPhase phase,
                EntityPool *pool, float dt, void *user);
