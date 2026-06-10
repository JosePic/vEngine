// physics.h

#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdint.h>
#include <stdbool.h>

#define PHYSICS_MAX_BODIES 5000
#define PHYSICS_MAX_CONTACTS 500000

#define PHYSICS_GRID_WIDTH 64
#define PHYSICS_GRID_HEIGHT 64
#define PHYSICS_GRID_CELL_COUNT (PHYSICS_GRID_WIDTH * PHYSICS_GRID_HEIGHT)

typedef struct {
    uint32_t a;
    uint32_t b;
} PhysicsContact;

typedef struct {
    float* posX;
    float* posY;

    float* velX;
    float* velY;

    float* radius;

    float* mass;

    uint32_t count;
} PhysicsBodies;

typedef struct {
    PhysicsContact* contacts;
    uint32_t count;
    uint32_t capacity;
    uint64_t pairCount;
} PhysicsContactBuffer;

typedef struct {
    uint32_t count;
    uint32_t first;
} PhysicsGridCell;

typedef struct {
    PhysicsGridCell cells[PHYSICS_GRID_CELL_COUNT];
    uint32_t counts[PHYSICS_GRID_CELL_COUNT];
    uint32_t offsets[PHYSICS_GRID_CELL_COUNT];
    uint32_t indices[PHYSICS_MAX_BODIES];
    float invCellSize;
} PhysicsGrid;

typedef struct {
    PhysicsBodies bodies;
    PhysicsContactBuffer contactBuffer;
    PhysicsGrid grid;
} PhysicsWorld;

// ======================================================
// Main API
// ======================================================

void PhysicsStep(PhysicsWorld* world, float dt);

void PhysicsStepScalar(PhysicsWorld* world, float dt);

void PhysicsStepSIMD(PhysicsWorld* world, float dt);

void PhysicsStepGridSIMD(PhysicsWorld* world, float dt);

// ======================================================
// Pipeline Stages
// ======================================================

void PhysicsIntegrate(
    PhysicsBodies* bodies,
    float dt);

void PhysicsIntegrateScalar(
    PhysicsBodies* bodies,
    float dt);

void PhysicsIntegrateSIMD(
    PhysicsBodies* bodies,
    float dt);

void PhysicsDetectCollisions(
    PhysicsBodies* bodies,
    PhysicsContactBuffer* contacts);

void PhysicsDetectCollisionsScalar(
    PhysicsBodies* bodies,
    PhysicsContactBuffer* contacts);

void PhysicsDetectCollisionsSIMD(
    PhysicsBodies* bodies,
    PhysicsContactBuffer* contacts);

void PhysicsDetectCollisionsGridSIMD(
    PhysicsBodies* bodies,
    PhysicsGrid* grid,
    PhysicsContactBuffer* contacts);

void PhysicsResolveCollisions(
    PhysicsBodies* bodies,
    PhysicsContactBuffer* contacts);

void PhysicsApplyVelocityResponse(
    PhysicsBodies* bodies,
    PhysicsContactBuffer* contacts);

// ======================================================
// Broadphase
// ======================================================

void PhysicsBuildBroadphase(
    PhysicsWorld* world);

void PhysicsQueryBroadphase(
    PhysicsWorld* world,
    PhysicsContactBuffer* contacts);

void PhysicsBuildGrid(
    PhysicsBodies* bodies,
    PhysicsGrid* grid);

// ======================================================
// Narrowphase
// ======================================================

bool PhysicsCircleVsCircle(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b);

bool PhysicsCircleVsCircleScalar(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b);

float PhysicsDistanceSquared(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b);

// ======================================================
// Contact Generation
// ======================================================

void PhysicsGenerateContact(
    uint32_t a,
    uint32_t b,
    PhysicsContactBuffer* contacts);

void PhysicsClearContacts(
    PhysicsContactBuffer* contacts);

void PhysicsPushContact(
    PhysicsContactBuffer* contacts,
    uint32_t a,
    uint32_t b);

// ======================================================
// Position Resolution
// ======================================================

void PhysicsResolveContact(
    PhysicsBodies* bodies,
    PhysicsContact contact);

void PhysicsSeparateBodies(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b);

// ======================================================
// Velocity Resolution
// ======================================================

void PhysicsResolveImpulse(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b);

void PhysicsApplyBounce(
    PhysicsBodies* bodies,
    uint32_t a,
    uint32_t b);

// ======================================================
// Utility
// ======================================================

void PhysicsInitializeWorld(
    PhysicsWorld* world,
    PhysicsBodies bodies,
    PhysicsContactBuffer contacts);

void PhysicsShutdownWorld(
    PhysicsWorld* world);

#endif
