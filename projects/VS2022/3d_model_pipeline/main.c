#include "vmodel.h"
#include "skinning_worker.h" // Include this to access the Thread Pool functions
#include "engine_task_scheduler.h" // Assuming this is where InitEngineThreadPool is defined

int main(void) {
    InitWindow(800, 450, "VEngine - Architecture Animation Module");
    SetTargetFPS(60);

    // 1. INITIALIZE ENGINE THREAD POOL AT STARTUP
    // Use 4 threads, or query system core counts via Win32
    InitEngineThreadPool(4);

    // Stream out fully layout-compliant engine asset structures
    VModelAsset myAsset = LoadVModelAsset("test.vmodel");
    TraceLog(LOG_INFO, "ENGINE DIAGNOSTIC: Loaded %u Bones", myAsset.boneCount);
    TraceLog(LOG_INFO, "ENGINE DIAGNOSTIC: Loaded %u Animations", myAsset.animationCount);

    Camera camera = { 0 };
    camera.position = (Vector3){ 5.0f, 5.0f, 5.0f };
    camera.target = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float animTime = 0.0f;

    while (!WindowShouldClose()) {
        UpdateCamera(&camera, CAMERA_FREE);

        animTime += GetFrameTime();

        // Safe interpolation evaluation execution pass
        if (myAsset.animationCount > 0) {
            UpdateVModelAnimation(&myAsset, 0, animTime);
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);
        BeginMode3D(camera);

        // Draw your running model submeshes safely
        if (myAsset.raylibModel.meshCount > 0) {
            DrawModel(myAsset.raylibModel, (Vector3) { 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
            DrawModel(myAsset.raylibModel, (Vector3) { 3.0f, 3.0f, 3.0f }, 1.0f, WHITE);
            DrawModel(myAsset.raylibModel, (Vector3) { 3.0f, 3.0f, 3.0f }, 1.0f, WHITE);
            DrawModel(myAsset.raylibModel, (Vector3) { 5.0f, 3.0f, 3.0f }, 1.0f, WHITE);
            DrawModel(myAsset.raylibModel, (Vector3) { 3.0f, 4.0f, 5.0f }, 1.0f, WHITE);
            DrawModel(myAsset.raylibModel, (Vector3) { 13.0f, 3.0f, 3.0f }, 1.0f, WHITE);
            DrawModel(myAsset.raylibModel, (Vector3) { 3.0f, 43.0f, 3.0f }, 1.0f, WHITE);

        }

        DrawGrid(10, 1.0f);
        EndMode3D();

        DrawText("Status: Streaming Decoupled Skeletal Transformations", 10, 40, 20, GREEN);
        DrawFPS(10, 10);
        EndDrawing();
    }

    // Free all system resources before cleanup exit paths
    UnloadVModelAsset(myAsset);

    // 2. SHUT DOWN THE THREAD POOL CLEANLY BEFORE EXITING
    ShutdownEngineThreadPool();

    CloseWindow();
    return 0;
}