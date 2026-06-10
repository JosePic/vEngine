#include "physics_system.h"
#include "vengine.h"

static int FindNearestEnemy(EntityPool *pool, int fromIdx, float maxRange) {
  Vector3 from = pool->positions[fromIdx];
  int best = -1;
  float bestD2 = maxRange * maxRange;

  Faction myFaction = pool->meta[fromIdx].faction;

  for (int i = 0; i < pool->count; i++) {
    ComponentMask mask = pool->masks[i];
    if (!(mask & COMP_ALIVE))
      continue;
    if (pool->meta[i].faction == myFaction)
      continue;
    bool isUnit = (mask & (COMP_PLAYER_CONTROL | COMP_ENEMY_AI)) != 0;
    if (!isUnit)
      continue;

    Vector3 d = Vector3Subtract(pool->positions[i], from);
    float d2 = Vector3LengthSqr(d);
    if (d2 < bestD2) {
      bestD2 = d2;
      best = i;
    }
  }
  return best;
}

static int SpawnProjectile(EntityPool *pool, int ownerIdx, Vector3 start, Vector3 dir) {
  dir = Vector3Normalize(dir);
  uint32_t mask = ENTITY_WALL;
  if (pool->meta[ownerIdx].faction == FACTION_ENEMY)
    mask |= ENTITY_PLAYER | ENTITY_FRIENDLY;
  else
    mask |= ENTITY_ENEMY;

  int p = SpawnEntity(pool, start, (Vector3){0.25f, 0.25f, 0.25f},
                      ENTITY_PROJECTILE, mask, false, ORANGE);
  if (p < 0)
    return -1;

  PhysicsSystem_SetBody(&world, p, 0.0f, 1.0f);

  pool->meta[p].owner = ownerIdx;
  pool->meta[p].faction = pool->meta[ownerIdx].faction;
  pool->health[p] = 1;

  float speed = 6.0f;
  pool->velocities[p] = _Vector3Scale(dir, speed);
  return p;
}

void UpdateCombat_Query(EntityPool *pool, float dt, const int *entities, int count) {
  for (int k = 0; k < count; k++) {
    int i = entities[k];
    ComponentMask mask = pool->masks[i];
    if (!(mask & COMP_ALIVE))
      continue;
    bool isUnit = (mask & (COMP_PLAYER_CONTROL | COMP_ENEMY_AI)) != 0;
    if (!isUnit)
      continue;

    if (pool->fireCooldown[i] > 0.0f)
      pool->fireCooldown[i] -= dt;
    if (pool->skillTimer[i] > 0.0f)
      pool->skillTimer[i] -= dt;
    if (pool->buffTimer[i] > 0.0f) {
      pool->buffTimer[i] -= dt;
      if (pool->buffTimer[i] <= 0.0f)
        pool->buffFireRateMul[i] = 1.0f;
    }

    int target = FindNearestEnemy(pool, i, pool->attackRange[i]);
    if (target < 0)
      continue;

    Vector3 from = pool->positions[i];
    Vector3 to = pool->positions[target];
    Vector3 dir = Vector3Subtract(to, from);
    dir.y = 0.0f;
    float dist = Vector3Length(dir);
    if (dist <= 0.0001f)
      continue;
    dir = _Vector3Scale(dir, 1.0f / dist);

    if (pool->fireCooldown[i] <= 0.0f) {
      float fireRateMul = pool->buffFireRateMul[i];
      float cd = pool->attackCooldown[i] / (fireRateMul > 0.001f ? fireRateMul : 1.0f);
      pool->fireCooldown[i] = cd;

      bool melee = (pool->meta[i].archetype == ARCH_MELEE);
      if (melee) {
        pool->health[target] -= pool->attackDamage[i];
      } else {
        Vector3 start = Vector3Add(from, (Vector3){0.0f, 0.4f, 0.0f});
        SpawnProjectile(pool, i, start, dir);
      }
    }
  }
}

void CastSkillAt(EntityPool *pool, int idx, Vector3 groundPoint) {
  if (!(pool->masks[idx] & COMP_ALIVE))
    return;

  SkillType s = pool->meta[idx].skill;
  if (s == SKILL_NONE)
    return;

  if (pool->skillTimer[idx] > 0.0f)
    return;

  pool->skillTimer[idx] = pool->skillCooldown[idx];

  Vector3 pos = pool->positions[idx];

  switch (s) {
  case SKILL_DASH: {
    Vector3 to = Vector3Subtract(groundPoint, pos);
    to.y = 0.0f;
    float d = Vector3Length(to);
    if (d > 0.001f) {
      Vector3 dir = _Vector3Scale(to, 1.0f / d);
      pool->positions[idx] = Vector3Add(pos, _Vector3Scale(dir, 3.5f));
      pool->hasMoveTarget[idx] = false;
      pool->velocities[idx] = (Vector3){0};
    }
  } break;

  case SKILL_BURST: {
    int bursts = 3;
    for (int b = 0; b < bursts; b++) {
      float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
      Vector3 dir = (Vector3){cosf(ang), 0.0f, sinf(ang)};
      Vector3 start = Vector3Add(pos, (Vector3){0.0f, 0.4f, 0.0f});
      SpawnProjectile(pool, idx, start, dir);
    }
  } break;

  case SKILL_NOVA: {
    float r = 3.0f;
    float r2 = r * r;
    for (int i = 0; i < pool->count; i++) {
      if (!(pool->masks[i] & COMP_ALIVE))
        continue;
      if (pool->meta[i].faction == pool->meta[idx].faction)
        continue;
      EntityType t = pool->meta[i].type;
      bool isUnit = (t == ENTITY_PLAYER) || (t == ENTITY_ENEMY) || (t == ENTITY_FRIENDLY);
      if (!isUnit)
        continue;
      Vector3 d = Vector3Subtract(pool->positions[i], pos);
      d.y = 0.0f;
      if (Vector3LengthSqr(d) <= r2)
        pool->health[i] -= 18;
    }
  } break;

  case SKILL_AURA: {
    float r = 6.0f;
    float r2 = r * r;
    for (int i = 0; i < pool->count; i++) {
      if (!(pool->masks[i] & COMP_ALIVE))
        continue;
      if (pool->meta[i].faction != pool->meta[idx].faction)
        continue;
      EntityType t = pool->meta[i].type;
      bool isUnit = (t == ENTITY_PLAYER) || (t == ENTITY_ENEMY) || (t == ENTITY_FRIENDLY);
      if (!isUnit)
        continue;
      Vector3 d = Vector3Subtract(pool->positions[i], pos);
      d.y = 0.0f;
      if (Vector3LengthSqr(d) <= r2) {
        pool->buffTimer[i] = 3.5f;
        pool->buffFireRateMul[i] = 2.2f;
      }
    }
  } break;

  default:
    break;
  }
}
