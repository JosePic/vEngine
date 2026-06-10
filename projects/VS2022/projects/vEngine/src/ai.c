#include "vengine.h"

void UpdateEnemyAI(EntityPool *pool, int playerIdx) {
  for (int i = 0; i < pool->count; i++) {
    ComponentMask mask = pool->masks[i];
    if (!(mask & COMP_ALIVE))
      continue;
    if (!(mask & COMP_ENEMY_AI))
      continue;

    Vector3 p = pool->positions[playerIdx];
    pool->hasMoveTarget[i] = true;
    pool->moveTargets[i] = (Vector3){p.x, pool->positions[i].y, p.z};
  }
}

void UpdateEnemyAI_Query(EntityPool *pool, int playerIdx, const int *entities, int count) {
  for (int k = 0; k < count; k++) {
    int i = entities[k];
    Vector3 p = pool->positions[playerIdx];
    pool->hasMoveTarget[i] = true;
    pool->moveTargets[i] = (Vector3){p.x, pool->positions[i].y, p.z};
  }
}
