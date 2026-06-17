#pragma once

#include "query.h"
#include "system.h"
#include "vinspector.h"

// 1. Declare it globally
VInspectorDB g_InspectorDB;


// Define engine-wide IDs for your systems
#define SYS_COMBAT 1
#define SYS_RESOLVER 2
#define SYS_CLEANUP 3

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