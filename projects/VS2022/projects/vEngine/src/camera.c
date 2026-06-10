#include "vengine.h"

Vector3 GetSelectionCenter(EntityPool *pool, const bool selected[],
                           int fallbackIdx) {
  Vector3 sum = (Vector3){0};
  int n = 0;
  for (int i = 0; i < pool->count; i++) {
    if (!(pool->masks[i] & COMP_ALIVE))
      continue;
    if (!selected[i])
      continue;
    sum = Vector3Add(sum, pool->positions[i]);
    n++;
  }

  if (n > 0)
    return _Vector3Scale(sum, 1.0f / (float)n);
  return pool->positions[fallbackIdx];
}

void UpdateIsometricCamera(Camera *camera, Vector3 target, float *yawRad,
                           float *pitchRad, float *distance, bool allowRotate) {
  camera->target = target;

  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f) {
    *distance *= (1.0f - wheel * 0.08f);
    *distance = Clamp(*distance, 8.0f, 120.0f);
  }

  if (allowRotate) {
    Vector2 md = GetMouseDelta();
    float sens = 0.01f;
    *yawRad -= md.x * sens;
    *pitchRad -= md.y * sens;
    *pitchRad = Clamp(*pitchRad, -1.35f, -0.15f);
  }

  float cp = cosf(*pitchRad);
  float sp = sinf(*pitchRad);
  float cy = cosf(*yawRad);
  float sy = sinf(*yawRad);

  Vector3 forward = (Vector3){sy * cp, sp, cy * cp};
  Vector3 offset = _Vector3Scale(forward, *distance);
  camera->position = Vector3Subtract(target, offset);
}
