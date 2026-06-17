#include "game_health_system.h"
#include "game_pool_data.h"
#include "query.h"
#include "system.h"
#include "vengine.h"
#include <math.h>
#include "interaction_resolver.h"
#include <sim.h>
#define COMP_HEALTH 1
#define FIELD_HP 1
#define REASON_DAMAGE 1
#define QUERY_ALIVE 1

void ResolveHealth(InteractionResolver* resolver, void* userData) {
    uint32_t count;
    const IR_Record* intents = IR_GetIntents(resolver, &count);

    for (uint32_t i = 0; i < count; i++) {
        const IR_Record* r = &intents[i];
        if (r->type != IR_INTENT_DAMAGE) continue;

        DamageIntent* in = (DamageIntent*)r->data;
        HealthStats* health = &gamePool.healthStats[in->target];

        // 1. Snapshot BEFORE state
        float oldHp = health->currentHealth;

        // 2. Perform the logic
        float finalDmg = HealthSystem_CalculateFinalDamage(in->target, in->damage, in->damageType);
        health->currentHealth -= finalDmg;

        // 3. Snapshot AFTER state and Log Mutation
        if (oldHp != health->currentHealth) {
            VMutationRecord mut = {
                .transactionId = r->id,     // The exact intent that caused this
                .reasonId = REASON_DAMAGE,
                .entityId = in->target,
                .componentId = COMP_HEALTH,
                .fieldId = FIELD_HP
            };
            // Copy pure floats directly into the 64-bit unions
            mut.oldVal.f32[0] = oldHp;
            mut.newVal.f32[0] = health->currentHealth;

            VInspect_LogMutation(&g_InspectorDB, &mut);

            // 4. Record Query Exclusion (Did the Goblin die?)
            if (oldHp > 0.0f && health->currentHealth <= 0.0f) {
                VInspect_LogQueryEvent(&g_InspectorDB, QUERY_ALIVE, in->target, false, r->id);
            }
        }
    }
}

void HealthSystem_DealDamage(int target, float baseDamage, DamageType type, int source)
{
    DamageIntent intent =
    {
        .target = target,
        .damage = baseDamage,
        .damageType = type,
        .source = source
    };

    IR_SubmitIntent(
        &gamePool.resolver,
        IR_INTENT_DAMAGE,
        &intent,
        sizeof(intent));
    //GamePool_TakeDamage(target, baseDamage, type, source);
}

void HealthSystem_Heal(
    int target,
    int amount)
{
    HealIntent intent =
    {
        .target = target,
        .amount = amount
    };

    IR_SubmitIntent(
        &gamePool.resolver,
        IR_INTENT_HEAL,
        &intent,
        sizeof(intent));
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
    DestroyIntent intent =
    {
        .entity = entity
    };

    IR_SubmitIntent(
        &gamePool.resolver,
        IR_INTENT_DESTROY,
        &intent,
        sizeof(intent));
}


// ============================================================================
// Buff Application During Combat
// ============================================================================

/**
 * Apply natural health regeneration to alive units.
 * Called every frame, but only after being out of combat for a while.
 *
 * Regen rate: 2 HP per second
 * Only starts regen after being out of combat for 5 seconds
 */
static void ApplyHealthRegen(int entity, float dt)
{
    HealthStats* health = &gamePool.healthStats[entity];

    // Only regenerate if not at max health
    if (health->currentHealth >= health->maxHealth)
        return;

    // Check if entity has taken damage recently
    // For now, we'll just apply slow regen at all times
    // In a real game, you'd track "time since last damage"

    // Regen rate: 2 HP per second
    float regenPerSec = 2.0f;

    // Apply regeneration buff multiplier (if any)
    float regenMult = 1.0f + GamePool_GetBuffMagnitude(entity, BUFF_REGENERATION);

    float totalRegen = regenPerSec * regenMult * dt;
    health->currentHealth = (int)fminf(
        (double)(health->currentHealth + (int)totalRegen),
        (double)health->maxHealth
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
        //ApplyHealthRegen(entity, dt);
        ApplyDamageOverTime(entity, dt);
        ApplyBarrier(entity, dt);

        // Sync health back to legacy pool field
        gamePool.pool->health[entity] = gamePool.healthStats[entity].currentHealth;

    }
}


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


void SysHealthGetAllUnitsHP(EntityPool* pool,  Query* q)
{
    for (int k = 0; k < q->count; k++) {
        int entity = q->entities[k];

        if (!(pool->masks[entity] & COMP_ALIVE))
            continue;
        if (pool->health[entity] == 1) continue;
        printf("Entity %d: %d HP\n", entity, pool->health[entity]);

    }
}

