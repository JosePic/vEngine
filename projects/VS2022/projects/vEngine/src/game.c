#include "game.h"
#include "render_assets.h"
#include "raylib.h"
#include "vengine.h"
#include "physics_system.h"
#include <stdint.h>
#include <game_health_system.h>
#include "game_archetypes.h"



void GatherInput(EntityPool *pool, Camera camera, int playerIdx,
                 bool selected[MAX_ENTITIES], FrameState *fs) {
  fs->ray = GetMouseRay(GetMousePosition(), camera);

  bool camRotate = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT) ||
                   IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);

  bool additive = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

  if (!camRotate && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    fs->selecting = true;
    fs->selectStart = GetMousePosition();
    fs->selectEnd = fs->selectStart;
  }

  if (fs->selecting)
    fs->selectEnd = GetMousePosition();

  if (fs->selecting && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    fs->selecting = false;

    float dx = fs->selectEnd.x - fs->selectStart.x;
    float dy = fs->selectEnd.y - fs->selectStart.y;
    float dragD2 = dx * dx + dy * dy;

    if (!additive)
      memset(selected, 0, sizeof(bool) * MAX_ENTITIES);

    if (dragD2 < 16.0f) {
      RaycastHit picked = RaycastSelectable(pool, fs->ray);
      if (picked.hit) {
        EntityType t = pool->meta[picked.entity].type;
        bool isSelectable = (t == ENTITY_PLAYER) || (t == ENTITY_FRIENDLY);
        if (isSelectable)
          selected[picked.entity] = true;
      }
    } else {
      Rectangle sel = {0};
      sel.x = fminf(fs->selectStart.x, fs->selectEnd.x);
      sel.y = fminf(fs->selectStart.y, fs->selectEnd.y);
      sel.width = fabsf(fs->selectEnd.x - fs->selectStart.x);
      sel.height = fabsf(fs->selectEnd.y - fs->selectStart.y);

      for (int k = 0; k < fs->qSelectable.count; k++) {
        int i = fs->qSelectable.entities[k];

        Vector2 sp = GetWorldToScreen(pool->positions[i], camera);
        float minX = sel.x;
        float maxX = sel.x + sel.width;
        float minY = sel.y;
        float maxY = sel.y + sel.height;
        if (sp.x >= minX && sp.x <= maxX && sp.y >= minY && sp.y <= maxY)
          selected[i] = true;
      }
    }
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    Vector3 p = {0};
    if (RayGroundHit(fs->ray, GROUND_Y, &p)) {
      fs->lastCommandPoint = p;
      int selCount = 0;
      int selIdx[MAX_ENTITIES];
      for (int i = 0; i < pool->count; i++) {
        if (!(pool->masks[i] & COMP_ALIVE))
          continue;
        if (!selected[i])
          continue;
        selIdx[selCount++] = i;
      }

      if (selCount > 0) {
        int cols = (int)ceilf(sqrtf((float)selCount));
        int rows = (int)ceilf((float)selCount / (float)cols);
        float spacing = 0.9f;
        int r = 0, c = 0;
        for (int s = 0; s < selCount; s++) {
          int idx = selIdx[s];
          float ox = ((float)c - (cols - 1) * 0.5f) * spacing;
          float oz = ((float)r - (rows - 1) * 0.5f) * spacing;
          pool->hasMoveTarget[idx] = true;
          pool->moveTargets[idx] =
              (Vector3){p.x + ox, pool->positions[idx].y, p.z + oz};
          c++;
          if (c >= cols) {
            c = 0;
            r++;
          }
        }
      }
    }
  }

  if (IsKeyPressed(KEY_Q)) {
    for (int i = 0; i < pool->count; i++) {
      if (!(pool->masks[i] & COMP_ALIVE))
        continue;
      if (!selected[i])
        continue;
      CastSkillAt(pool, i, fs->lastCommandPoint);
    }
  }

  if (IsKeyPressed(KEY_ONE)) {
    Vector3 p = pool->positions[playerIdx];
    p.x += 1.0f;
    int u =
        SpawnEntity(pool, p, (Vector3){0.6f, 0.6f, 0.6f}, ENTITY_FRIENDLY,
                    ENTITY_WALL | ENTITY_ENEMY | ENTITY_FRIENDLY, false, GREEN);
    if (u >= 0) {
      UnitArchetype arch = (UnitArchetype)GetRandomValue(0, 3);
      InitUnitStats(pool, u, arch, FACTION_PLAYER);
      PhysicsSystem_SetBody(&world, u, 0.0f, 1.0f); // radius=0 → derived from size
      AttachVisual(pool, u, VISUAL_FRIENDLY, 0, 1.0f);
    }
  }
}

void UpdateSpawning(EntityPool *pool, int playerIdx, float dt, FrameState *fs) {
  fs->enemySpawnTimer -= dt;
  if (fs->enemySpawnTimer > 0.0f)
    return;

  fs->enemySpawnTimer = 13.25f;
  Vector3 p = pool->positions[playerIdx];
  float r = WORLD_HALF * 0.45f;
  float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
  float halfH = 0.7f * 0.5f;
  Vector3 epos =
      (Vector3){p.x + cosf(ang) * r, GROUND_Y + halfH, p.z + sinf(ang) * r};
  epos.x = Clamp(epos.x, -WORLD_HALF + 1.0f, WORLD_HALF - 1.0f);
  epos.z = Clamp(epos.z, -WORLD_HALF + 1.0f, WORLD_HALF - 1.0f);
  int e =
      SpawnEntity(pool, epos, (Vector3){0.7f, 0.7f, 0.7f}, ENTITY_ENEMY,
                  ENTITY_WALL | ENTITY_PLAYER | ENTITY_FRIENDLY | ENTITY_ENEMY,
                  false, MAROON);
  if (e >= 0) {
    UnitArchetype arch = (UnitArchetype)GetRandomValue(0, 3);
    InitUnitStats(pool, e, arch, FACTION_ENEMY);
    PhysicsSystem_SetBody(pool, e, 0.0f, 1.0f);
    AttachVisual(pool, e, VISUAL_ENEMY, 0, 1.0f);
  }
}


void initTestScene(int playerIdx) {
  if (playerIdx < 0)
    return;

  float wallH = 5.0f;
  float wallY = wallH * 0.5f;
  float wallLen = WORLD_SIZE;
  float wallT = 1.0f;

  SpawnEntity(&world, (Vector3){-WORLD_HALF, wallY, 0.0f},
              (Vector3){wallT, wallH, wallLen}, ENTITY_WALL, 0, true, BLUE);
  SpawnEntity(&world, (Vector3){WORLD_HALF, wallY, 0.0f},
              (Vector3){wallT, wallH, wallLen}, ENTITY_WALL, 0, true, LIME);
  SpawnEntity(&world, (Vector3){0.0f, wallY, WORLD_HALF},
              (Vector3){wallLen, wallH, wallT}, ENTITY_WALL, 0, true, GOLD);
  SpawnEntity(&world, (Vector3){0.0f, wallY, -WORLD_HALF},
              (Vector3){wallLen, wallH, wallT}, ENTITY_WALL, 0, true, ORANGE);

  for (int i = 0; i < MAX_COLUMNS; i++) {
    float h = (float)GetRandomValue(1, 12);
    int range = (int)(WORLD_HALF - 3.0f);
    Vector3 pos = (Vector3){(float)GetRandomValue(-range, range), h / 2.0f,
                            (float)GetRandomValue(-range, range)};
    Color col =
        (Color){GetRandomValue(20, 255), GetRandomValue(10, 55), 30, 255};

    SpawnEntity(&world, pos, (Vector3){2.0f, h, 2.0f}, ENTITY_WALL, 0, true,
                col);
  }

  InitUnitStats(&world, playerIdx, ARCH_PLAYER, FACTION_PLAYER);
  AttachVisual(&world, playerIdx, VISUAL_PLAYER, 0, 1.0f);
}

int spawnPlayer() {
  float playerHalfH = 0.5f * 0.5f;
  uint32_t playerCollisionMask = ENTITY_WALL | ENTITY_ENEMY | ENTITY_FRIENDLY;
  Vector3 playerDimensions = (Vector3){1.8f, 1.8f, 1.8f};
  Vector3 playerSpawnPosition = (Vector3){0.0f, GROUND_Y + playerHalfH, 0.0f};
  int playerIdx = SpawnEntity(&world, playerSpawnPosition, playerDimensions,
      ENTITY_PLAYER, playerCollisionMask, false, PURPLE);
  if (playerIdx >= 0) {
    PhysicsSystem_SetBody(&world, playerIdx, 0.0f, 1.0f); // radius=0 → derived from size
    AttachVisual(&world, playerIdx, VISUAL_PLAYER, 0, 1.0f);
    InitUnitStats(&world, playerIdx, ARCH_PLAYER, FACTION_PLAYER);
  }

  return playerIdx;
}
