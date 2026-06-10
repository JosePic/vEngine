#include "render_assets.h"
#include <vmodel.h>

typedef struct {
    bool loaded;
    const char* name;
    char modelPath[128];
    char animationPath[128];
    Model fallbackModel;
    VModelAsset model;
    ModelAnimation* animations;
    int animationCount;
} RenderAsset;

static RenderAsset s_assets[VISUAL_COUNT];
static bool s_ready = false;

static Model LoadFallbackModel(void)
{
    Mesh mesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    return LoadModelFromMesh(mesh);
}

static void LoadAsset(RenderAsset* asset, const char* name, const char* modelPath,
    const char* animationPath)
{
    asset->loaded = true;
    asset->name = name;
    if (modelPath)
    {
        size_t len = strlen(modelPath);
        if (len >= sizeof(asset->modelPath))
            len = sizeof(asset->modelPath) - 1;
        memcpy(asset->modelPath, modelPath, len);
        asset->modelPath[len] = '\0';
    }
    else
    {
        asset->modelPath[0] = '\0';
    }

    if (animationPath)
    {
        size_t len = strlen(animationPath);
        if (len >= sizeof(asset->animationPath))
            len = sizeof(asset->animationPath) - 1;
        memcpy(asset->animationPath, animationPath, len);
        asset->animationPath[len] = '\0';
    }
    else
    {
        asset->animationPath[0] = '\0';
    }

    if (modelPath && FileExists(modelPath))
    {
        asset->model = LoadVModelAsset(modelPath);
    }
    else
    {
        asset->fallbackModel = LoadFallbackModel();
    }
}

void RenderAssets_Init(void)
{
    if (s_ready)
        return;

    memset(s_assets, 0, sizeof(s_assets));

    LoadAsset(&s_assets[VISUAL_PLAYER], "player", "assets/models/player.vmodel", "assets/models/player.vanim");
    LoadAsset(&s_assets[VISUAL_ENEMY], "enemy", "assets/models/enemy.vmodel", "assets/models/enemy.vanim");
    LoadAsset(&s_assets[VISUAL_FRIENDLY], "friendly", "assets/models/friendly.vmodel", "assets/models/friendly.vanim");
    LoadAsset(&s_assets[VISUAL_PROP], "prop", "assets/models/prop.vmodel", NULL);

    s_ready = true;
}

void RenderAssets_Shutdown(void)
{
    if (!s_ready)
        return;

    for (int i = 0; i < VISUAL_COUNT; i++)
    {
        if (s_assets[i].animations)
        {
            UnloadModelAnimations(s_assets[i].animations, s_assets[i].animationCount);
            s_assets[i].animations = NULL;
            s_assets[i].animationCount = 0;
        }
        
        if (s_assets[i].model.raylibModel.meshCount > 0) {
        UnloadVModelAsset(s_assets[i].model);

        } else if (s_assets[i].fallbackModel.meshCount > 0) {
            UnloadModel(s_assets[i].fallbackModel);  // ← FIXED
        }
               
        s_assets[i].loaded = false;
    }

    s_ready = false;
}

const VModelAsset* RenderAssets_GetModel(int id)
{
    if (id < 0 || id >= VISUAL_COUNT)
        return NULL;
    if (!s_assets[id].loaded)
        return NULL;
    return &s_assets[id].model;
}



void AttachVisual(EntityPool* pool, int entity, int modelId, int animationIndex,
    float animationSpeed)
{
    if (entity < 0 || entity >= pool->count)
        return;

    pool->visualModel[entity] = modelId;
    pool->visualAnimation[entity] = animationIndex;
    pool->visualAnimationFrame[entity] = 0.0f;
    pool->visualAnimationSpeed[entity] = animationSpeed;
    pool->visualRotationY[entity] = 0.0f;
    pool->masks[entity] |= COMP_MODEL;
    if (animationIndex >= 0)
        pool->masks[entity] |= COMP_ANIMATED;
}
void UpdateAnimatedVisuals(EntityPool* pool, const int* entities, int count, float dt)
{
    for (int i = 0; i < count; i++) {
        int idx = entities[i];
        if (idx < 0 || idx >= pool->count) continue;
        if (!(pool->masks[idx] & COMP_ANIMATED)) continue;

        // Advance animation frame by dt * speed
        pool->visualAnimationFrame[idx] += dt * pool->visualAnimationSpeed[idx];
    }
}
