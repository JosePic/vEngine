#include "game.h"
#include "sim.h"
#include "render.h"
#include "render_assets.h"
#include "physics_system.h"
#include "game_health_system.h"
#include "projectile.h"
#include <game_pool_data.h>


// ---------------------------------------------------------------------------
// System functions
// ---------------------------------------------------------------------------

static void SysEnemyAI(EntityPool* pool, const Query* q, float dt, void* user) {
    (void)dt;
    int playerIdx = *(const int*)user;
    UpdateEnemyAI_Query(pool, playerIdx, q->entities, q->count);
}

static void SysCombat(EntityPool* pool, const Query* q, float dt, void* user) {
    (void)user;
    UpdateCombat_Query(pool, dt, q->entities, q->count);
}

// Movement is now handled by the physics module.
// qMovement is kept below so kinematic (non-physics) movement can be added
// here later without touching query registration.
static void SysMovement(EntityPool* pool, const Query* q, float dt, void* user) {
    PhysicsSystem_ECSRun(pool, q, dt, user);
}

static void SysAnimation(EntityPool* pool, const Query* q, float dt, void* user) {
    (void)user;
    UpdateAnimatedVisuals(pool, q->entities, q->count, dt);
}

static void SysDestroyCleanup(EntityPool* pool, const Query* q, float dt, void* user) {
    (void)dt;
    (void)user;
    CleanupPendingDestroy_Query(pool, q->entities, q->count);
}

// ---------------------------------------------------------------------------
// Query registration
// ---------------------------------------------------------------------------

static void EnsureQueries(FrameState* fs)
{
    if (fs->queryDefCount != 0)
        return;

    fs->queryDefs[fs->queryDefCount++] = (QueryDef){
        .name = "Movement",
        .query = &fs->qMovement,
        .required = COMP_POSITION | COMP_VELOCITY | COMP_ALIVE | COMP_DYNAMIC_MOVILITY,
        .excluded = 0,
        .enabled = true,
    };
    fs->queryDefs[fs->queryDefCount++] = (QueryDef){
        .name = "EnemyAI",
        .query = &fs->qEnemyAI,
        .required = COMP_POSITION | COMP_ENEMY_AI | COMP_ALIVE,
        .excluded = 0,
        .enabled = true,
    };
    fs->queryDefs[fs->queryDefCount++] = (QueryDef){
        .name = "Combat",
        .query = &fs->qCombat,
        .required = COMP_POSITION | COMP_HEALTH | COMP_ALIVE,
        .excluded = 0,
        .enabled = true,
    };
    fs->queryDefs[fs->queryDefCount++] = (QueryDef){
        .name = "Projectiles",
        .query = &fs->qProjectiles,
        .required = COMP_IS_PROJECTILE | COMP_ALIVE,
        .excluded = 0,
        .enabled = true,
    };
    fs->queryDefs[fs->queryDefCount++] = (QueryDef){
        .name = "Destroy",
        .query = &fs->qDestroy,
        .required = COMP_PENDING_DESTROY,
        .excluded = 0,
        .enabled = true,
    };
    fs->queryDefs[fs->queryDefCount++] = (QueryDef){
        .name = "Renderable",
        .query = &fs->qRenderable,
        .required = COMP_RENDERABLE | COMP_POSITION | COMP_ALIVE,
        .excluded = 0,
        .enabled = true,
    };
    fs->queryDefs[fs->queryDefCount++] = (QueryDef){
        .name = "Selectable",
        .query = &fs->qSelectable,
        .required = COMP_PLAYER_CONTROL | COMP_POSITION | COMP_ALIVE,
        .excluded = 0,
        .enabled = true,
    };
    // Physics entities: must have COMP_PHYSICS in addition to the standard
    // dynamic movement requirements.  This keeps non-physics kinematic entities
    // (if any are added later) out of the physics pipeline.
    fs->queryDefs[fs->queryDefCount++] = (QueryDef){
        .name = "Physics",
        .query = &fs->qPhysics,
        .required = COMP_PHYSICS | COMP_POSITION | COMP_VELOCITY
                  | COMP_ALIVE | COMP_DYNAMIC_MOVILITY,
        .excluded = 0,
        .enabled = true,
    };
    fs->queryDefs[fs->queryDefCount++] = (QueryDef){
        .name = "Animated",
        .query = &fs->qAnimated,
        .required = COMP_MODEL | COMP_ANIMATED | COMP_ALIVE,
        .excluded = 0,
        .enabled = true,
    };
    fs->queryDefs[fs->queryDefCount++] = (QueryDef){
    .name = "Health",
    .query = &fs->qHealth,
    .required = COMP_ALIVE,
    .excluded = 0,
    .enabled = true,
    };
}

static void BuildQueries(EntityPool* pool, FrameState* fs)
{
    EnsureQueries(fs);
    BuildRegisteredQueries(pool, fs->queryDefs, fs->queryDefCount);
}

// ---------------------------------------------------------------------------
// System registration
// ---------------------------------------------------------------------------

static void EnsureSystems(FrameState* fs, System* systems, int* systemCount) {
    if (*systemCount != 0)
        return;
    RegisterSystem(systems, systemCount, 16, "EnemyAI", &fs->qEnemyAI, SysEnemyAI, PHASE_AI);
    RegisterSystem(systems, systemCount, 16, "Combat", &fs->qCombat, SysCombat, PHASE_COMBAT);
    RegisterSystem(systems, systemCount, 16, "Movement", &fs->qPhysics, SysMovement, PHASE_MOVEMENT);
    // Run projectile hits AFTER physics so we detect bouncing projectiles correctly
    RegisterSystem(systems, systemCount, 16, "ProjectileHits", &fs->qProjectiles, SysProjectileHits, PHASE_MOVEMENT);
    RegisterSystem(systems, systemCount, 16, "Animation", &fs->qAnimated, SysAnimation, PHASE_ANIMATION);
    RegisterSystem(systems, systemCount, 16, "DestroyCleanup", &fs->qDestroy, SysDestroyCleanup, PHASE_CLEANUP);
    RegisterSystem(systems, systemCount, 16, "HealthCleanup", &fs->qHealth, SysHealthCleanup, PHASE_CLEANUP);
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

static void Simulate(EntityPool* pool, int playerIdx, float dt, FrameState* fs,
    System* systems, int* systemCount) {
    EnsureSystems(fs, systems, systemCount);

    RunSystems(systems, *systemCount, PHASE_AI, pool, dt, &playerIdx);
    RunSystems(systems, *systemCount, PHASE_COMBAT, pool, dt, &playerIdx);
    RunSystems(systems, *systemCount, PHASE_MOVEMENT, pool, dt, &playerIdx);
    RunSystems(systems, *systemCount, PHASE_ANIMATION, pool, dt, &playerIdx);
}



void RunFrame(EntityPool* pool, Camera* camera, int playerIdx,
    bool selected[MAX_ENTITIES], FrameState* fs,
    float* camYaw, float* camPitch, float* camDist) {
    static System systems[16];
    static int systemCount = 0;
    static uint32_t frameCounter = 0;
    frameCounter++;


    VInspect_BeginFrame(&g_InspectorDB, frameCounter);
    float dt = GetFrameTime();

    memcpy(pool->prevPositions, pool->positions, sizeof(Vector3) * pool->count);

    pool->colors[playerIdx] = PURPLE;

    bool camRotate =
        IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT) ||
        IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    
    GamePool_BeginFrame();

    Vector3 camFocus = GetSelectionCenter(pool, selected, playerIdx);
    UpdateIsometricCamera(camera, camFocus, camYaw, camPitch, camDist, camRotate);

    UpdateSpawning(pool, playerIdx, dt, fs);

    BuildQueries(pool, fs);

    GatherInput(pool, *camera, playerIdx, selected, fs);

    VInspect_BeginSystem(&g_InspectorDB, SYS_COMBAT);
    Simulate(pool, playerIdx, dt, fs, systems, &systemCount);
    VInspect_EndSystem(&g_InspectorDB);

    VInspect_BeginSystem(&g_InspectorDB, SYS_RESOLVER);
    IR_Run(&gamePool.resolver);
    GamePool_ApplyChanges();
    VInspect_EndSystem(&g_InspectorDB);

    VInspect_BeginSystem(&g_InspectorDB, SYS_CLEANUP);
    CleanupDead(pool);
    VInspect_EndSystem(&g_InspectorDB);
    // Structural changes happened during sim — rebuild so later phases
    // (cleanup, render) see a consistent entity set.
    BuildQueries(pool, fs);
    SysHealthGetAllUnitsHP(pool, &fs->qHealth);
    EnsureSystems(fs, systems, &systemCount);
    RunSystems(systems, systemCount, PHASE_CLEANUP, pool, dt, &playerIdx);

    RenderFrame(pool, *camera, playerIdx, selected, fs->selecting,
        fs->selectStart, fs->selectEnd, &fs->qRenderable, dt);
}