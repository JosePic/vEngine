#include "vengine.h"

EntityPool world = {0};

int SpawnEntity(EntityPool *pool, Vector3 position, Vector3 size,
                EntityType type, uint32_t collidesWithMask, bool is_static,
                Color color) {
  if (pool->count >= MAX_ENTITIES)
    return -1;
  int i = pool->count++;

  pool->positions[i] = position;
  pool->prevPositions[i] = position;
  pool->sizes[i] = size;
  pool->dirty[i] = true;
  pool->colors[i] = color;
  pool->visualModel[i] = -1;
  pool->visualAnimation[i] = -1;
  pool->visualAnimationFrame[i] = 0.0f;
  pool->visualAnimationSpeed[i] = 1.0f;
  pool->visualRotationY[i] = 0.0f;

  pool->masks[i] =
      COMP_POSITION |
      COMP_HEALTH |
      COMP_RENDERABLE |
      COMP_DYNAMIC_MOVILITY |
      COMP_ALIVE;


  pool->meta[i].type = type;
  pool->meta[i].collidesWithMask = collidesWithMask;
  pool->meta[i].faction = FACTION_NEUTRAL;
  pool->meta[i].owner = -1;
  pool->meta[i].archetype = ARCH_RANGED;
  pool->meta[i].skill = SKILL_NONE;
  pool->health[i] = 100;

  pool->velocities[i] = (Vector3){0};
  pool->moveTargets[i] = position;
  pool->hasMoveTarget[i] = false;
  pool->fireCooldown[i] = 0.0f;

  pool->masks[i] |= COMP_VELOCITY;

  if (type == ENTITY_PLAYER || type == ENTITY_FRIENDLY)
    pool->masks[i] |= COMP_PLAYER_CONTROL;
  if (type == ENTITY_ENEMY)
    pool->masks[i] |= COMP_ENEMY_AI;

  pool->moveSpeed[i] = 3.5f;
  pool->attackRange[i] = 6.0f;
  pool->attackCooldown[i] = 0.35f;
  pool->attackDamage[i] = 10;
  pool->skillCooldown[i] = 6.0f;
  pool->skillTimer[i] = 0.0f;
  pool->buffTimer[i] = 0.0f;
  pool->buffFireRateMul[i] = 1.0f;

  Vector3 h = _Vector3Scale(size, 0.5f);
  pool->boxes[i].min =
      (Vector3){position.x - h.x, position.y - h.y, position.z - h.z};
  pool->boxes[i].max =
      (Vector3){position.x + h.x, position.y + h.y, position.z + h.z};

  return i;
}
