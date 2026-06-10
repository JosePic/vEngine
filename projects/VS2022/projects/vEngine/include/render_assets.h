#pragma once

#include "vengine.h"
#include "vmodel.h"
typedef enum {
    VISUAL_PLAYER = 0,
    VISUAL_ENEMY = 1,
    VISUAL_FRIENDLY = 2,
    VISUAL_PROP = 3,
    VISUAL_COUNT = 4,
} VisualAssetId;

void RenderAssets_Init(void);
void RenderAssets_Shutdown(void);

const VModelAsset* RenderAssets_GetModel(int id);

void AttachVisual(EntityPool* pool, int entity, int modelId, int animationIndex,
    float animationSpeed);
void UpdateAnimatedVisuals(EntityPool* pool, const int* entities, int count, float dt);
