#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "engine_task_scheduler.h"
#include <stdlib.h>
#include <stdbool.h>

#define MAX_TASKS 256

static HANDLE* g_threads = NULL;
static uint32_t g_threadCount = 0;
static bool g_shutdown = false;

// Ring buffer task queue
static EngineTask g_taskQueue[MAX_TASKS];
static uint32_t g_queueHead = 0;
static uint32_t g_queueTail = 0;
static uint32_t g_tasksPending = 0; // Tasks currently in queue
static uint32_t g_tasksActive = 0;  // Tasks currently being run by workers

static CRITICAL_SECTION g_queueLock;
static CONDITION_VARIABLE g_queueReady;
static CONDITION_VARIABLE g_tasksDone;

static DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
    (void)lpParam;
    while (true) {
        EngineTask task = { NULL, NULL };

        EnterCriticalSection(&g_queueLock);

        while (g_queueHead == g_queueTail && !g_shutdown) {
            SleepConditionVariableCS(&g_queueReady, &g_queueLock, INFINITE);
        }

        if (g_shutdown && g_queueHead == g_queueTail) {
            LeaveCriticalSection(&g_queueLock);
            break;
        }

        // Dequeue task
        task = g_taskQueue[g_queueTail];
        g_queueTail = (g_queueTail + 1) % MAX_TASKS;
        g_tasksPending--;
        g_tasksActive++;

        LeaveCriticalSection(&g_queueLock);

        // Execute job without holding the queue lock
        if (task.callback) {
            task.callback(task.arg);
        }

        EnterCriticalSection(&g_queueLock);
        g_tasksActive--;
        if (g_tasksPending == 0 && g_tasksActive == 0) {
            WakeAllConditionVariable(&g_tasksDone);
        }
        LeaveCriticalSection(&g_queueLock);
    }
    return 0;
}

void InitEngineThreadPool(uint32_t threadCount) {
    if (g_threads != NULL) return;

    g_threadCount = threadCount;
    g_threads = (HANDLE*)malloc(sizeof(HANDLE) * threadCount);
    g_shutdown = false;

    InitializeCriticalSection(&g_queueLock);
    InitializeConditionVariable(&g_queueReady);
    InitializeConditionVariable(&g_tasksDone);

    for (uint32_t i = 0; i < threadCount; i++) {
        g_threads[i] = CreateThread(NULL, 0, WorkerThreadProc, NULL, 0, NULL);
    }
}

void PushEngineTask(TaskCallback callback, void* arg) {
    EnterCriticalSection(&g_queueLock);

    uint32_t nextHead = (g_queueHead + 1) % MAX_TASKS;
    if (nextHead == g_queueTail) {
        // Queue full! In a real system, expand buffer or assert. For now, block/drop.
        LeaveCriticalSection(&g_queueLock);
        return;
    }

    g_taskQueue[g_queueHead] = (EngineTask){ callback, arg };
    g_queueHead = nextHead;
    g_tasksPending++;

    WakeConditionVariable(&g_queueReady);
    LeaveCriticalSection(&g_queueLock);
}

void WaitTaskGroup(void) {
    EnterCriticalSection(&g_queueLock);
    while (g_tasksPending > 0 || g_tasksActive > 0) {
        SleepConditionVariableCS(&g_tasksDone, &g_queueLock, INFINITE);
    }
    LeaveCriticalSection(&g_queueLock);
}

void ShutdownEngineThreadPool(void) {
    if (!g_threads) return;

    EnterCriticalSection(&g_queueLock);
    g_shutdown = true;
    WakeAllConditionVariable(&g_queueReady);
    LeaveCriticalSection(&g_queueLock);

    WaitForMultipleObjects(g_threadCount, g_threads, TRUE, INFINITE);

    for (uint32_t i = 0; i < g_threadCount; i++) {
        CloseHandle(g_threads[i]);
    }
    free(g_threads);
    g_threads = NULL;

    DeleteCriticalSection(&g_queueLock);
}

uint32_t GetEngineWorkerCount(void) {
    return g_threadCount;
}