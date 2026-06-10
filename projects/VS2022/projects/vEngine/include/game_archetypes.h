#pragma once

#include "game_stat_types.h"
#include "vengine.h"

/**
 * Initialize a unit with an archetype's stats.
 * Call this after spawning an entity.
 *
 * Example:
 *   int unit = SpawnEntity(&world, pos, size, ENTITY_FRIENDLY, ...);
 *   InitUnitStats(&world, unit, ARCH_MELEE, FACTION_PLAYER);
 */
void InitUnitStats(EntityPool* pool, int idx, UnitArchetype arch, Faction faction);

/**
 * Get a specific archetype config (for reading stats, debugging, UI, etc).
 * Returns NULL if arch is invalid.
 */
const ArchetypeConfig* GetArchetypeConfig(UnitArchetype arch);