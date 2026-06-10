#ifndef ENGINE_TASK_SCHEDULER_H
#define ENGINE_TASK_SCHEDULER_H

#include <stdint.h>

// Function pointer signature for an executable task
typedef void (*TaskCallback)(void* arg);

typedef struct {
    TaskCallback callback;
    void* arg;
} EngineTask;

// Core Management API
void InitEngineThreadPool(uint32_t threadCount);
void PushEngineTask(TaskCallback callback, void* arg);
void WaitTaskGroup(void);
void ShutdownEngineThreadPool(void);

uint32_t GetEngineWorkerCount(void);

#endif // ENGINE_TASK_SCHEDULER_H