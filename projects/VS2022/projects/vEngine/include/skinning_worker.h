#ifndef SKINNING_WORKER_H
#define SKINNING_WORKER_H

#include <stdint.h>

/**
 * Dispatches a parallelized, SIMD-accelerated linear blend skinning job
 * to the engine's central task scheduler thread pool.
 *
 * This function blocks the calling thread until all workers have completely
 * processed their allocated vertex slices, guaranteeing data readiness.
 */
void BuildParallelSkinningTasks(
    uint32_t vertexCount,
    const float* origV,
    const float* origN,
    float* destV,
    float* destN,
    const void* skins,                  // Passed as void* to bypass type dependency
    const float* bonePalette,           // 16 floats per matrix
    const float* boneRotationPalette    // 4 floats per quaternion
);

#endif // SKINNING_WORKER_H