// physics.c

#include "physics.h"

#include <math.h>
#include <string.h>

#if defined(_M_X64) || defined(__x86_64__) || defined(__SSE2__) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define PHYSICS_SIMD_SSE2 1
#include <emmintrin.h>
#else
#define PHYSICS_SIMD_SSE2 0
#endif

static uint32_t PhysicsClampCellCoord(int value, int maxValue)
{
    if (value < 0)
    {
        return 0;
    }

    if (value >= maxValue)
    {
        return (uint32_t)(maxValue - 1);
    }

    return (uint32_t)value;
}

static uint32_t PhysicsComputeGridCell(
    PhysicsGrid* grid,
    float x,
    float y)
{
    uint32_t cellX =
        PhysicsClampCellCoord((int)(x * grid->invCellSize), PHYSICS_GRID_WIDTH);

    uint32_t cellY =
        PhysicsClampCellCoord((int)(y * grid->invCellSize), PHYSICS_GRID_HEIGHT);

    return cellY * PHYSICS_GRID_WIDTH + cellX;
}

static void PhysicsTestBodyAgainstGridRangeSIMD(
    PhysicsBodies* bodies,
    uint32_t bodyIndex,
    uint32_t* indices,
    uint32_t start,
    uint32_t count,
    PhysicsContactBuffer* contacts)
{
#if PHYSICS_SIMD_SSE2
    uint32_t offset = 0;

    __m128 posXi = _mm_set1_ps(bodies->posX[bodyIndex]);
    __m128 posYi = _mm_set1_ps(bodies->posY[bodyIndex]);
    __m128 radiusI = _mm_set1_ps(bodies->radius[bodyIndex]);

    for (; offset + 3 < count; offset += 4)
    {
        uint32_t i0 = indices[start + offset + 0];
        uint32_t i1 = indices[start + offset + 1];
        uint32_t i2 = indices[start + offset + 2];
        uint32_t i3 = indices[start + offset + 3];

        contacts->pairCount += 4;

        __m128 posXj =
            _mm_set_ps(
                bodies->posX[i3],
                bodies->posX[i2],
                bodies->posX[i1],
                bodies->posX[i0]);

        __m128 posYj =
            _mm_set_ps(
                bodies->posY[i3],
                bodies->posY[i2],
                bodies->posY[i1],
                bodies->posY[i0]);

        __m128 radiusJ =
            _mm_set_ps(
                bodies->radius[i3],
                bodies->radius[i2],
                bodies->radius[i1],
                bodies->radius[i0]);

        __m128 dx = _mm_sub_ps(posXj, posXi);
        __m128 dy = _mm_sub_ps(posYj, posYi);
        __m128 radius = _mm_add_ps(radiusI, radiusJ);

        __m128 distSq =
            _mm_add_ps(
                _mm_mul_ps(dx, dx),
                _mm_mul_ps(dy, dy));

        __m128 radiusSq = _mm_mul_ps(radius, radius);
        __m128 cmp = _mm_cmplt_ps(distSq, radiusSq);
        int mask = _mm_movemask_ps(cmp);

        if ((mask & 1) != 0)
        {
            PhysicsGenerateContact(bodyIndex, i0, contacts);
        }

        if ((mask & 2) != 0)
        {
            PhysicsGenerateContact(bodyIndex, i1, contacts);
        }

        if ((mask & 4) != 0)
        {
            PhysicsGenerateContact(bodyIndex, i2, contacts);
        }

        if ((mask & 8) != 0)
        {
            PhysicsGenerateContact(bodyIndex, i3, contacts);
        }
    }

    for (; offset < count; offset++)
    {
        uint32_t otherIndex = indices[start + offset];

        contacts->pairCount++;

        if (PhysicsCircleVsCircleScalar(bodies, bodyIndex, otherIndex))
        {
            PhysicsGenerateContact(bodyIndex, otherIndex, contacts);
        }
    }
#else
    for (uint32_t offset = 0; offset < count; offset++)
    {
        uint32_t otherIndex = indices[start + offset];

        contacts->pairCount++;

        if (PhysicsCircleVsCircleScalar(bodies, bodyIndex, otherIndex))
        {
            PhysicsGenerateContact(bodyIndex, otherIndex, contacts);
        }
    }
#endif
}

void PhysicsStep(PhysicsWorld* world, float dt)
{
    PhysicsStepSIMD(world, dt);
    //PhysicsStepScalar(world, dt);
}

void PhysicsStepScalar(PhysicsWorld* world, float dt)
{
    PhysicsIntegrate(&world->bodies, dt);
    PhysicsClearContacts(&world->contactBuffer);
    PhysicsDetectCollisions(&world->bodies, &world->contactBuffer);
    PhysicsResolveCollisions(&world->bodies, &world->contactBuffer);
    PhysicsApplyVelocityResponse(&world->bodies, &world->contactBuffer);
}

void PhysicsStepSIMD(PhysicsWorld* world, float dt)
{
    PhysicsIntegrateSIMD(&world->bodies, dt);
    PhysicsClearContacts(&world->contactBuffer);
    PhysicsDetectCollisionsSIMD(&world->bodies, &world->contactBuffer);
    PhysicsResolveCollisions(&world->bodies, &world->contactBuffer);
    PhysicsApplyVelocityResponse(&world->bodies, &world->contactBuffer);
}

void PhysicsStepGridSIMD(PhysicsWorld* world, float dt)
{
    PhysicsIntegrateSIMD(&world->bodies, dt);
    PhysicsClearContacts(&world->contactBuffer);
    PhysicsBuildGrid(&world->bodies, &world->grid);
    PhysicsDetectCollisionsGridSIMD(
        &world->bodies,
        &world->grid,
        &world->contactBuffer);
    PhysicsResolveCollisions(&world->bodies, &world->contactBuffer);
    PhysicsApplyVelocityResponse(&world->bodies, &world->contactBuffer);
}

void PhysicsIntegrate(
    PhysicsBodies* bodies,
    float dt)
{
    PhysicsIntegrateSIMD(bodies, dt);
}

void PhysicsIntegrateScalar(
    PhysicsBodies* bodies,
    float dt)
{
    for (uint32_t i = 0; i < bodies->count; i++)
    {
        bodies->posX[i] += bodies->velX[i] * dt;
        bodies->posY[i] += bodies->velY[i] * dt;
    }
}

void PhysicsIntegrateSIMD(
    PhysicsBodies* bodies,
    float dt)
{
#if PHYSICS_SIMD_SSE2
    uint32_t i = 0;
    uint32_t simdCount = bodies->count & ~3u;
    __m128 dt4 = _mm_set1_ps(dt);

    for (; i < simdCount; i += 4)
    {
        __m128 posX = _mm_loadu_ps(&bodies->posX[i]);
        __m128 posY = _mm_loadu_ps(&bodies->posY[i]);
        __m128 velX = _mm_loadu_ps(&bodies->velX[i]);
        __m128 velY = _mm_loadu_ps(&bodies->velY[i]);

        posX = _mm_add_ps(posX, _mm_mul_ps(velX, dt4));
        posY = _mm_add_ps(posY, _mm_mul_ps(velY, dt4));

        _mm_storeu_ps(&bodies->posX[i], posX);
        _mm_storeu_ps(&bodies->posY[i], posY);
    }

    for (; i < bodies->count; i++)
    {
        bodies->posX[i] += bodies->velX[i] * dt;
        bodies->posY[i] += bodies->velY[i] * dt;
    }
#else
    PhysicsIntegrateScalar(bodies, dt);
#endif
}

void PhysicsDetectCollisions(
    PhysicsBodies* bodies,
    PhysicsContactBuffer* contacts)
{
    PhysicsDetectCollisionsSIMD(bodies, contacts);
}

void PhysicsDetectCollisionsScalar(
    PhysicsBodies* bodies,
    PhysicsContactBuffer* contacts)
{
    for (uint32_t i = 0; i < bodies->count; i++)
    {
        for (uint32_t j = i + 1; j < bodies->count; j++)
        {
            contacts->pairCount++;

            if (PhysicsCircleVsCircle(bodies, i, j))
            {
                PhysicsGenerateContact(i, j, contacts);
            }
        }
    }
}

void PhysicsDetectCollisionsSIMD(
    PhysicsBodies* bodies,
    PhysicsContactBuffer* contacts)
{
#if PHYSICS_SIMD_SSE2
    for (uint32_t i = 0; i < bodies->count; i++)
    {
        uint32_t j = i + 1;

        __m128 posXi = _mm_set1_ps(bodies->posX[i]);
        __m128 posYi = _mm_set1_ps(bodies->posY[i]);
        __m128 radiusI = _mm_set1_ps(bodies->radius[i]);

        for (; j + 3 < bodies->count; j += 4)
        {
            contacts->pairCount += 4;

            __m128 posXj = _mm_loadu_ps(&bodies->posX[j]);
            __m128 posYj = _mm_loadu_ps(&bodies->posY[j]);
            __m128 radiusJ = _mm_loadu_ps(&bodies->radius[j]);

            __m128 dx = _mm_sub_ps(posXj, posXi);
            __m128 dy = _mm_sub_ps(posYj, posYi);
            __m128 radius = _mm_add_ps(radiusI, radiusJ);

            __m128 distSq =
                _mm_add_ps(
                    _mm_mul_ps(dx, dx),
                    _mm_mul_ps(dy, dy));

            __m128 radiusSq = _mm_mul_ps(radius, radius);
            __m128 cmp = _mm_cmplt_ps(distSq, radiusSq);
            int mask = _mm_movemask_ps(cmp);

            while (mask != 0)
            {
                int lane = 0;

                while (((mask >> lane) & 1) == 0)
                {
                    lane++;
                }

                PhysicsGenerateContact(i, j + (uint32_t)lane, contacts);
                mask &= ~(1 << lane);
            }
        }

        for (; j < bodies->count; j++)
        {
            contacts->pairCount++;

            if (PhysicsCircleVsCircleScalar(bodies, i, j))
            {
                PhysicsGenerateContact(i, j, contacts);
            }
        }
    }
#else
    PhysicsDetectCollisionsScalar(bodies, contacts);
#endif
}

void PhysicsDetectCollisionsGridSIMD(
    PhysicsBodies* bodies,
    PhysicsGrid* grid,
    PhysicsContactBuffer* contacts)
{
    for (uint32_t cellIndex = 0; cellIndex < PHYSICS_GRID_CELL_COUNT; cellIndex++)
    {
        PhysicsGridCell cell = grid->cells[cellIndex];

        if (cell.count == 0)
        {
            continue;
        }

        uint32_t cellX = cellIndex % PHYSICS_GRID_WIDTH;
        uint32_t cellY = cellIndex / PHYSICS_GRID_WIDTH;

        for (uint32_t i = 0; i < cell.count; i++)
        {
            uint32_t bodyIndex = grid->indices[cell.first + i];
            uint32_t sameCellStart = cell.first + i + 1;
            uint32_t sameCellCount = cell.count - i - 1;

            PhysicsTestBodyAgainstGridRangeSIMD(
                bodies,
                bodyIndex,
                grid->indices,
                sameCellStart,
                sameCellCount,
                contacts);
        }

        for (int offsetY = -1; offsetY <= 1; offsetY++)
        {
            int neighborY = (int)cellY + offsetY;

            if (neighborY < 0 || neighborY >= PHYSICS_GRID_HEIGHT)
            {
                continue;
            }

            for (int offsetX = -1; offsetX <= 1; offsetX++)
            {
                int neighborX = (int)cellX + offsetX;

                if (neighborX < 0 || neighborX >= PHYSICS_GRID_WIDTH)
                {
                    continue;
                }

                uint32_t neighborIndex =
                    (uint32_t)neighborY * PHYSICS_GRID_WIDTH +
                    (uint32_t)neighborX;

                if (neighborIndex <= cellIndex)
                {
                    continue;
                }

                PhysicsGridCell neighbor = grid->cells[neighborIndex];

                if (neighbor.count == 0)
                {
                    continue;
                }

                for (uint32_t i = 0; i < cell.count; i++)
                {
                    uint32_t bodyIndex = grid->indices[cell.first + i];

                    PhysicsTestBodyAgainstGridRangeSIMD(
                        bodies,
                        bodyIndex,
                        grid->indices,
                        neighbor.first,
                        neighbor.count,
                        contacts);
                }
            }
        }
    }
}

void PhysicsResolveCollisions(
    PhysicsBodies* bodies,
    PhysicsContactBuffer* contacts)
{
    for (uint32_t i = 0; i < contacts->count; i++)
    {
        PhysicsResolveContact(bodies, contacts->contacts[i]);
    }
}

void PhysicsApplyVelocityResponse(
    PhysicsBodies* bodies,
    PhysicsContactBuffer* contacts)
{
    for (uint32_t i = 0; i < contacts->count; i++)
    {
        PhysicsContact contact = contacts->contacts[i];

        PhysicsResolveImpulse(bodies, contact.a, contact.b);
    }
}

void PhysicsBuildBroadphase(
    PhysicsWorld* world)
{
    PhysicsBuildGrid(&world->bodies, &world->grid);
}

void PhysicsQueryBroadphase(
    PhysicsWorld* world,
    PhysicsContactBuffer* contacts)
{
    PhysicsDetectCollisionsGridSIMD(&world->bodies, &world->grid, contacts);
}

void PhysicsBuildGrid(
    PhysicsBodies* bodies,
    PhysicsGrid* grid)
{
    memset(grid->counts, 0, sizeof(grid->counts));

    float maxRadius = 1.0f;

    for (uint32_t i = 0; i < bodies->count; i++)
    {
        if (bodies->radius[i] > maxRadius)
        {
            maxRadius = bodies->radius[i];
        }
    }

    grid->invCellSize = 1.0f / (maxRadius * 2.0f);

    for (uint32_t i = 0; i < bodies->count; i++)
    {
        uint32_t cellIndex =
            PhysicsComputeGridCell(grid, bodies->posX[i], bodies->posY[i]);

        grid->counts[cellIndex]++;
    }

    uint32_t runningOffset = 0;

    for (uint32_t i = 0; i < PHYSICS_GRID_CELL_COUNT; i++)
    {
        grid->cells[i].first = runningOffset;
        grid->cells[i].count = grid->counts[i];
        grid->offsets[i] = runningOffset;
        runningOffset += grid->counts[i];
    }

    for (uint32_t i = 0; i < bodies->count; i++)
    {
        uint32_t cellIndex =
            PhysicsComputeGridCell(grid, bodies->posX[i], bodies->posY[i]);

        uint32_t writeIndex = grid->offsets[cellIndex]++;

        if (writeIndex < PHYSICS_MAX_BODIES)
        {
            grid->indices[writeIndex] = i;
        }
    }
}

bool PhysicsCircleVsCircle(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b)
{
    return PhysicsCircleVsCircleScalar(bodies, a, b);
}

bool PhysicsCircleVsCircleScalar(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b)
{
    float radius =
        bodies->radius[a] +
        bodies->radius[b];

    return PhysicsDistanceSquared(bodies, a, b) < radius * radius;
}

float PhysicsDistanceSquared(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b)
{
    float dx =
        bodies->posX[b] -
        bodies->posX[a];

    float dy =
        bodies->posY[b] -
        bodies->posY[a];

    return dx * dx + dy * dy;
}

void PhysicsGenerateContact(
    uint32_t a,
    uint32_t b,
    PhysicsContactBuffer* contacts)
{
    PhysicsPushContact(contacts, a, b);
}

void PhysicsClearContacts(
    PhysicsContactBuffer* contacts)
{
    contacts->count = 0;
    contacts->pairCount = 0;
}

void PhysicsPushContact(
    PhysicsContactBuffer* contacts,
    uint32_t a,
    uint32_t b)
{
    if (contacts->count >= contacts->capacity)
    {
        return;
    }

    contacts->contacts[contacts->count] =
        (PhysicsContact) {
            .a = a,
            .b = b
        };

    contacts->count++;
}

void PhysicsResolveContact(
    PhysicsBodies* bodies,
    PhysicsContact contact)
{
    PhysicsSeparateBodies(bodies, contact.a, contact.b);
}

void PhysicsSeparateBodies(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b)
{
    float dx =
        bodies->posX[b] -
        bodies->posX[a];

    float dy =
        bodies->posY[b] -
        bodies->posY[a];

    float distSq =
        dx * dx +
        dy * dy;

    float radius =
        bodies->radius[a] +
        bodies->radius[b];

    if (distSq >= radius * radius)
    {
        return;
    }

    float distance = sqrtf(distSq);
    float nx = 1.0f;
    float ny = 0.0f;

    if (distance > 0.000001f)
    {
        nx = dx / distance;
        ny = dy / distance;
    }

    float penetration =
        radius -
        distance;

    float correction =
        penetration * 0.5f;

    bodies->posX[a] -= nx * correction;
    bodies->posY[a] -= ny * correction;

    bodies->posX[b] += nx * correction;
    bodies->posY[b] += ny * correction;
}

void PhysicsResolveImpulse(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b)
{
    float dx =
        bodies->posX[b] -
        bodies->posX[a];

    float dy =
        bodies->posY[b] -
        bodies->posY[a];

    float distSq =
        dx * dx +
        dy * dy;

    float nx = 1.0f;
    float ny = 0.0f;

    if (distSq > 0.000001f)
    {
        float invDistance = 1.0f / sqrtf(distSq);

        nx = dx * invDistance;
        ny = dy * invDistance;
    }

    float rvx =
        bodies->velX[b] -
        bodies->velX[a];

    float rvy =
        bodies->velY[b] -
        bodies->velY[a];

    float velocityAlongNormal =
        rvx * nx +
        rvy * ny;

    if (velocityAlongNormal > 0.0f)
    {
        return;
    }

    float massA = bodies->mass[a];
    float massB = bodies->mass[b];

    float invMassA =
        massA > 0.0f ? 1.0f / massA : 0.0f;

    float invMassB =
        massB > 0.0f ? 1.0f / massB : 0.0f;

    float invMassSum =
        invMassA +
        invMassB;

    if (invMassSum <= 0.0f)
    {
        return;
    }

    const float restitution = 0.8f;

    float impulseMagnitude =
        -(1.0f + restitution) *
        velocityAlongNormal /
        invMassSum;

    float impulseX =
        impulseMagnitude *
        nx;

    float impulseY =
        impulseMagnitude *
        ny;

    bodies->velX[a] -= impulseX * invMassA;
    bodies->velY[a] -= impulseY * invMassA;

    bodies->velX[b] += impulseX * invMassB;
    bodies->velY[b] += impulseY * invMassB;
}

void PhysicsApplyBounce(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b)
{
    PhysicsResolveImpulse(bodies, a, b);
}

void PhysicsInitializeWorld(
    PhysicsWorld* world,
    PhysicsBodies bodies,
    PhysicsContactBuffer contacts)
{
    world->bodies = bodies;
    world->contactBuffer = contacts;
}

void PhysicsShutdownWorld(
    PhysicsWorld* world)
{
    world->bodies = (PhysicsBodies) { 0 };
    world->contactBuffer = (PhysicsContactBuffer) { 0 };
}
