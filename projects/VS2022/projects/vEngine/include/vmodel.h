#ifndef VMODEL_H
#define VMODEL_H

#include "raylib.h"
#include "vmodel_types.h" // Includes <stdint.h> and raw chunk structures

typedef struct {
    uint8_t ids[4];
    float weights[4];
} VertexSkinWeights;

typedef struct {
    uint32_t boneIndex;
    uint32_t translationKeyCount;
    uint32_t rotationKeyCount;
    uint32_t scaleKeyCount;
    Vector3Key* translationKeys;
    QuaternionKey* rotationKeys;
    Vector3Key* scaleKeys;
} RuntimeBoneTrack;

typedef struct {
    char name[64];
    float duration;
    uint32_t trackCount;
    RuntimeBoneTrack* tracks;
} RuntimeAnimation;

typedef struct {
    Model raylibModel;          // Clean handle for renderer interaction
    uint32_t boneCount;
    BoneHeader* skeleton;
    VertexSkinWeights** skins;  // Dynamic skin array: [meshCount][vertexCount]
    uint32_t animationCount;
    RuntimeAnimation* animations;

    // Cache targets for high-speed software skin blending passes
    float* originalVertices[64];
    float* originalNormals[64];
} VModelAsset;

// Module Subsystem Interface Prototypes
VModelAsset LoadVModelAsset(const char* filename);
void UpdateVModelAnimation(VModelAsset* asset, uint32_t animIndex, float time);
void UnloadVModelAsset(VModelAsset asset);

#endif // VMODEL_H