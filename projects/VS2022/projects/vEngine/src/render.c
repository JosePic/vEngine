#include "render.h"
#include "render_assets.h"
#include "vmodel.h"
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

    EndDrawing();
}