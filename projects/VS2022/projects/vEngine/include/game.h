#pragma once
#include "vengine.h"
#include "sim.h"

int spawnPlayer();
void GatherInput(EntityPool *pool, Camera camera, int playerIdx,
                        bool selected[MAX_ENTITIES], FrameState *fs);
void UpdateSpawning(EntityPool *pool, int playerIdx, float dt,
                           FrameState *fs);
void InitUnitStats(EntityPool *pool, int idx, UnitArchetype arch, Faction faction);
void initTestScene(int playerIdx);
    
