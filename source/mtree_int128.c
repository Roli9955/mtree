/*
 * contrib/mtree_gist/mtree_int128.c
 */

#include "mtree_int128.h"

#include "mtree_int128_util.h"
#include "mtree_util.h"

#define bitcheck(byte,nbit) (((byte) &   (1<<(nbit)))/(1<<(nbit)))

 // TODO: Strategy should be a parameter!
const MtreeUnionStrategy UNION_STRATEGY_INT128 = MinMaxDistance;
const MtreePickSplitStrategy PICKSPLIT_STRATEGY_INT128 = SamplingMinOverlapArea;

PG_FUNCTION_INFO_V1(mtree_int128_input);
PG_FUNCTION_INFO_V1(mtree_int128_output);

PG_FUNCTION_INFO_V1(mtree_int128_consistent);
PG_FUNCTION_INFO_V1(mtree_int128_union);
PG_FUNCTION_INFO_V1(mtree_int128_same);

PG_FUNCTION_INFO_V1(mtree_int128_penalty);
PG_FUNCTION_INFO_V1(mtree_int128_picksplit);

PG_FUNCTION_INFO_V1(mtree_int128_compress);
PG_FUNCTION_INFO_V1(mtree_int128_decompress);

PG_FUNCTION_INFO_V1(mtree_int128_distance);

PG_FUNCTION_INFO_V1(mtree_int128_contains_operator);
PG_FUNCTION_INFO_V1(mtree_int128_contained_operator);
PG_FUNCTION_INFO_V1(mtree_int128_distance_operator);
PG_FUNCTION_INFO_V1(mtree_int128_overlap_operator);

Datum mtree_int128_input(PG_FUNCTION_ARGS) {
	char* input = PG_GETARG_CSTRING(0);

	mtree_int128* result = (mtree_int128*)palloc(MTREE_INT128_SIZE);
	result->coveringRadius = 0;
	result->parentDistance = 0;

	SET_VARSIZE(result, MTREE_INT128_SIZE);

	__int128 in = atoint128(input);
	result->data[16] = '\0';
	memcpy(result->data, &in, sizeof(__int128));

	PG_RETURN_POINTER(result);
}

Datum mtree_int128_output(PG_FUNCTION_ARGS) {
	mtree_int128* output = PG_GETARG_MTREE_INT128_P(0);
	char* result;

	if (output->coveringRadius == 0) {
		__int128 resint128 = *(__int128*)output->data;

		char res[129] = "";
		for(short i = 128 ; i > 0; i--){
			if(bitcheck((resint128 >> 128-i), 0) == 1){
				res[(i-1)] = '1';
			} else {
				res[(i-1)] = '0';
			}
		}
		res[128] = '\0';
		result = psprintf("%s", res);
	}
	else {
		result =
			psprintf("coveringRadius|%d parentDistance|%d data|%i",
				output->coveringRadius, output->parentDistance, output->data);
	}

	PG_RETURN_CSTRING(result);
}

Datum mtree_int128_consistent(PG_FUNCTION_ARGS) {
	GISTENTRY* entry = (GISTENTRY*)PG_GETARG_POINTER(0);
	mtree_int128* query = PG_GETARG_MTREE_INT128_P(1);
	StrategyNumber strategyNumber = (StrategyNumber)PG_GETARG_UINT16(2);
	bool* recheck = (bool*)PG_GETARG_POINTER(4);
	mtree_int128* key = DatumGetMtreeInt128(entry->key);
	float distance = mtree_int128_distance_internal(key, query);


	*recheck = false;

	bool returnValue;
	if (GIST_LEAF(entry)) {
		switch (strategyNumber) {
		case GIST_SN_SAME:
			returnValue = mtree_int128_equals(key, query);
			break;
		case GIST_SN_OVERLAPS:
			returnValue = mtree_int128_overlap_distance(key, query);
			break;
		case GIST_SN_CONTAINS:
			returnValue = mtree_int128_contains_distance(key, query);
			break;
		case GIST_SN_CONTAINED_BY:
			returnValue = mtree_int128_contained_distance(key, query);
			break;
		default:
			ereport(ERROR,
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("Invalid StrategyNumber for consistent function: %u", strategyNumber));
			break;
		}
	}
	else {
		switch (strategyNumber) {
		case GIST_SN_SAME:
			returnValue = mtree_int128_contains_distance(key, query);
			break;
		case GIST_SN_OVERLAPS:
			returnValue = mtree_int128_overlap_distance(key, query);
			break;
		case GIST_SN_CONTAINS:
			returnValue = mtree_int128_contains_distance(key, query);
			break;
		case GIST_SN_CONTAINED_BY:
			returnValue = mtree_int128_overlap_distance(key, query);
			break;
		default:
			ereport(ERROR,
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("Invalid StrategyNumber for consistent function: %u", strategyNumber));
			break;
		}
	}

	PG_RETURN_BOOL(returnValue);
}

Datum mtree_int128_union(PG_FUNCTION_ARGS) {
	GistEntryVector* entryVector = (GistEntryVector*)PG_GETARG_POINTER(0);
	GISTENTRY* entry = entryVector->vector;
	int ranges = entryVector->n;


	mtree_int128* entries[ranges];
	for (int i = 0; i < ranges; ++i) {
		entries[i] = DatumGetMtreeInt128(entry[i].key);
	}

	int searchRange;

	MtreeUnionStrategy UNION_STRATEGY_INT128 = MinMaxDistance;
	if (PG_HAS_OPCLASS_OPTIONS()){
		MtreeOptions* options = (MtreeOptions *) PG_GET_OPCLASS_OPTIONS();
		UNION_STRATEGY_INT128 = options->union_strategy;
	}

	switch (UNION_STRATEGY_INT128) {
	case First:
		searchRange = 1;
		break;
	case MinMaxDistance:
		searchRange = ranges;
		break;
	default:
		ereport(ERROR,
			errcode(ERRCODE_SYNTAX_ERROR),
			errmsg("Invalid StrategyNumber for union function: %u", UNION_STRATEGY_INT128));
		break;
	}

	float coveringRadii[searchRange];

	for (int i = 0; i < searchRange; ++i) {
		coveringRadii[i] = 0;

		for (int j = 0; j < ranges; ++j) {
			float distance = mtree_int128_distance_internal(entries[i], entries[j]);
			float newCoveringRadius;

			if (distance > 0){
				newCoveringRadius = distance + entries[i]->coveringRadius + (2 * entries[j]->coveringRadius);
			} else {
				float exact_distance = mtree_int128_exact_distance(entries[i], entries[j]);
				float intersect = exact_distance - (entries[i]->coveringRadius + entries[j]->coveringRadius);
				newCoveringRadius = entries[i]->coveringRadius + (2 * entries[j]->coveringRadius) + intersect;
			}

			if (coveringRadii[i] < newCoveringRadius) {
				coveringRadii[i] = newCoveringRadius;
			}
		}
	}

	int minimumIndex = 0;

	for (int i = 1; i < searchRange; ++i) {
		if (coveringRadii[i] < coveringRadii[minimumIndex]) {
			minimumIndex = i;
		}
	}

	mtree_int128* out = mtree_int128_deep_copy(entries[minimumIndex]);
	out->coveringRadius = coveringRadii[minimumIndex];

	PG_RETURN_MTREE_INT128_P(out);
}

Datum mtree_int128_same(PG_FUNCTION_ARGS) {
	mtree_int128* first = PG_GETARG_MTREE_INT128_P(0);
	mtree_int128* second = PG_GETARG_MTREE_INT128_P(1);
	PG_RETURN_BOOL(mtree_int128_equals(first, second));
}

Datum mtree_int128_penalty(PG_FUNCTION_ARGS) {
	GISTENTRY* originalEntry = (GISTENTRY*)PG_GETARG_POINTER(0);
	GISTENTRY* newEntry = (GISTENTRY*)PG_GETARG_POINTER(1);
	float* penalty = (float*)PG_GETARG_POINTER(2);
	mtree_int128* original = DatumGetMtreeInt128(originalEntry->key);
	mtree_int128* new = DatumGetMtreeInt128(newEntry->key);

	float distance = mtree_int128_distance_internal(original, new);
	*penalty = distance;

	PG_RETURN_POINTER(penalty);
}

/* TODO: Lots of duplicate code. */
Datum mtree_int128_picksplit(PG_FUNCTION_ARGS) {
	GistEntryVector* entryVector = (GistEntryVector*)PG_GETARG_POINTER(0);
	GIST_SPLITVEC* vector = (GIST_SPLITVEC*)PG_GETARG_POINTER(1);
	OffsetNumber maxOffset = (OffsetNumber)entryVector->n - 1;
	OffsetNumber numberBytes = (OffsetNumber)(maxOffset + 1) * sizeof(OffsetNumber);
	OffsetNumber* left;
	OffsetNumber* right;


	vector->spl_left = (OffsetNumber*)palloc(numberBytes);
	left = vector->spl_left;
	vector->spl_nleft = 0;

	vector->spl_right = (OffsetNumber*)palloc(numberBytes);
	right = vector->spl_right;
	vector->spl_nright = 0;

	mtree_int128* entries[maxOffset];
	for (OffsetNumber i = FirstOffsetNumber; i <= maxOffset; i = OffsetNumberNext(i)) {
		entries[i - FirstOffsetNumber] = DatumGetMtreeInt128(entryVector->vector[i].key);
	}

	float distances[maxOffset][maxOffset];
	init_distances_int128(maxOffset, *distances);

	int leftIndex, rightIndex, leftCandidateIndex, rightCandidateIndex;
	int trialCount = 100;
	float maxDistance = -1;
	float minCoveringSum = -1;
	float minCoveringMax = -1;
	float minOverlapArea = -1;
	float minSumArea = -1;

	MtreePickSplitStrategy PICKSPLIT_STRATEGY_INT128 = SamplingMinOverlapArea;
	if(PG_HAS_OPCLASS_OPTIONS()){
		MtreeOptions* options = (MtreeOptions *) PG_GET_OPCLASS_OPTIONS();
		PICKSPLIT_STRATEGY_INT128 = options->picksplit_strategy;
	}

	switch (PICKSPLIT_STRATEGY_INT128) {
	case Random:
		leftIndex = ((int)random()) % (maxOffset - 1);
		rightIndex = (leftIndex + 1) + (((int)random()) % (maxOffset - leftIndex - 1));
		break;
	case FirstTwo:
		leftIndex = 0;
		rightIndex = 1;
		break;
	case MaxDistanceFromFirst:
		maxDistance = -1;
		for (int r = 0; r < maxOffset; ++r) {
			float distance = get_int128_distance(maxOffset, entries, distances, 0, r);
			if (distance > maxDistance) {
				maxDistance = distance;
				rightCandidateIndex = r;
			}
		}
		leftIndex = 0;
		rightIndex = rightCandidateIndex;
		break;
	case MaxDistancePair:
		for (OffsetNumber l = 0; l < maxOffset; ++l) {
			for (OffsetNumber r = l; r < maxOffset; ++r) {
				float distance = get_int128_distance(maxOffset, entries, distances, l, r);
				if (distance > maxDistance) {
					maxDistance = distance;
					leftCandidateIndex = l;
					rightCandidateIndex = r;
				}
			}
		}
		leftIndex = leftCandidateIndex;
		rightIndex = rightCandidateIndex;
		break;
	case SamplingMinCoveringSum:
		for (int i = 0; i < trialCount; ++i) {
			leftCandidateIndex = ((int)random()) % (maxOffset - 1);
			rightCandidateIndex = (leftCandidateIndex + 1) + (((int)random()) % (maxOffset - leftCandidateIndex - 1));
			float leftRadius = 0, rightRadius = 0;

			for (int currentIndex = 0; currentIndex < maxOffset; currentIndex++) {
				float leftDistance = get_int128_distance(maxOffset, entries, distances, leftCandidateIndex, currentIndex);
				float rightDistance = get_int128_distance(maxOffset, entries, distances, rightCandidateIndex, currentIndex);

				if (leftDistance < rightDistance) {
					if (leftDistance + entries[currentIndex]->coveringRadius > leftRadius) {
						leftRadius = leftDistance + entries[currentIndex]->coveringRadius;
					}
				}
				else {
					if (rightDistance + entries[currentIndex]->coveringRadius >
						rightRadius) {
						rightRadius = rightDistance + entries[currentIndex]->coveringRadius;
					}
				}
			}

			if (minCoveringSum == -1 || leftRadius + rightRadius < minCoveringSum) {
				minCoveringSum = leftRadius + rightRadius;
				leftIndex = leftCandidateIndex;
				rightIndex = rightCandidateIndex;
			}
		}
		break;
	case SamplingMinCoveringMax:
		for (int i = 0; i < trialCount; ++i) {
			leftCandidateIndex = ((int)random()) % (maxOffset - 1);
			rightCandidateIndex = (leftCandidateIndex + 1) + (((int)random()) % (maxOffset - leftCandidateIndex - 1));
			float leftRadius = 0, rightRadius = 0;

			for (int currentIndex = 0; currentIndex < maxOffset; ++currentIndex) {
				float leftDistance = get_int128_distance(maxOffset, entries, distances, leftCandidateIndex, currentIndex);
				float rightDistance = get_int128_distance(maxOffset, entries, distances, rightCandidateIndex, currentIndex);

				if (leftDistance < rightDistance) {
					if (leftDistance + entries[currentIndex]->coveringRadius > leftRadius) {
						leftRadius = leftDistance + entries[currentIndex]->coveringRadius;
					}
				}
				else {
					if (rightDistance + entries[currentIndex]->coveringRadius > rightRadius) {
						rightRadius = rightDistance + entries[currentIndex]->coveringRadius;
					}
				}
			}

			if (minCoveringMax == -1 || MAX_2(leftRadius, rightRadius) < minCoveringMax) {
				minCoveringMax = MAX_2(leftRadius, rightRadius);
				leftIndex = leftCandidateIndex;
				rightIndex = rightCandidateIndex;
			}
		}
		break;
	case SamplingMinOverlapArea:
		for (int i = 0; i < trialCount; i++) {
			leftCandidateIndex = ((int)random()) % (maxOffset - 1);
			rightCandidateIndex = (leftCandidateIndex + 1) + (((int)random()) % (maxOffset - leftCandidateIndex - 1));
			float distance = get_int128_distance(maxOffset, entries, distances, leftCandidateIndex, rightCandidateIndex);
			float leftRadius = 0, rightRadius = 0;

			for (int currentIndex = 0; currentIndex < maxOffset; currentIndex++) {
				float leftDistance = get_int128_distance(maxOffset, entries, distances, leftCandidateIndex, currentIndex);
				float rightDistance = get_int128_distance(maxOffset, entries, distances, rightCandidateIndex, currentIndex);

				if (leftDistance < rightDistance) {
					if (leftDistance + entries[currentIndex]->coveringRadius > leftRadius) {
						leftRadius = leftDistance + entries[currentIndex]->coveringRadius;
					}
				}
				else {
					if (rightDistance + entries[currentIndex]->coveringRadius > rightRadius) {
						rightRadius = rightDistance + entries[currentIndex]->coveringRadius;
					}
				}
			}

			float currentOverlapArea = overlap_area(leftRadius, rightRadius, distance);
			if (minOverlapArea == -1 || currentOverlapArea < minOverlapArea) {
				minOverlapArea = (int)currentOverlapArea;
				leftIndex = leftCandidateIndex;
				rightIndex = rightCandidateIndex;
			}
		}
		break;
	case SamplingMinAreaSum:
		for (int i = 0; i < trialCount; i++) {
			leftCandidateIndex = ((int)random()) % (maxOffset - 1);
			rightCandidateIndex = (leftCandidateIndex + 1) + (((int)random()) % (maxOffset - leftCandidateIndex - 1));
			float leftRadius = 0, rightRadius = 0;

			for (int currentIndex = 0; currentIndex < maxOffset; currentIndex++) {
				float leftDistance = get_int128_distance(maxOffset, entries, distances, leftCandidateIndex, currentIndex);
				float rightDistance = get_int128_distance(maxOffset, entries, distances, rightCandidateIndex, currentIndex);

				if (leftDistance < rightDistance) {
					if (leftDistance + entries[currentIndex]->coveringRadius > leftRadius) {
						leftRadius = leftDistance + entries[currentIndex]->coveringRadius;
					}
				}
				else {
					if (rightDistance + entries[currentIndex]->coveringRadius > rightRadius) {
						rightRadius = rightDistance + entries[currentIndex]->coveringRadius;
					}
				}
			}

			float currentSumArea = leftRadius * leftRadius + rightRadius * rightRadius;
			if (minSumArea == -1 || currentSumArea < minSumArea) {
				minSumArea = currentSumArea;
				leftIndex = leftCandidateIndex;
				rightIndex = rightCandidateIndex;
			}
		}
		break;
	default:
		ereport(ERROR,
			errcode(ERRCODE_SYNTAX_ERROR),
			errmsg("Invalid StrategyNumber for picksplit function: %u", PICKSPLIT_STRATEGY_INT128));
		break;
	}

	mtree_int128* unionLeft = mtree_int128_deep_copy(entries[leftIndex]);
	mtree_int128* unionRight = mtree_int128_deep_copy(entries[rightIndex]);
	mtree_int128* current;

	for (OffsetNumber i = FirstOffsetNumber; i <= maxOffset;
		i = OffsetNumberNext(i)) {
		float distanceLeft = get_int128_distance(maxOffset, entries, distances, leftIndex, i - 1);
		float distanceRight = get_int128_distance(maxOffset, entries, distances, rightIndex, i - 1);
		current = entries[i - 1];

		if (distanceLeft < distanceRight) {
			if (distanceLeft + current->coveringRadius > unionLeft->coveringRadius) {
				unionLeft->coveringRadius = distanceLeft + current->coveringRadius;
			}
			*left = i;
			++left;
			++(vector->spl_nleft);
		}
		else {
			if (distanceRight + current->coveringRadius >
				unionRight->coveringRadius) {
				unionRight->coveringRadius = distanceRight + current->coveringRadius;
			}
			*right = i;
			++right;
			++(vector->spl_nright);
		}
	}

	vector->spl_ldatum = PointerGetDatum(unionLeft);
	vector->spl_rdatum = PointerGetDatum(unionRight);

	PG_RETURN_POINTER(vector);
}

Datum mtree_int128_compress(PG_FUNCTION_ARGS) {
	PG_RETURN_POINTER(PG_GETARG_POINTER(0));
}

Datum mtree_int128_decompress(PG_FUNCTION_ARGS) {
	PG_RETURN_POINTER(PG_GETARG_POINTER(0));
}

Datum mtree_int128_distance(PG_FUNCTION_ARGS) {
	GISTENTRY* entry = (GISTENTRY*)PG_GETARG_POINTER(0);
	mtree_int128* query = PG_GETARG_MTREE_INT128_P(1);
	mtree_int128* key = DatumGetMtreeInt128(entry->key);
	bool isLeaf = GistPageIsLeaf(entry->page);
	bool* recheck = (bool*)PG_GETARG_POINTER(4);

	PG_RETURN_FLOAT4((float4)mtree_int128_distance_internal(query, key));
}

Datum mtree_int128_distance_operator(PG_FUNCTION_ARGS) {
	mtree_int128* first = PG_GETARG_MTREE_INT128_P(0);
	mtree_int128* second = PG_GETARG_MTREE_INT128_P(1);

	PG_RETURN_FLOAT4((float4)mtree_int128_distance_internal(first, second));
}

Datum mtree_int128_overlap_operator(PG_FUNCTION_ARGS) {
	mtree_int128* first = PG_GETARG_MTREE_INT128_P(0);
	mtree_int128* second = PG_GETARG_MTREE_INT128_P(1);
	bool result = mtree_int128_contains_distance(first, second);

	PG_RETURN_BOOL(result);
}

Datum mtree_int128_contains_operator(PG_FUNCTION_ARGS) {
	mtree_int128* first = PG_GETARG_MTREE_INT128_P(0);
	mtree_int128* second = PG_GETARG_MTREE_INT128_P(1);
	bool result = mtree_int128_contains_distance(first, second);

	PG_RETURN_BOOL(result);
}

Datum mtree_int128_contained_operator(PG_FUNCTION_ARGS) {
	mtree_int128* first = PG_GETARG_MTREE_INT128_P(0);
	mtree_int128* second = PG_GETARG_MTREE_INT128_P(1);
	bool result = mtree_int128_contains_distance(second, first);

	PG_RETURN_BOOL(result);
}
