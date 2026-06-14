#include "vengine.h"

// TODO: Does this need error checking?
// NOTE: This ignores Y axis.
void SetAIUnitMoveTargetVector(EntityPool* pool, Vector3 target) {
    for (int i = 0; i < pool->count; i++) {
        ComponentMask mask = pool->masks[i];
        if (!(mask & COMP_ALIVE))
            continue;
        if (!(mask & COMP_ENEMY_AI))
            continue;
        pool->hasMoveTarget[i] = true;
        pool->moveTargets[i] = (Vector3){ target.x, pool->positions[i].y, target.z };
    }
}



void AIUnitFollowPlayer(EntityPool* pool, int playerIdx) {
    for (int i = 0; i < pool->count; i++) {
        ComponentMask mask = pool->masks[i];
        if (!(mask & COMP_ALIVE))
            continue;
        if (!(mask & COMP_ENEMY_AI))
            continue;

        Vector3 p = pool->positions[playerIdx];
        pool->hasMoveTarget[i] = true;
        pool->moveTargets[i] = (Vector3){ p.x, pool->positions[i].y, p.z };
    }
}



void UpdateEnemyAI_Query(EntityPool *pool, int playerIdx, const int *entities, int count) {
  for (int k = 0; k < count; k++) {
    int i = entities[k];
    Vector3 p = pool->positions[playerIdx];
    Vector3 worldOrigin = { 0,pool->positions[i].y,0 };
    SetAIUnitMoveTargetVector(pool, worldOrigin);
  }
}
