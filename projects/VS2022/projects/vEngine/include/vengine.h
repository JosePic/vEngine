#pragma once
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define MAX_COLUMNS 20
#define MAX_ENTITIES 1024
#define MAX_COLLISIONS 2048

#define GRID_W 96
#define GRID_H 96
#define MAX_CELL_ENTRIES 64

#define WORLD_SIZE 96.0f
#define WORLD_HALF (WORLD_SIZE * 0.5f)
#define CELL_SIZE (WORLD_SIZE / (float)GRID_W)

#define GROUND_Y 0.0f

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------
typedef enum {
    ENTITY_PLAYER = (1 << 0),
    ENTITY_ENEMY = (1 << 1),
    ENTITY_WALL = (1 << 2),
    ENTITY_PROJECTILE = (1 << 3),
    ENTITY_FRIENDLY = (1 << 4),
} EntityType;

typedef enum {
    MOBILITY_STATIC = 0,
    MOBILITY_DYNAMIC = 1,
} Mobility;

typedef enum {
    FACTION_NEUTRAL = 0,
    FACTION_PLAYER = 1,
    FACTION_ENEMY = 2,
} Faction;

typedef enum {
    ARCH_MELEE = 0,
    ARCH_RANGED = 1,
    ARCH_CASTER = 2,
    ARCH_BUFFER = 3,
    ARCH_PLAYER = 4,
} UnitArchetype;

typedef enum {
    SKILL_NONE = 0,
    SKILL_DASH = 1,
    SKILL_BURST = 2,
    SKILL_NOVA = 3,
    SKILL_AURA = 4,
} SkillType;

// ---------------------------------------------------------------------------
// Component masks
// ---------------------------------------------------------------------------
typedef uint64_t ComponentMask;

enum {
    COMP_POSITION = 1ull << 0,
    COMP_VELOCITY = 1ull << 1,
    COMP_HEALTH = 1ull << 2,
    COMP_RENDERABLE = 1ull << 3,
    COMP_MOVE_TARGET = 1ull << 4,
    COMP_ENEMY_AI = 1ull << 5,
    COMP_PLAYER_CONTROL = 1ull << 6,
    COMP_ALIVE = 1ull << 7,
    COMP_PENDING_DESTROY = 1ull << 8,
    COMP_DYNAMIC_MOVILITY = 1ull << 9,
    COMP_MODEL = 1ull << 10,
    COMP_ANIMATED = 1ull << 11,
    COMP_IS_PROJECTILE = 1ull << 12,

    // Entity participates in the physics module simulation as a dynamic
    // circle body.  Set at spawn time via PhysicsSystem_SetBody().
    // Static entities (walls, terrain) must NOT have this flag they are
    // not fed into PhysicsBodies and therefore cost no physics budget.
    COMP_PHYSICS = 1ull << 12,
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------
typedef struct {
    EntityType type;
    uint32_t collidesWithMask;
    Mobility mobility;
    Faction faction;
    int owner; // for projectiles; -1 otherwise
    UnitArchetype archetype;
    SkillType skill;
} EntityMeta;

typedef struct {
    BoundingBox boxes[MAX_ENTITIES];
    Vector3 positions[MAX_ENTITIES];
    Vector3 prevPositions[MAX_ENTITIES];
    Vector3 sizes[MAX_ENTITIES];
    bool dirty[MAX_ENTITIES];

    ComponentMask masks[MAX_ENTITIES];

    EntityMeta meta[MAX_ENTITIES];
    Color colors[MAX_ENTITIES];
    int health[MAX_ENTITIES];
    int visualModel[MAX_ENTITIES];
    int visualAnimation[MAX_ENTITIES];
    float visualAnimationFrame[MAX_ENTITIES];
    float visualAnimationSpeed[MAX_ENTITIES];
    float visualRotationY[MAX_ENTITIES];

    Vector3 velocities[MAX_ENTITIES];
    Vector3 moveTargets[MAX_ENTITIES];
    bool hasMoveTarget[MAX_ENTITIES];
    float fireCooldown[MAX_ENTITIES];

    float moveSpeed[MAX_ENTITIES];
    float attackRange[MAX_ENTITIES];
    float attackCooldown[MAX_ENTITIES];
    int attackDamage[MAX_ENTITIES];
    float skillCooldown[MAX_ENTITIES];
    float skillTimer[MAX_ENTITIES];
    float buffTimer[MAX_ENTITIES];
    float buffFireRateMul[MAX_ENTITIES];

    // -----------------------------------------------------------------------
    // Physics module bridge
    //
    // The physics module (physics.h) uses dense SoA float arrays, while
    // EntityPool is sparse.  This bridge handles:
    //   1. Per-entity properties (physRadius, physMass) â€” persistent, set at
    //      spawn time with PhysicsSystem_SetBody().
    //   2. Dense  sparse index maps â€” rebuilt every frame by
    //      PhysicsSystem_SyncIn(); only entities with COMP_PHYSICS|COMP_ALIVE
    //      are packed.
    //
    // Ground-plane convention:
    //   positions[i].x   physics posX
    //   positions[i].z   physics posY   (physics is 2-D; Y is up/height)
    //   velocities[i].x  physics velX
    //   velocities[i].z  physics velY
    // -----------------------------------------------------------------------
    float physRadius[MAX_ENTITIES]; // circle radius for the physics module
    float physMass[MAX_ENTITIES];    // mass; set to 0 for immovable bodies

    // Mapping rebuilt each frame by PhysicsSystem_SyncIn().
    // entityToPhys[i] == -1 means entity i is not in the current physics step.
    int physCount;                  // number of packed physics bodies this frame
    int physToEntity[MAX_ENTITIES];  // dense phys idx  sparse entity idx
    int entityToPhys[MAX_ENTITIES];  // sparse entity idx  dense phys idx

    int count;
} EntityPool;

typedef struct {
    bool hit;
    int entity;
    float distance;
    Vector3 point;
    Vector3 normal;
} RaycastHit;

static inline Vector3 _Vector3Scale(Vector3 v, float scalar)
{
    Vector3 result = { v.x * scalar, v.y * scalar, v.z * scalar };
    return result;
}

// ---------------------------------------------------------------------------
// World
// ---------------------------------------------------------------------------
extern EntityPool world;

int SpawnEntity(EntityPool* pool, Vector3 position, Vector3 size, EntityType type,
    uint32_t collidesWithMask, bool is_static, Color color);

// ---------------------------------------------------------------------------
// Physics
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Input / Picking
// ---------------------------------------------------------------------------
RaycastHit RaycastSelectable(EntityPool* pool, Ray ray);
bool RayGroundHit(Ray ray, float groundY, Vector3* outPoint);

// ---------------------------------------------------------------------------
// Gameplay
// ---------------------------------------------------------------------------
void CastSkillAt(EntityPool* pool, int idx, Vector3 groundPoint);
void CleanupDead(EntityPool* pool);

// ---------------------------------------------------------------------------
// Camera helpers
// ---------------------------------------------------------------------------
Vector3 GetSelectionCenter(EntityPool* pool, const bool selected[], int fallbackIdx);
void UpdateIsometricCamera(Camera* camera, Vector3 target, float* yawRad,
    float* pitchRad, float* distance, bool allowRotate);

// ---------------------------------------------------------------------------
// Query-driven systems
// ---------------------------------------------------------------------------
void UpdateEnemyAI_Query(EntityPool* pool, int playerIdx, const int* entities, int count);
void UpdateCombat_Query(EntityPool* pool, float dt, const int* entities, int count);

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void MarkPendingDestroy(EntityPool* pool, int entity);
void CleanupPendingDestroy_Query(EntityPool* pool, const int* entities, int count);

// ---------------------------------------------------------------------------
// Systems (scheduler)
// ---------------------------------------------------------------------------
// kept in system.h/system.c
