#include "vinspector.h"
#include <string.h>

static uint32_t HashTxId(uint64_t txId) {
    uint32_t hash = 2166136261u;
    hash ^= (uint32_t)(txId & 0xFFFFFFFF); hash *= 16777619u;
    hash ^= (uint32_t)(txId >> 32);         hash *= 16777619u;
    return hash & (8192 - 1);
}

void VInspect_Init(VInspectorDB* db) {
    memset(db, 0, sizeof(*db));
}

void VInspect_BeginFrame(VInspectorDB* db, uint32_t frameNumber) {
    uint32_t idx = db->totalFramesCaptured % VINSPECT_RING_BUFFER;
    VFrameCapture* frame = &db->ringBuffer[idx];

    // Zombie Cleanup: Wipe old transactions from the hash map before reuse
    if (frame->isPopulated) {
        for (uint32_t i = 0; i < frame->transactionCount; i++) {
            uint64_t deadTxId = frame->transactions[i].id;
            uint32_t mapIdx = HashTxId(deadTxId);

            for (int k = 0; k < 8192; k++) {
                if (db->txLookupMap[mapIdx].txId == deadTxId) {
                    db->txLookupMap[mapIdx].txId = 0;
                    break;
                }
                mapIdx = (mapIdx + 1) & (8192 - 1);
            }
        }
    }

    frame->frameNumber = frameNumber;
    frame->isPopulated = true;
    frame->systemCount = 0;
    frame->transactionCount = 0;
    frame->mutationCount = 0;
    frame->queryEventCount = 0;

    db->headIdx = idx;
    db->totalFramesCaptured++;
}

void VInspect_BeginSystem(VInspectorDB* db, uint16_t systemId) {
    VFrameCapture* frame = &db->ringBuffer[db->headIdx];
    if (frame->systemCount >= VINSPECT_MAX_SYSTEMS) return;

    VSystemMarker* marker = &frame->systems[frame->systemCount++];
    marker->systemId = systemId;
    marker->firstTransaction = frame->transactionCount;
    marker->transactionCount = 0;
    marker->firstMutation = frame->mutationCount;
    marker->mutationCount = 0;
}

void VInspect_EndSystem(VInspectorDB* db) {
    VFrameCapture* frame = &db->ringBuffer[db->headIdx];
    if (frame->systemCount == 0) return;

    VSystemMarker* marker = &frame->systems[frame->systemCount - 1];
    marker->transactionCount = frame->transactionCount - marker->firstTransaction;
    marker->mutationCount = frame->mutationCount - marker->firstMutation;
}

void VInspect_LogTransaction(VInspectorDB* db, const VTransactionNode* node) {
    VFrameCapture* frame = &db->ringBuffer[db->headIdx];
    if (frame->transactionCount >= VINSPECT_MAX_TX) return;

    uint16_t recIdx = frame->transactionCount++;
    VTransactionNode* dest = &frame->transactions[recIdx];
    *dest = *node;
    dest->frameNumber = frame->frameNumber;

    uint32_t mapIdx = HashTxId(node->id);
    for (int i = 0; i < 8192; i++) {
        if (db->txLookupMap[mapIdx].txId == 0) {
            db->txLookupMap[mapIdx].txId = node->id;
            db->txLookupMap[mapIdx].txRef = (VRecordRef){
                .ringIndex = (uint16_t)db->headIdx,
                .recordIndex = recIdx,
                .frameNumber = frame->frameNumber
            };
            break;
        }
        mapIdx = (mapIdx + 1) & (8192 - 1);
    }
}

void VInspect_LogMutation(VInspectorDB* db, const VMutationRecord* record) {
    VFrameCapture* frame = &db->ringBuffer[db->headIdx];
    if (frame->mutationCount >= VINSPECT_MAX_MUTATIONS) return;
    if (record->entityId < 0 || record->entityId >= VINSPECT_MAX_ENTITIES) return;

    uint16_t recIdx = frame->mutationCount++;
    VMutationRecord* dest = &frame->mutations[recIdx];
    *dest = *record;

    // Intrusive Link: Grab old head, become new head
    dest->prevMutationForEntity = db->entityLatestMutation[record->entityId];
    db->entityLatestMutation[record->entityId] = (VRecordRef){
        .ringIndex = (uint16_t)db->headIdx,
        .recordIndex = recIdx,
        .frameNumber = frame->frameNumber
    };
}

void VInspect_LogQueryEvent(VInspectorDB* db, uint16_t queryId, int32_t entityId, bool entered, uint64_t txId) {
    VFrameCapture* frame = &db->ringBuffer[db->headIdx];
    if (frame->queryEventCount >= VINSPECT_MAX_QUERY_EVENTS) return;
    if (entityId < 0 || entityId >= VINSPECT_MAX_ENTITIES) return;

    uint16_t recIdx = frame->queryEventCount++;
    VQueryEvent* dest = &frame->queryEvents[recIdx];

    dest->queryId = queryId;
    dest->entityId = entityId;
    dest->entered = entered;
    dest->causativeTxId = txId;

    dest->prevQueryEventForEntity = db->entityLatestQueryEvent[entityId];
    db->entityLatestQueryEvent[entityId] = (VRecordRef){
        .ringIndex = (uint16_t)db->headIdx,
        .recordIndex = recIdx,
        .frameNumber = frame->frameNumber
    };
}

const VTransactionNode* VInspect_FindTransaction(const VInspectorDB* db, uint64_t txId) {
    if (txId == 0) return NULL;
    uint32_t mapIdx = HashTxId(txId);

    for (int i = 0; i < 8192; i++) {
        if (db->txLookupMap[mapIdx].txId == txId) {
            VRecordRef ref = db->txLookupMap[mapIdx].txRef;
            const VFrameCapture* frame = &db->ringBuffer[ref.ringIndex];
            if (frame->isPopulated && frame->frameNumber == ref.frameNumber) {
                return &frame->transactions[ref.recordIndex];
            }
            return NULL; // Stale/Overwritten
        }
        if (db->txLookupMap[mapIdx].txId == 0) return NULL;
        mapIdx = (mapIdx + 1) & (8192 - 1);
    }
    return NULL;
}