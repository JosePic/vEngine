
#pragma once
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Damage Types
// ---------------------------------------------------------------------------
typedef enum {
    DAMAGE_PHYSICAL,
    DAMAGE_MAGICAL,
    DAMAGE_TRUE,        // Ignores armor/resistances
    DAMAGE_SPECIAL,     // Custom damage handling
} DamageType;

// ---------------------------------------------------------------------------
// Buff Types (extensible)
// ---------------------------------------------------------------------------
typedef enum {
    BUFF_HASTE,         // Speed increase
    BUFF_SLOW,          // Speed decrease
    BUFF_ARMOR,         // Damage reduction
    BUFF_VULNERABLE,    // Increased damage taken
    BUFF_REGENERATION,  // Health per second
    BUFF_BARRIER,       // Shield (temporary extra HP)
    BUFF_POISON,        // Damage over time
    BUFF_STUN,          // Can't move/attack
    BUFF_CUSTOM_0,      // Custom slots for designers
    BUFF_CUSTOM_1,
    BUFF_CUSTOM_2,
} BuffType;

// ---------------------------------------------------------------------------
// Resource Types (for future expansion: mana, energy, rage, etc.)
// ---------------------------------------------------------------------------
typedef enum {
    RESOURCE_MANA,
    RESOURCE_ENERGY,
    RESOURCE_RAGE,
    RESOURCE_COOLDOWN_ONLY,  // No resource cost
} ResourceType;

// ---------------------------------------------------------------------------
// Combat Stats (persistent per-entity, defined by archetype)
// ---------------------------------------------------------------------------
typedef struct {
    float moveSpeed;         // Units/second
    float attackRange;       // Units (max distance to attack)
    float attackCooldown;    // Seconds between attacks
    float attackDamage;      // Base damage (float allows buff multiplication)
    float attackSpeed;       // Multiplier to cooldown (1.5 = 50% faster)
} CombatStats;

// ---------------------------------------------------------------------------
// Health Stats (persistent, can change over lifetime)
// ---------------------------------------------------------------------------
typedef struct {
    int maxHealth;
    int currentHealth;
    int armor;               // Physical damage reduction (0-100+)
    float magicResist;       // Magical damage reduction (0.0-1.0)
    float damageMultiplier;  // Additional scaling (1.0 = normal)
} HealthStats;

// ---------------------------------------------------------------------------
// Skill/Ability Stats
// ---------------------------------------------------------------------------
typedef struct {
    float cooldown;          // Seconds between uses
    float range;             // Max distance to target
    ResourceType resourceCost;
    int resourceAmount;      // How much of the resource
    float baseDamage;        // If applicable
    int maxCharges;          // Multi-charge skills (0 = single)
} SkillStats;

// ---------------------------------------------------------------------------
// Runtime Combat State (changes every frame)
// ---------------------------------------------------------------------------
typedef struct {
    float attackCooldownTimer;
    float attackSpeedMultiplier;  // From buffs (1.0 = normal)
    bool canAttack;               // Derived: timer <= 0
} CombatState;

// ---------------------------------------------------------------------------
// Runtime Skill State (per skill)
// ---------------------------------------------------------------------------
typedef struct {
    float cooldownTimer;
    int currentCharges;
} SkillState;

#define MAX_SKILLS_PER_UNIT 4

typedef struct {
    SkillState skills[MAX_SKILLS_PER_UNIT];
    uint8_t skillCount;
} SkillStates;

// ---------------------------------------------------------------------------
// Mana/Resource System
// ---------------------------------------------------------------------------
typedef struct {
    int currentResource;
    int maxResource;
    float regenRate;          // Per second
    float regenDelay;         // Start regens after X seconds out of combat
    float regenTimer;
} ResourceState;

// ---------------------------------------------------------------------------
// Buff/Status Effect (individual)
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t type;             // BuffType enum
    float timeRemaining;      // Seconds (-1 = infinite)
    float magnitude;          // Strength (interpretation depends on type)
    int sourceEntity;         // Who applied this buff (-1 = system)
} ActiveBuff;

#define MAX_BUFFS_PER_ENTITY 16

// Stack of active buffs on an entity
typedef struct {
    ActiveBuff buffs[MAX_BUFFS_PER_ENTITY];
    uint8_t count;
} BuffStack;

// ---------------------------------------------------------------------------
// Archetype Configuration (data-driven)
// Defines initial stats for unit types
// ---------------------------------------------------------------------------
typedef struct {
    CombatStats combatStats;
    HealthStats healthStats;
    SkillStats skillStats[MAX_SKILLS_PER_UNIT];
    uint8_t skillCount;
    ResourceType primaryResource;
    int startingResource;
} ArchetypeConfig;

// ---------------------------------------------------------------------------
// Behavioral State
// ---------------------------------------------------------------------------
typedef enum {
    BEHAVIOR_IDLE,
    BEHAVIOR_MOVING,
    BEHAVIOR_ATTACKING,
    BEHAVIOR_CASTING,
    BEHAVIOR_STUNNED,
} BehaviorState;

typedef struct {
    BehaviorState state;
    float stateTimer;         // Time in current state
    int targetEntity;         // -1 = no target
} BehaviorData;

// ---------------------------------------------------------------------------
// Composite: Everything a unit needs for gameplay
// ---------------------------------------------------------------------------
typedef struct {
    // Persistent stats (from archetype)
    CombatStats combatStats;
    HealthStats healthStats;
    SkillStates skillStates;

    // Runtime state (changes per frame)
    CombatState combatState;
    ResourceState resourceState;
    BuffStack buffs;
    BehaviorData behavior;

    // Targeting
    int targetEntity;
    bool hasTargetEntity;
} UnitGameData;