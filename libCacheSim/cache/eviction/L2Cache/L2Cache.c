//
//  a learned log-structure cache module
//
//

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "../../dataStructure/hashtable/hashtable.h"
#include "L2CacheInternal.h"

#include "bucket.h"
#include "obj.h"
#include "segment.h"

#include "cacheState.h"
#include "eviction.h"
#include "segSel.h"

#include "learned.h"

#include "const.h"
#include "init.h"
#include "utils.h"

#include <assert.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

FILE *ofile_cmp_y = NULL;

void L2Cache_set_default_init_params(L2Cache_init_params_t *init_params) {
  init_params->segment_size = 100;
  init_params->n_merge = 2;
  init_params->type = LOGCACHE_LOG_ORACLE;
  init_params->rank_intvl = 0.05;
  init_params->merge_consecutive_segs = true;
  init_params->train_source_x = TRAIN_X_FROM_SNAPSHOT;
  init_params->train_source_y = TRAIN_Y_FROM_ORACLE;
  init_params->bucket_type = SIZE_BUCKET;
  init_params->retrain_intvl = 86400 * 2;
  init_params->hit_density_age_shift = 3;
}

cache_t *L2Cache_init(common_cache_params_t ccache_params, void *init_params) {
  L2Cache_init_params_t *L2Cache_init_params = init_params;
  cache_t *cache =
      cache_struct_init(L2Cache_type_names[L2Cache_init_params->type], ccache_params);

  cache->hashtable->external_obj = true;
  check_init_params((L2Cache_init_params_t *) init_params);
  cache->init_params = my_malloc(L2Cache_init_params_t);
  memcpy(cache->init_params, L2Cache_init_params, sizeof(L2Cache_init_params_t));

  L2Cache_params_t *params = my_malloc(L2Cache_params_t);
  memset(params, 0, sizeof(L2Cache_params_t));
  cache->eviction_params = params;

  params->curr_evict_bucket_idx = 0;

  params->type = L2Cache_init_params->type;
  params->bucket_type = L2Cache_init_params->bucket_type;
  params->obj_score_type = L2Cache_init_params->obj_score_type;
  params->train_source_x = L2Cache_init_params->train_source_x;
  params->train_source_y = L2Cache_init_params->train_source_y;

  params->merge_consecutive_segs = L2Cache_init_params->merge_consecutive_segs;
  params->segment_size = L2Cache_init_params->segment_size;
  params->n_merge = L2Cache_init_params->n_merge;
  params->n_retain_per_seg = params->segment_size / params->n_merge;
  params->rank_intvl = L2Cache_init_params->rank_intvl;

  switch (params->type) {
    case SEGCACHE:
    case LOGCACHE_LOG_ORACLE:
    case LOGCACHE_LEARNED:
      //      params->obj_score_type = OBJ_SCORE_FREQ_AGE;
      params->obj_score_type = OBJ_SCORE_FREQ_BYTE;
      // params->obj_score_type = OBJ_SCORE_FREQ_AGE_BYTE;
      //    params->obj_score_type = OBJ_SCORE_HIT_DENSITY;
      break;
    case LOGCACHE_ITEM_ORACLE:
    case LOGCACHE_BOTH_ORACLE: params->obj_score_type = OBJ_SCORE_ORACLE; break;
    default: abort();
  };

  init_global_params();
  init_seg_sel(cache);
  init_obj_sel(cache);
  init_learner(cache);
  init_buckets(cache, L2Cache_init_params->hit_density_age_shift);
  init_cache_state(cache);

  cache->cache_init = L2Cache_init;
  cache->cache_free = L2Cache_free;
  cache->get = L2Cache_get;
  cache->check = L2Cache_check;
  cache->insert = L2Cache_insert;
  cache->evict = L2Cache_evict;
  cache->remove = L2Cache_remove;

  //   INFO(
  //       "L2Cche, %.2lf MB, %s, obj sel %s, %s, "
  //       %ld bytes start training seg collection, sample %d, rank intvl %d, "
  //       "training truth %d re-train %d\n",
  //       (double) cache->cache_size / 1048576,
  //       L2Cache_type_names[params->type],
  //       obj_score_type_names[params->obj_score_type],
  //       bucket_type_names[params->bucket_type],
  // //      (long) params->learner.n_bytes_start_collect_train,
  // //      params->learner.sample_every_n_seg_for_training,
  //       0L, 0,
  //       params->rank_intvl,
  //       params->train_source_x,
  //       params->train_source_y,
  //       0
  // //      params->learner.retrain_intvl
  //   );
  return cache;
}

void L2Cache_free(cache_t *cache) {
  my_free(sizeof(L2Cache_init_params_t), cache->init_params);
  L2Cache_params_t *params = cache->eviction_params;
  bucket_t *bkt = &params->train_bucket;
  segment_t *seg = bkt->first_seg, *next_seg;

  while (seg != NULL) {
    next_seg = seg->next_seg;
    my_free(sizeof(cache_obj_t) * params->segment_size, seg->objs);
    my_free(sizeof(segment_t), seg);
    seg = next_seg;
  }

  for (int i = 0; i < MAX_N_BUCKET; i++) {
    bkt = &params->buckets[i];
    seg = bkt->first_seg;

    while (seg != NULL) {
      next_seg = seg->next_seg;
      my_free(sizeof(cache_obj_t) * params->segment_size, seg->objs);
      my_free(sizeof(segment_t), seg);
      seg = next_seg;
    }
  }

  my_free(sizeof(L2Cache_params_t), params);
  cache_struct_free(cache);
}

cache_ck_res_e L2Cache_check(cache_t *cache, request_t *req, bool update_cache) {
  L2Cache_params_t *params = cache->eviction_params;

  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);

  if (cache_obj == NULL) {
    return cache_ck_miss;
  }

  if (!update_cache) {
    assert(0);
  }

  DEBUG_ASSERT(cache_obj->L2Cache.in_cache);

  /* seg_hit update segment state features */
  seg_hit(params, cache_obj);
  /* object hit update training data y and object stat */
  object_hit(params, cache_obj, req);

  if (params->train_source_y == TRAIN_Y_FROM_ONLINE) {
    update_train_y(params, cache_obj);
  }
  return cache_ck_hit;
}

cache_ck_res_e L2Cache_get(cache_t *cache, request_t *req) {
  L2Cache_params_t *params = cache->eviction_params;

  update_cache_state(cache, req);

  cache_ck_res_e ret = cache_get_base(cache, req);

  if (ret == cache_ck_miss) {
    params->cache_state.n_miss += 1;
    params->cache_state.n_miss_bytes += req->obj_size;
  }

  if (params->type == LOGCACHE_LEARNED) {
    /* generate training data by taking a snapshot */
    learner_t *l = &params->learner;
    if (params->cache_state.n_evicted_bytes >= cache->cache_size / 2) {
      if (l->n_train == -1) {
        snapshot_segs_to_training_data(cache);
        l->last_train_rtime = params->curr_rtime;
        l->n_train = 0;
      }
      if (params->curr_rtime - l->last_train_rtime >= l->retrain_intvl) {
        train(cache);
        snapshot_segs_to_training_data(cache);
      }
    }
  }
  return ret;
}

void L2Cache_insert(cache_t *cache, request_t *req) {
  L2Cache_params_t *params = cache->eviction_params;
  bucket_t *bucket = &params->buckets[find_bucket_idx(params, req)];
  segment_t *seg = bucket->last_seg;
  DEBUG_ASSERT(seg == NULL || seg->next_seg == NULL);

  if (seg == NULL || seg->n_obj == params->segment_size) {
    if (seg != NULL) {
      /* set the state of the finished segment */
      seg->req_rate = params->cache_state.req_rate;
      seg->write_rate = params->cache_state.write_rate;
      seg->miss_ratio = params->cache_state.miss_ratio;
    }

    seg = allocate_new_seg(cache, bucket->bucket_id);
    append_seg_to_bucket(params, bucket, seg);
  }

  cache_obj_t *cache_obj = &seg->objs[seg->n_obj];
  object_init(cache, req, cache_obj, seg);
  hashtable_insert_obj(cache->hashtable, cache_obj);

  seg->n_byte += cache_obj->obj_size + cache->per_obj_overhead;
  seg->n_obj += 1;
  cache->occupied_size += cache_obj->obj_size + cache->per_obj_overhead;
  cache->n_obj += 1;

  DEBUG_ASSERT(cache->n_obj > (params->n_segs - params->n_used_buckets) * params->segment_size);
  DEBUG_ASSERT(cache->n_obj <= params->n_segs * params->segment_size);
}

void L2Cache_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *l = &params->learner;

  bucket_t *bucket = select_segs_to_evict(cache, params->obj_sel.segs_to_evict);
  if (bucket == NULL) {
    // this can happen when space is fragmented between buckets and we cannot merge
    // and we evict segs[0] and return
    segment_t *seg = params->obj_sel.segs_to_evict[0];
    bucket = &params->buckets[seg->bucket_id];

    static int64_t last_print_time = 0;
    if (params->curr_rtime - last_print_time > 3600 * 6) {
      last_print_time = params->curr_rtime;
      WARN("%.2lf hour, cache size %lu MB, %d segs, evicting and cannot merge\n",
           (double) params->curr_rtime / 3600.0, cache->cache_size / 1024 / 1024,
           params->n_segs);
    }

    remove_seg_from_bucket(params, bucket, seg);
    evict_one_seg(cache, params->obj_sel.segs_to_evict[0]);
    return;
  }

  for (int i = 0; i < params->n_merge; i++) {
    params->cache_state.n_evicted_bytes += params->obj_sel.segs_to_evict[i]->n_byte;
  }
  params->n_evictions += 1;

  L2Cache_merge_segs(cache, bucket, params->obj_sel.segs_to_evict);

  if (params->obj_score_type == OBJ_SCORE_HIT_DENSITY
      && params->curr_vtime - params->last_hit_prob_compute_vtime > HIT_PROB_COMPUTE_INTVL) {
    /* update hit prob for all buckets */
    for (int i = 0; i < MAX_N_BUCKET; i++) {
      update_hit_prob_cdf(&params->buckets[i]);
    }
    params->last_hit_prob_compute_vtime = params->curr_vtime;
  }
}

void L2Cache_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  L2Cache_params_t *params = cache->eviction_params;
  abort();
}

void L2Cache_remove(cache_t *cache, obj_id_t obj_id) { abort(); }

#ifdef __cplusplus
}
#endif
