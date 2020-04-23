//
// Created by Juncheng Yang on 4/22/20.
//

#ifndef LIBMIMIRCACHE_SLABCOMMON_H
#define LIBMIMIRCACHE_SLABCOMMON_H


#include <glib.h>
#include <glib/gprintf.h>
#include <gmodule.h>
#include "../../include/mimircache/request.h"
#include "../../include/mimircache/cacheObj.h"
#include "../../cache/include/cacheUtils.h"


#define N_SLABCLASS 48
static guint32 SLABCLASS_SIZES[] = {96, 120, 152, 192, 240, 304, 384, 480, 600, 752, 944, 1184, 1480, 1856, 2320, 2904,
                                    3632, 4544, 5680, 7104, 8880, 11104, 13880, 17352, 21696, 27120, 33904, 42384,
                                    52984, 66232, 82792, 103496, 129376, 161720, 202152, 252696, 315872, 394840, 524288,
                                    655360, 819200, 1024000, 1280000, 1600000, 2000000, 2500000, 3125000, 3906250};

typedef struct {
  gint slabclass_id;
  guint32 n_total_items;
  guint32 n_stored_items;
  gpointer *slab_items;
} slab_t;

typedef struct {
  guint32 chunk_size;
  guint32 n_stored_items;
  GQueue *free_slab_q;
} slabclass_t;

typedef struct {
  guint64 slab_size;
  slabclass_t slabclasses[N_SLABCLASS];
  guint64 n_total_slabs;
  guint64 n_allocated_slabs;
  GQueue *queue;    /* slab queue for slabLRC and slabLRU, item queue for obj LRU */
  int8_t per_obj_metadata_size; // memcached on 64-bit system has 48/56 byte metadata (no cas/cas)
} slab_params_t;


typedef void (*evict_func_ptr_t)(cache_t *, request_t *);

typedef void (*slab_move_func_ptr_t)(cache_t *, request_t *);


static gint find_slabclass_id(guint32 obj_size);

static slab_t *allocate_slab(slab_params_t *slab_params, gint id);

static void _slab_destroyer(gpointer data) {
  slab_t *slab = (slab_t *) data;
  g_free(slab->slab_items);
  g_free(data);
}

static inline void init_slabclass(slab_params_t *slab_params, gint id) {
  slab_params->slabclasses[id].chunk_size = SLABCLASS_SIZES[id];
  slab_params->slabclasses[id].n_stored_items = 0;
  slab_params->slabclasses[id].free_slab_q = g_queue_new();
  allocate_slab(slab_params, id);
}


static inline gboolean _add_to_slab(slab_t *slab, request_t *req, slab_cache_obj_t *cache_obj) {
  slab->slab_items[slab->n_stored_items++] = cache_obj;
  return slab->n_total_items == slab->n_stored_items;
}


static inline slab_t *allocate_slab(slab_params_t *slab_params, gint id) {
  slab_t *new_slab = g_new0(slab_t, 1);
  guint32 chunk_size = SLABCLASS_SIZES[id];
  new_slab->slabclass_id = id;
  new_slab->n_stored_items = 0;
  new_slab->n_total_items = slab_params->slab_size / chunk_size;
  new_slab->slab_items = g_new0(gpointer, new_slab->n_total_items);
  slabclass_t *slabclass = &slab_params->slabclasses[id];
  g_queue_push_tail(slab_params->queue, new_slab);
  g_queue_push_tail(slabclass->free_slab_q, new_slab);
  slab_params->n_allocated_slabs += 1;
  DEBUG3("allocate slab %d total %u items - current slab queue length %u\n", id, new_slab->n_total_items,
         g_queue_get_length(slab_params->queue));
  return new_slab;
}

static inline void add_to_slabclass(cache_t *cache, request_t *req, slab_cache_obj_t *cache_obj,
                                    slab_params_t *slab_params,
                                    evict_func_ptr_t evict_func, slab_move_func_ptr_t slab_move_func) {
  gint slab_id = find_slabclass_id(req->obj_size);
  slabclass_t *slabclass = &slab_params->slabclasses[slab_id];
  slab_t *slab = NULL;

  if (g_queue_peek_head(slabclass->free_slab_q) != NULL) {
    // we still have free space for items, no new slab is needed
    slab = (slab_t *) g_queue_peek_head(slabclass->free_slab_q);
  } else if (slab_params->n_allocated_slabs < slab_params->n_total_slabs) {
    // all slabs are full, but we still have free space to allocate new slabs
    slab = allocate_slab(slab_params, slab_id);
  }

  // store obj
  if (slab != NULL) {
    slabclass->n_stored_items += 1;
    cache_obj->slab = slab;
    cache_obj->item_pos_in_slab = slab->n_stored_items;
    slab->slab_items[slab->n_stored_items++] = cache_obj->obj_id_ptr;
    if (slab->n_total_items == slab->n_stored_items) {
      // current slab is full, remove it out of free_slab_q
      g_queue_pop_head(slabclass->free_slab_q);
    }
  } else {
//    DEBUG("%u/%u slabs\n", (unsigned) slab_params->n_allocated_slabs, (unsigned) slab_params->n_total_slabs);
    DEBUG3("add req %lld: slabclass %d, chunk size %lu - %lu total items - %lu stored items\n",
           (gint64) req->obj_id_ptr, slab_id, (unsigned long) slabclass->chunk_size,
           (unsigned long) slabclass->n_stored_items, (unsigned long) slabclass->n_stored_items);
    // we do not have free space, so we need to evict item/slab
    evict_func(cache, req);
    if (slab_move_func != NULL) {
      slab_move_func(cache, req);
    }
    add_to_slabclass(cache, req, cache_obj, slab_params, evict_func, slab_move_func);
  }
}


static inline gint find_slabclass_id(guint32 obj_size) {
  gint id = 0, imin, imax;

  /* binary search */
  imin = 0;
  imax = N_SLABCLASS;
  while (imax >= imin) {
    id = (imin + imax) / 2;
    if (obj_size > SLABCLASS_SIZES[id]) {
      imin = id + 1;
    } else if (id > 0 && obj_size <= SLABCLASS_SIZES[id - 1]) {
      imax = id - 1;
    } else {
      break;
    }
  }

  if (imin > imax) { /* size too big for any slab */
    return -1;
  }

  return id;
}


#endif //LIBMIMIRCACHE_SLABCOMMON_H
