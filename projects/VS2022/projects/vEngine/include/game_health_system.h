
#pragma once

#include "game_stat_types.h"
#include "vengine.h"   // Defines EntityPool
#include "query.h"     // Defines Query

/**
 * Health System - manages damage, healing, death
 */

 /**
  * Deal damage to a target.
  * Accounts for armor, resistances, buffs, etc.
  * Automatically triggers death cleanup if health <= 0.
  */
void HealthSystem_DealDamage(int target, float baseDamage, DamageType type, int source);

/**
 * Heal a target (capped at max health).
 */
void HealthSystem_Heal(int target, int amount);

/**
 * Check if entity is alive and has health > 0.
 */
bool HealthSystem_IsAlive(int entity);

/**
 * Get health as percentage (0.0 - 1.0).
 */
float HealthSystem_GetHealthPercent(int entity);

/**
 * Force kill an entity.
 */
void HealthSystem_Kill(int entity);

/**
 * System function - called during PHASE_CLEANUP.
 * Processes death markers and cleans up dead entities.
 */
void SysHealthCleanup(EntityPool* pool, const Query* q, float dt, void* user);