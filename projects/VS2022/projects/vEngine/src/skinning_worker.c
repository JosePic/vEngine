#include "skinning_worker.h"
#include "engine_task_scheduler.h"
#include <xmmintrin.h>
#include <emmintrin.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

// Mirror internal types completely decoupled from global structures
typedef struct {
    uint8_t ids[4];
    float weights[4];
} WorkerSkinWeights;

typedef struct {
    uint32_t startVertex;
    uint32_t endVertex;
    const float* origV;
    const float* origN;
    float* destV;
    float* destN;
    const WorkerSkinWeights* skins;
    const float* bonePalette;
    const float* boneRotationPalette;
} SkinningSubTask;

// Preallocated task array to prevent allocation inside runtime loops
#define MAX_SUB_TASKS 16
static SkinningSubTask g_subTaskCache[MAX_SUB_TASKS];

static inline __m128 TransformVectorSSE(const float* vec3, const float* mat) {
    __m128 vX = _mm_set1_ps(vec3[0]);
    __m128 vY = _mm_set1_ps(vec3[1]);
    __m128 vZ = _mm_set1_ps(vec3[2]);

    __m128 m0 = _mm_setr_ps(mat[0], mat[1], mat[2], mat[3]);
    __m128 m1 = _mm_setr_ps(mat[4], mat[5], mat[6], mat[7]);
    __m128 m2 = _mm_setr_ps(mat[8], mat[9], mat[10], mat[11]);
    __m128 m3 = _mm_setr_ps(mat[12], mat[13], mat[14], mat[15]);

    __m128 res = _mm_add_ps(_mm_mul_ps(vX, m0), _mm_mul_ps(vY, m1));
    res = _mm_add_ps(res, _mm_mul_ps(vZ, m2));
    res = _mm_add_ps(res, m3);
    return res;
}

static inline __m128 RotateVectorByQuaternionSSE(const float* vec3, const float* q) {
    __m128 v = _mm_setr_ps(vec3[0], vec3[1], vec3[2], 0.0f);
    __m128 qVec = _mm_setr_ps(q[0], q[1], q[2], 0.0f);
    __m128 qW = _mm_set1_ps(q[3]);

    __m128 qYZX = _mm_shuffle_ps(qVec, qVec, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 vZXY = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 qZXY = _mm_shuffle_ps(qVec, qVec, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 vYZX = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 0, 2, 1));

    __m128 cross = _mm_sub_ps(_mm_mul_ps(qYZX, vZXY), _mm_mul_ps(qZXY, vYZX));
    __m128 wCross = _mm_mul_ps(qW, cross);

    __m128 crossYZX = _mm_shuffle_ps(cross, cross, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 crossZXY = _mm_shuffle_ps(cross, cross, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 qCross = _mm_sub_ps(_mm_mul_ps(qYZX, crossZXY), _mm_mul_ps(qZXY, crossYZX));

    __m128 factor = _mm_set1_ps(2.0f);
    __m128 offset = _mm_mul_ps(factor, _mm_add_ps(wCross, qCross));

    return _mm_add_ps(v, offset);
}

// Concrete execution task logic fed to your Pool Workers
static void SkinningTaskEntryPoint(void* arg) {
    SkinningSubTask* task = (SkinningSubTask*)arg;
    float outV[4];
    float outN[4];

    for (uint32_t v = task->startVertex; v < task->endVertex; v++) {
        const WorkerSkinWeights* skin = &task->skins[v];
        const float* vOrig = &task->origV[v * 3];
        const float* origN = &task->origN[v * 3];

        __m128 vFinal = _mm_setzero_ps();
        __m128 nFinal = _mm_setzero_ps();

        for (int i = 0; i < 4; i++) {
            float w = skin->weights[i];
            if (w <= 0.0001f) continue;
            uint8_t boneId = skin->ids[i];

            __m128 vTrans = TransformVectorSSE(vOrig, &task->bonePalette[boneId * 16]);
            __m128 nTrans = RotateVectorByQuaternionSSE(origN, &task->boneRotationPalette[boneId * 4]);

            __m128 wQuad = _mm_set1_ps(w);
            vFinal = _mm_add_ps(vFinal, _mm_mul_ps(vTrans, wQuad));
            nFinal = _mm_add_ps(nFinal, _mm_mul_ps(nTrans, wQuad));
        }

        _mm_storeu_ps(outV, vFinal);
        task->destV[v * 3] = outV[0];
        task->destV[v * 3 + 1] = outV[1];
        task->destV[v * 3 + 2] = outV[2];

        _mm_storeu_ps(outN, nFinal);
        float lenSq = outN[0] * outN[0] + outN[1] * outN[1] + outN[2] * outN[2];
        if (lenSq > 0.00001f) {
            float invLen = 1.0f / sqrtf(lenSq);
            task->destN[v * 3] = outN[0] * invLen;
            task->destN[v * 3 + 1] = outN[1] * invLen;
            task->destN[v * 3 + 2] = outN[2] * invLen;
        }
        else {
            task->destN[v * 3] = origN[0];
            task->destN[v * 3 + 1] = origN[1];
            task->destN[v * 3 + 2] = origN[2];
        }
    }
}

// This function interface exposes no raw win32 definitions to raylib_model_loader
void BuildParallelSkinningTasks(
    uint32_t vertexCount,
    const float* origV, const float* origN,
    float* destV, float* destN,
    const void* skins,
    const float* bonePalette,
    const float* boneRotationPalette)
{
    uint32_t workerCount = GetEngineWorkerCount();
    if (workerCount > MAX_SUB_TASKS) workerCount = MAX_SUB_TASKS;
    if (workerCount == 0) return;

    uint32_t vertsPerTask = vertexCount / workerCount;

    for (uint32_t t = 0; t < workerCount; t++) {
        g_subTaskCache[t].startVertex = t * vertsPerTask;
        g_subTaskCache[t].endVertex = (t == workerCount - 1) ? vertexCount : (t + 1) * vertsPerTask;
        g_subTaskCache[t].origV = origV;
        g_subTaskCache[t].origN = origN;
        g_subTaskCache[t].destV = destV;
        g_subTaskCache[t].destN = destN;
        g_subTaskCache[t].skins = (const WorkerSkinWeights*)skins;
        g_subTaskCache[t].bonePalette = bonePalette;
        g_subTaskCache[t].boneRotationPalette = boneRotationPalette;

        // Dispatch directly into running worker threads
        PushEngineTask(SkinningTaskEntryPoint, &g_subTaskCache[t]);
    }

    // Block calling engine thread until workers finish computing the batches
    WaitTaskGroup();
}