#pragma once

#include "vengine.h"
#include "query.h"

/**
 * Projectile hit detection system.
 * Call this during PHASE_COMBAT to detect projectile impacts and deal damage.
 */
void SysProjectileHits(EntityPool* pool, const Query* q, float dt, void* user);

/**
 * Direct function for testing projectile hits.
 */
void UpdateProjectileHits(EntityPool* pool, const int* entities, int count);
