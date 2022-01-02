/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <stdlib.h>

#include "memory.h"

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
struct mem_block_pool;

struct mem_block {
    struct mem_block_pool *parent;
    struct mem_block *prev;
    struct mem_block *next;
    uint8_t data[];
};

struct mem_block_pool {
    size_t block_size;
    struct mem_block *free;
    struct mem_block *active;
};

struct mem_pool {
    struct mem_block_pool ctrl;
    struct mem_block_pool data;
};

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static void *_mem_alloc(struct mem_block_pool *);


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
pool_t *mem_init_pool(const struct mem_pool_config *cfg)
{
    pool_t *pool;

    if (!cfg) {
        return NULL;
    }

    pool = (pool_t *) malloc(sizeof(pool_t));
    if (pool) {
        pool->ctrl.block_size = cfg->control_block_size;
        pool->ctrl.free       = NULL;
        pool->ctrl.active     = NULL;

        pool->data.block_size = cfg->data_block_size;
        pool->data.free       = NULL;
        pool->data.active     = NULL;
    }

    return pool;
}


void mem_cleanup_pool(pool_t *pool)
{
    struct mem_block *p;

    if (!pool) {
        return;
    }

    while ((p = pool->ctrl.active) != NULL) {
        pool->ctrl.active = p->next;
        free(p);
    }

    while ((p = pool->ctrl.free) != NULL) {
        pool->ctrl.free = p->next;
        free(p);
    }

    while ((p = pool->data.active) != NULL) {
        pool->data.active = p->next;
        free(p);
    }

    while ((p = pool->data.free) != NULL) {
        pool->data.free = p->next;
        free(p);
    }

    free(pool);
}


void *mem_alloc_ctrl(pool_t *pool)
{
    return _mem_alloc(&pool->ctrl);
}


void *mem_alloc_data(pool_t *pool)
{
    return _mem_alloc(&pool->data);
}


void mem_free(void *ptr)
{
    struct mem_block *p;
    ptrdiff_t delta = offsetof(struct mem_block, data);

    /* We return the data pointer so back up from the data field to find the
     * actual struct so we can point there. */
    p = (struct mem_block *) (((uint8_t *) ptr) - delta);

    /* If this is the head of the free list, don't try to free it again. */
    if (p == p->parent->free) {
        return;
    }

    /* Repoint the active list if needed. */
    if (p == p->parent->active) {
        p->parent->active = p->next;
    }

    /* Pull the node out of the list. */
    if (p->prev) {
        p->prev->next = p->next;
    }
    if (p->next) {
        p->next->prev = p->prev;
    }
    p->prev = NULL;

    /* Insert the node at the head of the free list */
    p->next = p->parent->free;
    if (p->next) {
        p->next->prev = p;
    }
    p->parent->free = p;
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/**
 * A more generic allocator that works on the smaller block pool struct
 * allowing it to be re-used.
 *
 * @param pool the pool to allocate from
 *
 * @return the memory or NULL if there was an allocation error
 */
static void *_mem_alloc(struct mem_block_pool *pool)
{
    struct mem_block *p;

    p = pool->free;
    if (p) {
        /* Prefer re-using an existing buffer, so pull it from the free list. */
        pool->free = p->next;
        if (pool->free) {
            pool->free->prev = NULL;
        }
    } else {
        /* Not enough, make a new node */
        p = (struct mem_block *) malloc(sizeof(struct mem_block) + pool->block_size);
        if (!p) {
            return NULL;
        }
        p->parent = pool;
        p->next   = NULL;
    }

    /* Insert the node into the active list */
    p->prev = NULL;
    p->next = pool->active;
    if (p->next) {
        p->next->prev = p;
    }
    pool->active = p;

    return (void *) &p->data[0];
}
