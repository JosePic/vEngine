#include "game_stat_types.h"
#include "game_pool_data.h"
#include "vengine.h"

// ---------------------------------------------------------------------------
// Archetype Configurations - Data-Driven Unit Definitions
//
// Define unit archetypes once here. Spawn functions just reference these.
// To balance units, edit values here, no code changes needed.
// ---------------------------------------------------------------------------
#define ARCH_COUNT  5

static const ArchetypeConfig archetypeConfigs[ARCH_COUNT] = {
    [ARCH_MELEE] = {
        .combatStats = {
            .moveSpeed = 2.8f,
            .attackRange = 1.2f,
            .attackCooldown = 0.55f,
            .attackDamage = 14.0f,
            .attackSpeed = 1.0f,
        },
        .healthStats = {
            .maxHealth = 120,
            .currentHealth = 120,
            .armor = 15,
            .magicResist = 0.1f,
            .damageMultiplier = 1.0f,
        },
        .skillStats = {
            [0] = {
                .cooldown = 5.0f,
                .range = 3.5f,
                .baseDamage = 20.0f,
                .resourceCost = RESOURCE_COOLDOWN_ONLY,
                .resourceAmount = 0,
            },
        },
        .skillCount = 1,
        .primaryResource = RESOURCE_COOLDOWN_ONLY,
        .startingResource = 0,
    },

    [ARCH_RANGED] = {
        .combatStats = {
            .moveSpeed = 3.9f,
            .attackRange = 8.5f,
            .attackCooldown = 0.45f,
            .attackDamage = 10.0f,
            .attackSpeed = 1.0f,
        },
        .healthStats = {
            .maxHealth = 80,
            .currentHealth = 80,
            .armor = 0,
            .magicResist = 0.0f,
            .damageMultiplier = 1.0f,
        },
        .skillStats = {
            [0] = {
                .cooldown = 6.0f,
                .range = 8.0f,
                .baseDamage = 8.0f,
                .resourceCost = RESOURCE_COOLDOWN_ONLY,
                .resourceAmount = 0,
            },
        },
        .skillCount = 1,
        .primaryResource = RESOURCE_COOLDOWN_ONLY,
        .startingResource = 0,
    },

    [ARCH_CASTER] = {
        .combatStats = {
            .moveSpeed = 1.3f,
            .attackRange = 9.5f,
            .attackCooldown = 0.9f,
            .attackDamage = 12.0f,
            .attackSpeed = 1.0f,
        },
        .healthStats = {
            .maxHealth = 60,
            .currentHealth = 60,
            .armor = 0,
            .magicResist = 0.2f,
            .damageMultiplier = 1.0f,
        },
        .skillStats = {
            [0] = {
                .cooldown = 8.0f,
                .range = 9.5f,
                .baseDamage = 18.0f,
                .resourceCost = RESOURCE_MANA,
                .resourceAmount = 30,
            },
        },
        .skillCount = 1,
        .primaryResource = RESOURCE_MANA,
        .startingResource = 100,
    },

    [ARCH_BUFFER] = {
        .combatStats = {
            .moveSpeed = 1.5f,
            .attackRange = 6.5f,
            .attackCooldown = 0.8f,
            .attackDamage = 6.0f,
            .attackSpeed = 1.0f,
        },
        .healthStats = {
            .maxHealth = 100,
            .currentHealth = 100,
            .armor = 10,
            .magicResist = 0.1f,
            .damageMultiplier = 1.0f,
        },
        .skillStats = {
            [0] = {
                .cooldown = 6.0f,
                .range = 6.0f,
                .baseDamage = 0.0f,  // Buff ability, not damage
                .resourceCost = RESOURCE_COOLDOWN_ONLY,
                .resourceAmount = 0,
            },
        },
        .skillCount = 1,
        .primaryResource = RESOURCE_COOLDOWN_ONLY,
        .startingResource = 0,
    },

    [ARCH_PLAYER] = {
        .combatStats = {
            .moveSpeed = 3.5f,
            .attackRange = 6.5f,
            .attackCooldown = 0.8f,
            .attackDamage = 6.0f,
            .attackSpeed = 1.0f,
        },
        .healthStats = {
            .maxHealth = 100,
            .currentHealth = 100,
            .armor = 5,
            .magicResist = 0.0f,
            .damageMultiplier = 1.0f,
        },
        .skillStats = {
            [0] = {
                .cooldown = 8.0f,
                .range = 3.0f,
                .baseDamage = 18.0f,
                .resourceCost = RESOURCE_COOLDOWN_ONLY,
                .resourceAmount = 0,
            },
        },
        .skillCount = 1,
        .primaryResource = RESOURCE_COOLDOWN_ONLY,
        .startingResource = 0,
    },
};

/**
 * Initialize a unit with an archetype's stats.
 * Call this after spawning an entity.
 */
void InitUnitStats(EntityPool* pool, int idx, UnitArchetype arch, Faction faction)
{
    if (idx < 0 || idx >= pool->count)
        return;
    if (arch < 0 || arch >= ARCH_COUNT)
        return;

    // Set entity metadata
    pool->meta[idx].archetype = arch;
    pool->meta[idx].faction = faction;
    pool->meta[idx].skill = SKILL_NONE;  // Skills handled separately now

    // Initialize game stats from archetype
    GamePool_InitFromArchetype(idx, &archetypeConfigs[arch]);
}

/**
 * Get a specific archetype config (for reading stats, debugging, etc).
 */
const ArchetypeConfig* GetArchetypeConfig(UnitArchetype arch)
{
    if (arch < 0 || arch >= ARCH_COUNT)
        return NULL;
    return &archetypeConfigs[arch];
}