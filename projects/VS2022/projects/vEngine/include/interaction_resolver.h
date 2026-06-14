#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef IR_MAX_INTENTS
#define IR_MAX_INTENTS 4096
#endif

#ifndef IR_MAX_EVENTS
#define IR_MAX_EVENTS 4096
#endif

#ifndef IR_MAX_CHANGES
#define IR_MAX_CHANGES 4096
#endif

#ifndef IR_MAX_RESOLVERS
#define IR_MAX_RESOLVERS 32
#endif

#ifndef IR_PAYLOAD_SIZE
#define IR_PAYLOAD_SIZE 64
#endif

typedef struct InteractionResolver InteractionResolver;

/* ============================================================
   Generic Record
   ============================================================ */

typedef struct
{
    uint32_t type;
    uint32_t size;

    uint8_t data[IR_PAYLOAD_SIZE];

} IR_Record;

/* ============================================================
   Resolver Callback
   ============================================================ */

typedef void (*IR_ResolverFn)(
    InteractionResolver* resolver,
    void* userData);

/* ============================================================
   Registered Resolver
   ============================================================ */

typedef struct
{
    IR_ResolverFn fn;
    void* userData;

} IR_ResolverEntry;

/* ============================================================
   Resolver
   ============================================================ */

struct InteractionResolver
{
    IR_Record intents[IR_MAX_INTENTS];
    uint32_t intentCount;

    IR_Record events[IR_MAX_EVENTS];
    uint32_t eventCount;

    IR_Record changes[IR_MAX_CHANGES];
    uint32_t changeCount;

    IR_ResolverEntry resolvers[IR_MAX_RESOLVERS];
    uint32_t resolverCount;
};

/* ============================================================
   Lifetime
   ============================================================ */

void IR_Init(
    InteractionResolver* resolver);

void IR_BeginFrame(
    InteractionResolver* resolver);

void IR_EndFrame(
    InteractionResolver* resolver);

/* ============================================================
   Resolver Registration
   ============================================================ */

bool IR_RegisterResolver(
    InteractionResolver* resolver,
    IR_ResolverFn fn,
    void* userData);

/* ============================================================
   Execution
   ============================================================ */

void IR_Run(
    InteractionResolver* resolver);

/* ============================================================
   Submission
   ============================================================ */

bool IR_SubmitIntent(
    InteractionResolver* resolver,
    uint32_t type,
    const void* data,
    uint32_t size);

bool IR_SubmitEvent(
    InteractionResolver* resolver,
    uint32_t type,
    const void* data,
    uint32_t size);

bool IR_SubmitChange(
    InteractionResolver* resolver,
    uint32_t type,
    const void* data,
    uint32_t size);

/* ============================================================
   Query
   ============================================================ */

const IR_Record* IR_GetIntents(
    const InteractionResolver* resolver,
    uint32_t* count);

const IR_Record* IR_GetEvents(
    const InteractionResolver* resolver,
    uint32_t* count);

const IR_Record* IR_GetChanges(
    const InteractionResolver* resolver,
    uint32_t* count);