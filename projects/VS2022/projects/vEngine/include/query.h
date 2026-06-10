#pragma once

#include "vengine.h"

typedef struct {
    ComponentMask required;
    ComponentMask excluded;

    int entities[MAX_ENTITIES];
    int count;
} Query;

typedef struct {
    const char *name;
    Query *query;
    ComponentMask required;
    ComponentMask excluded;
    bool enabled;
} QueryDef;

static inline void Query_Clear(Query *q)
{
    q->count = 0;
}

static inline bool Query_Match(ComponentMask mask, const Query *q)
{
    if ((mask & q->required) != q->required)
        return false;
    if (mask & q->excluded)
        return false;
    return true;
}

static inline void Query_Build(Query *q, const EntityPool *pool)
{
    q->count = 0;

    for (int i = 0; i < pool->count; i++) {
        ComponentMask mask = pool->masks[i];
        if (!Query_Match(mask, q))
            continue;
        q->entities[q->count++] = i;
    }
}

static inline void BuildRegisteredQueries(const EntityPool *pool, QueryDef *defs, int defCount)
{
    for (int i = 0; i < defCount; i++) {
        QueryDef *d = &defs[i];
        if (!d->enabled)
            continue;
        d->query->required = d->required;
        d->query->excluded = d->excluded;
        Query_Build(d->query, pool);
    }
}
