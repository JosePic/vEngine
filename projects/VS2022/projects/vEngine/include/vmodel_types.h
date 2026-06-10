#ifndef VMODEL_TYPES_H
#define VMODEL_TYPES_H

#include <stdint.h>

#define CHUNK_MESH 0x4853454D // "MESH"
#define CHUNK_MATL 0x4C54414D // "MATL"
#define CHUNK_SKEL 0x4C454B53 // "SKEL"
#define CHUNK_SKIN 0x4E494B53 // "SKIN"
#define CHUNK_ANIM 0x4D494E41 // "ANIM"

#define MAX_BONES 128

typedef struct {
    uint32_t magic;       // "VMDL" (0x4C444D56)
    uint32_t version;     // 2 (Upgraded version for decoupled pipelines)
    uint32_t chunkCount;
    uint64_t fileSize;
} FileHeader;

typedef struct {
    char name[64];
    uint32_t materialIndex;
    float min[3];
    float max[3];
    uint32_t vertexCount;
    uint32_t indexCount;
} MeshHeader;

typedef struct {
    char albedoTexture[256];
} MaterialHeader;

typedef struct {
    char name[32];
    int32_t parentIndex;

    float localTranslation[3];
    float localRotation[4];
    float localScale[3];
    float inverseBind[16];
} BoneHeader;

typedef struct {
    uint32_t meshIndex;        // Submesh target index
    uint32_t vertexCount;      // Must match mesh vertex count
} SkinHeader;

typedef struct {
    char name[64];
    float duration;            // Animation duration in seconds
    uint32_t trackCount;       // Number of bones with active animation channels
} AnimHeader;

typedef struct {
    uint32_t boneIndex;
    uint32_t translationKeyCount;
    uint32_t rotationKeyCount;
    uint32_t scaleKeyCount;
} BoneTrackHeader;

typedef struct { float time; float value[3]; } Vector3Key;
typedef struct { float time; float value[4]; } QuaternionKey;

#endif
