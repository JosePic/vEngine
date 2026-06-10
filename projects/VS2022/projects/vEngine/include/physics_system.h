#pragma once
// ---------------------------------------------------------------------------
// Physics System Overview
//
// This module bridges the low-level 2D physics engine (physics.h) with the
// 3D game's entity system. It handles:
//
//   1. COORDINATE MAPPING:
//      The game is 3D isometric; the physics engine is 2D (for performance).
//      Mapping convention:
//        Game X-axis      ↔  Physics X-axis
//        Game Z-axis (depth) ↔  Physics Y-axis
//        Game Y-axis (height) is untouched (managed by ApplyGroundLock)
//
//   2. SPARSE-TO-DENSE CONVERSION:
//      EntityPool uses sparse storage (entities may have gaps).
//      Physics uses dense SoA arrays for SIMD performance.
//      PhysicsSystem_SyncIn() packs active physics entities.
//      PhysicsSystem_WriteBack() unpacks results.
//
//   3. ENTITY LIFECYCLE:
//      Only entities with COMP_PHYSICS flag participate in physics.
//      Static walls/terrain must NOT have COMP_PHYSICS.
//      This keeps the physics budget low.
//
// Pipeline each frame:
//   1. BuildQueries() → identify entities with COMP_PHYSICS
//   2. PhysicsSystem_ECSRun() called in PHASE_MOVEMENT:
//      a. PhysicsSystem_UpdateMovementIntent() (velocity from AI/player)
//      b. PhysicsSystem_SyncIn() (pack into dense arrays)
//      c. PhysicsIntegrateSIMD() (update positions)
//      d. PhysicsDetectCollisionsGridSIMD() (grid-based broadphase)
//      e. PhysicsResolveCollisions() (resolve overlaps)
//      f. PhysicsApplyVelocityResponse() (impulse resolution)
//      g. PhysicsSystem_ApplyBounds() (world bounds)
//      h. PhysicsSystem_ApplyGroundLock() (maintain ground plane)
//      i. PhysicsSystem_WriteBack() (unpack back to pool)
//
// ---------------------------------------------------------------------------

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
