
#include "game_health_system.h"
#include "game_pool_data.h"
#include "query.h"
#include "system.h"
#include "vengine.h"
#include <math.h>

void HealthSystem_DealDamage(int target, float baseDamage, DamageType type, int source)
{
    GamePool_TakeDamage(target, baseDamage, type, source);
}

void HealthSystem_Heal(int target, int amount)
{
    GamePool_Heal(target, amount);
}

bool HealthSystem_IsAlive(int entity)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return false;

    EntityPool* pool = gamePool.pool;
    if (!(pool->masks[entity] & COMP_ALIVE))
        return false;

    return gamePool.healthStats[entity].currentHealth > 0;
}

float HealthSystem_GetHealthPercent(int entity)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return 0.0f;

    HealthStats* health = &gamePool.healthStats[entity];
    if (health->maxHealth <= 0)
        return 0.0f;

    return (float)health->currentHealth / (float)health->maxHealth;
}

void HealthSystem_Kill(int entity)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return;

    gamePool.healthStats[entity].currentHealth = 0;
    gamePool.pool->health[entity] = 0;
    MarkPendingDestroy(gamePool.pool, entity);
}

// ============================================================================
// Buff Application During Combat
// ============================================================================

/**
 * Apply natural health regeneration to alive units.
 * Called every frame.
 */
static void ApplyHealthRegen(int entity, float dt)
{
    HealthStats* health = &gamePool.healthStats[entity];

    // Natural regen rate
    float regenPerSec = 0.5f;  // 0.5 HP per second

    // Regeneration buff multiplier
    float regenMult = 1.0f + GamePool_GetBuffMagnitude(entity, BUFF_REGENERATION);

    float totalRegen = regenPerSec * regenMult * dt;
    health->currentHealth = (int)fminf(
        health->currentHealth + (int)ceilf(totalRegen),
        health->maxHealth
    );
}

/**
 * Apply damage over time from poison/bleed effects.
 */
static void ApplyDamageOverTime(int entity, float dt)
{
    BuffStack* buffs = &gamePool.buffs[entity];

    for (int i = 0; i < buffs->count; i++) {
        if (buffs->buffs[i].type == BUFF_POISON) {
            // Poison deals damage based on magnitude
            // magnitude = 5.0 means 5 damage per second
            float damageThisFrame = buffs->buffs[i].magnitude * dt;
            HealthSystem_DealDamage(entity, damageThisFrame, DAMAGE_MAGICAL, -1);
            break;  // Only one poison per entity for simplicity
        }
    }
}

/**
 * Apply barrier/shield effect (temporary extra HP).
 * Barriers are represented as temporary max health increase.
 */
static void ApplyBarrier(int entity, float dt)
{
    BuffStack* buffs = &gamePool.buffs[entity];
    HealthStats* health = &gamePool.healthStats[entity];

    int totalBarrier = 0;
    for (int i = 0; i < buffs->count; i++) {
        if (buffs->buffs[i].type == BUFF_BARRIER) {
            totalBarrier += (int)buffs->buffs[i].magnitude;
        }
    }

    // Apply barrier as temporary max health
    // (In a real game, barriers would be separate from health)
    (void)dt;  // Not used in simple implementation
}

// ============================================================================
// System Function
// ============================================================================

void SysHealthCleanup(EntityPool* pool, const Query* q, float dt, void* user)
{
    (void)user;

    for (int k = 0; k < q->count; k++) {
        int entity = q->entities[k];

        if (!(pool->masks[entity] & COMP_ALIVE))
            continue;

        // Apply passive health effects
        ApplyHealthRegen(entity, dt);
        ApplyDamageOverTime(entity, dt);
        ApplyBarrier(entity, dt);

        // Sync health back to legacy pool field
        gamePool.pool->health[entity] = gamePool.healthStats[entity].currentHealth;

        // Mark dead entities for destruction
        if (gamePool.healthStats[entity].currentHealth <= 0) {
            MarkPendingDestroy(pool, entity);
        }
    }
}

// ============================================================================
// Example Helper: Get damage taken with full scaling
// ============================================================================

/**
 * Calculate final damage after all scaling.
 * Useful for UI/tooltips/debugging.
 */
float HealthSystem_CalculateFinalDamage(int target, float baseDamage, DamageType type)
{
    if (target < 0 || target >= MAX_ENTITIES)
        return baseDamage;

    HealthStats* health = &gamePool.healthStats[target];
    float finalDamage = baseDamage;

    // Apply armor (physical damage only)
    if (type == DAMAGE_PHYSICAL) {
        float armorFactor = 1.0f - (float)health->armor / (100.0f + health->armor);
        finalDamage *= armorFactor;
    }

    // Apply magic resistance (magical damage only)
    if (type == DAMAGE_MAGICAL) {
        finalDamage *= (1.0f - health->magicResist);
    }

    // Apply vulnerability buff
    float vulnMult = 1.0f + GamePool_GetBuffMagnitude(target, BUFF_VULNERABLE);
    finalDamage *= vulnMult;

    // Apply damage multiplier
    finalDamage *= health->damageMultiplier;

    return finalDamage;
}