#pragma once
#include <stdint.h>
#include <stdbool.h>

#define VINSPECT_MAX_TX           1024
#define VINSPECT_MAX_MUTATIONS    2048
#define VINSPECT_MAX_QUERY_EVENTS 512
#define VINSPECT_MAX_SYSTEMS      32
#define VINSPECT_RING_BUFFER      300
#define VINSPECT_MAX_ENTITIES     1024 
#define VINSPECT_PAYLOAD_INLINE_SIZE 64 

// Stable reference across ring buffer frames
typedef struct {
    uint16_t ringIndex;
    uint16_t recordIndex;
    uint32_t frameNumber; // Generation validation
} VRecordRef;

// --- System Profiling Markers ---
typedef struct {
    uint16_t systemId;
    uint16_t firstTransaction;
    uint16_t transactionCount;
    uint16_t firstMutation;
    uint16_t mutationCount;
} VSystemMarker;

// --- Transaction Node (The Suspect) ---
typedef struct {
    uint64_t id;
    uint32_t frameNumber;
    uint16_t sourceSystem;
    uint16_t intentType;
    int32_t sourceEntity;
    int32_t targetEntity;

    uint16_t payloadSize;
    uint8_t payload[VINSPECT_PAYLOAD_INLINE_SIZE];
} VTransactionNode;

// --- Mutation Ledger (The Surveillance Footage) ---
typedef struct {
    uint64_t transactionId;
    uint16_t reasonId;
    int32_t entityId;
    uint16_t componentId;
    uint16_t fieldId;

    // Intrusive backward linkage for instantaneous entity history
    VRecordRef prevMutationForEntity;

    // Pure primitive storage (floats, ints, bools, handles)
    union { uint64_t u64; int64_t i64; double f64; float f32[2]; uint32_t u32[2]; } oldVal;
    union { uint64_t u64; int64_t i64; double f64; float f32[2]; uint32_t u32[2]; } newVal;
} VMutationRecord;

// --- Query Ledger (The Ghost Tracker) ---
typedef struct {
    uint16_t queryId;
    int32_t entityId;
    bool entered;
    uint64_t causativeTxId;
    VRecordRef prevQueryEventForEntity;
} VQueryEvent;

// --- Frame Container ---
typedef struct {
    uint32_t frameNumber;
    bool isPopulated;

    VSystemMarker systems[VINSPECT_MAX_SYSTEMS];
    uint32_t systemCount;

    VTransactionNode transactions[VINSPECT_MAX_TX];
    uint32_t transactionCount;

    VMutationRecord mutations[VINSPECT_MAX_MUTATIONS];
    uint32_t mutationCount;

    VQueryEvent queryEvents[VINSPECT_MAX_QUERY_EVENTS];
    uint32_t queryEventCount;
} VFrameCapture;

// --- Global Forensic Database ---
typedef struct {
    VFrameCapture ringBuffer[VINSPECT_RING_BUFFER];
    uint32_t totalFramesCaptured;
    uint32_t headIdx;

    // O(1) Transaction Hash Map
    struct {
        uint64_t txId;
        VRecordRef txRef;
    } txLookupMap[8192];

    // O(1) List Heads for Entity Histories
    VRecordRef entityLatestMutation[VINSPECT_MAX_ENTITIES];
    VRecordRef entityLatestQueryEvent[VINSPECT_MAX_ENTITIES];
} VInspectorDB;

// --- API ---
void VInspect_Init(VInspectorDB* db);
void VInspect_BeginFrame(VInspectorDB* db, uint32_t frameNumber);
void VInspect_BeginSystem(VInspectorDB* db, uint16_t systemId);
void VInspect_EndSystem(VInspectorDB* db);

void VInspect_LogTransaction(VInspectorDB* db, const VTransactionNode* node);
void VInspect_LogMutation(VInspectorDB* db, const VMutationRecord* record);
void VInspect_LogQueryEvent(VInspectorDB* db, uint16_t queryId, int32_t entityId, bool entered, uint64_t txId);

const VTransactionNode* VInspect_FindTransaction(const VInspectorDB* db, uint64_t txId);