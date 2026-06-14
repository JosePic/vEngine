#include "vengine.h"
#include "game_health_system.h"
#include "query.h"

/**
 * Projectile Hit System
 *
 * Handles projectile-to-enemy collisions and damage application.
 * Projectiles are destroyed on first collision with a valid target.
 */

static bool projectileHit[MAX_ENTITIES] = { 0 };

void UpdateProjectileHits(EntityPool* pool, const int* entities, int count)
{
    // Reset hit tracking each frame
    for (int i = 0; i < MAX_ENTITIES; i++) {
        projectileHit[i] = false;
    }

    // Iterate through query results (only projectiles with COMP_IS_PROJECTILE)
    for (int k = 0; k < count; k++) {
        int projectile = entities[k];

        if (projectileHit[projectile])
            continue;

        int owner = pool->meta[projectile].owner;
        if (owner < 0 || owner >= pool->count)
            continue;

        Vector3 projPos = pool->positions[projectile];
        float projRadius = pool->physRadius[projectile];
        uint32_t projCollisionMask = pool->meta[projectile].collidesWithMask;

        // Check against ALL entities to find targets
        for (int i = 0; i < pool->count; i++) {
            if (i == projectile || i == owner)
                continue;
            if (!(pool->masks[i] & COMP_ALIVE))
                continue;

            uint32_t targetType = pool->meta[i].type;

            // Only damage actual units, not walls
            if (targetType != ENTITY_ENEMY && targetType != ENTITY_PLAYER && targetType != ENTITY_FRIENDLY)
                continue;

            // Check collision mask
            if (!(projCollisionMask & targetType))
                continue;

            // Sphere-sphere collision test
            Vector3 targetPos = pool->positions[i];
            Vector3 delta = Vector3Subtract(targetPos, projPos);
            float distSq = Vector3LengthSqr(delta);
            float minDist = projRadius + pool->physRadius[i];
            float minDistSq = minDist * minDist;

            if (distSq < minDistSq) {
                // Hit detected!
                float baseDamage = 200.0f;
                HealthSystem_DealDamage(i, baseDamage, DAMAGE_PHYSICAL, owner);
                projectileHit[projectile] = true;
                HealthSystem_Kill(projectile);
                break;
            }
        }
    }
}

void SysProjectileHits(EntityPool* pool, const Query* q, float dt, void* user)
{
    (void)dt;
    (void)user;
    UpdateProjectileHits(pool, q->entities, q->count);
}