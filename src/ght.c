/*
 * ght.c - Generic Hash Table implementation
 * 
 * Copyright (c) 2024 Laurent Mailloux-Bourassa
 * 
 * This file is part of the Generic Hash Table (GHT) library.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <threads.h>
#include "ght.h"

#define GHT_DEFAULT_WIDTH   (100)

#define GHT_MUTEX_LOCK(ght)     mtx_lock(&ght->mutex)
#define GHT_MUTEX_UNLOCK(ght)   mtx_unlock(&ght->mutex)

typedef struct ght_bucket ght_bucket_t;
typedef struct ght_bucket
{
    ght_key_t key;
    ght_hash_t hash;
    ght_data_t data;
    ght_bucket_t* next;
} ght_bucket_t;

typedef struct ght_table
{
    mtx_t mutex;
    ght_digestor_t digestor;
    ght_deallocator_t deallocator;
    ght_width_t width;
    ght_load_factor_t auto_resize;
    ght_bucket_t** buckets;
    ght_load_t load;
} ght_table_t;

static ght_hash_t _ght_digestor_murmur3(ght_key_t key);
static GHT_FORCE_INLINE uint32_t _ght_digestor_murmur3_32(ght_key_t key, uint32_t seed);
static GHT_FORCE_INLINE uint64_t _ght_digestor_murmur3_64(ght_key_t key, uint64_t seed);
static void _ght_delete_recursive(ght_bucket_t* bucket, ght_load_t* load, ght_deallocator_t deallocator);
static void _ght_move_recursive(ght_bucket_t* bucket, ght_load_t* moved, ght_table_t* to_table);

#define GHT_DIGESTOR_MURMUR3(key, seed) _Generic((key), \
        uint64_t: _ght_digestor_murmur3_64,             \
        default: _ght_digestor_murmur3_32               \
        )(key, seed)

ght_table_t* ght_create(ght_cfg_t* cfg)
{
    ght_digestor_t digestor;
    ght_deallocator_t deallocator;
    ght_width_t width;
    ght_load_factor_t auto_resize;

    if (cfg)
    {
        digestor = cfg->digestor ? cfg->digestor : _ght_digestor_murmur3;
        deallocator = cfg->deallocator;
        width = cfg->width ? cfg->width : GHT_DEFAULT_WIDTH;
        auto_resize = cfg->auto_resize;
    }
    else
    {
        digestor = _ght_digestor_murmur3;
        deallocator = NULL;
        width = GHT_DEFAULT_WIDTH;
        auto_resize = 0.0;
    }
    
    ght_table_t* table = calloc(1, sizeof(ght_table_t));

    if (table)
    {
        if (thrd_success != mtx_init(&table->mutex, mtx_plain | mtx_recursive))
        {
            free(table);
            return NULL;
        }

        table->buckets = calloc(width, sizeof(ght_bucket_t*));
        table->digestor = digestor;
        table->deallocator = deallocator;
        table->width = width;
        table->auto_resize = auto_resize;
    }
    
    return table;
}

ght_status_t ght_destroy(ght_table_t* table)
{
    if (!table) return -1;
    
    for (ght_load_t i = 0; table->load && (i < table->width); i++)
    {
        _ght_delete_recursive(table->buckets[i], &table->load, table->deallocator);
        table->buckets[i] = NULL;
    }
    
    free(table->buckets);
    table->buckets = NULL;

    mtx_destroy(&table->mutex);
    free(table);
    
    return 0;
}

ght_status_t ght_insert(ght_table_t* table, ght_key_t key, ght_data_t data)
{
    if (!table) return -1;
    GHT_MUTEX_LOCK(table);

    ght_index_t index = table->digestor(key) % table->width;
    ght_bucket_t* bucket = table->buckets[index];
    ght_bucket_t* prev = NULL;
    
    while (bucket && (key != bucket->key))
    {
        prev = bucket;
        bucket = bucket->next;
    }

    if (bucket)
    {
        if (table->deallocator)
        {
            table->deallocator(bucket->key, bucket->data);
        }

        bucket->data = data;
        
        if (prev)
        {
            prev->next = bucket->next;
            bucket->next = table->buckets[index];
            table->buckets[index] = bucket;
        }
        
        GHT_MUTEX_UNLOCK(table);
        return 0;
    }

    bucket = calloc(1, sizeof(ght_bucket_t));

    if (!bucket)
    {
        GHT_MUTEX_UNLOCK(table);
        return -1;
    }
    
    if (table->auto_resize > 0.0 && (ght_load_factor_t) (table->load + 1)/(ght_load_factor_t) table->width > table->auto_resize)
    {
        ght_resize(table, table->width * 2);
    }
    
    bucket->key = key;
    bucket->data = data;
    
    bucket->hash = table->digestor(key);
    index = bucket->hash % table->width;
    bucket->next = table->buckets[index];
    table->buckets[index] = bucket;
    table->load++; 
    
    GHT_MUTEX_UNLOCK(table);
    return 0;
}

ght_data_t ght_search(ght_table_t* table, ght_key_t key)
{
    if (!table) return 0;
    GHT_MUTEX_LOCK(table);
    
    ght_index_t index = table->digestor(key) % table->width;
    ght_bucket_t* bucket = table->buckets[index];
    ght_bucket_t* prev = NULL;
    
    while (bucket && (key != bucket->key))
    {
        prev = bucket;
        bucket = bucket->next;
    }
    
    if (!bucket)
    {
        GHT_MUTEX_UNLOCK(table);
        return 0;
    }

    if (prev)
    {
        prev->next = bucket->next;
        bucket->next = table->buckets[index];
        table->buckets[index] = bucket;
    }
    
    ght_data_t data = bucket->data;

    GHT_MUTEX_UNLOCK(table);
    return data;
}

ght_status_t ght_delete(ght_table_t* table, ght_key_t key)
{
    if (!table) return -1;
    GHT_MUTEX_LOCK(table);
    
    ght_index_t index = table->digestor(key) % table->width;
    ght_bucket_t* bucket = table->buckets[index];
    ght_bucket_t* prev = NULL;
    
    while (bucket && (key != bucket->key))
    {
        prev = bucket;
        bucket = bucket->next;
    }
    
    if (!bucket)
    {
        GHT_MUTEX_UNLOCK(table);
        return -1;
    }

    if (prev)
    {
        prev->next = bucket->next;
    }
    else
    {
        table->buckets[index] = bucket->next;
    }
    
    if (table->deallocator)
    {
        table->deallocator(bucket->key, bucket->data);
    }
    
    free(bucket);
    table->load--;
    
    GHT_MUTEX_UNLOCK(table);
    return 0;
}

ght_load_t ght_load(ght_table_t* table)
{
    if (!table) return 0;
    GHT_MUTEX_LOCK(table);

    ght_load_t load = table->load;

    GHT_MUTEX_UNLOCK(table);
    return load;
}

ght_width_t ght_width(ght_table_t* table)
{
    if (!table) return 0;
    GHT_MUTEX_LOCK(table);

    ght_width_t width = table->width;

    GHT_MUTEX_UNLOCK(table);
    return width;
}

ght_load_factor_t ght_load_factor(ght_table_t* table)
{
    if (!table) return 0.0;
    GHT_MUTEX_LOCK(table);

    if (table->width == 0)
    {
        GHT_MUTEX_UNLOCK(table);
        return 0.0;
    }

    ght_load_factor_t load_factor = (ght_load_factor_t) table->load / (ght_load_factor_t) table->width;
    
    GHT_MUTEX_UNLOCK(table);
    return load_factor;
}

ght_status_t ght_resize(ght_table_t* table, ght_width_t width)
{
    if (!table || !width) return -1;
    GHT_MUTEX_LOCK(table);
    
    ght_cfg_t cfg = {
                        .digestor = table->digestor,
                        .deallocator = table->deallocator,
                        .width = width,
                        .auto_resize = 0.0
                    };

    ght_table_t* new = ght_create(&cfg);
    
    ght_load_t moved = 0;
    for (ght_load_t i = 0; moved < table->load && i < table->width; i++)
    {
        _ght_move_recursive(table->buckets[i], &moved, new);
    }
    
    free(table->buckets);
    table->buckets = new->buckets;
    table->width = new->width;
    mtx_destroy(&new->mutex);
    free(new);
    
    GHT_MUTEX_UNLOCK(table);
    return 0;
}

static ght_hash_t _ght_digestor_murmur3(ght_key_t key)
{
    uint32_t seed = 0x9747b28c;
    return GHT_DIGESTOR_MURMUR3(key, seed);
}

static GHT_FORCE_INLINE uint32_t _ght_digestor_murmur3_32(ght_key_t key, uint32_t seed)
{
    uint32_t hash = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    
    uint32_t k1 = key;
    
    k1 *= c1;
    k1 = (k1 << 15) | (k1 >> (32 - 15));  // ROTL32(k1, 15)
    k1 *= c2;

    hash ^= k1;
    hash = (hash << 13) | (hash >> (32 - 13));  // ROTL32(hash, 13)
    hash = hash * 5 + 0xe6546b64;

    // Finalization
    hash ^= sizeof(uint32_t);
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    hash *= 0xc2b2ae35;
    hash ^= (hash >> 16);
    
    return hash;
}

static GHT_FORCE_INLINE uint64_t _ght_digestor_murmur3_64(ght_key_t key, uint64_t seed)
{
    uint64_t hash = seed;
    const uint64_t c1 = 0x87c37b91114253d5ULL;
    const uint64_t c2 = 0x4cf5ad432745937fULL;
    
    uint64_t k1 = key;
    
    k1 *= c1;
    k1 = (k1 << 31) | (k1 >> (64 - 31));  // ROTL64(k1, 31)
    k1 *= c2;

    hash ^= k1;
    hash = (hash << 27) | (hash >> (64 - 27));  // ROTL64(hash, 27)
    hash = hash * 5 + 0x52dce729;

    // Finalization
    hash ^= sizeof(uint64_t);
    hash ^= (hash >> 33);
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= (hash >> 33);
    hash *= 0xc4ceb9fe1a85ec53ULL;
    hash ^= (hash >> 33);

    return hash;
}

static void _ght_delete_recursive(ght_bucket_t* bucket, ght_load_t* load, ght_deallocator_t deallocator)
{
    if (!bucket) return;
    
    if (bucket->next)
    {
        _ght_delete_recursive(bucket->next, load, deallocator);
        bucket->next = NULL;
    }
    
    if (deallocator)
    {
        deallocator(bucket->key, bucket->data);
    }
    
    free(bucket);
    (*load)--;
}

static void _ght_move_recursive(ght_bucket_t* bucket, ght_load_t* moved, ght_table_t* to_table)
{
    if (!bucket) return;
    
    if (bucket->next)
    {
        _ght_move_recursive(bucket->next, moved, to_table);
    }

    ght_index_t index = bucket->hash % to_table->width;
    bucket->next = to_table->buckets[index];
    to_table->buckets[index] = bucket;
    (*moved)++;
}
