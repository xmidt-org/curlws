/*
 * SPDX-FileCopyrightText: 2016 Gustavo Sverzut Barbieri
 * SPDX-FileCopyrightText: 2021-2022 Comcast Cable Communications Management, LLC
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stddef.h>

/* Opaque typedef */
typedef struct mem_pool pool_t;

struct mem_pool_config {
    /* The maximum control block size (all inclusive).  All control blocks will
     * be this size (or larger) when returned. */
    size_t control_block_size;

    /* The maximum data block size (all inclusive).  All data blocks will
     * be this size (or larger) when returned. */
    size_t data_block_size;
};


/**
 * Initalize a memory pool allocator customized for websockets.
 *
 * @param cfg the configuration of the pool
 *
 * @return the pointer to the pool or NULL on error.
 */
pool_t *mem_init_pool(const struct mem_pool_config *cfg);


/**
 * Cleans up all allocated memory for the specified pool.
 *
 * @note This call will free all memory that the pool has ever allocated,
 *       regardless of it is in use or not.  It is suggested to mem_free()
 *       all memory first & make this call after you are done needing the
 *       pool
 *
 * @param pool the pool to clean up
 */
void mem_cleanup_pool(pool_t *pool);


/**
 * Allocates a control buffer of the size specified by the control_block_size
 * from the specified pool.
 *
 * @param pool the pool to allocate from
 *
 * @return the memory block, or NULL on error.
 */
void *mem_alloc_ctrl(pool_t *pool);


/**
 * Allocates a control buffer of the size specified by the control_block_size
 * from the specified pool.
 *
 * @param pool the pool to allocate from
 *
 * @return the memory block, or NULL on error.
 */
void *mem_alloc_data(pool_t *pool);


/**
 * Used to free the memory block back to the allocation pool.
 *
 * @note Only use this function to free blocks allocated by this allocator.
 *
 * @param ptr the pointer to the memory to free
 */
void mem_free(void *ptr);
#endif
