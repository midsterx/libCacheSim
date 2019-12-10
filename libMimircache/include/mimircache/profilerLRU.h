//
//  profilerLRU.h
//  profilerLRU
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef profilerLRU_h
#define profilerLRU_h

#ifdef __cplusplus
extern "C"
{
#endif


#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <math.h>
#include "reader.h"
#include "utils.h"
#include "const.h"
#include "distUtils.h"



double *get_lru_miss_ratio_seq(reader_t *reader, gint64 size);

guint64 *get_lru_miss_count_seq(reader_t *reader, gint64 size);
guint64 *get_lru_miss_byte_seq(reader_t *reader, gint64 size);

double *get_lru_obj_miss_ratio_seq(reader_t *reader, gint64 size);
double *get_lru_byte_miss_ratio_seq(reader_t *reader, gint64 size);


guint64 *get_lru_hit_count_seq(reader_t *reader, gint64 size);

double *get_lru_hit_ratio_seq(reader_t *reader, gint64 size);



guint64 *get_lru_hit_count_seq_shards(reader_t *reader, gint64 size, double sample_ratio);

double *get_lru_hit_ratio_seq_shards(reader_t *reader, gint64 size, double sample_ratio);





#ifdef __cplusplus
}
#endif


#endif /* profilerLRU_h */
