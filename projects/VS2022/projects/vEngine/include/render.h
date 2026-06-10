#include "query.h"
#include "vengine.h"

void RenderFrame(EntityPool *pool, Camera camera, int playerIdx,
                 const bool selected[MAX_ENTITIES], bool selecting,
                 Vector2 selectStart, Vector2 selectEnd,
                 const Query *qRenderable, float dt);
