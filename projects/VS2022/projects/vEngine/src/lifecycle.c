#include "vengine.h"

void CleanupDead(EntityPool *pool) {
  for (int i = 0; i < pool->count; i++) {
    if (!(pool->masks[i] & COMP_ALIVE))
      continue;
    if (pool->health[i] <= 0) {
      MarkPendingDestroy(pool, i);
    }
  }
}

void MarkPendingDestroy(EntityPool *pool, int entity) {
  if (entity < 0 || entity >= pool->count)
    return;

  pool->masks[entity] &= ~COMP_ALIVE;
  pool->masks[entity] |= COMP_PENDING_DESTROY;

  // stop movement intent
  pool->hasMoveTarget[entity] = false;
  pool->masks[entity] &= ~COMP_MOVE_TARGET;

  // stop motion
  pool->velocities[entity] = (Vector3){0};
}

void CleanupPendingDestroy_Query(EntityPool *pool, const int *entities, int count) {
  for (int k = 0; k < count; k++) {
    int i = entities[k];
    pool->masks[i] &= ~COMP_PENDING_DESTROY;
  }
}
