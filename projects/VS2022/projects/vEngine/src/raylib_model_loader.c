#define _CRT_SECURE_NO_WARNINGS
#include "vmodel.h"
#include "raymath.h"
#include "skinning_worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SAFE_READ(ptr, size, count, stream) \
    if (fread(ptr, size, count, stream) != count) goto cleanup;

static void NormalizeWeights4(float* w) {
    float sum = w[0] + w[1] + w[2] + w[3];
    if (sum > 0.00001f) {
        w[0] /= sum;
        w[1] /= sum;
        w[2] /= sum;
        w[3] /= sum;
    }
    else {
        w[0] = 1.0f;
        w[1] = 0.0f;
        w[2] = 0.0f;
        w[3] = 0.0f;
    }
}

VModelAsset LoadVModelAsset(const char* filename) {
    VModelAsset asset = { 0 };
    FILE* file = fopen(filename, "rb");
    if (!file) return asset;

    FileHeader fHeader;
    SAFE_READ(&fHeader, sizeof(FileHeader), 1, file);

    if (fHeader.magic != 0x4C444D56 || fHeader.version != 2) goto cleanup;

    long startPos = ftell(file);
    uint32_t meshAllocCount = 0, matAllocCount = 0, animAllocCount = 0;
    while (ftell(file) < (long)fHeader.fileSize) {
        uint32_t cId = 0, cSize = 0;
        if (fread(&cId, sizeof(uint32_t), 1, file) != 1) break;
        fread(&cSize, sizeof(uint32_t), 1, file);
        if (cId == CHUNK_MESH) meshAllocCount++;
        if (cId == CHUNK_MATL) matAllocCount++;
        if (cId == CHUNK_ANIM) animAllocCount++;
        fseek(file, cSize, SEEK_CUR);
    }

    fseek(file, startPos, SEEK_SET);

    asset.raylibModel.meshCount = meshAllocCount;
    asset.raylibModel.materialCount = (matAllocCount > 0) ? matAllocCount : meshAllocCount;
    asset.raylibModel.meshes = (Mesh*)MemAlloc(sizeof(Mesh) * asset.raylibModel.meshCount);
    asset.raylibModel.materials = (Material*)MemAlloc(sizeof(Material) * asset.raylibModel.materialCount);
    asset.raylibModel.meshMaterial = (int*)MemAlloc(sizeof(int) * asset.raylibModel.meshCount);

    if (!asset.raylibModel.meshes || !asset.raylibModel.materials || !asset.raylibModel.meshMaterial) goto cleanup;

    memset(asset.raylibModel.meshes, 0, sizeof(Mesh) * asset.raylibModel.meshCount);
    memset(asset.raylibModel.materials, 0, sizeof(Material) * asset.raylibModel.materialCount);
    memset(asset.raylibModel.meshMaterial, 0, sizeof(int) * asset.raylibModel.meshCount);

    asset.raylibModel.transform = MatrixIdentity();
    asset.skins = (VertexSkinWeights**)calloc(asset.raylibModel.meshCount, sizeof(VertexSkinWeights*));

    if (animAllocCount > 0) {
        asset.animationCount = animAllocCount;
        asset.animations = (RuntimeAnimation*)calloc(animAllocCount, sizeof(RuntimeAnimation));
    }

    uint32_t meshLoadedIdx = 0, matLoadedIdx = 0, animLoadedIdx = 0;

    while (ftell(file) < (long)fHeader.fileSize) {
        uint32_t chunkId = 0, chunkSize = 0;
        if (fread(&chunkId, sizeof(uint32_t), 1, file) != 1) break;
        SAFE_READ(&chunkSize, sizeof(uint32_t), 1, file);

        long chunkStart = ftell(file);

        if (chunkId == CHUNK_MESH) {
            MeshHeader mHead;
            SAFE_READ(&mHead, sizeof(MeshHeader), 1, file);

            Mesh* m = &asset.raylibModel.meshes[meshLoadedIdx];
            m->vertexCount = mHead.vertexCount;
            m->triangleCount = mHead.indexCount / 3;

            TraceLog(LOG_INFO, "Mesh '%s': vertices=%u triangles=%u", mHead.name, m->vertexCount, m->triangleCount);
            m->vertices = (float*)MemAlloc(sizeof(float) * 3 * m->vertexCount);
            m->normals = (float*)MemAlloc(sizeof(float) * 3 * m->vertexCount);
            m->texcoords = (float*)MemAlloc(sizeof(float) * 2 * m->vertexCount);
            m->indices = (unsigned short*)MemAlloc(sizeof(unsigned short) * mHead.indexCount);

            SAFE_READ(m->vertices, sizeof(float) * 3, m->vertexCount, file);
            SAFE_READ(m->normals, sizeof(float) * 3, m->vertexCount, file);
            SAFE_READ(m->texcoords, sizeof(float) * 2, m->vertexCount, file);
            if (mHead.indexCount > 0) SAFE_READ(m->indices, sizeof(unsigned short), mHead.indexCount, file);

            asset.originalVertices[meshLoadedIdx] = malloc(sizeof(float) * 3 * m->vertexCount);
            asset.originalNormals[meshLoadedIdx] = malloc(sizeof(float) * 3 * m->vertexCount);
            if (asset.originalVertices[meshLoadedIdx]) memcpy(asset.originalVertices[meshLoadedIdx], m->vertices, sizeof(float) * 3 * m->vertexCount);
            if (asset.originalNormals[meshLoadedIdx])  memcpy(asset.originalNormals[meshLoadedIdx], m->normals, sizeof(float) * 3 * m->vertexCount);

            UploadMesh(m, true);
            asset.raylibModel.meshMaterial[meshLoadedIdx] = (int)mHead.materialIndex;
            meshLoadedIdx++;
        }
        else if (chunkId == CHUNK_SKIN) {
            SkinHeader sHead;
            SAFE_READ(&sHead, sizeof(SkinHeader), 1, file);

            uint8_t* bIds = malloc(sHead.vertexCount * 4 * sizeof(uint8_t));
            float* bWts = malloc(sHead.vertexCount * 4 * sizeof(float));
            if (bIds && bWts) {
                SAFE_READ(bIds, sizeof(uint8_t) * 4, sHead.vertexCount, file);
                SAFE_READ(bWts, sizeof(float) * 4, sHead.vertexCount, file);

                asset.skins[sHead.meshIndex] = malloc(sHead.vertexCount * sizeof(VertexSkinWeights));
                if (asset.skins[sHead.meshIndex]) {
                    for (uint32_t v = 0; v < sHead.vertexCount; v++) {
                        memcpy(asset.skins[sHead.meshIndex][v].ids, &bIds[v * 4], 4);
                        memcpy(asset.skins[sHead.meshIndex][v].weights, &bWts[v * 4], sizeof(float) * 4);
                        NormalizeWeights4(asset.skins[sHead.meshIndex][v].weights);
                    }
                }
            }
            free(bIds); free(bWts);
        }
        else if (chunkId == CHUNK_MATL) {
            MaterialHeader mFormat;
            SAFE_READ(&mFormat, sizeof(MaterialHeader), 1, file);

            Material mat = LoadMaterialDefault();
            if (strlen(mFormat.albedoTexture) > 0) {
                Texture2D tex = LoadTexture(mFormat.albedoTexture);
                mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
            }
            asset.raylibModel.materials[matLoadedIdx] = mat;
            matLoadedIdx++;
        }
        else if (chunkId == CHUNK_SKEL) {
            SAFE_READ(&asset.boneCount, sizeof(uint32_t), 1, file);
            asset.skeleton = (BoneHeader*)malloc(sizeof(BoneHeader) * asset.boneCount);
            if (asset.skeleton) SAFE_READ(asset.skeleton, sizeof(BoneHeader), asset.boneCount, file);
        }
        else if (chunkId == CHUNK_ANIM) {
            AnimHeader aHead;
            SAFE_READ(&aHead, sizeof(AnimHeader), 1, file);

            RuntimeAnimation* anim = &asset.animations[animLoadedIdx];
            strncpy(anim->name, aHead.name, 63);
            anim->duration = aHead.duration;
            anim->trackCount = aHead.trackCount;
            anim->tracks = (RuntimeBoneTrack*)calloc(aHead.trackCount, sizeof(RuntimeBoneTrack));

            if (anim->tracks) {
                for (uint32_t t = 0; t < aHead.trackCount; t++) {
                    BoneTrackHeader tHead;
                    SAFE_READ(&tHead, sizeof(BoneTrackHeader), 1, file);

                    RuntimeBoneTrack* track = &anim->tracks[t];
                    track->boneIndex = tHead.boneIndex;
                    track->translationKeyCount = tHead.translationKeyCount;
                    track->rotationKeyCount = tHead.rotationKeyCount;
                    track->scaleKeyCount = tHead.scaleKeyCount;

                    if (tHead.translationKeyCount > 0) {
                        track->translationKeys = malloc(sizeof(Vector3Key) * tHead.translationKeyCount);
                        if (track->translationKeys) SAFE_READ(track->translationKeys, sizeof(Vector3Key), tHead.translationKeyCount, file);
                    }
                    if (tHead.rotationKeyCount > 0) {
                        track->rotationKeys = malloc(sizeof(QuaternionKey) * tHead.rotationKeyCount);
                        if (track->rotationKeys) SAFE_READ(track->rotationKeys, sizeof(QuaternionKey), tHead.rotationKeyCount, file);
                    }
                    if (tHead.scaleKeyCount > 0) {
                        track->scaleKeys = malloc(sizeof(Vector3Key) * tHead.scaleKeyCount);
                        if (track->scaleKeys) SAFE_READ(track->scaleKeys, sizeof(Vector3Key), tHead.scaleKeyCount, file);
                    }
                }
            }
            animLoadedIdx++;
        }

        fseek(file, chunkStart + chunkSize, SEEK_SET);
    }

    if (matLoadedIdx == 0) {
        for (int i = 0; i < asset.raylibModel.materialCount; i++) {
            asset.raylibModel.materials[i] = LoadMaterialDefault();
        }
    }

    fclose(file);
    return asset;

cleanup:
    if (file) fclose(file);
    return (VModelAsset) { 0 };
}

static Vector3 SampleTranslation(RuntimeBoneTrack* track, float time, Vector3 fallback) {
    if (track->translationKeyCount == 0) return fallback;
    if (time <= track->translationKeys[0].time) return *(Vector3*)track->translationKeys[0].value;
    if (time >= track->translationKeys[track->translationKeyCount - 1].time)
        return *(Vector3*)track->translationKeys[track->translationKeyCount - 1].value;

    for (uint32_t i = 0; i < track->translationKeyCount - 1; i++) {
        if (time >= track->translationKeys[i].time && time <= track->translationKeys[i + 1].time) {
            float factor = (time - track->translationKeys[i].time) / (track->translationKeys[i + 1].time - track->translationKeys[i].time);
            return Vector3Lerp(*(Vector3*)track->translationKeys[i].value, *(Vector3*)track->translationKeys[i + 1].value, factor);
        }
    }
    return fallback;
}

static Quaternion SampleRotation(RuntimeBoneTrack* track, float time, Quaternion fallback) {
    if (track->rotationKeyCount == 0) return fallback;
    if (time <= track->rotationKeys[0].time) return *(Quaternion*)track->rotationKeys[0].value;
    if (time >= track->rotationKeys[track->rotationKeyCount - 1].time)
        return *(Quaternion*)track->rotationKeys[track->rotationKeyCount - 1].value;

    for (uint32_t i = 0; i < track->rotationKeyCount - 1; i++) {
        if (time >= track->rotationKeys[i].time && time <= track->rotationKeys[i + 1].time) {
            float factor = (time - track->rotationKeys[i].time) / (track->rotationKeys[i + 1].time - track->rotationKeys[i].time);
            return QuaternionSlerp(*(Quaternion*)track->rotationKeys[i].value, *(Quaternion*)track->rotationKeys[i + 1].value, factor);
        }
    }
    return fallback;
}

static Vector3 SampleScale(RuntimeBoneTrack* track, float time, Vector3 fallback) {
    if (track->scaleKeyCount == 0) return fallback;
    if (time <= track->scaleKeys[0].time) return *(Vector3*)track->scaleKeys[0].value;
    if (time >= track->scaleKeys[track->scaleKeyCount - 1].time)
        return *(Vector3*)track->scaleKeys[track->scaleKeyCount - 1].value;

    for (uint32_t i = 0; i < track->scaleKeyCount - 1; i++) {
        if (time >= track->scaleKeys[i].time && time <= track->scaleKeys[i + 1].time) {
            float factor = (time - track->scaleKeys[i].time) / (track->scaleKeys[i + 1].time - track->scaleKeys[i].time);
            return Vector3Lerp(*(Vector3*)track->scaleKeys[i].value, *(Vector3*)track->scaleKeys[i + 1].value, factor);
        }
    }
    return fallback;
}

void UpdateVModelAnimation(VModelAsset* asset, uint32_t animIndex, float time) {
    if (asset->boneCount == 0 || animIndex >= asset->animationCount) return;
    if (asset->boneCount > MAX_BONES) {
        TraceLog(LOG_WARNING, "VModel skinning skipped: boneCount=%u exceeds MAX_BONES=%d", asset->boneCount, MAX_BONES);
        return;
    }

    double t0 = GetTime();

    RuntimeAnimation* anim = &asset->animations[animIndex];
    static int skinTraceDone = 0;
    int traceSkinning = !skinTraceDone;

    if (anim->duration > 0.00001f && time > anim->duration) time = fmodf(time, anim->duration);

    Matrix localTransforms[MAX_BONES] = { 0 };
    Matrix globalMatrices[MAX_BONES] = { 0 };
    Matrix bonePalette[MAX_BONES] = { 0 };
    Quaternion boneRotationPalette[MAX_BONES] = { 0 };

    RuntimeBoneTrack* translationTracks[MAX_BONES] = { 0 };
    RuntimeBoneTrack* rotationTracks[MAX_BONES] = { 0 };
    RuntimeBoneTrack* scaleTracks[MAX_BONES] = { 0 };

    for (uint32_t tk = 0; tk < anim->trackCount; tk++) {
        RuntimeBoneTrack* track = &anim->tracks[tk];
        if (track->boneIndex >= asset->boneCount) continue;
        if (track->translationKeyCount > 0) translationTracks[track->boneIndex] = track;
        if (track->rotationKeyCount > 0) rotationTracks[track->boneIndex] = track;
        if (track->scaleKeyCount > 0) scaleTracks[track->boneIndex] = track;
    }

    for (uint32_t b = 0; b < asset->boneCount; b++) {
        BoneHeader* bone = &asset->skeleton[b];
        Vector3 t = *(Vector3*)bone->localTranslation;
        Quaternion r = *(Quaternion*)bone->localRotation;
        Vector3 s = *(Vector3*)bone->localScale;

        if (translationTracks[b]) t = SampleTranslation(translationTracks[b], time, t);
        if (rotationTracks[b]) r = SampleRotation(rotationTracks[b], time, r);
        if (scaleTracks[b]) s = SampleScale(scaleTracks[b], time, s);

        Matrix T = MatrixTranslate(t.x, t.y, t.z);
        Matrix R = QuaternionToMatrix(r);
        Matrix S = MatrixScale(s.x, s.y, s.z);

        localTransforms[b] = MatrixMultiply(T, MatrixMultiply(R, S));
    }

    for (uint32_t b = 0; b < asset->boneCount; b++) {
        int32_t parent = asset->skeleton[b].parentIndex;
        if (parent == -1) {
            globalMatrices[b] = localTransforms[b];
        }
        else {
            globalMatrices[b] = MatrixMultiply(globalMatrices[parent], localTransforms[b]);
        }
    }

    for (uint32_t b = 0; b < asset->boneCount; b++) {
        Matrix invBind = *(Matrix*)asset->skeleton[b].inverseBind;
        bonePalette[b] = MatrixMultiply(globalMatrices[b], invBind);
        boneRotationPalette[b] = QuaternionFromMatrix(bonePalette[b]);
    }

    double t1 = GetTime();
    double totalUploadTime = 0.0;

    if (traceSkinning && asset->boneCount > 0) {
        BoneHeader* bone = &asset->skeleton[0];
        TraceLog(LOG_INFO, "VModel skin trace: bones=%u tracks=%u anim='%s' duration=%f bone0='%s'",
            asset->boneCount, anim->trackCount, anim->name, anim->duration, bone->name);
    }

    for (int mIdx = 0; mIdx < (int)asset->raylibModel.meshCount; mIdx++) {
        if (!asset->skins[mIdx]) continue;
        Mesh* m = &asset->raylibModel.meshes[mIdx];

        if (traceSkinning) {
            TraceLog(LOG_INFO, "Skinning mesh %d: vertices=%u", mIdx, m->vertexCount);
        }

        // Dispatch computing workload chunking straight to engine worker threads via clean interface 
        BuildParallelSkinningTasks(
            m->vertexCount,
            asset->originalVertices[mIdx],
            asset->originalNormals[mIdx],
            m->vertices,
            m->normals,
            asset->skins[mIdx],
            (const float*)bonePalette,
            (const float*)boneRotationPalette
        );

        double t2_sub = GetTime();

        UpdateMeshBuffer(*m, 0, m->vertices, sizeof(float) * 3 * m->vertexCount, 0);
        UpdateMeshBuffer(*m, 2, m->normals, sizeof(float) * 3 * m->vertexCount, 0);

        totalUploadTime += (GetTime() - t2_sub);
    }

    double t3 = GetTime();

    static int telemetryFrames = 0;
    if (telemetryFrames++ % 60 == 0) {
        TraceLog(LOG_INFO, "VModel Profile -> Hierarchy Evaluation: %.2fms | Parallel Task-Pool Skinning: %.2fms | GPU VRAM Upload: %.2fms",
            (t1 - t0) * 1000.0,
            (t3 - t1 - totalUploadTime) * 1000.0,
            totalUploadTime * 1000.0);
    }

    if (traceSkinning) skinTraceDone = 1;
}

void UnloadVModelAsset(VModelAsset asset) {
    UnloadModel(asset.raylibModel);
    if (asset.skeleton) free(asset.skeleton);
    if (asset.skins) {
        for (uint32_t i = 0; i < asset.raylibModel.meshCount; i++) {
            if (asset.skins[i]) free(asset.skins[i]);
            if (asset.originalVertices[i]) free(asset.originalVertices[i]);
            if (asset.originalNormals[i]) free(asset.originalNormals[i]);
        }
        free(asset.skins);
    }
    if (asset.animations) {
        for (uint32_t i = 0; i < asset.animationCount; i++) {
            RuntimeAnimation* anim = &asset.animations[i];
            if (anim->tracks) {
                for (uint32_t t = 0; t < anim->trackCount; t++) {
                    if (anim->tracks[t].translationKeys) free(anim->tracks[t].translationKeys);
                    if (anim->tracks[t].rotationKeys) free(anim->tracks[t].rotationKeys);
                    if (anim->tracks[t].scaleKeys) free(anim->tracks[t].scaleKeys);
                }
                free(anim->tracks);
            }
        }
        free(asset.animations);
    }
}