#pragma once

#include "query.h"
#include "system.h"

typedef struct {
    Ray ray;
    bool selecting;
    Vector2 selectStart;
    Vector2 selectEnd;
    float enemySpawnTimer;
    Vector3 lastCommandPoint;

    Query qMovement;
    Query qEnemyAI;
    Query qCombat;
    Query qProjectiles;
    Query qDestroy;
    Query qRenderable;
    Query qSelectable;
    Query qPhysics;
    Query qAnimated;
    Query qHealth;

    QueryDef queryDefs[10];
    int queryDefCount;
} FrameState;

void RunFrame(EntityPool* pool, Camera* camera, int playerIdx,
    bool selected[MAX_ENTITIES], FrameState* fs,
    float* camYaw, float* camPitch, float* camDist);