#pragma once

#include "game_stat_types.h"
#include "vengine.h"
#include "interaction_resolver.h"


typedef struct
{
    float damage;
    float healing;
} PendingHealth;

typedef struct
{
    bool destroy;
} PendingLifetime;

typedef struct
{
    bool spawnProjectile;
    int owner;
    Vector3 start;
    Vector3 dir;
} PendingSpawn;


// ---------------------------------------------------------------------------
// GamePool: Parallel storage for all game-specific data
//
// This is NOT part of EntityPool to keep engine and game separate.
// Index into GamePool with entity ID just like EntityPool.
// Example: gamePool->combatStats[entity_id].moveSpeed = 5.0f;
//
// Uses dynamic allocation to avoid stack overflow.
// ---------------------------------------------------------------------------
typedef struct {
    // --- Combat System ---
    CombatStats* combatStats;
    CombatState* combatState;

    // --- Health System ---
    HealthStats* healthStats;

    // --- Skill System ---
    SkillStates* skillStates;

    // --- Resource ---
    ResourceState* resourceState;

    // --- Buffs ---
    BuffStack* buffs;

    // --- Behavior ---
    BehaviorData* behavior;

    // --- Interaction Framework ---
    InteractionResolver resolver;

    PendingHealth* pendingHealth;
    PendingLifetime* pendingLifetime;
    PendingSpawn* pendingSpawn;

    // --- Back-reference ---
    EntityPool* pool;

    // --- Capacity ---
    int capacity;

} GamePool;

extern GamePool gamePool;

// ---------------------------------------------------------------------------
// Initialization & Lifecycle
// ---------------------------------------------------------------------------

/**
 * Initialize the GamePool.
 * Call once at startup.
 * Allocates memory for capacity entities.
 */
void GamePool_Init(EntityPool* pool);

/**
 * Shutdown GamePool and free all asllocated memory.
 */
void GamePool_Shutdown(void);

/**
 * Clear a single entity's game data (for respawn/reset).
 */
void GamePool_BeginFrame(void);


/**
 * Clear a single entity's game data (for respawn/reset).
 */
void GamePool_ClearEntity(int entity);

/**
 * Copy archetype config into an entity's game data.
 * Call after spawning a unit.
 */
void GamePool_InitFromArchetype(int entity, const ArchetypeConfig* archetype);

// ---------------------------------------------------------------------------
// Utility Functions
// ---------------------------------------------------------------------------

/**
 * Apply a buff to an entity.
 * Returns true if added, false if out of buff slots.
 */
bool GamePool_AddBuff(int entity, BuffType type, float duration, float magnitude);

/**
 * Remove a specific buff (e.g., cleanse on skill cast).
 */
void GamePool_RemoveBuff(int entity, BuffType type);

/**
 * Remove all buffs of a category.
 * Pass 0xFF to remove all.
 */
void GamePool_ClearBuffs(int entity, BuffType typeFilter);

/**
 * Check if entity has a specific buff active.
 */
bool GamePool_HasBuff(int entity, BuffType type);

/**
 * Get magnitude of a buff (0.0 if not present).
 */
float GamePool_GetBuffMagnitude(int entity, BuffType type);

/**
 * Apply damage with scaling from buffs/stats.
 * Handles armor, vulnerability, barriers, etc.
 */
void GamePool_TakeDamage(int target, float baseDamage, DamageType type, int source);

/**
 * Heal an entity (capped at max health).
 */
void GamePool_Heal(int entity, int amount);

/**
 * Consume resource (mana, energy, etc).
 * Returns true if had enough.
 */
bool GamePool_ConsumeResource(int entity, int amount);

/**
 * Restore resource.
 */
void GamePool_RestoreResource(int entity, int amount);

/**
 * Set a unit's behavior state.
 */
void GamePool_SetBehavior(int entity, BehaviorState state);

/**
 * Get effective attack cooldown (including buffs/stats).
 */
float GamePool_GetEffectiveAttackCooldown(int entity);

/**
 * Get effective move speed (including buffs/stats).
 */
float GamePool_GetEffectiveMoveSpeed(int entity);

/*
    Resolver
*/

void ResolveHealth(InteractionResolver* resolver, void* userData);