#include "interaction_resolver.h"
#include <string.h>
#include <vinspector.h>
#include <vengine.h>
#include <sim.h>

static uint64_t g_GlobalTxCounter = 1;

static bool IR_PushRecord(
    IR_Record* buffer, uint32_t* count, uint32_t capacity,
    uint32_t type, const void* data, uint32_t size)
{
    if (*count >= capacity || size > IR_PAYLOAD_SIZE) return false;

    IR_Record* r = &buffer[*count];
    r->id = g_GlobalTxCounter++; // Assign causality ID
    r->type = type;
    r->size = size;

    if (size > 0) memcpy(r->data, data, size);
    (*count)++;

    // LOG TO DATABASE:
    VTransactionNode tx = {
        .id = r->id,
        .sourceSystem = SYS_COMBAT, // Or map to whoever is currently active
        .intentType = type,
        .sourceEntity = -1, // You could pull this from your struct if needed
        .targetEntity = -1,
        .payloadSize = size
    };
    if (size > 0) memcpy(tx.payload, data, size);
    VInspect_LogTransaction(&g_InspectorDB, &tx);

    return true;
}

void IR_Init(
    InteractionResolver* resolver)
{
    memset(resolver, 0, sizeof(*resolver));
}

void IR_BeginFrame(
    InteractionResolver* resolver)
{
    resolver->intentCount = 0;
    resolver->eventCount = 0;
    resolver->changeCount = 0;
}

void IR_EndFrame(
    InteractionResolver* resolver)
{
}

bool IR_RegisterResolver(
    InteractionResolver* resolver,
    IR_ResolverFn fn,
    void* userData)
{
    if (resolver->resolverCount >= IR_MAX_RESOLVERS)
        return false;

    resolver->resolvers[
        resolver->resolverCount++
    ] = (IR_ResolverEntry)
        {
            fn,
            userData
        };

        return true;
}

void IR_Run(
    InteractionResolver* resolver)
{
    for (uint32_t i = 0;
        i < resolver->resolverCount;
        i++)
    {
        resolver->resolvers[i].fn(
            resolver,
            resolver->resolvers[i].userData);
    }
}

bool IR_SubmitIntent(
    InteractionResolver* resolver,
    uint32_t type,
    const void* data,
    uint32_t size)
{
    return IR_PushRecord(
        resolver->intents,
        &resolver->intentCount,
        IR_MAX_INTENTS,
        type,
        data,
        size);
}

bool IR_SubmitEvent(
    InteractionResolver* resolver,
    uint32_t type,
    const void* data,
    uint32_t size)
{
    return IR_PushRecord(
        resolver->events,
        &resolver->eventCount,
        IR_MAX_EVENTS,
        type,
        data,
        size);
}

bool IR_SubmitChange(
    InteractionResolver* resolver,
    uint32_t type,
    const void* data,
    uint32_t size)
{
    return IR_PushRecord(
        resolver->changes,
        &resolver->changeCount,
        IR_MAX_CHANGES,
        type,
        data,
        size);
}

const IR_Record* IR_GetIntents(
    const InteractionResolver* resolver,
    uint32_t* count)
{
    *count = resolver->intentCount;
    return resolver->intents;
}

const IR_Record* IR_GetEvents(
    const InteractionResolver* resolver,
    uint32_t* count)
{
    *count = resolver->eventCount;
    return resolver->events;
}

const IR_Record* IR_GetChanges(
    const InteractionResolver* resolver,
    uint32_t* count)
{
    *count = resolver->changeCount;
    return resolver->changes;
}