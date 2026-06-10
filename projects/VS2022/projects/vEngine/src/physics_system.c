// physics_system.c

#include "physics_system.h"
#include <math.h>
#include <string.h>
#include "query.h"

// ---------------------------------------------------------------------------
// Static SoA backing storage for the physics world.
//
// Dense: index 0..(physCount-1) maps to pool->physToEntity[0..physCount-1].
// Sized to PHYSICS_MAX_BODIES (5000) so grid/SIMD paths never overrun,
// even though MAX_ENTITIES is only 1024.
// ---------------------------------------------------------------------------
static float          s_posX[PHYSICS_MAX_BODIES];
static float          s_posY[PHYSICS_MAX_BODIES];
static float          s_velX[PHYSICS_MAX_BODIES];
static float          s_velY[PHYSICS_MAX_BODIES];
static float          s_radius[PHYSICS_MAX_BODIES];
static float          s_mass[PHYSICS_MAX_BODIES];
static PhysicsContact s_contacts[PHYSICS_MAX_CONTACTS];

PhysicsWorld gPhysicsWorld;

static void PhysicsSystem_UpdateMovementIntent(EntityPool* pool, const int* entities, int count)
{
    for (int qi = 0; qi < count; qi++)
    {
        const int i = entities[qi];
        const ComponentMask mask = pool->masks[i];

        if (!(mask & COMP_ALIVE))
            continue;

        if ((mask & (COMP_PLAYER_CONTROL | COMP_ENEMY_AI)) == 0)
            continue;

        if (pool->hasMoveTarget[i])
        {
            pool->masks[i] |= COMP_MOVE_TARGET;

            Vector3 to = Vector3Subtract(pool->moveTargets[i], pool->positions[i]);
            to.y = 0.0f;

            float dist = Vector3Length(to);
            if (dist < 0.15f)
            {
                pool->hasMoveTarget[i] = false;
                pool->masks[i] &= ~COMP_MOVE_TARGET;
                pool->velocities[i] = (Vector3){ 0.0f };
            }
            else
            {
                Vector3 dir = _Vector3Scale(to, 1.0f / dist);
                pool->velocities[i] = _Vector3Scale(dir, pool->moveSpeed[i]);
            }
        }
        else
        {
            pool->masks[i] &= ~COMP_MOVE_TARGET;
            pool->velocities[i] = (Vector3){ 0.0f };
        }
    }
}

// ---------------------------------------------------------------------------
void PhysicsSystem_Init(void)
{
    PhysicsBodies bodies = {
        .posX = s_posX,
        .posY = s_posY,
        .velX = s_velX,
        .velY = s_velY,
        .radius = s_radius,
        .mass = s_mass,
        .count = 0
    };

    PhysicsContactBuffer contacts = {
        .contacts = s_contacts,
        .count = 0,
        .capacity = PHYSICS_MAX_CONTACTS,
        .pairCount = 0
    };

    PhysicsInitializeWorld(&gPhysicsWorld, bodies, contacts);

    // Grid cell size: WORLD_SIZE / PHYSICS_GRID_WIDTH = 96 / 64 = 1.5 units.
    // Remove this line if PhysicsInitializeWorld or PhysicsBuildGrid already
    // sets invCellSize internally.
    gPhysicsWorld.grid.invCellSize = (float)PHYSICS_GRID_WIDTH / WORLD_SIZE;
}

void PhysicsSystem_Shutdown(void)
{
    PhysicsShutdownWorld(&gPhysicsWorld);
}

// ---------------------------------------------------------------------------
// Per-frame bridge
// ---------------------------------------------------------------------------

void PhysicsSystem_SyncIn(EntityPool* pool, const int* entities, int count)
{
    // Clear the full sparse→dense map so stale entries from entities that
    // died or lost COMP_PHYSICS since the last frame never mislead callers.
    // This is a memset of pool->count ints (~4 KB for MAX_ENTITIES=1024).
    memset(pool->entityToPhys, -1, sizeof(int) * (size_t)pool->count);

    int p = 0;
    for (int qi = 0; qi < count && p < PHYSICS_MAX_BODIES; qi++)
    {
        const int i = entities[qi];  // already filtered by the query

        pool->physToEntity[p] = i;
        pool->entityToPhys[i] = p;

        s_posX[p] = pool->positions[i].x;
        s_posY[p] = pool->positions[i].z;  // 3-D Z → physics Y
        s_velX[p] = pool->velocities[i].x;
        s_velY[p] = pool->velocities[i].z;
        s_radius[p] = pool->physRadius[i];
        s_mass[p] = pool->physMass[i];

        p++;
    }

    pool->physCount = p;
    gPhysicsWorld.bodies.count = (uint32_t)p;
}

void PhysicsSystem_WriteBack(EntityPool* pool)
{
    for (int p = 0; p < pool->physCount; p++)
    {
        const int e = pool->physToEntity[p];

        pool->positions[e].x = s_posX[p];
        pool->positions[e].z = s_posY[p];  // physics Y → 3-D Z
        pool->velocities[e].x = s_velX[p];
        pool->velocities[e].z = s_velY[p];
        pool->dirty[e] = true;
    }
}

void PhysicsSystem_ApplyBounds(EntityPool* pool,
    float minX, float maxX,
    float minZ, float maxZ)
{
    for (int p = 0; p < pool->physCount; p++)
    {
        const float r = s_radius[p];

        if (s_posX[p] - r < minX) { s_posX[p] = minX + r; s_velX[p] = fabsf(s_velX[p]); }
        if (s_posX[p] + r > maxX) { s_posX[p] = maxX - r; s_velX[p] = -fabsf(s_velX[p]); }
        if (s_posY[p] - r < minZ) { s_posY[p] = minZ + r; s_velY[p] = fabsf(s_velY[p]); }
        if (s_posY[p] + r > maxZ) { s_posY[p] = maxZ - r; s_velY[p] = -fabsf(s_velY[p]); }
    }
}



void PhysicsSystem_ApplyGroundLock(EntityPool* pool)
{
    for (int i = 0; i < pool->count; i++)
    {
        if (!(pool->masks[i] & COMP_ALIVE))
            continue;
        if (pool->meta[i].mobility != MOBILITY_DYNAMIC)
            continue;
        pool->positions[i].y = GROUND_Y + pool->sizes[i].y * 0.5f;
    }
}
 
// ---------------------------------------------------------------------------
// ECS entry point
// ---------------------------------------------------------------------------

void PhysicsSystem_ECSRun(EntityPool* pool, const Query* q, float dt, void* user)
{
    (void)user;

    PhysicsSystem_UpdateMovementIntent(pool, q->entities, q->count);

    // 1. Pack the query's entity list into dense SoA arrays.
    PhysicsSystem_SyncIn(pool, q->entities, q->count);

    if (gPhysicsWorld.bodies.count == 0) return;

    PhysicsIntegrateSIMD(&gPhysicsWorld.bodies, dt);

    PhysicsClearContacts(&gPhysicsWorld.contactBuffer);
    PhysicsBuildGrid(&gPhysicsWorld.bodies, &gPhysicsWorld.grid);

    PhysicsDetectCollisionsGridSIMD(
        &gPhysicsWorld.bodies,
        &gPhysicsWorld.grid,
        &gPhysicsWorld.contactBuffer);

    PhysicsResolveCollisions(
        &gPhysicsWorld.bodies,
        &gPhysicsWorld.contactBuffer);

    PhysicsApplyVelocityResponse(
        &gPhysicsWorld.bodies,
        &gPhysicsWorld.contactBuffer);

    // 3. Clamp to world extents.
    PhysicsSystem_ApplyBounds(pool,
        -WORLD_HALF, WORLD_HALF,
        -WORLD_HALF, WORLD_HALF);

    // 4. Write results back into EntityPool.
    PhysicsSystem_WriteBack(pool);
}

// ---------------------------------------------------------------------------
// Spawn helper
// ---------------------------------------------------------------------------

void PhysicsSystem_SetBody(EntityPool* pool, int entity, float radius, float mass)
{
    if (radius <= 0.0f)
    {
        const Vector3 s = pool->sizes[entity];
        const float minXZ = s.x < s.z ? s.x : s.z;
        radius = minXZ * 0.5f;
    }

    pool->physRadius[entity] = radius;
    pool->physMass[entity] = mass;
    pool->masks[entity] |= COMP_PHYSICS;
    pool->masks[entity] |= COMP_POSITION;
    pool->masks[entity] |= COMP_VELOCITY;
    pool->masks[entity] |= COMP_ALIVE;
    pool->masks[entity] |= COMP_DYNAMIC_MOVILITY;
}
