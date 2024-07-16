/*
 * contrib/mtree_gist/mtree_int128_util.h
 */

#ifndef __MTREE_FLOAT_UTIL_H__
#define __MTREE_FLOAT_UTIL_H__

#include "mtree_int128.h"

float mtree_int128_distance_internal(mtree_int128* first, mtree_int128* second);
float mtree_int128_exact_distance(mtree_int128* first, mtree_int128* second);
bool mtree_int128_equals(mtree_int128* first, mtree_int128* second);
bool mtree_int128_overlap_distance(mtree_int128* first, mtree_int128* second);
bool mtree_int128_contains_distance(mtree_int128* first, mtree_int128* second);
bool mtree_int128_contained_distance(mtree_int128* first, mtree_int128* second);
mtree_int128* mtree_int128_deep_copy(mtree_int128* source);
float get_int128_distance(int size, mtree_int128* entries[size], float distances[size][size], int i, int j);

float int128_hamming_distance(mtree_int128* first, mtree_int128* second);

#endif
