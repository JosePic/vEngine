#include "game_pool_data.h"
#include "vengine.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "interaction_resolver.h"

GamePool gamePool = { 0 };


/*
*  Temporarily putting all resolver logic here, this should be moved to its own place.
*/


typedef enum
{
    IR_INTENT_DAMAGE,
    IR_INTENT_HEAL,
    IR_INTENT_DESTROY,
    IR_INTENT_SPAWN_PROJECTILE
} GameIntentType;

typedef enum
{
    IR_CHANGE_DAMAGE,
    IR_CHANGE_HEAL,
    IR_CHANGE_DESTROY,
    IR_CHANGE_SPAWN
} GameChangeType;

typedef struct
{
    int entity;
} DestroyIntent;


typedef struct
{
    int entity;
} DestroyChange;



void ResolveLifetime(
    InteractionResolver* resolver,
    void* userData)
{
    (void)userData;

    uint32_t count;
    const IR_Record* intents =
        IR_GetIntents(resolver, &count);

    for (uint32_t i = 0; i < count; i++)
    {
        const IR_Record* r = &intents[i];

        if (r->type != IR_INTENT_DESTROY)
            continue;

        DestroyIntent* in =
            (DestroyIntent*)r->data;

        DestroyChange out =
        {
            .entity = in->entity
        };

        IR_SubmitChange(
            resolver,
            IR_CHANGE_DESTROY,
            &out,
            sizeof(out));
    }
}
void ResolveSpawning()
{

}
void RegisterGameResolvers(InteractionResolver* resolver)
{
    IR_RegisterResolver(resolver, ResolveHealth, NULL);
    IR_RegisterResolver(resolver, ResolveLifetime, NULL);
    IR_RegisterResolver(resolver, ResolveSpawning, NULL);
}




void GamePool_Init(EntityPool* pool)
{
    if (!pool)
        return;

    gamePool.pool = pool;
    gamePool.capacity = MAX_ENTITIES;

    // Allocate arrays
    gamePool.combatStats = (CombatStats*)malloc(sizeof(CombatStats) * MAX_ENTITIES);
    gamePool.combatState = (CombatState*)malloc(sizeof(CombatState) * MAX_ENTITIES);
    gamePool.healthStats = (HealthStats*)malloc(sizeof(HealthStats) * MAX_ENTITIES);
    gamePool.skillStates = (SkillStates*)malloc(sizeof(SkillStates) * MAX_ENTITIES);
    gamePool.resourceState = (ResourceState*)malloc(sizeof(ResourceState) * MAX_ENTITIES);
    gamePool.buffs = (BuffStack*)malloc(sizeof(BuffStack) * MAX_ENTITIES);
    gamePool.behavior = (BehaviorData*)malloc(sizeof(BehaviorData) * MAX_ENTITIES);

    // Clear all memory
    memset(gamePool.combatStats, 0, sizeof(CombatStats) * MAX_ENTITIES);
    memset(gamePool.combatState, 0, sizeof(CombatState) * MAX_ENTITIES);
    memset(gamePool.healthStats, 0, sizeof(HealthStats) * MAX_ENTITIES);
    memset(gamePool.skillStates, 0, sizeof(SkillStates) * MAX_ENTITIES);
    memset(gamePool.resourceState, 0, sizeof(ResourceState) * MAX_ENTITIES);
    memset(gamePool.buffs, 0, sizeof(BuffStack) * MAX_ENTITIES);
    memset(gamePool.behavior, 0, sizeof(BehaviorData) * MAX_ENTITIES);

    gamePool.pendingHealth =
        calloc(MAX_ENTITIES, sizeof(PendingHealth));

    gamePool.pendingLifetime =
        calloc(MAX_ENTITIES, sizeof(PendingLifetime));

    // Initialize default values
    for (int i = 0; i < MAX_ENTITIES; i++) {
        gamePool.behavior[i].targetEntity = -1;
        gamePool.combatState[i].canAttack = true;
        gamePool.combatState[i].attackSpeedMultiplier = 1.0f;
    }

    IR_Init(&gamePool.resolver);
    RegisterGameResolvers(&gamePool.resolver);

}

void GamePool_BeginFrame(void)
{
    memset(
        gamePool.pendingHealth,
        0,
        sizeof(PendingHealth) * gamePool.capacity);

    memset(
        gamePool.pendingLifetime,
        0,
        sizeof(PendingLifetime) * gamePool.capacity);

    IR_BeginFrame(&gamePool.resolver);
}

void GamePool_Shutdown(void)
{
    if (gamePool.combatStats) free(gamePool.combatStats);
    if (gamePool.combatState) free(gamePool.combatState);
    if (gamePool.healthStats) free(gamePool.healthStats);
    if (gamePool.skillStates) free(gamePool.skillStates);
    if (gamePool.resourceState) free(gamePool.resourceState);
    if (gamePool.buffs) free(gamePool.buffs);
    if (gamePool.behavior) free(gamePool.behavior);
    if (gamePool.pendingHealth) free(gamePool.pendingHealth);
    if (gamePool.pendingLifetime) free(gamePool.pendingLifetime);


    memset(&gamePool, 0, sizeof(GamePool));
}

void GamePool_ClearEntity(int entity)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return;

    memset(&gamePool.combatStats[entity], 0, sizeof(CombatStats));
    memset(&gamePool.combatState[entity], 0, sizeof(CombatState));
    memset(&gamePool.healthStats[entity], 0, sizeof(HealthStats));
    memset(&gamePool.skillStates[entity], 0, sizeof(SkillStates));
    memset(&gamePool.resourceState[entity], 0, sizeof(ResourceState));
    memset(&gamePool.buffs[entity], 0, sizeof(BuffStack));
    memset(&gamePool.behavior[entity], 0, sizeof(BehaviorData));


    gamePool.behavior[entity].targetEntity = -1;
    gamePool.combatState[entity].canAttack = true;
}

void GamePool_InitFromArchetype(int entity, const ArchetypeConfig* archetype)
{
    if (entity < 0 || entity >= MAX_ENTITIES || !archetype)
        return;

    GamePool_ClearEntity(entity);

    // Copy persistent stats
    gamePool.combatStats[entity] = archetype->combatStats;
    gamePool.healthStats[entity] = archetype->healthStats;
    gamePool.healthStats[entity].currentHealth = archetype->healthStats.maxHealth;

    // Initialize skills
    gamePool.skillStates[entity].skillCount = archetype->skillCount;

    // Initialize resource
    gamePool.resourceState[entity].maxResource = archetype->startingResource;
    gamePool.resourceState[entity].currentResource = archetype->startingResource;
    gamePool.resourceState[entity].regenRate = 10.0f;

    // Initialize behavior
    gamePool.behavior[entity].state = BEHAVIOR_IDLE;
    gamePool.behavior[entity].targetEntity = -1;

    // Initialize combat state
    gamePool.combatState[entity].canAttack = true;
    gamePool.combatState[entity].attackSpeedMultiplier = 1.0f;
}

bool GamePool_AddBuff(int entity, BuffType type, float duration, float magnitude)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return false;

    BuffStack* stack = &gamePool.buffs[entity];

    for (int i = 0; i < stack->count; i++) {
        if (stack->buffs[i].type == type) {
            stack->buffs[i].timeRemaining = (float)fmaxf((double)stack->buffs[i].timeRemaining, (double)duration);
            stack->buffs[i].magnitude = (float)fmaxf((double)stack->buffs[i].magnitude, (double)magnitude);
            return true;
        }
    }

    if (stack->count >= MAX_BUFFS_PER_ENTITY)
        return false;

    int idx = stack->count++;
    stack->buffs[idx].type = type;
    stack->buffs[idx].timeRemaining = duration;
    stack->buffs[idx].magnitude = magnitude;
    stack->buffs[idx].sourceEntity = -1;

    return true;
}

void GamePool_RemoveBuff(int entity, BuffType type)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return;

    BuffStack* stack = &gamePool.buffs[entity];

    for (int i = 0; i < stack->count; i++) {
        if (stack->buffs[i].type == type) {
            stack->buffs[i] = stack->buffs[--stack->count];
            return;
        }
    }
}

void GamePool_ClearBuffs(int entity, BuffType typeFilter)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return;

    BuffStack* stack = &gamePool.buffs[entity];

    if (typeFilter == 0xFF) {
        stack->count = 0;
    }
    else {
        int writeIdx = 0;
        for (int i = 0; i < stack->count; i++) {
            if (stack->buffs[i].type != typeFilter) {
                stack->buffs[writeIdx++] = stack->buffs[i];
            }
        }
        stack->count = writeIdx;
    }
}

bool GamePool_HasBuff(int entity, BuffType type)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return false;

    BuffStack* stack = &gamePool.buffs[entity];

    for (int i = 0; i < stack->count; i++) {
        if (stack->buffs[i].type == type)
            return true;
    }
    return false;
}

float GamePool_GetBuffMagnitude(int entity, BuffType type)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return 0.0f;

    BuffStack* stack = &gamePool.buffs[entity];

    for (int i = 0; i < stack->count; i++) {
        if (stack->buffs[i].type == type)
            return stack->buffs[i].magnitude;
    }
    return 0.0f;
}

void GamePool_TakeDamage(int target, float baseDamage, DamageType type, int source)
{
    if (target < 0 || target >= MAX_ENTITIES)
        return;
    if (!gamePool.pool || !(gamePool.pool->masks[target] & COMP_ALIVE))
        return;

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

    // Apply to health
    health->currentHealth -= (int)ceilf((double)finalDamage);

    // IMPORTANT: Sync immediately so pool->health reflects the change
    gamePool.pool->health[target] = health->currentHealth;

    // Mark for death if health depleted
    if (health->currentHealth <= 0) {
        gamePool.pool->health[target] = 0;
        MarkPendingDestroy(gamePool.pool, target);
    }
}

void GamePool_Heal(int entity, int amount)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return;

    HealthStats* health = &gamePool.healthStats[entity];
    health->currentHealth = (int)fminf((double)(health->currentHealth + amount), (double)health->maxHealth);
    gamePool.pool->health[entity] = health->currentHealth;
}

bool GamePool_ConsumeResource(int entity, int amount)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return false;

    ResourceState* res = &gamePool.resourceState[entity];

    if (res->currentResource < amount)
        return false;

    res->currentResource -= amount;
    return true;
}

void GamePool_RestoreResource(int entity, int amount)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return;

    ResourceState* res = &gamePool.resourceState[entity];
    res->currentResource = (int)fminf((double)(res->currentResource + amount), (double)res->maxResource);
}

void GamePool_SetBehavior(int entity, BehaviorState state)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return;

    gamePool.behavior[entity].state = state;
    gamePool.behavior[entity].stateTimer = 0.0f;
}

float GamePool_GetEffectiveAttackCooldown(int entity)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return 1.0f;

    CombatStats* stats = &gamePool.combatStats[entity];
    CombatState* state = &gamePool.combatState[entity];

    float cooldown = stats->attackCooldown;

    float hasteMult = 1.0f + GamePool_GetBuffMagnitude(entity, BUFF_HASTE);

    float slowMult = 1.0f - GamePool_GetBuffMagnitude(entity, BUFF_SLOW);
    slowMult = (float)fmaxf((double)slowMult, 0.2);

    cooldown /= (state->attackSpeedMultiplier * hasteMult * slowMult);

    return (float)fmaxf((double)cooldown, 0.1);
}

float GamePool_GetEffectiveMoveSpeed(int entity)
{
    if (entity < 0 || entity >= MAX_ENTITIES)
        return 0.0f;

    CombatStats* stats = &gamePool.combatStats[entity];

    float hasteMult = 1.0f + GamePool_GetBuffMagnitude(entity, BUFF_HASTE);

    float slowMult = 1.0f - GamePool_GetBuffMagnitude(entity, BUFF_SLOW);
    slowMult = (float)fmaxf((double)slowMult, 0.2);

    return stats->moveSpeed * hasteMult * slowMult;
}