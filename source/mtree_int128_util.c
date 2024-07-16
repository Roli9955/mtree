/*
 * contrib/mtree_gist/mtree_int128_util.c
 */

#include "mtree_int128_util.h"

float mtree_int128_distance_internal(mtree_int128* first, mtree_int128* second) {
	float distance = int128_hamming_distance(first, second);

	float retval = (distance - first->coveringRadius) - second->coveringRadius;

	if(retval < 0.0){
		retval = 0.0;
	}

	return retval;
}

float mtree_int128_exact_distance(mtree_int128* first, mtree_int128* second){
	return int128_hamming_distance(first, second);
}

bool mtree_int128_equals(mtree_int128* first, mtree_int128* second) {
	return ((*(__int128*)first->data) - (*(__int128*)second->data)) == 0;
}

bool mtree_int128_overlap_distance(mtree_int128* first, mtree_int128* second) {
	float exact_distance = mtree_int128_exact_distance(first, second);
	return exact_distance - (first->coveringRadius + second->coveringRadius) < 0;
}

bool mtree_int128_contains_distance(mtree_int128* first, mtree_int128* second) {
	float exact_distance = mtree_int128_exact_distance(first, second);
	return exact_distance + second->coveringRadius < first->coveringRadius;
}

bool mtree_int128_contained_distance(mtree_int128* first, mtree_int128* second) {
	return mtree_int128_contains_distance(second, first);
}

mtree_int128* mtree_int128_deep_copy(mtree_int128* source) {
	mtree_int128* destination = (mtree_int128*)palloc(VARSIZE_ANY(source));
	memcpy(destination, source, VARSIZE_ANY(source));
	return destination;
}

float get_int128_distance(int size, mtree_int128* entries[size], float distances[size][size], int i, int j) {
	if (distances[i][j] == -1) {
		distances[i][j] = mtree_int128_distance_internal(entries[i], entries[j]);
	}
	return distances[i][j];
}

float int128_hamming_distance(mtree_int128* first, mtree_int128* second){
	__int128 x = (*(__int128*)first->data) ^ (*(__int128*)second->data);
    float setBits = 0;
 
    while (x > 0) {
        setBits += x & 1;
        x >>= 1;
    }
 
    return setBits;
}
