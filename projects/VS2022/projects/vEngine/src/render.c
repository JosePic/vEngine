#include "render.h"
#include "render_assets.h"
#include "vmodel.h"
#include <vinspector.h>
static void DrawEntityVisual(EntityPool* pool, int i)
{
    const VModelAsset* assetModel = RenderAssets_GetModel(pool->visualModel[i]);
    if (!assetModel)
    {
        DrawCube(pool->positions[i], pool->sizes[i].x, pool->sizes[i].y,
            pool->sizes[i].z, pool->colors[i]);
        DrawCubeWires(pool->positions[i], pool->sizes[i].x, pool->sizes[i].y,
            pool->sizes[i].z, BLACK);
        return;
    }

    // Cast away const to update the animation (animation frame was already advanced by UpdateAnimatedVisuals)
    VModelAsset* model = (VModelAsset*)assetModel;
    if (pool->visualAnimation[i] >= 0 && pool->visualAnimation[i] < (int)model->animationCount) {
        UpdateVModelAnimation(model, pool->visualAnimation[i], pool->visualAnimationFrame[i]);
    }

    float yaw = pool->visualRotationY[i];
    float speed = fabsf(pool->velocities[i].x) + fabsf(pool->velocities[i].z);
    if (speed > 0.001f)
        yaw = atan2f(pool->velocities[i].x, pool->velocities[i].z);

    DrawModelEx(model->raylibModel, pool->positions[i], (Vector3) { 0.0f, 1.0f, 0.0f },
        yaw* RAD2DEG, pool->sizes[i], pool->colors[i]);
    //DrawModelWiresEx(model.raylibModel, pool->positions[i], (Vector3){0.0f, 1.0f, 0.0f},
      //  yaw * RAD2DEG, pool->sizes[i], BLACK);
}

void DrawRaylibForensics(const VInspectorDB* db, int screenWidth, int screenHeight) {
    // UI State
    static bool showDebugger = false;
    static int targetEntity = 0;
    static float scrollY = 0;

    // Toggle debugger with F3
    if (IsKeyPressed(KEY_F3)) showDebugger = !showDebugger;
    if (!showDebugger) return;

    // --- Input Handling ---
    // Change target entity with Up/Down arrows
    if (IsKeyPressed(KEY_UP)) targetEntity++;
    if (IsKeyPressed(KEY_DOWN) && targetEntity > 0) targetEntity--;

    // Handle scrolling
    scrollY += GetMouseWheelMove() * 30.0f;
    if (scrollY > 0) scrollY = 0; // Cap scrolling at the top

    // --- Background Panel ---
    int panelWidth = 500;
    int panelX = screenWidth - panelWidth;
    DrawRectangle(panelX, 0, panelWidth, screenHeight, Fade(BLACK, 0.85f));
    DrawLine(panelX, 0, panelX, screenHeight, DARKGRAY);

    // --- Header ---
    DrawText(TextFormat("FORENSIC DEBUGGER [F3 to hide]"), panelX + 10, 10, 20, RAYWHITE);
    DrawText(TextFormat("Target Entity: %d (Up/Down to change)", targetEntity), panelX + 10, 40, 10, LIGHTGRAY);
    DrawLine(panelX, 60, screenWidth, 60, GRAY);

    // --- Forensic Chain Rendering ---
    int yCursor = 70 + (int)scrollY;

    VRecordRef ref = db->entityLatestMutation[targetEntity];

    if (ref.frameNumber == 0) {
        DrawText("No data recorded for this entity.", panelX + 10, yCursor, 10, GRAY);
        return;
    }

    // Walk backward through time
    while (ref.frameNumber > 0) {
        const VFrameCapture* frame = &db->ringBuffer[ref.ringIndex];

        // Stale check
        if (!frame->isPopulated || frame->frameNumber != ref.frameNumber) {
            DrawText("[--- Data truncated: Ring buffer wrapped ---]", panelX + 10, yCursor, 10, ORANGE);
            break;
        }

        const VMutationRecord* mut = &frame->mutations[ref.recordIndex];
        const VTransactionNode* tx = VInspect_FindTransaction(db, mut->transactionId);

        // Only draw if it's on screen (culling)
        if (yCursor > 60 && yCursor < screenHeight) {
            // Draw Timeline Node
            DrawCircle(panelX + 15, yCursor + 5, 4, GREEN);
            DrawLine(panelX + 15, yCursor + 9, panelX + 15, yCursor + 45, DARKGRAY);

            // Draw Frame & Value Change
            DrawText(TextFormat("Frame %u | Field %u Mutated", frame->frameNumber, mut->fieldId), panelX + 30, yCursor, 10, RAYWHITE);
            DrawText(TextFormat("Old: %.1f  ->  New: %.1f", mut->oldVal.f32[0], mut->newVal.f32[0]), panelX + 30, yCursor + 15, 10, GREEN);

            // Draw Causality
            if (tx) {
                DrawText(TextFormat("Cause TX: 0x%X (System %u)", (uint32_t)tx->id, tx->sourceSystem), panelX + 30, yCursor + 30, 10, GRAY);
            }
            else {
                DrawText("Cause TX: [Dropped from buffer]", panelX + 30, yCursor + 30, 10, RED);
            }
        }

        yCursor += 50; // Step down for the next record

        // Intrusive list traversal
        ref = mut->prevMutationForEntity;
    }
}

extern VInspectorDB g_InspectorDB; // Your global DB

void RenderFrame(EntityPool* pool, Camera camera, int playerIdx,
    const bool selected[MAX_ENTITIES], bool selecting,
    Vector2 selectStart, Vector2 selectEnd,
    const Query* qRenderable, float dt) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(camera);

    DrawPlane((Vector3) { 0.0f, 0.0f, 0.0f }, (Vector2) { WORLD_SIZE, WORLD_SIZE },
        LIGHTGRAY);

    for (int k = 0; k < qRenderable->count; k++) {
        int i = qRenderable->entities[k];
        DrawEntityVisual(pool, i);

        if (selected[i] && (pool->meta[i].type == ENTITY_PLAYER ||
            pool->meta[i].type == ENTITY_FRIENDLY)) {
            DrawCubeWires(pool->positions[i], pool->sizes[i].x, pool->sizes[i].y,
                pool->sizes[i].z, ORANGE);
        }
    }

    EndMode3D();

    DrawText(
        TextFormat("Player: (%.2f, %.2f, %.2f)", pool->positions[playerIdx].x,
            pool->positions[playerIdx].y, pool->positions[playerIdx].z),
        10, 10, 20, BLACK);
    DrawText("LMB select/drag. Shift add. RMB move. 1 spawn ally.", 10, 60, 20,
        DARKGRAY);

    if (selecting) {
        float x = fminf(selectStart.x, selectEnd.x);
        float y = fminf(selectStart.y, selectEnd.y);
        float w = fabsf(selectEnd.x - selectStart.x);
        float h = fabsf(selectEnd.y - selectStart.y);
        DrawRectangleLinesEx((Rectangle) { x, y, w, h }, 2, SKYBLUE);
    }
    DrawRaylibForensics(&g_InspectorDB, GetScreenWidth(), GetScreenHeight());
    EndDrawing();
}