/*
 * contrib/mtree_gist/mtree_int128.h
 */

#ifndef __MTREE_INT128_H__
#define __MTREE_INT128_H__

#include "postgres.h"
#include "access/gist.h"

#include <math.h>

#include "mtree_gist.h"

typedef struct {
	float parentDistance;
	float coveringRadius;
	// __int128 data;
	char data[sizeof(__int128) + 1];
} mtree_int128;

#define MTREE_INT128_SIZE (2 * sizeof(float) + sizeof(char) * (sizeof(__int128) + 1))
#define DatumGetMtreeInt128(x) ((mtree_int128*)PG_DETOAST_DATUM(x))
#define PG_GETARG_MTREE_INT128_P(x) DatumGetMtreeInt128(PG_GETARG_DATUM(x))
#define PG_RETURN_MTREE_INT128_P(x) PG_RETURN_POINTER(x)

#endif
