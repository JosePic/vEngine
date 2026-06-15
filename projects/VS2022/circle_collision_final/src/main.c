#include "physics.h"
#include "raylib.h"

#define START_BODY_COUNT 100

static float posX[PHYSICS_MAX_BODIES];
static float posY[PHYSICS_MAX_BODIES];
static float velX[PHYSICS_MAX_BODIES];
static float velY[PHYSICS_MAX_BODIES];
static float radius[PHYSICS_MAX_BODIES];
static float mass[PHYSICS_MAX_BODIES];
static bool colliding[PHYSICS_MAX_BODIES];
static PhysicsContact contacts[PHYSICS_MAX_CONTACTS];

typedef struct {
    double integrateMs;
    double buildMs;
    double detectMs;
    double resolveMs;
    double velocityMs;
    double totalMs;
} PhysicsProfile;

typedef enum {
    PhysicsRunModeScalar,
    PhysicsRunModeSIMD,
    PhysicsRunModeGridSIMD
} PhysicsRunMode;

static const char* GetRunModeName(PhysicsRunMode mode)
{
    switch (mode)
    {
        case PhysicsRunModeScalar: return "Scalar";
        case PhysicsRunModeSIMD: return "SIMD";
        case PhysicsRunModeGridSIMD: return "Grid SIMD";
        default: return "Unknown";
    }
}

static void ResetBodies(int screenWidth, int screenHeight)
{
    for (uint32_t i = 0; i < PHYSICS_MAX_BODIES; i++)
    {
        posX[i] = (float)GetRandomValue(0, screenWidth);
        posY[i] = (float)GetRandomValue(0, screenHeight);

        velX[i] = (float)GetRandomValue(-100, 100);
        velY[i] = (float)GetRandomValue(-100, 100);

        radius[i] = 4.0f + (float)GetRandomValue(0, 8);
        mass[i] = radius[i];
        colliding[i] = false;
    }
}

static void SetBodyCount(PhysicsWorld* world, uint32_t count)
{
    if (count > PHYSICS_MAX_BODIES)
    {
        count = PHYSICS_MAX_BODIES;
    }

    world->bodies.count = count;
}

static void ApplyWallBounce(PhysicsBodies* bodies, int screenWidth, int screenHeight)
{
    for (uint32_t i = 0; i < bodies->count; i++)
    {
        float minX = bodies->radius[i];
        float maxX = (float)screenWidth - bodies->radius[i];
        float minY = bodies->radius[i];
        float maxY = (float)screenHeight - bodies->radius[i];

        if (bodies->posX[i] < minX)
        {
            bodies->posX[i] = minX;
            bodies->velX[i] *= -1.0f;
        }

        if (bodies->posX[i] > maxX)
        {
            bodies->posX[i] = maxX;
            bodies->velX[i] *= -1.0f;
        }

        if (bodies->posY[i] < minY)
        {
            bodies->posY[i] = minY;
            bodies->velY[i] *= -1.0f;
        }

        if (bodies->posY[i] > maxY)
        {
            bodies->posY[i] = maxY;
            bodies->velY[i] *= -1.0f;
        }
    }
}

static void MarkCollidingBodies(PhysicsWorld* world)
{
    for (uint32_t i = 0; i < world->bodies.count; i++)
    {
        colliding[i] = false;
    }

    for (uint32_t i = 0; i < world->contactBuffer.count; i++)
    {
        PhysicsContact contact = world->contactBuffer.contacts[i];

        colliding[contact.a] = true;
        colliding[contact.b] = true;
    }
}

static void StepPhysicsProfiled(
    PhysicsWorld* world,
    float dt,
    PhysicsRunMode mode,
    PhysicsProfile* profile)
{
    double start = GetTime();
    double t0 = start;

    if (mode == PhysicsRunModeScalar)
    {
        PhysicsIntegrateScalar(&world->bodies, dt);
    }
    else
    {
        PhysicsIntegrateSIMD(&world->bodies, dt);
    }

    double t1 = GetTime();

    PhysicsClearContacts(&world->contactBuffer);

    if (mode == PhysicsRunModeGridSIMD)
    {
        PhysicsBuildGrid(&world->bodies, &world->grid);
    }

    double t2 = GetTime();

    if (mode == PhysicsRunModeGridSIMD)
    {
        PhysicsDetectCollisionsGridSIMD(
            &world->bodies,
            &world->grid,
            &world->contactBuffer);
    }
    else if (mode == PhysicsRunModeSIMD)
    {
        PhysicsDetectCollisionsSIMD(&world->bodies, &world->contactBuffer);
    }
    else
    {
        PhysicsDetectCollisionsScalar(&world->bodies, &world->contactBuffer);
    }

    double t3 = GetTime();

    PhysicsResolveCollisions(&world->bodies, &world->contactBuffer);

    double t4 = GetTime();

    PhysicsApplyVelocityResponse(&world->bodies, &world->contactBuffer);

    double t5 = GetTime();

    profile->integrateMs = (t1 - t0) * 1000.0;
    profile->buildMs = (t2 - t1) * 1000.0;
    profile->detectMs = (t3 - t2) * 1000.0;
    profile->resolveMs = (t4 - t3) * 1000.0;
    profile->velocityMs = (t5 - t4) * 1000.0;
    profile->totalMs = (t5 - start) * 1000.0;
}

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "Physics Stress Test");
    SetTargetFPS(0);

    ResetBodies(screenWidth, screenHeight);

    PhysicsBodies bodies = {
        .posX = posX,
        .posY = posY,
        .velX = velX,
        .velY = velY,
        .radius = radius,
        .mass = mass,
        .count = START_BODY_COUNT
    };

    PhysicsContactBuffer buffer = {
        .contacts = contacts,
        .count = 0,
        .capacity = PHYSICS_MAX_CONTACTS,
        .pairCount = 0
    };

    PhysicsWorld world;

    PhysicsInitializeWorld(&world, bodies, buffer);

    PhysicsRunMode runMode = PhysicsRunModeGridSIMD;
    bool paused = true;
    PhysicsProfile profile = { 0 };

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_SPACE))
        {
            runMode = (PhysicsRunMode)(((int)runMode + 1) % 3);
        }

        if (IsKeyPressed(KEY_P))
        {
            paused = !paused;
        }

        if (IsKeyPressed(KEY_R))
        {
            ResetBodies(screenWidth, screenHeight);
            PhysicsClearContacts(&world.contactBuffer);
            MarkCollidingBodies(&world);
        }

        if (IsKeyPressed(KEY_ONE))
        {
            SetBodyCount(&world, 500);
        }

        if (IsKeyPressed(KEY_TWO))
        {
            SetBodyCount(&world, 1000);
        }

        if (IsKeyPressed(KEY_THREE))
        {
            SetBodyCount(&world, 2000);
        }

        if (IsKeyPressed(KEY_FOUR))
        {
            SetBodyCount(&world, 5000);
        }

        if (!paused)
        {
            float dt = GetFrameTime();

            if (dt > 1.0f / 30.0f)
            {
                dt = 1.0f / 30.0f;
            }

            StepPhysicsProfiled(&world, dt, runMode, &profile);

            ApplyWallBounce(&world.bodies, screenWidth, screenHeight);
            MarkCollidingBodies(&world);
        }

        BeginDrawing();

        ClearBackground(RAYWHITE);

        for (uint32_t i = 0; i < world.bodies.count; i++)
        {
            DrawCircleV(
                (Vector2) {
                    world.bodies.posX[i],
                    world.bodies.posY[i]
                },
                world.bodies.radius[i],
                colliding[i] ? RED : DARKGRAY);
        }

        DrawText(
            TextFormat("Mode: %s", GetRunModeName(runMode)),
            10,
            10,
            20,
            BLACK);

        DrawText(
            TextFormat("State: %s", paused ? "Paused" : "Running"),
            10,
            35,
            20,
            BLACK);

        DrawText(
            TextFormat("Bodies: %u", world.bodies.count),
            10,
            60,
            20,
            BLACK);

        DrawText(
            TextFormat("Pairs: %llu",
                (unsigned long long)world.contactBuffer.pairCount),
            10,
            85,
            20,
            BLACK);

        DrawText(
            TextFormat("Contacts: %u", world.contactBuffer.count),
            10,
            110,
            20,
            BLACK);

        DrawText(
            TextFormat("Integrate: %.3f ms", profile.integrateMs),
            10,
            135,
            20,
            BLACK);

        DrawText(
            TextFormat("BuildGrid: %.3f ms", profile.buildMs),
            10,
            160,
            20,
            BLACK);

        DrawText(
            TextFormat("Detect: %.3f ms", profile.detectMs),
            10,
            185,
            20,
            BLACK);

        DrawText(
            TextFormat("Resolve: %.3f ms", profile.resolveMs),
            10,
            210,
            20,
            BLACK);

        DrawText(
            TextFormat("Velocity: %.3f ms", profile.velocityMs),
            10,
            235,
            20,
            BLACK);

        DrawText(
            TextFormat("Physics: %.3f ms", profile.totalMs),
            10,
            260,
            20,
            BLACK);

        DrawText(
            "P run, SPACE mode, R reset, 1-4 bodies",
            10,
            285,
            20,
            BLACK);

        DrawFPS(10, 310);

        EndDrawing();
    }

    PhysicsShutdownWorld(&world);
    CloseWindow();

    return 0;
}
