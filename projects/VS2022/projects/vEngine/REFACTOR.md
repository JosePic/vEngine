# Gameplay Systems Implementation Checklist

## Before You Start
- [ ] Backup your current codebase (git commit)
- [ ] Have all the new files ready (game_stat_types.h, game_pool_data.h/c, etc.)
- [ ] Plan to do this incrementally - don't convert everything at once

---

## Phase 0: Setup (No Logic Changes Yet)

### Create New Files
- [ ] `game_stat_types.h` - Data structures for stats, buffs, state
- [ ] `game_pool_data.h` - GamePool struct and utility functions
- [ ] `game_pool_data.c` - Implementation of GamePool utilities

### Add to main.c (Startup)
```c
#include "game_pool_data.h"

int main(void) {
    // ... existing code ...
    
    // ADD THIS:
    GamePool_Init(&world);  // Initialize GamePool after EntityPool
    
    // ... rest of main ...
}
```

### No changes to game.c yet
- Keep all existing functionality
- Both old and new systems will coexist for now

---

## Phase 1: Migrate Health System

### Create New File
- [ ] `game_health_system.h` - Health system declarations
- [ ] `game_health_system.c` - Health system implementation

### Update game.c - Keep spawnPlayer() etc, but call init function

In `game.c`, add to `spawnPlayer()`:
```c
int spawnPlayer() {
    // ... existing spawn code ...
    
    int playerIdx = SpawnEntity(&world, ...);
    if (playerIdx >= 0) {
        // ... existing setup ...
        
        // NEW: Initialize game data
        ArchetypeConfig cfg = {
            .combatStats = {.moveSpeed = 3.5f, .attackRange = 6.5f, ...},
            .healthStats = {.maxHealth = 100, .armor = 10, ...},
            // ... etc ...
        };
        GamePool_InitFromArchetype(playerIdx, &cfg);
    }
    
    return playerIdx;
}
```

### Add to sim.c - Register health system

In `EnsureQueries()`:
```c
fs->queryDefs[fs->queryDefCount++] = (QueryDef){
    .name = "Health",
    .query = &fs->qHealth,
    .required = COMP_ALIVE,
    .excluded = 0,
    .enabled = true,
};
```

Add to `FrameState` in `sim.h`:
```c
Query qHealth;
```

In `EnsureSystems()`:
```c
RegisterSystem(systems, systemCount, 16, "HealthCleanup", 
               &fs->qHealth, SysHealthCleanup, PHASE_CLEANUP);
```

### Test
- [ ] Code compiles
- [ ] Game runs
- [ ] Player takes damage from enemies
- [ ] Enemies die when health reaches 0

---

## Phase 2: Migrate Combat System

### Create New Files
- [ ] `game_combat_system.h` - Combat declarations
- [ ] `game_combat_system.c` - Combat logic using new data

### Refactor combat.c

**Delete from combat.c:**
- `UpdateCombat_Query()` - we'll rewrite it
- `FindNearestEnemy()` - move to game_combat_system.c

**Keep in combat.c (for now):**
- `SpawnProjectile()` - can be refactored later
- `CastSkillAt()` - still used, can migrate later

### Update game_combat_system.c

```c
static void SysCombat(EntityPool *pool, const Query *q, float dt, void *user) {
    GamePool *gamePool = (GamePool *)user;
    
    for (int k = 0; k < q->count; k++) {
        int entity = q->entities[k];
        
        CombatState *state = &gamePool->combatState[entity];
        CombatStats *stats = &gamePool->combatStats[entity];
        
        // Decay cooldown
        if (state->attackCooldownTimer > 0.0f) {
            state->attackCooldownTimer -= dt;
        }
        
        // Find enemy and attack
        if (state->attackCooldownTimer <= 0.0f) {
            int target = FindNearestEnemy(pool, entity, stats->attackRange);
            if (target >= 0) {
                // Deal damage
                HealthSystem_DealDamage(target, stats->attackDamage, DAMAGE_PHYSICAL, entity);
                
                // Reset cooldown
                state->attackCooldownTimer = GamePool_GetEffectiveAttackCooldown(entity);
            }
        }
    }
}
```

### Register in sim.c
```c
RegisterSystem(systems, systemCount, 16, "Combat", 
               &fs->qCombat, SysCombat, PHASE_COMBAT);
```

### Test
- [ ] Combat still works
- [ ] Attack cooldowns respect buffs
- [ ] Damage scaling with vulnerability buffs works

---

## Phase 3: Migrate Cooldown/Buff System

### Create New File
- [ ] `game_cooldown_system.h` - Cooldown declarations
- [ ] `game_cooldown_system.c` - Buff decay, cooldown decay

### Key Functions
```c
void SysCooldowns(EntityPool *pool, const Query *q, float dt, void *user) {
    GamePool *gamePool = (GamePool *)user;
    
    for (int k = 0; k < q->count; k++) {
        int entity = q->entities[k];
        BuffStack *buffs = &gamePool->buffs[entity];
        
        // Decay buff timers
        int writeIdx = 0;
        for (int i = 0; i < buffs->count; i++) {
            buffs->buffs[i].timeRemaining -= dt;
            if (buffs->buffs[i].timeRemaining > 0.0f) {
                buffs->buffs[writeIdx++] = buffs->buffs[i];
            }
        }
        buffs->count = writeIdx;
    }
}
```

### Register in sim.c
```c
RegisterSystem(systems, systemCount, 16, "Cooldowns", 
               &fs->qCooldowns, SysCooldowns, PHASE_COMBAT);
```

### Test
- [ ] Buffs expire correctly
- [ ] Multiple buffs stack
- [ ] Removing buffs works (for future cleanse abilities)

---

## Phase 4: Migrate Skills System

### Create New Files
- [ ] `game_skill_system.h` - Skill declarations
- [ ] `game_skill_system.c` - Skill casting and effects

### Refactor CastSkillAt()

Currently in `combat.c`, move to `game_skill_system.c`:

```c
void SkillSystem_CastSkill(int caster, int skillIndex, Vector3 targetPos) {
    if (!(gamePool.pool->masks[caster] & COMP_ALIVE))
        return;
    
    // Get skill config
    SkillStats *skill = &gamePool.skillStats[caster].skills[skillIndex];
    SkillState *state = &gamePool.skillStates[caster].skills[skillIndex];
    
    // Check cooldown
    if (state->cooldownTimer > 0.0f)
        return;
    
    // Check resource
    if (!GamePool_ConsumeResource(caster, skill->resourceAmount))
        return;
    
    // Execute skill (you define skill behavior)
    ExecuteSkillEffect(caster, skill, targetPos);
    
    // Apply cooldown
    state->cooldownTimer = skill->cooldown;
}
```

### Add to game input

In `game.c:GatherInput()`:
```c
if (IsKeyPressed(KEY_Q)) {
    for (int i = 0; i < pool->count; i++) {
        if (selected[i]) {
            SkillSystem_CastSkill(i, 0, fs->lastCommandPoint);  // Skill 0
        }
    }
}
```

### Test
- [ ] Skills cast correctly
- [ ] Cooldowns apply
- [ ] Resource costs deduct correctly
- [ ] Skill effects trigger (damage, buffs, etc.)

---

## Phase 5: Cleanup & Optimization

### Deprecate old code
Once new systems are stable:
- [ ] Remove old `UpdateCombat_Query()` from combat.c
- [ ] Remove old `UpdateMovement_Query()` if not used
- [ ] Remove old cooldown tracking from EntityPool

### Add parallelization (optional)

For CooldownSystem, parallelize buff decay:
```c
typedef struct {
    const int *entities;
    int count;
    GamePool *gamePool;
    float dt;
} CooldownTask;

static void CooldownTask_Execute(void *arg) {
    CooldownTask *task = (CooldownTask *)arg;
    // ... process buffs for assigned entities ...
}

// Then in SysCooldowns:
uint32_t workerCount = GetEngineWorkerCount();
// ... split work into tasks and push to thread pool ...
WaitTaskGroup();
```

---

## Integration Examples

### Full Config Example

```c
// In game_archetypes.c (data-driven)

static const ArchetypeConfig archetypeConfigs[ARCH_COUNT] = {
    [ARCH_MELEE] = {
        .combatStats = {
            .moveSpeed = 2.8f,
            .attackRange = 1.2f,
            .attackCooldown = 0.55f,
            .attackDamage = 14.0f,
        },
        .healthStats = {
            .maxHealth = 120,
            .armor = 15,
            .magicResist = 0.1f,
        },
        .skillStats = {
            [0] = {
                .cooldown = 5.0f,
                .range = 3.5f,
                .baseDamage = 20.0f,
                .resourceCost = RESOURCE_COOLDOWN_ONLY,
            },
        },
        .skillCount = 1,
        .primaryResource = RESOURCE_MANA,
        .startingResource = 100,
    },
    // ... more archetypes ...
};

void InitUnitStats(EntityPool *pool, int idx, UnitArchetype arch, Faction faction) {
    pool->meta[idx].archetype = arch;
    pool->meta[idx].faction = faction;
    
    const ArchetypeConfig *cfg = &archetypeConfigs[arch];
    GamePool_InitFromArchetype(idx, cfg);
}
```

### Skill Execution Example

```c
typedef void (*SkillEffect)(int caster, Vector3 target, float baseDamage);

void SkillEffect_DirectDamage(int caster, Vector3 target, float baseDamage) {
    // Find nearest enemy to target
    int enemy = FindNearestEnemyAt(target, 2.0f);
    if (enemy >= 0) {
        HealthSystem_DealDamage(enemy, baseDamage, DAMAGE_MAGICAL, caster);
    }
}

void SkillEffect_AreaAura(int caster, Vector3 target, float baseDamage) {
    // Apply buff to allies in area
    for (int i = 0; i < gamePool.pool->count; i++) {
        if (gamePool.pool->meta[i].faction != gamePool.pool->meta[caster].faction)
            continue;
        
        Vector3 d = Vector3Subtract(gamePool.pool->positions[i], target);
        if (Vector3Length(d) < 5.0f) {
            GamePool_AddBuff(i, BUFF_HASTE, 3.0f, 0.5f);  // +50% speed for 3s
        }
    }
}

void ExecuteSkillEffect(int caster, const SkillStats *skill, Vector3 targetPos) {
    // In real implementation, would dispatch to skill-specific function
    // For now, example direct damage
    SkillEffect_DirectDamage(caster, targetPos, skill->baseDamage);
}
```

---

## Testing Checklist

### Phase 1 (Health)
- [ ] Spawn units with health
- [ ] Take damage from enemies
- [ ] Die when health reaches 0
- [ ] UI shows health correctly (if implemented)

### Phase 2 (Combat)
- [ ] Attack cooldowns work
- [ ] Effective cooldown changes with haste/slow buffs
- [ ] Ranged vs melee attacks work
- [ ] Projectiles spawn and disappear

### Phase 3 (Cooldowns/Buffs)
- [ ] Apply buff to unit
- [ ] Buff expires after duration
- [ ] Multiple buffs on same unit
- [ ] Buff magnitudes affect stats

### Phase 4 (Skills)
- [ ] Cast skill with cooldown
- [ ] Skill effects trigger (damage, buffs, heal)
- [ ] Resource costs deduct
- [ ] Can't cast on cooldown

### Phase 5 (Integration)
- [ ] All systems work together
- [ ] No conflicts with physics/rendering
- [ ] Performance acceptable
- [ ] No memory leaks

---

## Common Pitfalls & Solutions

### Pitfall 1: Old & New Systems Conflict
**Problem:** Old code still modifying EntityPool fields, new code reading GamePool.  
**Solution:** Keep them in sync during migration. Have old system update GamePool after each change.

### Pitfall 2: Forgetting to Sync Health
**Problem:** New system uses gamePool.healthStats, but rendering/UI reads pool->health.  
**Solution:** After every damage/heal, sync: `pool->health[entity] = gamePool.healthStats[entity].currentHealth;`

### Pitfall 3: Buffer Overflows
**Problem:** Too many buffs on entity, MAX_BUFFS_PER_ENTITY is exceeded.  
**Solution:** Compact dead buffs every frame, limit buff types applied, or increase MAX_BUFFS_PER_ENTITY.

### Pitfall 4: Query Not Finding Entities
**Problem:** System query returns 0 entities even though units exist.  
**Solution:** Verify query required components match what you're spawning. Check masks are set correctly.

### Pitfall 5: Parallelization Bugs
**Problem:** Two workers modify same entity, race condition.  
**Solution:** Never parallelize per-entity - each worker handles disjoint entity slice. Test sequential first.

---

## Next Steps After Integration

Once core systems are migrated:

1. **Add Ability/Skill Variety**
   - Design 3-5 unique skills per archetype
   - Implement cool effects (projectiles, AOE, buffs, debuffs)

2. **Polish Combat Feel**
   - Add animation/VFX for attacks
   - Add impact feedback (knockback, stun)
   - Tune cooldowns and damage values

3. **Add Special Mechanics**
   - Resource systems (mana, energy, rage)
   - Combo/chain systems
   - Item/equipment system

4. **Optimize & Profile**
   - Use thread pool for expensive operations
   - Profile hot loops
   - Cache-friendly data layout

---

## Files Deliverable

```
game/
├── game_stat_types.h       ✓ (ready)
├── game_pool_data.h        ✓ (ready)
├── game_pool_data.c        ✓ (ready)
├── game_health_system.h    ✓ (ready)
├── game_health_system.c    ✓ (ready)
├── game_cooldown_system.h  (you write based on template)
├── game_cooldown_system.c  (you write based on template)
├── game_combat_system.h    (you write, refactor from combat.c)
├── game_combat_system.c    (you write, refactor from combat.c)
├── game_skill_system.h     (you write, migrate CastSkillAt)
├── game_skill_system.c     (you write, migrate CastSkillAt)
└── game_archetypes.c       (you write, move InitUnitStats here)
```

---

## Time Estimate

- Phase 0 (Setup): 15 minutes
- Phase 1 (Health): 1-2 hours (includes testing)
- Phase 2 (Combat): 1-2 hours
- Phase 3 (Cooldowns/Buffs): 1 hour
- Phase 4 (Skills): 1-2 hours
- Phase 5 (Cleanup): 30 minutes

**Total: ~6-9 hours for full migration**

Recommend doing one phase per day/session so you don't get overwhelmed.