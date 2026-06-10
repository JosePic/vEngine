#include "game.h"
#include "render_assets.h"
#include "sim.h"
#include "physics_system.h"
#include "game_pool_data.h"


void initCamera(Camera *camera) {
  camera->position = (Vector3){0.0f, 2.0f, -30.0f};
  camera->target = (Vector3){0.0f, 0.5f, 0.0f};
  camera->up = (Vector3){0.0f, 1.0f, 0.0f};
  camera->projection = CAMERA_ORTHOGRAPHIC;
  camera->fovy = 20.0f;
}

#include "skinning_worker.h" // Include this to access the Thread Pool functions
#include "engine_task_scheduler.h" // Assuming this is where InitEngineThreadPool is defined
#include "vmodel.h"



int main(void) {
  const int screenWidth = 1280;
  const int screenHeight = 720;

  InitWindow(screenWidth, screenHeight, "vEngine test scene");

  InitEngineThreadPool(4);

  GamePool_Init(&world);  // Initialize GamePool after EntityPool

  Camera camera = {0};
  initCamera(&camera);
  RenderAssets_Init();

  float camYaw = -135.0f * DEG2RAD;
  float camPitch = -45.0f * DEG2RAD;
  float camDist = 40.0f;
  int playerIdx = spawnPlayer();
  initTestScene(playerIdx);
  PhysicsSystem_Init();

  static bool selected[MAX_ENTITIES] = {0};
  selected[playerIdx] = true;


  SetTargetFPS(60);

  FrameState fs = {0};
  while (!WindowShouldClose()) {
    RunFrame(&world, &camera, playerIdx, selected, &fs, &camYaw, &camPitch,
             &camDist);
  }

  RenderAssets_Shutdown();
  PhysicsSystem_Shutdown();
  CloseWindow();

  return 0;
}
