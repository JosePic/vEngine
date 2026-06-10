#include "vengine.h"



RaycastHit RaycastSelectable(EntityPool *pool, Ray ray) {
  RaycastHit best = {0};
  best.distance = FLT_MAX;
  best.entity = -1;

  for (int i = 0; i < pool->count; i++) {
    if (!(pool->masks[i] & COMP_ALIVE))
      continue;
    if (!(pool->masks[i] & COMP_PLAYER_CONTROL))
      continue;

    RayCollision hit = GetRayCollisionBox(ray, pool->boxes[i]);
    if (hit.hit && hit.distance < best.distance) {
      best.hit = true;
      best.entity = i;
      best.distance = hit.distance;
      best.point = hit.point;
      best.normal = hit.normal;
    }
  }

  return best;
}

bool RayGroundHit(Ray ray, float groundY, Vector3 *outPoint) {
  if (fabsf(ray.direction.y) < 0.0001f)
    return false;
  float t = (groundY - ray.position.y) / ray.direction.y;
  if (t < 0.0f)
    return false;
  if (outPoint)
    *outPoint = Vector3Add(ray.position, _Vector3Scale(ray.direction, t));
  return true;
}
