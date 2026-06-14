#pragma once


#include "physics.h"
#include "query.h"
#include "vengine.h"

// ---------------------------------------------------------------------------
// Global physics world — backed by static arrays in physics_system.c
// ---------------------------------------------------------------------------
extern PhysicsWorld gPhysicsWorld;

// ---------------------------------------------------------------------------
// Lifecycle — call once at startup / shutdown
// ---------------------------------------------------------------------------
void PhysicsSystem_Init(void);
void PhysicsSystem_Shutdown(void);

// ---------------------------------------------------------------------------
// Per-frame bridge
// ---------------------------------------------------------------------------

// Pack the pre-filtered entity list (from a Query) into the dense SoA
// arrays expected by the physics module.  Rebuilds pool->physToEntity and
// pool->entityToPhys.  The query must include COMP_PHYSICS so only physics
// entities are passed in.
void PhysicsSystem_SyncIn(EntityPool *pool, const int *entities, int count);

// Write physics results (posX/Y, velX/Y) back into pool->positions and
// pool->velocities (X and Z only).  Sets dirty[e] = true for each entity.
void PhysicsSystem_WriteBack(EntityPool *pool);

// Bounce all packed bodies at an axis-aligned 2-D boundary.
// Call after the physics step but before WriteBack, or let RunQuery handle it.
void PhysicsSystem_ApplyBounds(EntityPool *pool, float minX, float maxX,
                               float minZ, float maxZ);

void PhysicsSystem_ApplyGroundLock(EntityPool *pool);

// ---------------------------------------------------------------------------
// ECS entry point
//
// Matches the SystemFn signature used by RegisterSystem / RunSystems:
//   void fn(EntityPool*, const Query*, float dt, void *user)
//
// Plug it into PHASE_MOVEMENT in EnsureSystems():
//   RegisterSystem(systems, count, max, "Movement", &fs->qPhysics,
//                  PhysicsSystem_ECSRun, PHASE_MOVEMENT);
//
// Internal order: SyncIn → Integrate → BuildGrid → ClearContacts →
//   DetectCollisions → ResolvePositions → ApplyVelocityResponse →
//   ApplyBounds → WriteBack
// ---------------------------------------------------------------------------
void PhysicsSystem_ECSRun(EntityPool *pool, const Query *q, float dt,
                          void *user);

// ---------------------------------------------------------------------------
// Spawn helper — call once per entity right after SpawnEntity()
//
//   radius  pass 0 to auto-derive from min(size.x, size.z) * 0.5
//   mass    1.0f is a sensible default for unit-sized bodies
//
// Adds COMP_PHYSICS to the entity's mask automatically.
// ---------------------------------------------------------------------------
void PhysicsSystem_SetBody(EntityPool *pool, int entity, float radius,
                           float mass);
