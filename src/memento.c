/*
 * ============================================================================
 *
 *        Memento filter 
 *          Autors:   ---
 *
 *        RSQF 
 *          Authors:  Prashant Pandey <ppandey@cs.stonybrook.edu>
 *                    Rob Johnson <robj@vmware.com>   
 *
 * ============================================================================
 */

/* 
 * IMPORTANT:
 *      The new/edited functions for Memento filter are annotated using
 *      comments of the form "NEW IN MEMENTO."
 */

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hashutil.h"
#include "memento.h"
#include "memento_int.h"

/******************************************************************
 * Code for managing the metadata bits and slots w/o interpreting *
 * the content of the slots.
 ******************************************************************/

#define MAX_VALUE(nbits) ((1ULL << (nbits)) - 1)
#define BITMASK(nbits)                                    \
  ((nbits) == 64 ? 0xffffffffffffffff : MAX_VALUE(nbits))
#define NUM_SLOTS_TO_LOCK (1ULL<<16)
#define CLUSTER_SIZE (1ULL<<14)
#define METADATA_WORD(qf,field,slot_index)                              \
  (get_block((qf), (slot_index) /                                       \
             QF_SLOTS_PER_BLOCK)->field[((slot_index)  % QF_SLOTS_PER_BLOCK) / 64])

#define GET_NO_LOCK(flag) (flag & QF_NO_LOCK)
#define GET_TRY_ONCE_LOCK(flag) (flag & QF_TRY_ONCE_LOCK)
#define GET_WAIT_FOR_LOCK(flag) (flag & QF_WAIT_FOR_LOCK)
#define GET_KEY_HASH(flag) (flag & QF_KEY_IS_HASH)

// NEW IN MEMENTO
#define GET_FINGERPRINT(qf, slot_index) (get_slot(qf, slot_index) >> qf->metadata->memento_bits)
// NEW IN MEMENTO
#define GET_MEMENTO(qf, slot_index) (get_slot(qf, slot_index) & BITMASK(qf->metadata->memento_bits))

// NEW IN MEMENTO
#define CMP_MASK_FINGERPRINT(a, b, mask) ((((a) ^ (b)) & (mask)) == 0)

/*
 * Writing the data into the slots of the filter efficiently is a challenge, as
 * doing it naively may cause many memory accesses. These macros aid in
 * efficiently writing data to these slots. They work by buffering the mementos
 * in machine words and flushing whenever the run out of space. They are
 * defined as macros because it was more convenient for me to have direct
 * access to the state of this buffering mechanism so that I could read and
 * write from it as I saw fit. They also handle all the nitty gritty details of
 * correctly writing the bits and overwriting the appropriate memory segments.
 * Furthermore, they don't pollute the namespace of the code using them, as
 * they place curly braces around the newly added segment of code.
 * The parameters are defined as follows:
 *      `qf`: The memento filter instance being operated on
 *      `data`: The buffered slots read from the filter
 *      `payload`: The buffered payload of data
 *      `filled_bits`: The number of bits already in use in `data`
 *      `bit_cnt`: The number of bits to read/write
 *      `bit_pos`: The bit offset of the current slot with respect to the start 
 *                 of the block
 *      `block_ind`: The block that the current slots is contained in
 * These parameters are literally variables defined in the calling function,
 * allowing the caller to have direct access to the state of the algorithm.
 */
// NEW IN MEMENTO
#define GET_NEXT_DATA_WORD_IF_EMPTY(qf, data, filled_bits, bit_cnt, bit_pos, block_ind) \
    { \
    while (filled_bits < (bit_cnt)) { \
        const uint64_t byte_pos = bit_pos / 8; \
        uint64_t *p = (uint64_t *)&get_block(qf, (block_ind))->slots[byte_pos]; \
        uint64_t tmp; \
        memcpy(&tmp, p, sizeof(tmp)); \
        tmp >>= (bit_pos % 8); \
        data |= tmp << filled_bits; \
        const uint64_t bits_per_block = QF_SLOTS_PER_BLOCK * qf->metadata->bits_per_slot; \
        const uint64_t move_amount = 64 - filled_bits - (bit_pos % 8); \
        filled_bits += move_amount; \
        bit_pos += move_amount; \
        if (bit_pos >= bits_per_block) { \
            filled_bits -= bit_pos - bits_per_block; \
            data &= BITMASK(filled_bits); \
            bit_pos = 0; \
            block_ind++; \
        } \
    } \
    }
// NEW IN MEMENTO
#define INIT_PAYLOAD_WORD(qf, payload, filled_bits, bit_pos, block_ind) \
    { \
    uint64_t byte_pos = bit_pos / 8; \
    uint64_t *p = (uint64_t *)&get_block(qf, (block_ind))->slots[byte_pos]; \
    memcpy(&payload, p, sizeof(payload)); \
    filled_bits = bit_pos % 8; \
    bit_pos -= bit_pos % 8; \
    }
// NEW IN MEMENTO
#define PRINT_WORD_BITS(word) \
    fprintf(stderr, "word: "); \
    for (int32_t i = 63; i >= 0; i--) \
        fprintf(stderr, "%lu", (word >> i) & 1); \
    fprintf(stderr, "\n");
// NEW IN MEMENTO
#define APPEND_WRITE_PAYLOAD_WORD(qf, payload, filled_bits, val, val_len, bit_pos, block_ind) \
    { \
    const uint64_t bits_per_block = QF_SLOTS_PER_BLOCK * qf->metadata->bits_per_slot; \
    const uint32_t max_filled_bits = (bits_per_block - bit_pos < 64 ? \
                                        bits_per_block - bit_pos : 64); \
    const uint32_t val_bit_cnt = (val_len); \
    if (filled_bits + val_bit_cnt > max_filled_bits) { \
        const uint64_t mask = BITMASK(max_filled_bits - filled_bits); \
        payload &= ~(mask << filled_bits); \
        payload |= (val & mask) << filled_bits; \
        filled_bits += val_bit_cnt; \
        filled_bits -= max_filled_bits; \
        uint64_t byte_pos = bit_pos / 8; \
        uint64_t *p = (uint64_t *)&get_block(qf, (block_ind))->slots[byte_pos]; \
        memcpy(p, &payload, sizeof(payload)); \
        bit_pos += max_filled_bits; \
        if (bit_pos >= bits_per_block) { \
            bit_pos = 0; \
            block_ind++; \
        } \
        p = (uint64_t *)&get_block(qf, (block_ind))->slots[bit_pos / 8]; \
        memcpy(&payload, p, sizeof(payload)); \
        payload &= ~BITMASK(filled_bits); \
        payload |= val >> (val_bit_cnt - filled_bits); \
    } \
    else { \
        payload &= ~(BITMASK(val_bit_cnt) << filled_bits); \
        payload |= val << filled_bits; \
        filled_bits += val_bit_cnt; \
    } \
    }
// NEW IN MEMENTO
#define FLUSH_PAYLOAD_WORD(qf, payload, filled_bits, bit_pos, block_ind) \
    { \
    uint64_t byte_pos = bit_pos / 8; \
    uint64_t *p = (uint64_t *)&get_block(qf, (block_ind))->slots[byte_pos]; \
    memcpy(p, &payload, sizeof(payload)); \
    }

#define DISTANCE_FROM_HOME_SLOT_CUTOFF 1000
#define BILLION 1000000000L

#ifdef DEBUG
#define PRINT_DEBUG 1
#else
#define PRINT_DEBUG 0
#endif

#define DEBUG_CQF(fmt, ...) \
	do { if (PRINT_DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

#define DEBUG_DUMP(qf) \
	do { if (PRINT_DEBUG) qf_dump_metadata(qf); } while (0)

#ifdef LOG_WAIT_TIME
static inline bool qf_spin_lock(QF *qf, volatile int *lock, uint64_t idx,
																uint8_t flag)
{
	struct timespec start, end;
	bool ret;

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	if (GET_WAIT_FOR_LOCK(flag) != QF_WAIT_FOR_LOCK) {
		ret = !__sync_lock_test_and_set(lock, 1);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
		qf->runtimedata->wait_times[idx].locks_acquired_single_attempt++;
		qf->runtimedata->wait_times[idx].total_time_single += BILLION * (end.tv_sec -
																												start.tv_sec) +
			end.tv_nsec - start.tv_nsec;
	} else {
		if (!__sync_lock_test_and_set(lock, 1)) {
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
			qf->runtimedata->wait_times[idx].locks_acquired_single_attempt++;
			qf->runtimedata->wait_times[idx].total_time_single += BILLION * (end.tv_sec -
																													start.tv_sec) +
			end.tv_nsec - start.tv_nsec;
		} else {
			while (__sync_lock_test_and_set(lock, 1))
				while (*lock);
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
			qf->runtimedata->wait_times[idx].total_time_spinning += BILLION * (end.tv_sec -
																														start.tv_sec) +
				end.tv_nsec - start.tv_nsec;
		}
		ret = true;
	}
	qf->runtimedata->wait_times[idx].locks_taken++;

	return ret;

	/*start = rdtsc();*/
	/*if (!__sync_lock_test_and_set(lock, 1)) {*/
		/*clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);*/
		/*qf->runtimedata->wait_times[idx].locks_acquired_single_attempt++;*/
		/*qf->runtimedata->wait_times[idx].total_time_single += BILLION * (end.tv_sec -
		 * start.tv_sec) + end.tv_nsec - start.tv_nsec;*/
	/*} else {*/
		/*while (__sync_lock_test_and_set(lock, 1))*/
			/*while (*lock);*/
		/*clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);*/
		/*qf->runtimedata->wait_times[idx].total_time_spinning += BILLION * (end.tv_sec -
		 * start.tv_sec) + end.tv_nsec - start.tv_nsec;*/
	/*}*/

	/*end = rdtsc();*/
	/*qf->runtimedata->wait_times[idx].locks_taken++;*/
	/*return;*/
}
#else

// NEW IN MEMENTO
__attribute__((always_inline))
static inline uint32_t fast_reduce(uint32_t hash, uint32_t n) {
    // A fast replacement for modulo operations. Credit to Daniel Lemiere.
    // See http://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
    return (uint32_t) (((uint64_t) hash * n) >> 32);
}

/**
 * Try to acquire a lock once and return even if the lock is busy.
 * If spin flag is set, then spin until the lock is available.
 */
static inline bool qf_spin_lock(volatile int *lock, uint8_t flag)
{
	if (GET_WAIT_FOR_LOCK(flag) != QF_WAIT_FOR_LOCK) {
		return !__sync_lock_test_and_set(lock, 1);
	} else {
		while (__sync_lock_test_and_set(lock, 1))
			while (*lock);
		return true;
	}

	return false;
}
#endif

static inline void qf_spin_unlock(volatile int *lock)
{
	__sync_lock_release(lock);
	return;
}

static bool qf_lock(QF *qf, uint64_t hash_bucket_index, bool small, uint8_t
										runtime_lock)
{
	uint64_t hash_bucket_lock_offset  = hash_bucket_index % NUM_SLOTS_TO_LOCK;
	if (small) {
#ifdef LOG_WAIT_TIME
		if (!qf_spin_lock(qf, &qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK],
											hash_bucket_index/NUM_SLOTS_TO_LOCK,
											runtime_lock))
			return false;
		if (NUM_SLOTS_TO_LOCK - hash_bucket_lock_offset <= CLUSTER_SIZE) {
			if (!qf_spin_lock(qf, &qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK+1],
												hash_bucket_index/NUM_SLOTS_TO_LOCK+1,
												runtime_lock)) {
				qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK]);
				return false;
			}
		}
#else
		if (!qf_spin_lock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK],
											runtime_lock))
			return false;
		if (NUM_SLOTS_TO_LOCK - hash_bucket_lock_offset <= CLUSTER_SIZE) {
			if (!qf_spin_lock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK+1],
												runtime_lock)) {
				qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK]);
				return false;
			}
		}
#endif
	} else {
#ifdef LOG_WAIT_TIME
		if (hash_bucket_index >= NUM_SLOTS_TO_LOCK && hash_bucket_lock_offset <=
				CLUSTER_SIZE) {
			if (!qf_spin_lock(qf,
												&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK-1],
												runtime_lock))
				return false;
		}
		if (!qf_spin_lock(qf,
											&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK],
											runtime_lock)) {
			if (hash_bucket_index >= NUM_SLOTS_TO_LOCK && hash_bucket_lock_offset <=
					CLUSTER_SIZE)
				qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK-1]);
			return false;
		}
		if (!qf_spin_lock(qf, &qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK+1],
											runtime_lock)) {
			qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK]);
			if (hash_bucket_index >= NUM_SLOTS_TO_LOCK && hash_bucket_lock_offset <=
					CLUSTER_SIZE)
				qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK-1]);
			return false;
		}
#else
		if (hash_bucket_index >= NUM_SLOTS_TO_LOCK && hash_bucket_lock_offset <=
				CLUSTER_SIZE) {
			if
				(!qf_spin_lock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK-1],
											 runtime_lock))
				return false;
		}
		if (!qf_spin_lock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK],
											runtime_lock)) {
			if (hash_bucket_index >= NUM_SLOTS_TO_LOCK && hash_bucket_lock_offset <=
					CLUSTER_SIZE)
				qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK-1]);
			return false;
		}
		if (!qf_spin_lock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK+1],
											runtime_lock)) {
			qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK]);
			if (hash_bucket_index >= NUM_SLOTS_TO_LOCK && hash_bucket_lock_offset <=
					CLUSTER_SIZE)
				qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK-1]);
			return false;
		}
#endif
	}
	return true;
}

static void qf_unlock(QF *qf, uint64_t hash_bucket_index, bool small)
{
	uint64_t hash_bucket_lock_offset  = hash_bucket_index % NUM_SLOTS_TO_LOCK;
	if (small) {
		if (NUM_SLOTS_TO_LOCK - hash_bucket_lock_offset <= CLUSTER_SIZE) {
			qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK+1]);
		}
		qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK]);
	} else {
		qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK+1]);
		qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK]);
		if (hash_bucket_index >= NUM_SLOTS_TO_LOCK && hash_bucket_lock_offset <=
				CLUSTER_SIZE)
			qf_spin_unlock(&qf->runtimedata->locks[hash_bucket_index/NUM_SLOTS_TO_LOCK-1]);
	}
}

static void modify_metadata(QF *qf, uint64_t *metadata, int cnt)
{
#ifdef LOG_WAIT_TIME
	qf_spin_lock(qf, &qf->runtimedata->metadata_lock,
							 qf->runtimedata->num_locks, QF_WAIT_FOR_LOCK);
#else
	qf_spin_lock(&qf->runtimedata->metadata_lock, QF_WAIT_FOR_LOCK);
#endif
	*metadata = *metadata + cnt;
	qf_spin_unlock(&qf->runtimedata->metadata_lock);
	return;
}

static inline int popcnt(uint64_t val)
{
	asm("popcnt %[val], %[val]"
			: [val] "+r" (val)
			:
			: "cc");
	return val;
}

static inline int64_t bitscanreverse(uint64_t val)
{
	if (val == 0) {
		return -1;
	} else {
		asm("bsr %[val], %[val]"
				: [val] "+r" (val)
				:
				: "cc");
		return val;
	}
}

static inline int popcntv(const uint64_t val, int ignore)
{
	if (ignore % 64)
		return popcnt (val & ~BITMASK(ignore % 64));
	else
		return popcnt(val);
}

// Returns the number of 1s up to (and including) the pos'th bit
// Bits are numbered from 0
static inline int bitrank(uint64_t val, int pos) {
	val = val & ((2ULL << pos) - 1);
	asm("popcnt %[val], %[val]"
			: [val] "+r" (val)
			:
			: "cc");
	return val;
}

/**
 * Returns the position of the k-th 1 in the 64-bit word x.
 * k is 0-based, so k=0 returns the position of the first 1.
 *
 * Uses the broadword selection algorithm by Vigna [1], improved by Gog
 * and Petri [2] and Vigna [3].
 *
 * [1] Sebastiano Vigna. Broadword Implementation of Rank/Select
 *    Queries. WEA, 2008
 *
 * [2] Simon Gog, Matthias Petri. Optimized succinct data
 * structures for massive data. Softw. Pract. Exper., 2014
 *
 * [3] Sebastiano Vigna. MG4J 5.2.1. http://mg4j.di.unimi.it/
 * The following code is taken from
 * https://github.com/facebook/folly/blob/b28186247104f8b90cfbe094d289c91f9e413317/folly/experimental/Select64.h
 */
const uint8_t kSelectInByte[2048] = {
	8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0,
	1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0,
	2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0,
	1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0,
	3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7, 0,
	1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0,
	2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0,
	1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0,
	1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 8, 8, 8, 1,
	8, 2, 2, 1, 8, 3, 3, 1, 3, 2, 2, 1, 8, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2,
	2, 1, 8, 5, 5, 1, 5, 2, 2, 1, 5, 3, 3, 1, 3, 2, 2, 1, 5, 4, 4, 1, 4, 2, 2, 1,
	4, 3, 3, 1, 3, 2, 2, 1, 8, 6, 6, 1, 6, 2, 2, 1, 6, 3, 3, 1, 3, 2, 2, 1, 6, 4,
	4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1, 6, 5, 5, 1, 5, 2, 2, 1, 5, 3, 3, 1,
	3, 2, 2, 1, 5, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1, 8, 7, 7, 1, 7, 2,
	2, 1, 7, 3, 3, 1, 3, 2, 2, 1, 7, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1,
	7, 5, 5, 1, 5, 2, 2, 1, 5, 3, 3, 1, 3, 2, 2, 1, 5, 4, 4, 1, 4, 2, 2, 1, 4, 3,
	3, 1, 3, 2, 2, 1, 7, 6, 6, 1, 6, 2, 2, 1, 6, 3, 3, 1, 3, 2, 2, 1, 6, 4, 4, 1,
	4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1, 6, 5, 5, 1, 5, 2, 2, 1, 5, 3, 3, 1, 3, 2,
	2, 1, 5, 4, 4, 1, 4, 2, 2, 1, 4, 3, 3, 1, 3, 2, 2, 1, 8, 8, 8, 8, 8, 8, 8, 2,
	8, 8, 8, 3, 8, 3, 3, 2, 8, 8, 8, 4, 8, 4, 4, 2, 8, 4, 4, 3, 4, 3, 3, 2, 8, 8,
	8, 5, 8, 5, 5, 2, 8, 5, 5, 3, 5, 3, 3, 2, 8, 5, 5, 4, 5, 4, 4, 2, 5, 4, 4, 3,
	4, 3, 3, 2, 8, 8, 8, 6, 8, 6, 6, 2, 8, 6, 6, 3, 6, 3, 3, 2, 8, 6, 6, 4, 6, 4,
	4, 2, 6, 4, 4, 3, 4, 3, 3, 2, 8, 6, 6, 5, 6, 5, 5, 2, 6, 5, 5, 3, 5, 3, 3, 2,
	6, 5, 5, 4, 5, 4, 4, 2, 5, 4, 4, 3, 4, 3, 3, 2, 8, 8, 8, 7, 8, 7, 7, 2, 8, 7,
	7, 3, 7, 3, 3, 2, 8, 7, 7, 4, 7, 4, 4, 2, 7, 4, 4, 3, 4, 3, 3, 2, 8, 7, 7, 5,
	7, 5, 5, 2, 7, 5, 5, 3, 5, 3, 3, 2, 7, 5, 5, 4, 5, 4, 4, 2, 5, 4, 4, 3, 4, 3,
	3, 2, 8, 7, 7, 6, 7, 6, 6, 2, 7, 6, 6, 3, 6, 3, 3, 2, 7, 6, 6, 4, 6, 4, 4, 2,
	6, 4, 4, 3, 4, 3, 3, 2, 7, 6, 6, 5, 6, 5, 5, 2, 6, 5, 5, 3, 5, 3, 3, 2, 6, 5,
	5, 4, 5, 4, 4, 2, 5, 4, 4, 3, 4, 3, 3, 2, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 3, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 4, 8, 4, 4, 3, 8, 8, 8, 8, 8, 8,
	8, 5, 8, 8, 8, 5, 8, 5, 5, 3, 8, 8, 8, 5, 8, 5, 5, 4, 8, 5, 5, 4, 5, 4, 4, 3,
	8, 8, 8, 8, 8, 8, 8, 6, 8, 8, 8, 6, 8, 6, 6, 3, 8, 8, 8, 6, 8, 6, 6, 4, 8, 6,
	6, 4, 6, 4, 4, 3, 8, 8, 8, 6, 8, 6, 6, 5, 8, 6, 6, 5, 6, 5, 5, 3, 8, 6, 6, 5,
	6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 7, 8, 7,
	7, 3, 8, 8, 8, 7, 8, 7, 7, 4, 8, 7, 7, 4, 7, 4, 4, 3, 8, 8, 8, 7, 8, 7, 7, 5,
	8, 7, 7, 5, 7, 5, 5, 3, 8, 7, 7, 5, 7, 5, 5, 4, 7, 5, 5, 4, 5, 4, 4, 3, 8, 8,
	8, 7, 8, 7, 7, 6, 8, 7, 7, 6, 7, 6, 6, 3, 8, 7, 7, 6, 7, 6, 6, 4, 7, 6, 6, 4,
	6, 4, 4, 3, 8, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 3, 7, 6, 6, 5, 6, 5,
	5, 4, 6, 5, 5, 4, 5, 4, 4, 3, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 5, 8, 8, 8, 8, 8, 8, 8, 5, 8, 8, 8, 5, 8, 5, 5, 4, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 6, 8, 8, 8, 8, 8, 8, 8, 6, 8, 8, 8, 6, 8, 6,
	6, 4, 8, 8, 8, 8, 8, 8, 8, 6, 8, 8, 8, 6, 8, 6, 6, 5, 8, 8, 8, 6, 8, 6, 6, 5,
	8, 6, 6, 5, 6, 5, 5, 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8,
	8, 8, 8, 8, 8, 7, 8, 8, 8, 7, 8, 7, 7, 4, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 7,
	8, 7, 7, 5, 8, 8, 8, 7, 8, 7, 7, 5, 8, 7, 7, 5, 7, 5, 5, 4, 8, 8, 8, 8, 8, 8,
	8, 7, 8, 8, 8, 7, 8, 7, 7, 6, 8, 8, 8, 7, 8, 7, 7, 6, 8, 7, 7, 6, 7, 6, 6, 4,
	8, 8, 8, 7, 8, 7, 7, 6, 8, 7, 7, 6, 7, 6, 6, 5, 8, 7, 7, 6, 7, 6, 6, 5, 7, 6,
	6, 5, 6, 5, 5, 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 5, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 6, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 6, 8, 8, 8, 8, 8, 8, 8, 6, 8, 8, 8, 6,
	8, 6, 6, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7,
	8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 7, 8, 7, 7, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 7, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 7, 8, 7, 7, 6, 8, 8, 8, 8,
	8, 8, 8, 7, 8, 8, 8, 7, 8, 7, 7, 6, 8, 8, 8, 7, 8, 7, 7, 6, 8, 7, 7, 6, 7, 6,
	6, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 6,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 7, 8, 7, 7, 6, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7
};

static inline uint64_t _select64(uint64_t x, int k)
{
	if (k >= popcnt(x)) { return 64; }

	const uint64_t kOnesStep4  = 0x1111111111111111ULL;
	const uint64_t kOnesStep8  = 0x0101010101010101ULL;
	const uint64_t kMSBsStep8  = 0x80ULL * kOnesStep8;

	uint64_t s = x;
	s = s - ((s & 0xA * kOnesStep4) >> 1);
	s = (s & 0x3 * kOnesStep4) + ((s >> 2) & 0x3 * kOnesStep4);
	s = (s + (s >> 4)) & 0xF * kOnesStep8;
	uint64_t byteSums = s * kOnesStep8;

	uint64_t kStep8 = k * kOnesStep8;
	uint64_t geqKStep8 = (((kStep8 | kMSBsStep8) - byteSums) & kMSBsStep8);
	uint64_t place = popcnt(geqKStep8) * 8;
	uint64_t byteRank = k - (((byteSums << 8) >> place) & (uint64_t)(0xFF));
	return place + kSelectInByte[((x >> place) & 0xFF) | (byteRank << 8)];
}

// Returns the position of the rank'th 1.  (rank = 0 returns the 1st 1)
// Returns 64 if there are fewer than rank+1 1s.
static inline uint64_t bitselect(uint64_t val, int rank) {
#ifdef __SSE4_2_
	uint64_t i = 1ULL << rank;
	asm("pdep %[val], %[mask], %[val]"
			: [val] "+r" (val)
			: [mask] "r" (i));
	asm("tzcnt %[bit], %[index]"
			: [index] "=r" (i)
			: [bit] "g" (val)
			: "cc");
	return i;
#endif
	return _select64(val, rank);
}

// Returns the position of the lowbit of val.
// Returns 64 if there are zero set bits.
static inline uint64_t lowbit_position(uint64_t val) {  // NEW IN MEMENTO
#ifdef __SSE4_2_
	uint64_t i = 1ULL;
	asm("tzcnt %[bit], %[index]"
			: [index] "=r" (i)
			: [bit] "g" (val)
			: "cc");
	return i;
#endif
	return _select64(val, 0);
}

// Returns the position of the highbit of val.
// Returns 0 if there are zero set bits.
static inline uint64_t highbit_position(uint64_t val) { // NEW IN MEMENTO
#ifdef __SSE4_2_
	uint64_t i = 1ULL;
	asm("lzcnt %[bit], %[index]"
			: [index] "=r" (i)
			: [bit] "g" (val)
			: "cc");
	return 64 - i - 1;
#endif
    uint64_t res;
    for (res = -1; val > 0; val >>= 1)
        res++;
    return res;
}

static inline uint64_t bitselectv(const uint64_t val, int ignore, int rank)
{
	return bitselect(val & ~BITMASK(ignore % 64), rank);
}

#if QF_BITS_PER_SLOT > 0
static inline qfblock *get_block(const QF *qf, uint64_t block_index)
{
	return &qf->blocks[block_index];
}
#else
static inline qfblock *get_block(const QF *qf, uint64_t block_index)
{
	return (qfblock *)(((char *)qf->blocks) + block_index * (sizeof(qfblock) +
						QF_SLOTS_PER_BLOCK * qf->metadata->bits_per_slot / 8));
}
#endif

static inline int is_runend(const QF *qf, uint64_t index)
{
	return (METADATA_WORD(qf, runends, index) >> ((index % QF_SLOTS_PER_BLOCK) %
                                                                64)) & 1ULL;
}

static inline int is_occupied(const QF *qf, uint64_t index)
{
	return (METADATA_WORD(qf, occupieds, index) >> ((index % QF_SLOTS_PER_BLOCK) %
                                                                64)) & 1ULL;
}

#if QF_BITS_PER_SLOT == 8 || QF_BITS_PER_SLOT == 16 || QF_BITS_PER_SLOT == 32 || QF_BITS_PER_SLOT == 64

static inline uint64_t get_slot(const QF *qf, uint64_t index)
{
	assert(index < qf->metadata->xnslots);
	return get_block(qf, index / QF_SLOTS_PER_BLOCK)->slots[index % QF_SLOTS_PER_BLOCK];
}

static inline void set_slot(const QF *qf, uint64_t index, uint64_t value)
{
	assert(index < qf->metadata->xnslots);
	get_block(qf, index / QF_SLOTS_PER_BLOCK)->slots[index % QF_SLOTS_PER_BLOCK] =
		value & BITMASK(qf->metadata->bits_per_slot);
}

#elif QF_BITS_PER_SLOT > 0

/* Little-endian code ....  Big-endian is TODO */

static inline uint64_t get_slot(const QF *qf, uint64_t index)
{
	/* Should use __uint128_t to support up to 64-bit remainders, but gcc seems
	 * to generate buggy code.  :/  */
	assert(index < qf->metadata->xnslots);
    uint64_t *p = (uint64_t *)&get_block(qf, index /
            QF_SLOTS_PER_BLOCK)->slots[(index %
                QF_SLOTS_PER_BLOCK)
            * QF_BITS_PER_SLOT / 8];
    return (uint64_t)(((*p) >> (((index % QF_SLOTS_PER_BLOCK) * QF_BITS_PER_SLOT) %
                    8)) & BITMASK(QF_BITS_PER_SLOT));
}

static inline void set_slot(const QF *qf, uint64_t index, uint64_t value)
{
	/* Should use __uint128_t to support up to 64-bit remainders, but gcc seems
	 * to generate buggy code.  :/  */
	assert(index < qf->metadata->xnslots);
    uint64_t *p = (uint64_t *)&get_block(qf, index /
            QF_SLOTS_PER_BLOCK)->slots[(index %
                QF_SLOTS_PER_BLOCK)
            * QF_BITS_PER_SLOT / 8];
	uint64_t t = *p;
	uint64_t mask = BITMASK(QF_BITS_PER_SLOT);
	uint64_t v = value;
	int shift = ((index % QF_SLOTS_PER_BLOCK) * QF_BITS_PER_SLOT) % 8;
	mask <<= shift;
	v <<= shift;
	t &= ~mask;
	t |= v;
	*p = t;
}

#else

/* Little-endian code ....  Big-endian is TODO */

static inline uint64_t get_slot(const QF *qf, uint64_t index)
{
	assert(index < qf->metadata->xnslots);
	/* Should use __uint128_t to support up to 64-bit remainders, but gcc seems
	 * to generate buggy code.  :/  */
    uint64_t *p = (uint64_t *)&get_block(qf, index /
            QF_SLOTS_PER_BLOCK)->slots[(index %
                QF_SLOTS_PER_BLOCK)
            * qf->metadata->bits_per_slot / 8];
    // you cannot just do *p to get the value, undefined behavior
    uint64_t pvalue;
    memcpy(&pvalue,p,sizeof(pvalue));
    return (uint64_t)((pvalue >> (((index % QF_SLOTS_PER_BLOCK) *
                        qf->metadata->bits_per_slot) % 8)) &
            BITMASK(qf->metadata->bits_per_slot));
}

static inline void set_slot(const QF *qf, uint64_t index, uint64_t value)
{
	assert(index < qf->metadata->xnslots);
	/* Should use __uint128_t to support up to 64-bit remainders, but gcc seems
	 * to generate buggy code.  :/  */
	uint64_t *p = (uint64_t *) &get_block(qf, index / QF_SLOTS_PER_BLOCK)
                                ->slots[(index % QF_SLOTS_PER_BLOCK) 
                                        * qf->metadata->bits_per_slot / 8];
	// This is undefined:
	//uint64_t t = *p;
	uint64_t t;
	memcpy(&t,p,sizeof(t));
	uint64_t mask = BITMASK(qf->metadata->bits_per_slot);
	uint64_t v = value;
	int shift = ((index % QF_SLOTS_PER_BLOCK) * qf->metadata->bits_per_slot) % 8;
	mask <<= shift;
	v <<= shift;
	t &= ~mask;
	t |= v;
	// this is undefined
	//*p = t;
	memcpy(p, &t, sizeof(t));
}

#endif

static inline uint64_t run_end(const QF *qf, uint64_t hash_bucket_index);

static inline uint64_t block_offset(const QF *qf, uint64_t blockidx)
{
	/* If we have extended counters and a 16-bit (or larger) offset
		 field, then we can safely ignore the possibility of overflowing
		 that field. */
	if (sizeof(qf->blocks[0].offset) > 1 ||
			get_block(qf, blockidx)->offset < BITMASK(8*sizeof(qf->blocks[0].offset)))
		return get_block(qf, blockidx)->offset;

	return run_end(qf, QF_SLOTS_PER_BLOCK * blockidx - 1) - QF_SLOTS_PER_BLOCK *
		blockidx + 1;
}

static inline uint64_t run_end(const QF *qf, uint64_t hash_bucket_index)
{
	uint64_t bucket_block_index = hash_bucket_index / QF_SLOTS_PER_BLOCK;
	uint64_t bucket_intrablock_offset = hash_bucket_index % QF_SLOTS_PER_BLOCK;
	uint64_t bucket_blocks_offset = block_offset(qf, bucket_block_index);

    uint64_t bucket_intrablock_rank = bitrank(get_block(qf, bucket_block_index)->occupieds[0],
                                                bucket_intrablock_offset);

	if (bucket_intrablock_rank == 0) {
		if (bucket_blocks_offset <= bucket_intrablock_offset)
			return hash_bucket_index;
		else
			return QF_SLOTS_PER_BLOCK * bucket_block_index + bucket_blocks_offset - 1;
	}

	uint64_t runend_block_index  = bucket_block_index + bucket_blocks_offset /
                                                            QF_SLOTS_PER_BLOCK;
	uint64_t runend_ignore_bits  = bucket_blocks_offset % QF_SLOTS_PER_BLOCK;
	uint64_t runend_rank         = bucket_intrablock_rank - 1;
    uint64_t runend_block_offset = bitselectv(get_block(qf, runend_block_index)->runends[0],
                                                        runend_ignore_bits, runend_rank);
	if (runend_block_offset == QF_SLOTS_PER_BLOCK) {
        if (bucket_blocks_offset == 0 && bucket_intrablock_rank == 0) {
            /* The block begins in empty space, and this bucket is in that region of
             * empty space */
            return hash_bucket_index;
        } else {
            do {
                runend_rank -= popcntv(get_block(qf, runend_block_index)->runends[0],
                                        runend_ignore_bits);
                runend_block_index++;
                runend_ignore_bits  = 0;
                runend_block_offset = bitselectv(get_block(qf, runend_block_index)->runends[0],
                                                runend_ignore_bits, runend_rank);
            } while (runend_block_offset == QF_SLOTS_PER_BLOCK);
        }
    }

    uint64_t runend_index = QF_SLOTS_PER_BLOCK * runend_block_index +
        runend_block_offset;
    if (runend_index < hash_bucket_index)
        return hash_bucket_index;
    else
		return runend_index;
}

static inline int offset_lower_bound(const QF *qf, uint64_t slot_index)
{
	const qfblock *b = get_block(qf, slot_index / QF_SLOTS_PER_BLOCK);
	const uint64_t slot_offset = slot_index % QF_SLOTS_PER_BLOCK;
	const uint64_t boffset = b->offset;
	const uint64_t occupieds = b->occupieds[0] & BITMASK(slot_offset+1);
	assert(QF_SLOTS_PER_BLOCK == 64);
	if (boffset <= slot_offset) {
		const uint64_t runends = (b->runends[0] & BITMASK(slot_offset)) >> boffset;
		return popcnt(occupieds) - popcnt(runends);
	}
	return boffset - slot_offset + popcnt(occupieds);
}

static inline int qf_is_empty(const QF *qf, uint64_t slot_index)
{
	return offset_lower_bound(qf, slot_index) == 0;
}

static inline int might_be_empty(const QF *qf, uint64_t slot_index)
{
	return !is_occupied(qf, slot_index)
		&& !is_runend(qf, slot_index);
}

static inline int probably_is_empty(const QF *qf, uint64_t slot_index)
{
	return get_slot(qf, slot_index) == 0
		&& !is_occupied(qf, slot_index)
		&& !is_runend(qf, slot_index);
}

static inline uint64_t find_first_empty_slot(QF *qf, uint64_t from)
{
	do {
		int t = offset_lower_bound(qf, from);
		assert(t>=0);
		if (t == 0)
			break;
		from = from + t;
	} while(1);
    assert(!is_occupied(qf, from) && !is_runend(qf, from));
	return from;
}

// Resulting value is at most 64, since that is enough to store all mementos.
static inline uint64_t get_number_of_consecutive_empty_slots(QF *qf, uint64_t first_empty,
        uint64_t goal_slots)    // NEW IN MEMENTO
{
#ifdef DEBUG
    fprintf(stderr, "GETTING NUMBER OF CONSECUTIVE EMPTY SLOTS FROM %lu\n", first_empty);
#endif /* DEBUG */
    uint64_t inter_block_offset = first_empty % QF_SLOTS_PER_BLOCK;
    uint64_t occupieds = METADATA_WORD(qf, occupieds, first_empty) & (~BITMASK(inter_block_offset));
    
    uint64_t res = 0;
    while (true) {
        uint64_t empty_bits = lowbit_position(occupieds);
        res += empty_bits - inter_block_offset;
#ifdef DEBUG
        fprintf(stderr, "OCCUPIEDS=%lu - EMPTY_BITS=%lu - RES=%lu\n", occupieds, empty_bits, res);
#endif /* DEBUG */

        if (empty_bits < 64 || res >= goal_slots)
            break;

        inter_block_offset = 0;
        first_empty += QF_SLOTS_PER_BLOCK - first_empty % QF_SLOTS_PER_BLOCK;
        occupieds = METADATA_WORD(qf, occupieds, first_empty);
    }
    return res < goal_slots ? res : goal_slots;
}

static inline uint64_t shift_into_b(const uint64_t a, const uint64_t b,
                                    const int bstart, const int bend,
                                    const int amount)
{
	const uint64_t a_component = bstart == 0 ? (a >> (64 - amount)) : 0;
	const uint64_t b_shifted_mask = BITMASK(bend - bstart) << bstart;
	const uint64_t b_shifted = ((b_shifted_mask & b) << amount) & b_shifted_mask;
	const uint64_t b_mask = ~b_shifted_mask;
	return a_component | b_shifted | (b & b_mask);
}

#if QF_BITS_PER_SLOT == 8 || QF_BITS_PER_SLOT == 16 || QF_BITS_PER_SLOT == 32 || QF_BITS_PER_SLOT == 64

static inline void shift_remainders(QF *qf, uint64_t start_index, uint64_t
																		empty_index)
{
	uint64_t start_block  = start_index / QF_SLOTS_PER_BLOCK;
	uint64_t start_offset = start_index % QF_SLOTS_PER_BLOCK;
	uint64_t empty_block  = empty_index / QF_SLOTS_PER_BLOCK;
	uint64_t empty_offset = empty_index % QF_SLOTS_PER_BLOCK;

	assert (start_index <= empty_index && empty_index < qf->metadata->xnslots);

	while (start_block < empty_block) {
		memmove(&get_block(qf, empty_block)->slots[1],
						&get_block(qf, empty_block)->slots[0],
						empty_offset * sizeof(qf->blocks[0].slots[0]));
		get_block(qf, empty_block)->slots[0] = get_block(qf,
																			empty_block-1)->slots[QF_SLOTS_PER_BLOCK-1];
		empty_block--;
		empty_offset = QF_SLOTS_PER_BLOCK-1;
	}

	memmove(&get_block(qf, empty_block)->slots[start_offset+1],
					&get_block(qf, empty_block)->slots[start_offset],
					(empty_offset - start_offset) * sizeof(qf->blocks[0].slots[0]));
}

#else

#define REMAINDER_WORD(qf, i) ((uint64_t *)&(get_block(qf, (i)/qf->metadata->bits_per_slot)->slots[8 * ((i) % qf->metadata->bits_per_slot)]))

static inline void shift_remainders(QF *qf, const uint64_t start_index, 
        const uint64_t empty_index)
{
	uint64_t last_word = (empty_index + 1) * qf->metadata->bits_per_slot / 64;
	const uint64_t first_word = start_index * qf->metadata->bits_per_slot / 64;
	int bend = ((empty_index + 1) * qf->metadata->bits_per_slot) % 64;
	const int bstart = (start_index * qf->metadata->bits_per_slot) % 64;

    assert(first_word <= last_word);
	while (last_word != first_word) {
		*REMAINDER_WORD(qf, last_word) = shift_into_b(*REMAINDER_WORD(qf, last_word-1),
                                                      *REMAINDER_WORD(qf, last_word),
                                                       0, bend, qf->metadata->bits_per_slot);
		last_word--;
		bend = 64;
	}
    *REMAINDER_WORD(qf, last_word) = shift_into_b(0, *REMAINDER_WORD(qf, last_word),
            bstart, bend, qf->metadata->bits_per_slot);
}

#endif

void qf_dump_block(const QF *qf, uint64_t i)
{
	uint64_t j;

	printf("%-192d", get_block(qf, i)->offset);
	printf("\n");

	for (j = 0; j < QF_SLOTS_PER_BLOCK; j++) {
        for (int k = 0; k < qf->metadata->bits_per_slot - 2; k++)
            printf(" ");
		printf("%02lx ", j);
    }
	printf("\n");

	for (j = 0; j < QF_SLOTS_PER_BLOCK; j++) {
        for (int k = 0; k < qf->metadata->bits_per_slot - 1; k++)
            printf(" ");
		printf("%d ", (get_block(qf, i)->occupieds[j/64] & (1ULL << (j%64))) ? 1 : 0);
    }
	printf("\npopcnt=%d\n", popcnt(get_block(qf, i)->occupieds[0]));
    puts("______________________________________________________________________");

	for (j = 0; j < QF_SLOTS_PER_BLOCK; j++) {
        for (int k = 0; k < qf->metadata->bits_per_slot - 1; k++)
            printf(" ");
		printf("%d ", (get_block(qf, i)->runends[j/64] & (1ULL << (j%64))) ? 1 : 0);
    }
	printf("\npopcnt=%d\n", popcnt(get_block(qf, i)->runends[0]));
    puts("______________________________________________________________________");

#if QF_BITS_PER_SLOT == 8 || QF_BITS_PER_SLOT == 16 || QF_BITS_PER_SLOT == 32
	for (j = 0; j < QF_SLOTS_PER_BLOCK; j++)
		printf("%02x ", get_block(qf, i)->slots[j]);
#elif QF_BITS_PER_SLOT == 64
	for (j = 0; j < QF_SLOTS_PER_BLOCK; j++)
		printf("%02lx ", get_block(qf, i)->slots[j]);
#else
	//for (j = 0; j < QF_SLOTS_PER_BLOCK * qf->metadata->bits_per_slot / 8; j++)
	//	printf("%02x ", get_block(qf, i)->slots[j]);
	for (j = 0; j < QF_SLOTS_PER_BLOCK; j++) {
        uint64_t ind = i * QF_SLOTS_PER_BLOCK + j;
        if (ind >= qf->metadata->xnslots)
            break;
        uint64_t current_slot = get_slot(qf, ind);
        for (int k = qf->metadata->bits_per_slot - 1; k >= 0; k--)
            printf("%lu", (current_slot >> k) & 1);
        printf(" ");
    }
#endif

	printf("\n");

	printf("\n");
}

void qf_dump_metadata(const QF *qf) {
	printf("Slots: %lu Occupied: %lu Elements: %lu Distinct: %lu\n",
				 qf->metadata->nslots,
				 qf->metadata->noccupied_slots,
				 qf->metadata->nelts,
				 qf->metadata->ndistinct_elts);
	printf("Key_bits: %lu Memento_bits: %lu Fingerprint_bits: %lu Bits_per_slot: %lu\n",
				 qf->metadata->key_bits,
				 qf->metadata->memento_bits,
				 qf->metadata->fingerprint_bits,
				 qf->metadata->bits_per_slot);
}

void qf_dump(const QF *qf)
{
	uint64_t i;

	printf("%lu %lu %lu\n",
				 qf->metadata->nblocks,
				 qf->metadata->ndistinct_elts,
				 qf->metadata->nelts);

	for (i = 0; i < qf->metadata->nblocks; i++) {
		qf_dump_block(qf, i);
	}
}

static inline void find_next_n_empty_slots(QF *qf, uint64_t from, uint64_t n,
                                            uint64_t *indices)
{
	while (n) {
		indices[--n] = find_first_empty_slot(qf, from);
		from = indices[n] + 1;
	}
}

/*
 * Returns pairs of the form (pos, len) denoting ranges where empty slots start
 * and how many slots after them are empty.
 */
static inline uint32_t find_next_empty_slot_runs_of_size_n(QF *qf, uint64_t from, 
                                                    uint64_t n, uint64_t *indices)  // NEW IN MEMENTO
{
    uint32_t ind = 0;
    while (n > 0) {
        indices[ind++] = find_first_empty_slot(qf, from);
        indices[ind] = get_number_of_consecutive_empty_slots(qf, indices[ind - 1], n);
        from = indices[ind - 1] + indices[ind];
        n -= indices[ind];
        ind++;

        if (from >= qf->metadata->xnslots) {
            indices[ind++] = from;
            indices[ind++] = 1;
            return ind;
        }
    }
    return ind;
}

static inline void left_shift_slots_by_words(QF *qf, int64_t first, int64_t last,
                                                int64_t distance)   // NEW IN MEMENTO
{
#ifdef DEBUG
    fprintf(stderr, "LEFT SHIFTING first=%ld last=%ld --- distance=%ld\n", first, last, distance);
#endif /* DEBUG */
    const uint32_t slot_size = qf->metadata->bits_per_slot;
    uint64_t src_data = 0;
    int32_t src_filled_bits = 0;
    int64_t src_bit_pos = (first % QF_SLOTS_PER_BLOCK) * slot_size;
    uint64_t src_block_ind = first / QF_SLOTS_PER_BLOCK;
    GET_NEXT_DATA_WORD_IF_EMPTY(qf, src_data, src_filled_bits, slot_size,
                                src_bit_pos, src_block_ind);

    const int64_t dst_slot_pos = first + distance;
#ifdef DEBUG
    fprintf(stderr, "dst_slot_pos=%ld\n", dst_slot_pos);
#endif /* DEBUG */
    uint64_t dst_data = 0, dst_filled_bits = 0;
    uint64_t dst_bit_pos = (dst_slot_pos % QF_SLOTS_PER_BLOCK) * slot_size;
    uint64_t dst_block_ind = dst_slot_pos / QF_SLOTS_PER_BLOCK;
    INIT_PAYLOAD_WORD(qf, dst_data, dst_filled_bits, dst_bit_pos, dst_block_ind);

    for (int64_t i = first; i <= last; i++) {
        GET_NEXT_DATA_WORD_IF_EMPTY(qf, src_data, src_filled_bits, slot_size,
                                    src_bit_pos, src_block_ind);
        const uint64_t slot = src_data & BITMASK(slot_size);
#ifdef DEBUG
        PRINT_WORD_BITS(slot);
#endif /* DEBUG */
        src_data >>= slot_size;
        src_filled_bits -= slot_size;
        APPEND_WRITE_PAYLOAD_WORD(qf, dst_data, dst_filled_bits, slot, slot_size,
                                    dst_bit_pos, dst_block_ind);
    }
    if (dst_filled_bits) {
        FLUSH_PAYLOAD_WORD(qf, dst_data, dst_filled_bits, dst_bit_pos,
                            dst_block_ind);
    }
}

static inline void shift_slots(QF *qf, int64_t first, uint64_t last, 
                                uint64_t distance)  // NEW IN MEMENTO
{
	int64_t i, j;
	if (distance == 1)
		shift_remainders(qf, first, last+1);
	else {
        // Simple implementation
		//for (i = last; i >= first; i--)
		//	set_slot(qf, i + distance, get_slot(qf, i));

        // Faster implementation? It follows a very similar logic as the macros
        // implemented at the start of this file where it moves the data one
        // word at a time as opposed to one slots at a time, but it's a bit
        // more optimized for the specific use case of shifting many slots.
        const uint32_t bits_in_block = qf->metadata->bits_per_slot * QF_SLOTS_PER_BLOCK;

        int64_t x_last_block = last / QF_SLOTS_PER_BLOCK;
        int64_t x_bits_in_prev_block = x_last_block * bits_in_block;
        int64_t x_bits_in_block;

        int64_t y_last_block = (last + distance) / QF_SLOTS_PER_BLOCK;
        int64_t y_bits_in_prev_block = y_last_block * bits_in_block;
        int64_t y_bits_in_block;

        int64_t x_first_bit = first * qf->metadata->bits_per_slot;
        int64_t x_last_bit = last * qf->metadata->bits_per_slot + qf->metadata->bits_per_slot - 1;
        int64_t y_last_bit = x_last_bit + distance * qf->metadata->bits_per_slot;

        
        int32_t x_extra, y_extra;
        int32_t x_prefix, y_prefix;
        uint64_t w_i, w_j, payload;
        do 
        {
            x_bits_in_block = x_last_bit - x_bits_in_prev_block;
            y_bits_in_block = y_last_bit - y_bits_in_prev_block;
            i = x_bits_in_block / 8;
            j = y_bits_in_block / 8;

            int32_t mn = (i < j ? i : j);
            if (7 < mn)
                mn = 7;

            i -= mn;
            j -= mn;
            int32_t move_bits = 8 * mn + (x_last_bit % 8 < y_last_bit % 8 ?
                                            x_last_bit % 8 : y_last_bit % 8) + 1;
            if (x_last_bit - x_first_bit + 1 < move_bits)
                move_bits = x_last_bit - x_first_bit + 1;

            x_prefix = x_bits_in_block - move_bits + 1 - 8 * i;
            y_prefix = y_bits_in_block - move_bits + 1 - 8 * j;
            x_extra = 8 * sizeof(payload) - x_prefix - move_bits;
            y_extra = 8 * sizeof(payload) - y_prefix - move_bits;

            uint8_t *dest = get_block(qf, y_last_block)->slots + j;
            uint8_t *src = get_block(qf, x_last_block)->slots + i;
            memcpy(&w_i, src, sizeof(w_i));
            memcpy(&w_j, dest, sizeof(w_j));
            payload = (w_j & (BITMASK(y_prefix) | (BITMASK(y_extra) << (64 - y_extra))))
                        | (((w_i >> x_prefix) & BITMASK(64 - x_extra - x_prefix)) << y_prefix);
            memcpy(dest, &payload, sizeof(payload));

            x_bits_in_block -= move_bits;
            if (x_bits_in_block < 0) {
                x_bits_in_prev_block -= bits_in_block;
                x_last_block--;
            }
            y_bits_in_block -= move_bits;
            if (y_bits_in_block < 0) {
                y_bits_in_prev_block -= bits_in_block;
                y_last_block--;
            }
            x_last_bit -= move_bits;
            y_last_bit -= move_bits;

        } while (x_last_bit >= x_first_bit);
    }
}

static inline void shift_runends(QF *qf, int64_t first, uint64_t last,
                                     uint64_t distance)     // NEW IN MEMENTO
{
	assert(last < qf->metadata->xnslots && distance < 64);
	uint64_t first_word = first / 64;
	uint64_t bstart = first % 64;
	uint64_t last_word = (last + distance + 1) / 64;
	uint64_t bend = (last + distance + 1) % 64;

    if (last_word != first_word) {
        // The code in the original RSQF implementation had a weird issue with
        // overwriting parts of the bitmap that it shouldn't have touched. The
        // issue came up when `distance > 1`, and is fixed now.
        const uint64_t first_runends_replacement = METADATA_WORD(qf, runends, first) & (~BITMASK(bstart));
        METADATA_WORD(qf, runends, 64*last_word) = shift_into_b((last_word == first_word + 1 ? first_runends_replacement
                                                                                             : METADATA_WORD(qf, runends, 64*(last_word-1))),
                                                                METADATA_WORD(qf, runends, 64*last_word),
                                                                0, bend, distance);
        bend = 64;
        last_word--;
        while (last_word != first_word) {
            METADATA_WORD(qf, runends, 64*last_word) = shift_into_b((last_word == first_word + 1 ? first_runends_replacement
                                                                                             : METADATA_WORD(qf, runends, 64*(last_word-1))),
                                                                    METADATA_WORD(qf, runends, 64*last_word),
                                                                    0, bend, distance);
            last_word--;
        }
    }
    METADATA_WORD(qf, runends, 64*last_word) = shift_into_b(0LL, METADATA_WORD(qf, runends, 64*last_word),
                                                            bstart, bend, distance);

}

static inline bool insert_replace_slots_and_shift_remainders_and_runends_and_offsets(QF	*qf,
        int operation, uint64_t bucket_index, uint64_t overwrite_index,
        const uint64_t *remainders, uint64_t total_remainders,
        uint64_t noverwrites)
{
	uint64_t empties[67];
	uint64_t i;
	int64_t j;
	int64_t ninserts = total_remainders - noverwrites;
	uint64_t insert_index = overwrite_index + noverwrites;

	if (ninserts > 0) {
		/* First, shift things to create n empty spaces where we need them. */
		find_next_n_empty_slots(qf, insert_index, ninserts, empties);
		if (empties[0] >= qf->metadata->xnslots) {
			return false;
		}
		for (j = 0; j < ninserts - 1; j++)
			shift_slots(qf, empties[j+1] + 1, empties[j] - 1, j + 1);
		shift_slots(qf, insert_index, empties[ninserts - 1] - 1, ninserts);

		for (j = 0; j < ninserts - 1; j++)
			shift_runends(qf, empties[j+1] + 1, empties[j] - 1, j + 1);
		shift_runends(qf, insert_index, empties[ninserts - 1] - 1, ninserts);

		for (i = noverwrites; i < total_remainders - 1; i++)
            METADATA_WORD(qf, runends, overwrite_index + i) &= ~(1ULL <<
                                (((overwrite_index + i) % QF_SLOTS_PER_BLOCK)
                                % 64));

		switch (operation) {
			case 0: /* insert into empty bucket */
				assert (noverwrites == 0);
				METADATA_WORD(qf, runends, overwrite_index + total_remainders - 1) |=
					1ULL << (((overwrite_index + total_remainders - 1) %
										QF_SLOTS_PER_BLOCK) % 64);
				break;
			case 1: /* append to bucket */
				METADATA_WORD(qf, runends, overwrite_index + noverwrites - 1)      &=
					~(1ULL << (((overwrite_index + noverwrites - 1) % QF_SLOTS_PER_BLOCK) %
										 64));
				METADATA_WORD(qf, runends, overwrite_index + total_remainders - 1) |=
					1ULL << (((overwrite_index + total_remainders - 1) %
										QF_SLOTS_PER_BLOCK) % 64);
				break;
			case 2: /* insert into bucket */
				METADATA_WORD(qf, runends, overwrite_index + total_remainders - 1) &=
					~(1ULL << (((overwrite_index + total_remainders - 1) %
											QF_SLOTS_PER_BLOCK) % 64));
				break;
			default:
				fprintf(stderr, "Invalid operation %d\n", operation);
				abort();
		}

		uint64_t npreceding_empties = 0;
		for (i = bucket_index / QF_SLOTS_PER_BLOCK + 1; i <= empties[0]/QF_SLOTS_PER_BLOCK; i++) {
			while ((int64_t)npreceding_empties < ninserts &&
						 empties[ninserts - 1 - npreceding_empties]  / QF_SLOTS_PER_BLOCK < i)
				npreceding_empties++;

			if (get_block(qf, i)->offset + ninserts - npreceding_empties < BITMASK(8*sizeof(qf->blocks[0].offset)))
				get_block(qf, i)->offset += ninserts - npreceding_empties;
			else
				get_block(qf, i)->offset = (uint8_t) BITMASK(8*sizeof(qf->blocks[0].offset));
		}
	}

	for (i = 0; i < total_remainders; i++)
		set_slot(qf, overwrite_index + i, remainders[i]);

	modify_metadata(qf, &qf->metadata->noccupied_slots, ninserts);

	return true;
}

static inline int remove_replace_slots_and_shift_remainders_and_runends_and_offsets(QF *qf,
        int operation,
        uint64_t bucket_index,
        uint64_t overwrite_index,
        const uint64_t *remainders,
        uint64_t total_remainders,
        uint64_t old_length)
{
	uint64_t i;

	// Update the slots
	for (i = 0; i < total_remainders; i++)
		set_slot(qf, overwrite_index + i, remainders[i]);

	// If this is the last thing in its run, then we may need to set a new runend bit
	if (is_runend(qf, overwrite_index + old_length - 1)) {
	  if (total_remainders > 0) {
	    // If we're not deleting this entry entirely, then it will still the last entry in this run
	    METADATA_WORD(qf, runends, overwrite_index + total_remainders - 1) |= 1ULL << ((overwrite_index + total_remainders - 1) % 64);
	  } else if (overwrite_index > bucket_index &&
		     !is_runend(qf, overwrite_index - 1)) {
	    // If we're deleting this entry entirely, but it is not the first entry in this run,
	    // then set the preceding entry to be the runend
	    METADATA_WORD(qf, runends, overwrite_index - 1) |= 1ULL << ((overwrite_index - 1) % 64);
	  }
	}

	// shift slots back one run at a time
	uint64_t original_bucket = bucket_index;
	uint64_t current_bucket = bucket_index;
	uint64_t current_slot = overwrite_index + total_remainders;
	uint64_t current_distance = old_length - total_remainders;
	int ret_current_distance = current_distance;

	while (current_distance > 0) {
		if (is_runend(qf, current_slot + current_distance - 1)) {
			do {
				current_bucket++;
			} while (current_bucket < current_slot + current_distance &&
							 !is_occupied(qf, current_bucket));
		}

		if (current_bucket <= current_slot) {
			set_slot(qf, current_slot, get_slot(qf, current_slot + current_distance));
			if (is_runend(qf, current_slot) !=
					is_runend(qf, current_slot + current_distance))
				METADATA_WORD(qf, runends, current_slot) ^= 1ULL << (current_slot % 64);
			current_slot++;

		} else if (current_bucket <= current_slot + current_distance) {
			uint64_t i;
			for (i = current_slot; i < current_slot + current_distance; i++) {
				set_slot(qf, i, 0);
				METADATA_WORD(qf, runends, i) &= ~(1ULL << (i % 64));
			}

			current_distance = current_slot + current_distance - current_bucket;
			current_slot = current_bucket;
		} else {
			current_distance = 0;
		}
	}

	// reset the occupied bit of the hash bucket index if the hash is the
	// only item in the run and is removed completely.
	if (operation && !total_remainders)
		METADATA_WORD(qf, occupieds, bucket_index) &= ~(1ULL << (bucket_index % 64));

	// update the offset bits.
	// find the number of occupied slots in the original_bucket block.
	// Then find the runend slot corresponding to the last run in the
	// original_bucket block.
	// Update the offset of the block to which it belongs.
	uint64_t original_block = original_bucket / QF_SLOTS_PER_BLOCK;
	while (1 && old_length > total_remainders) {	// we only update offsets if we shift/delete anything
		int32_t last_occupieds_bit = bitscanreverse(get_block(qf, original_block)->occupieds[0]);
		// there is nothing in the block
		if (last_occupieds_bit == -1) {
			if (get_block(qf, original_block + 1)->offset == 0)
				break;
			get_block(qf, original_block + 1)->offset = 0;
		} else {
			uint64_t last_occupieds_hash_index = QF_SLOTS_PER_BLOCK * original_block + last_occupieds_bit;
			uint64_t runend_index = run_end(qf, last_occupieds_hash_index);
			// runend spans across the block
			// update the offset of the next block
			if (runend_index / QF_SLOTS_PER_BLOCK == original_block) { // if the run ends in the same block
				if (get_block(qf, original_block + 1)->offset == 0)
					break;
				get_block(qf, original_block + 1)->offset = 0;
			} else if (runend_index / QF_SLOTS_PER_BLOCK == original_block + 1) { // if the last run spans across one block
				if (get_block(qf, original_block + 1)->offset == (runend_index % QF_SLOTS_PER_BLOCK) + 1)
					break;
				get_block(qf, original_block + 1)->offset = (runend_index % QF_SLOTS_PER_BLOCK) + 1;
			} else { // if the last run spans across multiple blocks
				uint64_t i;
				for (i = original_block + 1; i < runend_index / QF_SLOTS_PER_BLOCK - 1; i++)
					get_block(qf, i)->offset = QF_SLOTS_PER_BLOCK;
				if (get_block(qf, runend_index / QF_SLOTS_PER_BLOCK)->offset == (runend_index % QF_SLOTS_PER_BLOCK) + 1)
					break;
				get_block(qf, runend_index / QF_SLOTS_PER_BLOCK)->offset = (runend_index % QF_SLOTS_PER_BLOCK) + 1;
			}
		}
		original_block++;
	}

	int num_slots_freed = old_length - total_remainders;
	modify_metadata(qf, &qf->metadata->noccupied_slots, -num_slots_freed);
	/*qf->metadata->noccupied_slots -= (old_length - total_remainders);*/
	if (!total_remainders) {
		modify_metadata(qf, &qf->metadata->ndistinct_elts, -1);
		/*qf->metadata->ndistinct_elts--;*/
	}

	return ret_current_distance;
}

static inline int32_t make_empty_slot_for_memento_list(QF *qf,
                                        uint64_t bucket_index, uint64_t pos)    // NEW IN MEMENTO
{
    const int64_t next_empty = find_first_empty_slot(qf, pos);
    if (next_empty >= qf->metadata->xnslots) {  // Check that the new data fits
        return QF_NO_SPACE;
    }
    if (pos < next_empty)
        shift_slots(qf, pos, next_empty - 1, 1);
    shift_runends(qf, pos - 1, next_empty - 1, 1);
    for (uint32_t i = bucket_index / QF_SLOTS_PER_BLOCK + 1; 
            i <= next_empty / QF_SLOTS_PER_BLOCK; i++) {
        if (get_block(qf, i)->offset + 1
                <= BITMASK(8 * sizeof(qf->blocks[0].offset)))
            get_block(qf, i)->offset++;
    }
    modify_metadata(qf, &qf->metadata->noccupied_slots, 1);
    return 0;
}

static inline int32_t make_n_empty_slots_for_memento_list(QF *qf,
                            uint64_t bucket_index, uint64_t pos, uint32_t n)    // NEW IN MEMENTO
{
    uint64_t empty_runs[2 * n];
    uint64_t empty_runs_ind = find_next_empty_slot_runs_of_size_n(qf, pos,
                                                                n, empty_runs);
    if (empty_runs[empty_runs_ind - 2] + empty_runs[empty_runs_ind - 1] - 1
            >= qf->metadata->xnslots) {     // Check that the new data fits
        return QF_NO_SPACE;
    }

    uint64_t shift_distance = 0;
    for (int i = empty_runs_ind - 2; i >= 2; i -= 2) {
        shift_distance += empty_runs[i + 1];
        shift_slots(qf, empty_runs[i - 2] + empty_runs[i - 1], empty_runs[i] - 1,
                shift_distance);
        shift_runends(qf, empty_runs[i - 2] + empty_runs[i - 1], empty_runs[i] - 1,
                shift_distance);
    }
    if (pos < empty_runs[0])
        shift_slots(qf, pos, empty_runs[0] - 1, n);
    shift_runends(qf, pos - 1, empty_runs[0] - 1, n);
    // Update offsets
    uint64_t npreceding_empties = 0;
    uint32_t empty_iter = 0;
    uint32_t last_block_to_update_offset = (empty_runs[empty_runs_ind - 2] + 
            empty_runs[empty_runs_ind - 1] - 1) 
        / QF_SLOTS_PER_BLOCK;
#ifdef DEBUG
    fprintf(stderr, "last_block_to_update_offset=%u\n", last_block_to_update_offset);
#endif /* DEBUG */
    for (uint64_t i = bucket_index / QF_SLOTS_PER_BLOCK + 1; 
            i <= last_block_to_update_offset; i++) {
        while (npreceding_empties < n) {
            uint64_t r = i * QF_SLOTS_PER_BLOCK;
            uint64_t l = r - QF_SLOTS_PER_BLOCK;
            uint64_t empty_run_start = empty_runs[empty_iter];
            uint64_t empty_run_end = empty_runs[empty_iter] 
                + empty_runs[empty_iter + 1];
            if (r <= empty_run_start)
                break;
            if (l < empty_run_start)
                l = empty_run_start;
            if (r > empty_run_end) {
                r = empty_run_end;
                npreceding_empties += r - l;
                empty_iter += 2;
            }
            else {
                npreceding_empties += r - l;
                break;
            }
        }
        if (get_block(qf, i)->offset + n - npreceding_empties 
                < BITMASK(8 * sizeof(qf->blocks[0].offset)))
            get_block(qf, i)->offset += n - npreceding_empties;
        else
            get_block(qf, i)->offset = BITMASK(8 * sizeof(qf->blocks[0].offset));
    }
    modify_metadata(qf, &qf->metadata->noccupied_slots, n);

    return 0;
}

static inline int32_t write_prefix_set(QF *qf, const uint64_t pos,
        const uint64_t fingerprint, const uint64_t *mementos, 
        const uint64_t memento_cnt)     // NEW IN MEMENTO
{
#ifdef DEBUG
    fprintf(stderr, "WRITING PREFIX SET pos=%lu fingerprint=", pos);
    for (int i = qf->metadata->fingerprint_bits; i >= 0; i--)
        fprintf(stderr, "%lu", (fingerprint >> i) & 1);
    fprintf(stderr, " [");
    for (int i = 0; i < memento_cnt; i++)
        fprintf(stderr, "%lu, ", mementos[i]);
    fprintf(stderr, "\b\b]\n");
#endif /* DEBUG */

    if (memento_cnt == 1) {
        set_slot(qf, pos, (fingerprint << qf->metadata->memento_bits) | mementos[0]);
        return 1;
    }

    if (fingerprint == 0) {     // Can't use a void fingerprint
        uint64_t payload = 0, current_full_prefix = 0;
        uint64_t dest_bit_pos = (pos % QF_SLOTS_PER_BLOCK) * qf->metadata->bits_per_slot;
        uint64_t dest_block_ind = pos / QF_SLOTS_PER_BLOCK;
        INIT_PAYLOAD_WORD(qf, payload, current_full_prefix, dest_bit_pos, dest_block_ind);
        for (uint32_t i = 0; i < memento_cnt; i++) {
            uint64_t val = mementos[i];
            APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, val,
                    qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
        }
        if (current_full_prefix) {
            FLUSH_PAYLOAD_WORD(qf, payload, current_full_prefix, dest_bit_pos, 
                                dest_block_ind);
        }
        return memento_cnt;
    }

    uint32_t res = 2;
    uint64_t payload = 0, current_full_prefix = 0;
    uint64_t dest_bit_pos = (pos % QF_SLOTS_PER_BLOCK) * qf->metadata->bits_per_slot;
    uint64_t dest_block_ind = pos / QF_SLOTS_PER_BLOCK;
    INIT_PAYLOAD_WORD(qf, payload, current_full_prefix, dest_bit_pos, dest_block_ind);
    if (memento_cnt == 2) {
        uint64_t val = (fingerprint << qf->metadata->memento_bits) | mementos[0];
        APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, val,
                qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
        val = mementos[1];
        APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, val,
                qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
        if (mementos[0] == mementos[1]) {
            APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, 0LL,
                    qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
            res++;
        }
    }
    else {
        uint64_t val = (fingerprint << qf->metadata->memento_bits) | mementos[memento_cnt - 1];
        APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, val,
                qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
        val = mementos[0];
        APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, val,
                qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
    }
    if (memento_cnt > 2) {
        const uint32_t memento_bits = qf->metadata->memento_bits;
        const uint64_t max_memento_value = (1ULL << qf->metadata->memento_bits) - 1;
        const uint64_t list_len = memento_cnt - 2;
        int32_t written_bits = memento_bits;
        if (list_len >= max_memento_value) {
            uint64_t fragments[5], frag_cnt = 0;
            for (uint32_t cnt = list_len; cnt; cnt /= max_memento_value) {
                fragments[frag_cnt++] = cnt % max_memento_value;
            }
            for (uint32_t i = 0; i < frag_cnt - 1; i++) {
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, 
                    max_memento_value, memento_bits, dest_bit_pos, dest_block_ind);
            }
            for (uint32_t i = 0; i < frag_cnt; i++) {
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, 
                    fragments[i], memento_bits, dest_bit_pos, dest_block_ind);
            }
            written_bits += 2 * (frag_cnt - 1) * memento_bits;
        }
        else {
            APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, 
                    list_len, memento_bits, dest_bit_pos, dest_block_ind);
        }
        for (uint32_t i = 1; i < memento_cnt - 1; i++) {
            written_bits += memento_bits;
            APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, 
                    mementos[i], memento_bits, dest_bit_pos, dest_block_ind);
        }
        while (written_bits > 0) {
            res++;
            written_bits -= qf->metadata->bits_per_slot;
        }
        
        // Optional, can be removed
        if (written_bits < 0) {
            APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, 0LL,
                    -written_bits, dest_bit_pos,
                    dest_block_ind);
        }
    }
    if (current_full_prefix) {
        FLUSH_PAYLOAD_WORD(qf, payload, current_full_prefix, dest_bit_pos, 
                            dest_block_ind);
    }
    return res;
}

static inline int32_t remove_mementos_from_prefix_set(QF *qf, const uint64_t pos, 
            const uint64_t *mementos, bool *handled, const uint32_t memento_cnt,
            int32_t *new_slot_count, int32_t *old_slot_count)   // NEW IN MEMENTO
{
#ifdef DEBUG
    fprintf(stderr, "REMOVING mementos=[");
    for (int i = 0; i < memento_cnt; i++) {
        fprintf(stderr, "%lu, ", mementos[i]);
    }
    fprintf(stderr, "\b\b] handled=[");
    for (int i = 0; i < memento_cnt; i++) {
        fprintf(stderr, "%u, ", handled[i]);
    }
    fprintf(stderr, "\b\b]\n");
#endif /* DEBUG */

    const uint64_t f1 = GET_FINGERPRINT(qf, pos);
    const uint64_t m1 = GET_MEMENTO(qf, pos);
    const uint64_t f2 = GET_FINGERPRINT(qf, pos + 1);
    const uint64_t m2 = GET_MEMENTO(qf, pos + 1);
    const uint64_t memento_bits = qf->metadata->memento_bits;
    const uint64_t max_memento_value = BITMASK(memento_bits);

    if (f1 <= f2 || is_runend(qf, pos)) {
        for (uint32_t i = 0; i < memento_cnt; i++) {
            if (m1 == mementos[i]) {
                handled[i] = true;
                *old_slot_count = 1;
                *new_slot_count = 0;
                return 1;
            }
        }
        *new_slot_count = -1;
        return 0;
    }

    *old_slot_count = 2;
    uint32_t old_memento_cnt = 2, old_unary_cnt = 0;
    uint64_t data = 0;
    int32_t filled_bits = 0;
    int64_t data_bit_pos = ((pos + 2) % QF_SLOTS_PER_BLOCK) * qf->metadata->bits_per_slot;
    uint64_t data_block_ind = (pos + 2) / QF_SLOTS_PER_BLOCK;
    GET_NEXT_DATA_WORD_IF_EMPTY(qf, data, filled_bits, memento_bits,
                                data_bit_pos, data_block_ind);
    if (m1 > m2) {
        old_memento_cnt += data & max_memento_value;
        data >>= memento_bits;
        filled_bits -= memento_bits;
        if (old_memento_cnt == max_memento_value + 2) {
            uint64_t length = 2, pw = 1;
            old_memento_cnt = 2;
            old_unary_cnt = 1;
            while (length) {
                GET_NEXT_DATA_WORD_IF_EMPTY(qf, data, filled_bits, memento_bits,
                                            data_bit_pos, data_block_ind);
                const uint64_t current_fragment = data & max_memento_value;
                if (current_fragment == max_memento_value) {
                    length++;
                    old_unary_cnt++;
                }
                else {
                    length--;
                    old_memento_cnt += pw * current_fragment;
                    pw *= max_memento_value;
                }
                data >>= memento_bits;
                filled_bits -= memento_bits;
            }
        }
    }

    uint64_t res_mementos[old_memento_cnt], res_cnt = 0;
    uint32_t cmp_ind = 0, val = (m1 < m2 ? m1 : m2);
    int32_t newly_handled_cnt = 0;
    // Handle the minimum
    while (cmp_ind < memento_cnt && (handled[cmp_ind] || mementos[cmp_ind] < val)) {
        cmp_ind++;
    }
    if (cmp_ind < memento_cnt && mementos[cmp_ind] == val) {
        handled[cmp_ind++] = true;
        newly_handled_cnt++;
#ifdef DEBUG
        fprintf(stderr, "FILTERED %u OUT\n", val);
#endif /* DEBUG */
    }
    else {
        res_mementos[res_cnt++] = val;
#ifdef DEBUG
        fprintf(stderr, "KEEP %u\n", val);
#endif /* DEBUG */
    }
#ifdef DEBUG
    perror("HMMMMMMMMMMMMM");
    fprintf(stderr, "old_memento_cnt=%u\n", old_memento_cnt);
#endif /* DEBUG */
    // Handle the actual list
    for (uint32_t i = 1; i < old_memento_cnt - 1; i++) {
        GET_NEXT_DATA_WORD_IF_EMPTY(qf, data, filled_bits, memento_bits,
                                    data_bit_pos, data_block_ind);
        val = data & max_memento_value;
        data >>= memento_bits;
        filled_bits -= memento_bits;
        while (cmp_ind < memento_cnt && (handled[cmp_ind] || mementos[cmp_ind] < val)) {
            cmp_ind++;
        }
        if (cmp_ind < memento_cnt && mementos[cmp_ind] == val) {
            handled[cmp_ind++] = true;
            newly_handled_cnt++;
#ifdef DEBUG
            fprintf(stderr, "FILTERED %u OUT\n", val);
#endif /* DEBUG */
        }
        else {
            res_mementos[res_cnt++] = val;
#ifdef DEBUG
            fprintf(stderr, "KEEP %u\n", val);
#endif /* DEBUG */
        }
    }
#ifdef DEBUG
    perror("DONE WITH THE ACTUAL LIST ITSELF");
#endif /* DEBUG */
    // Handle the maximum
    val = (m1 < m2 ? m2 : m1);
    while (cmp_ind < memento_cnt && (handled[cmp_ind] || mementos[cmp_ind] < val)) {
        cmp_ind++;
    }
    if (cmp_ind < memento_cnt && mementos[cmp_ind] == val) {
        handled[cmp_ind++] = true;
        newly_handled_cnt++;
#ifdef DEBUG
        fprintf(stderr, "FILTERED %u OUT\n", val);
#endif /* DEBUG */
    }
    else {
        res_mementos[res_cnt++] = val;
#ifdef DEBUG
        fprintf(stderr, "KEEP %u\n", val);
#endif /* DEBUG */
    }

    if (res_cnt != old_memento_cnt) {
        // Something changed
        if (old_memento_cnt > 2) {
            int32_t old_list_bits = (old_memento_cnt - 1 + 2 * old_unary_cnt) * memento_bits;
#ifdef DEBUG
            fprintf(stderr, "======== old_list_bits=%d --- old_memento_cnt=%u old_unary_cnt=%u\n",
                    old_list_bits, old_memento_cnt, old_unary_cnt);
#endif /* DEBUG */
            while (old_list_bits > 0) {
                old_list_bits -= qf->metadata->bits_per_slot;
                (*old_slot_count)++;
            }
        }
        *new_slot_count = res_cnt ? write_prefix_set(qf, pos, f1, res_mementos, res_cnt) : 0;
    }
    else {
        // Nothing changed
        *new_slot_count = -1;
    }

    return newly_handled_cnt;
}

static inline int32_t add_memento_to_sorted_list(QF *qf, const uint64_t bucket_index,
                                            const uint64_t pos, uint64_t new_memento)   // NEW IN MEMENTO
{
    const uint64_t f1 = GET_FINGERPRINT(qf, pos);
    const uint64_t m1 = GET_MEMENTO(qf, pos);
    const uint64_t f2 = GET_FINGERPRINT(qf, pos + 1);
    const uint64_t m2 = GET_MEMENTO(qf, pos + 1);
    const uint64_t memento_bits = qf->metadata->memento_bits;

    const bool singleton_prefix_set = (is_runend(qf, pos) || f1 <= f2);
    if (singleton_prefix_set) {
        if (new_memento == m1) {
            int32_t err = make_n_empty_slots_for_memento_list(qf, bucket_index, pos + 1, 2);
            if (err < 0)    // Check that the new data fits
                return err;

            if (new_memento < m1) {
                set_slot(qf, pos, (f1 << memento_bits) | new_memento);
                set_slot(qf, pos + 1, m1);
            }
            else {
                set_slot(qf, pos + 1, new_memento);
            }
            set_slot(qf, pos + 2, 0);
        }
        else {
            int32_t err = make_empty_slot_for_memento_list(qf, bucket_index, pos + 1);
            if (err < 0)    // Check that the new data fits
                return err;

            if (new_memento < m1) {
                set_slot(qf, pos, (f1 << memento_bits) | new_memento);
                set_slot(qf, pos + 1, m1);
            }
            else {
                set_slot(qf, pos, (f1 << memento_bits) | m1);
                set_slot(qf, pos + 1, new_memento);
            }
        }
        return 0;
    }

    const bool size_two_prefix_set = (m1 < m2);
    if (size_two_prefix_set) {
        if (qf->metadata->bits_per_slot < 2 * qf->metadata->memento_bits) {
            int32_t err = make_n_empty_slots_for_memento_list(qf, bucket_index, pos + 1, 2);
            if (err < 0)    // Check that the new data fits
                return err;

            uint64_t payload = 0, dest_pos = pos;
            uint64_t dest_bit_pos = (dest_pos % QF_SLOTS_PER_BLOCK) * qf->metadata->bits_per_slot;
            uint64_t dest_block_ind = dest_pos / QF_SLOTS_PER_BLOCK;
            uint32_t current_full_prefix = 0;
            INIT_PAYLOAD_WORD(qf, payload, current_full_prefix, dest_bit_pos, dest_block_ind);
            uint64_t value;
            if (new_memento < m1) {
                value = (f1 << memento_bits) | m2;
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                    value, qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
                value = (f2 << memento_bits) | new_memento;
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                    value, qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
                value = (m1 << memento_bits) | 1ULL;
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                    value, 2 * memento_bits, dest_bit_pos, dest_block_ind);
            }
            else if (m2 < new_memento) {
                value = (f1 << memento_bits) | new_memento;
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                    value, qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
                value = (f2 << memento_bits) | m1;
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                    value, qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
                value = (m2 << memento_bits) | 1ULL;
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                    value, 2 * memento_bits, dest_bit_pos, dest_block_ind);
            }
            else {
                value = (f1 << memento_bits) | m2;
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                    value, qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
                value = (f2 << memento_bits) | m1;
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                    value, qf->metadata->bits_per_slot, dest_bit_pos, dest_block_ind);
                value = (new_memento << memento_bits) | 1ULL;
                APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                    value, 2 * memento_bits, dest_bit_pos, dest_block_ind);
            }

            if (current_full_prefix)
                FLUSH_PAYLOAD_WORD(qf, payload, current_full_prefix, dest_bit_pos, 
                        dest_block_ind);
        }
        else {
            int32_t err = make_empty_slot_for_memento_list(qf, bucket_index, pos + 2);
            if (err < 0)    // Check that the new data fits
                return err;

            if (new_memento < m1) {
                set_slot(qf, pos, (f1 << memento_bits) | m2);
                set_slot(qf, pos + 1, (f2 << memento_bits) | new_memento);
                set_slot(qf, pos + 2, (m1 << memento_bits) | 1ULL);
            }
            else if (m2 < new_memento) {
                set_slot(qf, pos, (f1 << memento_bits) | new_memento);
                set_slot(qf, pos + 1, (f2 << memento_bits) | m1);
                set_slot(qf, pos + 2, (m2 << memento_bits) | 1ULL);
            }
            else {
                set_slot(qf, pos, (f1 << memento_bits) | m2);
                set_slot(qf, pos + 1, (f2 << memento_bits) | m1);
                set_slot(qf, pos + 2, (new_memento << memento_bits) | 1ULL);
            }
        }
        return 0;
    }

    if (new_memento < m2) {
        set_slot(qf, pos + 1, (f2 << memento_bits) | new_memento);
        new_memento = m2;
    }
    else if (m1 < new_memento) {
        set_slot(qf, pos, (f1 << memento_bits) | new_memento);
        new_memento = m1;
    }

    const uint64_t max_memento_value = BITMASK(memento_bits);
    uint64_t ind = pos + 2, ind_cnt = 0;
    uint64_t data = 0;
    int32_t filled_bits = 0;
    int64_t data_bit_pos = (ind % QF_SLOTS_PER_BLOCK) * qf->metadata->bits_per_slot;
    uint64_t data_block_ind = ind / QF_SLOTS_PER_BLOCK;
    GET_NEXT_DATA_WORD_IF_EMPTY(qf, data, filled_bits, memento_bits,
                                data_bit_pos, data_block_ind);

#ifdef DEBUG
    perror("HMMMM");
    fprintf(stderr, "ind=%lu data_bit_pos=%lu data_block_ind=%lu\n", ind, data_bit_pos, data_block_ind);
    PRINT_WORD_BITS(data);
#endif /* ifdef DEBUG */

    uint64_t memento_count = data & max_memento_value, unary_count = 0;
    data >>= memento_bits;
    filled_bits -= memento_bits;
    bool counter_overflow = (memento_count == max_memento_value - 1);
    if (memento_count == max_memento_value) {
        uint64_t length = 2, pw = 1;
        unary_count = 1;
        counter_overflow = true;
        memento_count = 0;
        while (length) {
            GET_NEXT_DATA_WORD_IF_EMPTY(qf, data, filled_bits, memento_bits,
                                        data_bit_pos, data_block_ind);
            const uint64_t current_fragment = data & max_memento_value;
            ind_cnt += memento_bits;
            if (ind_cnt >= qf->metadata->bits_per_slot) {
                ind++;
                ind_cnt -= qf->metadata->bits_per_slot;
            }

            if (current_fragment == max_memento_value) {
                length++;
                unary_count++;
            }
            else {
                length--;
                counter_overflow &= (current_fragment == max_memento_value - 1);
                memento_count += pw * current_fragment;
                pw *= max_memento_value;
            }
            data >>= memento_bits;
            filled_bits -= memento_bits;
        }
    }
#ifdef DEBUG
    fprintf(stderr, "============================== memento_count=%lu\n", memento_count);
#endif /* DEBUG */

    uint32_t mementos[memento_count + 1];
    uint32_t cnt = 0;
    while (cnt < memento_count) {
        GET_NEXT_DATA_WORD_IF_EMPTY(qf, data, filled_bits, memento_bits,
                                    data_bit_pos, data_block_ind);
        mementos[cnt] = data & max_memento_value;
        ind_cnt += memento_bits;
        if (ind_cnt >= qf->metadata->bits_per_slot) {
            ind++;
            ind_cnt -= qf->metadata->bits_per_slot;
        }
        data >>= memento_bits;
        filled_bits -= memento_bits;
        cnt++;
    }

#ifdef DEBUG
    fprintf(stderr, "memento_count=%lu --- mementos=[", memento_count);
    for (uint32_t i = 0; i < memento_count; i++) {
        fprintf(stderr, "%u, ", mementos[i]);
    }
    fprintf(stderr, "\b\b]\n");
#endif /* DEBUG */

    int32_t extra_bits = (2 * unary_count + memento_count + 1) * memento_bits;
    while (extra_bits > 0) {
        extra_bits -= qf->metadata->bits_per_slot;
    }
    int32_t extra_slots = 0;
    extra_bits += memento_bits + counter_overflow * 2 * memento_bits;
    while (extra_bits > 0) {
        extra_bits -= qf->metadata->bits_per_slot;
        extra_slots++;
    }
#ifdef DEBUG
    fprintf(stderr, "extra_slots=%d\n", extra_slots);
#endif /* DEBUG */

    if (extra_slots) {
        // Find empty slots and shift everything to fit the new mementos
        ind++;
        make_n_empty_slots_for_memento_list(qf, bucket_index, ind, extra_slots);
    }

    // Update the actual list 
    memento_count++;
    uint64_t payload = 0, dest_pos = pos + 2;
    uint64_t dest_bit_pos = (dest_pos % QF_SLOTS_PER_BLOCK) * qf->metadata->bits_per_slot;
    uint64_t dest_block_ind = dest_pos / QF_SLOTS_PER_BLOCK;
    uint32_t current_full_prefix = 0;
    INIT_PAYLOAD_WORD(qf, payload, current_full_prefix, dest_bit_pos, dest_block_ind);
#ifdef DEBUG
    fprintf(stderr, "current_full_prefix=%u dest_bit_pos=%lu dest_block_ind=%lu\n", 
            current_full_prefix, dest_bit_pos, dest_block_ind);
    PRINT_WORD_BITS(payload);
#endif /* DEBUG */
    unary_count += counter_overflow;
    if (unary_count) {
        while (unary_count) {
            APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                max_memento_value, memento_bits, dest_bit_pos, dest_block_ind);
            unary_count--;
        }
        for (uint32_t cnt = memento_count; cnt; cnt /= max_memento_value) {
            const uint64_t appendee = cnt % max_memento_value;
            APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                        appendee, memento_bits, dest_bit_pos, dest_block_ind);
        }
    }
    else {
        APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix, 
                    memento_count, memento_bits, dest_bit_pos, dest_block_ind);
    }
#ifdef DEBUG
    PRINT_WORD_BITS(payload);
    perror("=============================================");
#endif /* DEBUG */
    bool written_new_memento = false;
    for (uint32_t i = 0; i < memento_count - 1; i++) {
        if (!written_new_memento && mementos[i] > new_memento) {
            written_new_memento = true;
            APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                        new_memento, memento_bits, dest_bit_pos, dest_block_ind);
#ifdef DEBUG
            fprintf(stderr, "current_full_prefix=%u dest_bit_pos=%lu dest_block_ind=%lu --- val=%lu\n", 
                            current_full_prefix, dest_bit_pos, dest_block_ind, new_memento);
            PRINT_WORD_BITS(payload);
#endif /* DEBUG */
        }
        const uint64_t memento = mementos[i];
        APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                        memento, memento_bits, dest_bit_pos, dest_block_ind);
#ifdef DEBUG
        fprintf(stderr, "current_full_prefix=%u dest_bit_pos=%lu dest_block_ind=%lu --- val=%lu\n", 
                        current_full_prefix, dest_bit_pos, dest_block_ind, memento);
        PRINT_WORD_BITS(payload);
#endif /* DEBUG */
    }
    if (!written_new_memento) {
        APPEND_WRITE_PAYLOAD_WORD(qf, payload, current_full_prefix,
                    new_memento, memento_bits, dest_bit_pos, dest_block_ind);
    }
    if (current_full_prefix) {
        FLUSH_PAYLOAD_WORD(qf, payload, current_full_prefix, dest_bit_pos, 
                            dest_block_ind);
    }

    return 0;
}

/*****************************************************************************
 * Code that uses the above to implement a QF with keys and inline mementos. *
 *****************************************************************************/

// Here, `pos` must point to the slot that is the start of the actual memento 
// list, and not any of the slots containing fingerprints.
__attribute__((always_inline))
static inline uint64_t number_of_slots_used_for_memento_list(const QF *qf,
                                                            uint64_t pos)   // NEW IN MEMENTO
{
    const uint64_t max_memento = ((1ULL << qf->metadata->memento_bits) - 1);
    uint64_t data = get_slot(qf, pos);
    int64_t memento_count = (data & max_memento) + 1;
    if (memento_count == max_memento + 1) {
        // This is very unlikely to execute
        uint64_t length = 2, pw = 1;
        uint64_t bits_left = qf->metadata->bits_per_slot - qf->metadata->memento_bits;
        data >>= qf->metadata->memento_bits;
        memento_count = 1;
        pos++;
        while (length > 0) {
            if (bits_left < qf->metadata->memento_bits) {
                data |= get_slot(qf, pos) << bits_left;
                bits_left += qf->metadata->bits_per_slot;
                pos++;
            }
            uint64_t current_part = data & max_memento;
            if (current_part == max_memento) {
                length++;
                memento_count++;
            }
            else {
                memento_count += pw * current_part + 1;
                pw *= max_memento;
                length--;
            }
            data >>= qf->metadata->memento_bits;
            bits_left -= qf->metadata->memento_bits;
        }
    }

    int64_t bits_left = memento_count * qf->metadata->memento_bits;
    uint64_t res = 0;
    // Slight optimization for doing this division?
    const int64_t step = qf->metadata->bits_per_slot * 16;
    while (bits_left >= step) {
        bits_left -= step;
        res += 16;
    }
    while (bits_left > 0) {
        bits_left -= qf->metadata->bits_per_slot;
        res++;
    }
    return res;
}

__attribute__((always_inline))
static inline int64_t next_matching_fingerprint_in_run(const QF *qf, uint64_t pos,
        const uint64_t fingerprint)     // NEW IN MEMENTO
{
    uint64_t current_fingerprint, current_memento;
    uint64_t next_fingerprint, next_memento;
    while (true) {
        current_fingerprint = GET_FINGERPRINT(qf, pos);
        current_memento = GET_MEMENTO(qf, pos);
        if (fingerprint < current_fingerprint)
            return -1;

        pos++;
        if (fingerprint == current_fingerprint)
            return pos - 1;
        else if (is_runend(qf, pos - 1))
            return -1;

        next_fingerprint = GET_FINGERPRINT(qf, pos);
        if (current_fingerprint > next_fingerprint) {
            next_memento = GET_MEMENTO(qf, pos);
            pos++;
            if (current_memento >= next_memento) {
                // Mementos encoded as a sorted list
                pos += number_of_slots_used_for_memento_list(qf, pos);
            }
            if (is_runend(qf, pos - 1)) {
                return -1;
            }
        }
    }
    return pos;
}

static inline uint64_t lower_bound_fingerprint_in_run(const QF *qf, uint64_t pos,
        uint64_t fingerprint)   // NEW IN MEMENTO
{
    uint64_t current_fingerprint, current_memento;
    uint64_t next_fingerprint, next_memento;
    do {
        current_fingerprint = GET_FINGERPRINT(qf, pos);
        current_memento = GET_MEMENTO(qf, pos);
        if (fingerprint <= current_fingerprint) {
            break;
        }

        pos++;
        if (is_runend(qf, pos - 1))
            break;

        next_fingerprint = GET_FINGERPRINT(qf, pos);
        if (next_fingerprint < current_fingerprint) {
            next_memento = GET_MEMENTO(qf, pos);
            if (current_memento < next_memento)
                pos++;
            else {
                // Mementos encoded as a sorted list
                pos++;
                pos += number_of_slots_used_for_memento_list(qf, pos);
            }
        }
    } while (!is_runend(qf, pos - 1));
    return pos;
}

static inline uint64_t upper_bound_fingerprint_in_run(const QF *qf, uint64_t pos,
        uint64_t fingerprint)   // NEW IN MEMENTO
{
    uint64_t current_fingerprint, current_memento;
    do {
        current_fingerprint = GET_FINGERPRINT(qf, pos);
        current_memento = GET_MEMENTO(qf, pos);
        if (fingerprint < current_fingerprint) {
            break;
        }

        pos++;
        if (is_runend(qf, pos - 1))
            break;

        if (GET_FINGERPRINT(qf, pos) < current_fingerprint) {
            if (current_memento < GET_MEMENTO(qf, pos))
                pos++;
            else {
                // Mementos encoded as a sorted list
                pos++;
                pos += number_of_slots_used_for_memento_list(qf, pos);
            }
        }
    } while (!is_runend(qf, pos - 1));
    return pos;
}

static inline int insert_mementos(QF *qf, const __uint128_t hash,
        const uint64_t mementos[], const uint64_t memento_count, 
        const uint32_t actual_fingerprint_size, const uint8_t runtime_lock)     // NEW IN MEMENTO
{
	int ret_distance = 0;
    const uint32_t bucket_index_hash_size = qf->metadata->key_bits - qf->metadata->fingerprint_bits;
    const uint64_t hash_fingerprint = (hash >> bucket_index_hash_size) 
                                            & BITMASK(actual_fingerprint_size);
    const uint32_t orig_quotient_size = qf->metadata->original_quotient_bits;
	const uint64_t hash_bucket_index = ((hash & BITMASK(orig_quotient_size)) << (bucket_index_hash_size - orig_quotient_size))
                        | ((hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));


#ifdef DEBUG
    fprintf(stderr, "IND=%lu - FINGERPRINT=", hash_bucket_index);
    for (int i = qf->metadata->fingerprint_bits - 1; i >= 0; i--)
        fprintf(stderr, "%lu", (hash_fingerprint >> i) & 1);
    fprintf(stderr, " - EXTENSION=");
    for (int i = qf->metadata->fingerprint_bits - 1; i >= 0; i--)
        fprintf(stderr, "%lu", (uint64_t)(hash >> (i + qf->metadata->key_bits)) & 1);
    fprintf(stderr, " mementos=[");
    for (int i = 0; i < memento_count; i++)
        fprintf(stderr, "%lu, ", mementos[i]);
    fprintf(stderr, "\b\b]\n");
#endif /* DEBUG */
#ifdef DEBUG
    fprintf(stderr, "IND=%lu - FINGERPRINT=", hash_bucket_index);
    for (int i = qf->metadata->fingerprint_bits - 1; i >= 0; i--)
        fprintf(stderr, "%lu", (hash_fingerprint >> i) & 1);
    fprintf(stderr, " - EXTENSION=");
    for (int i = qf->metadata->fingerprint_bits - 1; i >= 0; i--)
        fprintf(stderr, "%lu", (uint64_t)(hash >> (i + qf->metadata->key_bits)) & 1);
    fprintf(stderr, "\n");
#endif /* DEBUG */

    uint32_t new_slot_count = memento_count, memento_unary_count = 0;
    const uint64_t max_memento_value = (1ULL << qf->metadata->memento_bits) - 1;
    if (hash_fingerprint && memento_count > 2) {
        new_slot_count = 0;
        int32_t total_new_bits = 2 * qf->metadata->bits_per_slot + 
            (memento_count - 2 + (qf->metadata->memento_bits > 2)) * qf->metadata->memento_bits;

        if (memento_count - 2 >= max_memento_value) {
            // Must take into account the extra length of the memento counter.
            // This will rarely execute
            uint32_t val = max_memento_value - 1;
            for (uint32_t tmp_cnt = val; tmp_cnt < memento_count - 2; tmp_cnt += val) {
                val *= max_memento_value;
                memento_unary_count++;
            }
            total_new_bits += 2 * memento_unary_count * qf->metadata->memento_bits;
        }

        while (total_new_bits > 0) {
            total_new_bits -= qf->metadata->bits_per_slot;
            new_slot_count++;
        }   // Result of new_slot_count same as using normal division: `total_new_bits / qf->metadata->bits_per_slot`
            // Hopefully this is a bit faster to calculate.
    }
    else if (memento_count == 2 && mementos[0] == mementos[1]) {
        new_slot_count = 3;
    }

	if (GET_NO_LOCK(runtime_lock) != QF_NO_LOCK) {
		if (!qf_lock(qf, hash_bucket_index, /*small*/ true, runtime_lock))
			return QF_COULDNT_LOCK;
	}

    // Find empty slots and shift everything to fit the new mementos
    uint64_t empty_runs[65];
    uint64_t empty_runs_ind = find_next_empty_slot_runs_of_size_n(qf, hash_bucket_index,
                                                            new_slot_count, empty_runs);
    if (empty_runs[empty_runs_ind - 2] + empty_runs[empty_runs_ind - 1] - 1
            >= qf->metadata->xnslots) {     // Check that the new data fits
        if (GET_NO_LOCK(runtime_lock) != QF_NO_LOCK) {
            qf_unlock(qf, hash_bucket_index, /*small*/ true);
        }
        return QF_NO_SPACE;
    }

    uint64_t shift_distance = 0;
    for (int i = empty_runs_ind - 2; i >= 2; i -= 2) {
        shift_distance += empty_runs[i + 1];
        shift_slots(qf, empty_runs[i - 2] + empty_runs[i - 1], empty_runs[i] - 1,
                    shift_distance);
        shift_runends(qf, empty_runs[i - 2] + empty_runs[i - 1], empty_runs[i] - 1,
                    shift_distance);
    }

    // Update offsets
    uint64_t npreceding_empties = 0;
    uint32_t empty_iter = 0;
    uint32_t last_block_to_update_offset = (empty_runs[empty_runs_ind - 2] + 
                                            empty_runs[empty_runs_ind - 1] - 1) 
                                                / QF_SLOTS_PER_BLOCK;
    for (uint64_t i = hash_bucket_index / QF_SLOTS_PER_BLOCK + 1; 
                i <= last_block_to_update_offset; i++) {
        while (npreceding_empties < new_slot_count) {
            uint64_t r = i * QF_SLOTS_PER_BLOCK;
            uint64_t l = r - QF_SLOTS_PER_BLOCK;
            uint64_t empty_run_start = empty_runs[empty_iter];
            uint64_t empty_run_end = empty_runs[empty_iter] + empty_runs[empty_iter + 1];
            if (r <= empty_run_start)
                break;
            if (l < empty_run_start)
                l = empty_run_start;
            if (r > empty_run_end) {
                r = empty_run_end;
                npreceding_empties += r - l;
                empty_iter += 2;
            }
            else {
                npreceding_empties += r - l;
                break;
            }
        }

        if (get_block(qf, i)->offset + new_slot_count - npreceding_empties 
                                < BITMASK(8 * sizeof(qf->blocks[0].offset)))
            get_block(qf, i)->offset += new_slot_count - npreceding_empties;
        else
            get_block(qf, i)->offset = BITMASK(8 * sizeof(qf->blocks[0].offset));
    }

    uint64_t runend_index = run_end(qf, hash_bucket_index);
    uint64_t runstart_index = hash_bucket_index == 0 ? 0 
                                : run_end(qf, hash_bucket_index - 1) + 1;
    uint64_t insert_index;
    if (is_occupied(qf, hash_bucket_index)) {
        insert_index = upper_bound_fingerprint_in_run(qf, runstart_index,
                                                hash_fingerprint);

        if (insert_index < empty_runs[0]) {
            shift_slots(qf, insert_index, empty_runs[0] - 1, new_slot_count);
            shift_runends(qf, insert_index, empty_runs[0] - 1, new_slot_count);
        }
        METADATA_WORD(qf, runends, runend_index) &= ~(1ULL << 
                ((runend_index % QF_SLOTS_PER_BLOCK) % 64));
        METADATA_WORD(qf, runends, runend_index + new_slot_count) |= 1ULL << 
                (((runend_index + new_slot_count) % QF_SLOTS_PER_BLOCK) % 64);
    }
    else {
        if (hash_bucket_index == empty_runs[0]) {
            insert_index = hash_bucket_index;
        }
        else {
            insert_index = runend_index + 1;
            if (insert_index < empty_runs[0]) {
                shift_slots(qf, insert_index, empty_runs[0] - 1, new_slot_count);
                shift_runends(qf, insert_index, empty_runs[0] - 1, new_slot_count);
            }
        }

        METADATA_WORD(qf, runends, insert_index + new_slot_count - 1) |= 1ULL << 
                (((insert_index + new_slot_count - 1) % QF_SLOTS_PER_BLOCK) % 64);
        METADATA_WORD(qf, occupieds, hash_bucket_index) |= 1ULL <<
                ((hash_bucket_index % QF_SLOTS_PER_BLOCK) % 64);
    }

    // Move in the payload!
    write_prefix_set(qf, insert_index, hash_fingerprint, mementos, memento_count);

    modify_metadata(qf, &qf->metadata->ndistinct_elts, 1);
    modify_metadata(qf, &qf->metadata->noccupied_slots, new_slot_count);
    modify_metadata(qf, &qf->metadata->nelts, memento_count);

	if (GET_NO_LOCK(runtime_lock) != QF_NO_LOCK) {
		qf_unlock(qf, hash_bucket_index, /*small*/ true);
	}

	return ret_distance;
}

/*************************************************************************
 * Code that uses the above to implement fingerprint-memento operations. *
 *************************************************************************/

static inline uint64_t init_filter(QF *qf, uint64_t nslots, uint64_t key_bits,
        uint64_t memento_bits, enum qf_hashmode hash_mode, uint32_t seed,
        void *buffer, uint64_t buffer_len, const uint64_t orig_quotient_bit_cnt)    // NEW IN MEMENTO
{
	uint64_t num_slots, xnslots, nblocks;
	uint64_t fingerprint_bits, bits_per_slot;
	uint64_t size;
	uint64_t total_num_bytes;

    /* nslots can be any number now, as opposed to just being able to be a power of 2! */
	num_slots = nslots;
	xnslots = nslots + 10 * sqrt((double) nslots);
	nblocks = (xnslots + QF_SLOTS_PER_BLOCK - 1) / QF_SLOTS_PER_BLOCK;
	fingerprint_bits = key_bits;
	while (nslots > 1) {
		assert(fingerprint_bits > 0);
		fingerprint_bits--;
		nslots >>= 1;
	}
    fingerprint_bits -= (popcnt(num_slots) > 1);

	bits_per_slot = fingerprint_bits + memento_bits;
	assert(QF_BITS_PER_SLOT == 0 || QF_BITS_PER_SLOT == bits_per_slot);
	assert(bits_per_slot > 1);
#if QF_BITS_PER_SLOT == 8 || QF_BITS_PER_SLOT == 16 || QF_BITS_PER_SLOT == 32 || QF_BITS_PER_SLOT == 64
	size = nblocks * sizeof(qfblock);
#else
	size = nblocks * (sizeof(qfblock) + QF_SLOTS_PER_BLOCK * bits_per_slot / 8);
#endif

	total_num_bytes = sizeof(qfmetadata) + size;
	if (buffer == NULL || total_num_bytes > buffer_len)
		return total_num_bytes;
	memset(buffer, 0, total_num_bytes);
	qf->metadata = (qfmetadata *)(buffer);
	qf->blocks = (qfblock *)(qf->metadata + 1);

	qf->metadata->magic_endian_number = MAGIC_NUMBER;
	qf->metadata->auto_resize = 0;
	qf->metadata->hash_mode = hash_mode;
	qf->metadata->total_size_in_bytes = size;
	qf->metadata->seed = seed;
	qf->metadata->nslots = num_slots;
	qf->metadata->xnslots = xnslots;
	qf->metadata->key_bits = key_bits;
	qf->metadata->original_quotient_bits = (orig_quotient_bit_cnt ?
                                              orig_quotient_bit_cnt 
                                            : key_bits - fingerprint_bits);
	qf->metadata->memento_bits = memento_bits;
	qf->metadata->fingerprint_bits = fingerprint_bits;
	qf->metadata->bits_per_slot = bits_per_slot;

	qf->metadata->range = qf->metadata->nslots;
	qf->metadata->range <<= qf->metadata->fingerprint_bits \
                            + qf->metadata->memento_bits;
	qf->metadata->nblocks = (qf->metadata->xnslots + QF_SLOTS_PER_BLOCK - 1) \
                            / QF_SLOTS_PER_BLOCK;
	qf->metadata->nelts = 0;
	qf->metadata->ndistinct_elts = 0;
	qf->metadata->noccupied_slots = 0;

	qf->runtimedata->num_locks = (qf->metadata->xnslots / NUM_SLOTS_TO_LOCK) + 2;
	qf->runtimedata->f_info.filepath = NULL;

	/* initialize all the locks to 0 */
	qf->runtimedata->metadata_lock = 0;
	qf->runtimedata->locks = (volatile int *)calloc(qf->runtimedata->num_locks, 
                                                    sizeof(volatile int));
	if (qf->runtimedata->locks == NULL) {
		perror("Couldn't allocate memory for runtime locks.");
		exit(EXIT_FAILURE);
	}
#ifdef LOG_WAIT_TIME
	qf->runtimedata->wait_times = (wait_time_data *)calloc(qf->runtimedata->num_locks + 1,
														    sizeof(wait_time_data));
	if (qf->runtimedata->wait_times == NULL) {
		perror("Couldn't allocate memory for runtime wait_times.");
		exit(EXIT_FAILURE);
	}
#endif
	return total_num_bytes;
}

uint64_t qf_init(QF *qf, uint64_t nslots, uint64_t key_bits, uint64_t memento_bits,
                 enum qf_hashmode hash_mode, uint32_t seed, void *buffer,
                 uint64_t buffer_len)   // NEW IN MEMENTO
{
    return init_filter(qf, nslots, key_bits, memento_bits, hash_mode, seed,
                        buffer, buffer_len, 0);
}

uint64_t qf_use(QF* qf, void* buffer, uint64_t buffer_len)
{
	qf->metadata = (qfmetadata *)(buffer);
	if (qf->metadata->total_size_in_bytes + sizeof(qfmetadata) > buffer_len) {
		return qf->metadata->total_size_in_bytes + sizeof(qfmetadata);
	}
	qf->blocks = (qfblock *)(qf->metadata + 1);

	qf->runtimedata = (qfruntime *)calloc(sizeof(qfruntime), 1);
	if (qf->runtimedata == NULL) {
		perror("Couldn't allocate memory for runtime data.");
		exit(EXIT_FAILURE);
	}
	/* initialize all the locks to 0 */
	qf->runtimedata->metadata_lock = 0;
	qf->runtimedata->locks = (volatile int *)calloc(qf->runtimedata->num_locks,
													sizeof(volatile int));
	if (qf->runtimedata->locks == NULL) {
		perror("Couldn't allocate memory for runtime locks.");
		exit(EXIT_FAILURE);
	}
#ifdef LOG_WAIT_TIME
	qf->runtimedata->wait_times = (wait_time_data *)calloc(qf->runtimedata->num_locks + 1,
															sizeof(wait_time_data));
	if (qf->runtimedata->wait_times == NULL) {
		perror("Couldn't allocate memory for runtime wait_times.");
		exit(EXIT_FAILURE);
	}
#endif

	return sizeof(qfmetadata) + qf->metadata->total_size_in_bytes;
}

void *qf_destroy(QF *qf)
{
	assert(qf->runtimedata->locks != NULL);
	free((void *)qf->runtimedata->locks);
	assert(qf->runtimedata != NULL);
	free(qf->runtimedata);

	return (void *)qf->metadata;
}

static inline bool malloc_filter(QF *qf, const uint64_t nslots, const uint64_t key_bits, 
        const uint64_t memento_bits, const enum qf_hashmode hash_mode, const uint32_t seed, 
        const uint64_t orig_quotient_size)  // NEW IN MEMENTO
{
	uint64_t total_num_bytes = init_filter(qf, nslots, key_bits, memento_bits,
                                    hash_mode, seed, NULL, 0, orig_quotient_size);

	void *buffer = malloc(total_num_bytes);
	if (buffer == NULL) {
		perror("Couldn't allocate memory for the CQF.");
		exit(EXIT_FAILURE);
	}

	qf->runtimedata = (qfruntime *)calloc(sizeof(qfruntime), 1);
	if (qf->runtimedata == NULL) {
		perror("Couldn't allocate memory for runtime data.");
		exit(EXIT_FAILURE);
	}

	uint64_t init_size = init_filter(qf, nslots, key_bits, memento_bits, hash_mode, 
                                seed, buffer, total_num_bytes, orig_quotient_size);

	if (init_size == total_num_bytes)
		return true;
	else
		return false;
}

bool qf_malloc(QF *qf, uint64_t nslots, uint64_t key_bits, uint64_t memento_bits, 
                enum qf_hashmode hash_mode, uint32_t seed)  // NEW IN MEMENTO
{
    return malloc_filter(qf, nslots, key_bits, memento_bits, hash_mode, seed, 0);
}

bool qf_free(QF *qf)
{
	assert(qf->metadata != NULL);
	void *buffer = qf_destroy(qf);
	if (buffer != NULL) {
		free(buffer);
		return true;
	}

	return false;
}

void qf_copy(QF *dest, const QF *src)
{
	DEBUG_CQF("%s\n","Source CQF");
	DEBUG_DUMP(src);
	memcpy(dest->runtimedata, src->runtimedata, sizeof(qfruntime));
	memcpy(dest->metadata, src->metadata, sizeof(qfmetadata));
	memcpy(dest->blocks, src->blocks, src->metadata->total_size_in_bytes);
	DEBUG_CQF("%s\n","Destination CQF after copy.");
	DEBUG_DUMP(dest);
}

void qf_reset(QF *qf)
{
	qf->metadata->nelts = 0;
	qf->metadata->ndistinct_elts = 0;
	qf->metadata->noccupied_slots = 0;

#ifdef LOG_WAIT_TIME
	memset(qf->wait_times, 0, (qf->runtimedata->num_locks + 1) 
                                * sizeof(wait_time_data));
#endif
#if QF_BITS_PER_SLOT == 8 || QF_BITS_PER_SLOT == 16 || QF_BITS_PER_SLOT == 32 || QF_BITS_PER_SLOT == 64
	memset(qf->blocks, 0, qf->metadata->nblocks* sizeof(qfblock));
#else
	memset(qf->blocks, 0, qf->metadata->nblocks*(sizeof(qfblock) + 
                QF_SLOTS_PER_BLOCK * qf->metadata->bits_per_slot / 8));
#endif
}

int64_t qf_resize_malloc(QF *qf, uint64_t nslots)   // NEW IN MEMENTO
{
#ifdef DEBUG
    uint64_t occupied_cnt = 0, runend_cnt = 0;
    for (uint32_t i = 0; i < qf->metadata->nblocks; i++) {
        occupied_cnt += popcnt(get_block(qf, i)->occupieds[0]);
        runend_cnt += popcnt(get_block(qf, i)->runends[0]);
        assert(occupied_cnt >= runend_cnt);
    }
    assert(occupied_cnt == runend_cnt);
#endif /* DEBUG */

	QF new_qf;
	if (!malloc_filter(&new_qf, nslots, qf->metadata->key_bits,
                         qf->metadata->memento_bits, qf->metadata->hash_mode,
                         qf->metadata->seed, qf->metadata->original_quotient_bits))
		return false;
	if (qf->metadata->auto_resize)
		qf_set_auto_resize(&new_qf, true);

	// copy keys from qf into new_qf
	QFi qfi;
	qf_iterator_from_position(qf, &qfi, 0);
	int64_t ret_numkeys = 0;
    uint64_t key, memento_count, mementos[1024];
	do {
		memento_count = qfi_get_hash(&qfi, &key, mementos);
#ifdef DEBUG
        fprintf(stderr, "@ run=%lu current=%lu\n", qfi.run, qfi.current);
#endif /* DEBUG */

#ifdef DEBUG
        assert(fingerprint_size < 64);
        assert(fingerprint_size > 0);

        fprintf(stderr, "MOVING PREFIX SET: hash=");
        for (int32_t i = 63; i >= 0; i--)
            fprintf(stderr, "%lu", (key >> i) & 1);
        fprintf(stderr, " fingerprint_size=%lu --- memento_count=%lu mementos=[", fingerprint_size, memento_count);
        for (uint32_t i = 0; i < memento_count; i++)
            fprintf(stderr, "%lu, ", mementos[i]);
        fprintf(stderr, "\b\b]\n");
#endif /* DEBUG */
		qfi_next(&qfi);

		int ret = insert_mementos(&new_qf, key, mementos, memento_count, 
                        new_qf.metadata->fingerprint_bits, QF_NO_LOCK | QF_KEY_IS_HASH);
		if (ret < 0) {
			fprintf(stderr, "Failed to insert key: %" PRIx64 " into the new CQF.\n", key);
			return ret;
		}
#ifdef DEBUG
        qf_dump(&new_qf);
#endif /* DEBUG */
		ret_numkeys += memento_count;
	} while(!qfi_end(&qfi));

	qf_free(qf);
	memcpy(qf, &new_qf, sizeof(QF));

#ifdef DEBUG
    perror("FINAL CHECK");
    occupied_cnt = 0, runend_cnt = 0;
    for (uint32_t i = 0; i < qf->metadata->nblocks; i++) {
        occupied_cnt += popcnt(get_block(qf, i)->occupieds[0]);
        runend_cnt += popcnt(get_block(qf, i)->runends[0]);
        assert(occupied_cnt >= runend_cnt);

        if (0 < get_block(qf, i)->offset && get_block(qf, i)->offset < 255) {
            assert(is_runend(qf, i * QF_SLOTS_PER_BLOCK + get_block(qf, i)->offset - 1));
        }
    }
    assert(occupied_cnt == runend_cnt);
#endif /* DEBUG */

	return ret_numkeys;
}

uint64_t qf_resize(QF *qf, uint64_t nslots, void* buffer, uint64_t buffer_len)  // NEW IN MEMENTO
{
	QF new_qf;
	new_qf.runtimedata = (qfruntime *)calloc(sizeof(qfruntime), 1);
	if (new_qf.runtimedata == NULL) {
		perror("Couldn't allocate memory for runtime data.\n");
		exit(EXIT_FAILURE);
	}

	uint64_t init_size = init_filter(&new_qf, nslots, qf->metadata->key_bits + 1,
                                    qf->metadata->memento_bits,
                                    qf->metadata->hash_mode, qf->metadata->seed,
                                    buffer, buffer_len, qf->metadata->original_quotient_bits);

	if (init_size > buffer_len)
		return init_size;

	if (qf->metadata->auto_resize)
		qf_set_auto_resize(&new_qf, true);

	// copy keys from qf into new_qf
	QFi qfi;
	qf_iterator_from_position(qf, &qfi, 0);
    uint64_t key, memento_count, mementos[1024], fingerprint_size;
	do {
		memento_count = qfi_get_hash(&qfi, &key, mementos);
		qfi_next(&qfi);
        fingerprint_size = highbit_position(key) - qf->metadata->key_bits 
                                        + qf->metadata->fingerprint_bits;
        if (memento_count > 1)
            fingerprint_size /= 2;

		int ret = insert_mementos(&new_qf, key, mementos, memento_count, 
                            fingerprint_size - 1, QF_NO_LOCK | QF_KEY_IS_HASH);
		if (ret < 0) {
			fprintf(stderr, "Failed to insert key: %" PRIx64 " into the new CQF.\n", key);
			abort();
		}
	} while(!qfi_end(&qfi));

	qf_free(qf);
	memcpy(qf, &new_qf, sizeof(QF));

	return init_size;
}

void qf_set_auto_resize(QF* qf, bool enabled)
{
	if (enabled)
		qf->metadata->auto_resize = 1;
	else
		qf->metadata->auto_resize = 0;
}

int qf_insert_mementos(QF *qf, uint64_t key, uint64_t mementos[], uint64_t memento_count, 
                        uint8_t flags)  // NEW IN MEMENTO
{
    uint32_t new_slot_count = 1 + (memento_count + 1) / 2;
	// We fill up the CQF up to 95% load factor.
	// This is a very conservative check.
	if (qf->metadata->noccupied_slots >= qf->metadata->nslots * 0.95 ||
            qf->metadata->noccupied_slots + new_slot_count >= qf->metadata->nslots) {
		if (qf->metadata->auto_resize) {
			fprintf(stdout, "Resizing the CQF.\n");
			qf_resize_malloc(qf, qf->metadata->nslots * 2);
		} else {
			return QF_NO_SPACE;
        }
	}
	if (memento_count == 0)
		return 0;

	if (GET_KEY_HASH(flags) != QF_KEY_IS_HASH) {
		if (qf->metadata->hash_mode == QF_HASH_DEFAULT)
			key = MurmurHash64A(((void *)&key), sizeof(key), qf->metadata->seed);
		else if (qf->metadata->hash_mode == QF_HASH_INVERTIBLE)
            // Large hash!
			key = hash_64(key, BITMASK(63));
	}
    const uint64_t orig_nslots = qf->metadata->nslots >> (qf->metadata->key_bits 
                                                        - qf->metadata->fingerprint_bits 
                                                        - qf->metadata->original_quotient_bits);
    uint64_t fast_reduced_part = fast_reduce(((key & BITMASK(qf->metadata->original_quotient_bits)) 
                                << (32 - qf->metadata->original_quotient_bits)), orig_nslots);
    key &= ~(BITMASK(qf->metadata->original_quotient_bits));
    key |= fast_reduced_part;
	uint64_t hash = key;
#ifdef DEBUG
    fprintf(stderr, "KEY HASH=%lu\n", hash);
#endif /* DEBUG */
	int ret = insert_mementos(qf, hash, mementos, memento_count, 
                                qf->metadata->fingerprint_bits, flags);
#ifdef DEBUG
    perror("DONE!");
#endif /* DEBUG */

	// check for fullness based on the distance from the home slot to the slot
	// in which the key is inserted
	if (ret > DISTANCE_FROM_HOME_SLOT_CUTOFF) {
		if (qf->metadata->auto_resize) {
			fprintf(stdout, "Resizing the CQF.\n");
			qf_resize_malloc(qf, qf->metadata->nslots * 2);
		} else {
			fprintf(stderr, "The CQF is filling up.\n");
		}
	}
	return ret;
}

int64_t qf_insert_single(QF *qf, uint64_t key, uint64_t memento, uint8_t flags)     // NEW IN MEMENTO
{
#ifdef DEBUG
    uint64_t occupied_cnt = 0, runend_cnt = 0;
    for (uint32_t i = 0; i < qf->metadata->nblocks; i++) {
        occupied_cnt += popcnt(get_block(qf, i)->occupieds[0]);
        runend_cnt += popcnt(get_block(qf, i)->runends[0]);
        assert(occupied_cnt >= runend_cnt);
    }
    assert(occupied_cnt == runend_cnt);
#endif /* DEBUG */

	// We fill up the CQF up to 95% load factor.
	// This is a very conservative check.
	if (qf->metadata->noccupied_slots >= qf->metadata->nslots * 0.95 ||
            qf->metadata->noccupied_slots + 1 >= qf->metadata->nslots) {
		if (qf->metadata->auto_resize) {
			fprintf(stderr, "========================================================= Resizing the CQF.\n");
            fflush(stderr);
			qf_resize_malloc(qf, qf->metadata->nslots * 2);
            perror("RESIZING DONE");
		} else {
			return QF_NO_SPACE;
        }
	}

	if (GET_KEY_HASH(flags) != QF_KEY_IS_HASH) {
		if (qf->metadata->hash_mode == QF_HASH_DEFAULT) {
			key = MurmurHash64A(((void *)&key), sizeof(key), qf->metadata->seed);
        }
		else if (qf->metadata->hash_mode == QF_HASH_INVERTIBLE)
            // Large hash!
			key = hash_64(key, BITMASK(63));
	}
    const uint64_t orig_nslots = qf->metadata->nslots >> (qf->metadata->key_bits 
                                                        - qf->metadata->fingerprint_bits 
                                                        - qf->metadata->original_quotient_bits);
    const uint64_t fast_reduced_part = fast_reduce(((key & BITMASK(qf->metadata->original_quotient_bits)) 
                                << (32 - qf->metadata->original_quotient_bits)), orig_nslots);
    key &= ~(BITMASK(qf->metadata->original_quotient_bits));
    key |= fast_reduced_part;
	uint64_t hash = key;

	int64_t res = 0;
    const uint32_t bucket_index_hash_size = qf->metadata->key_bits - qf->metadata->fingerprint_bits;
    const uint64_t hash_fingerprint = (hash >> bucket_index_hash_size) & BITMASK(qf->metadata->fingerprint_bits);
    const uint32_t orig_quotient_size = qf->metadata->original_quotient_bits;
	const uint64_t hash_bucket_index = (fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                        | ((hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));


#ifdef DEBUG
    fprintf(stderr, "noccupied_slots=%lu ||| INSERTING SINGLE key=%lu memento=%lu --- hash_bucket_index=%lu hash_fingerprint=", qf->metadata->noccupied_slots, key, memento, hash_bucket_index);
    for (int i = qf->metadata->fingerprint_bits - 1; i >= 0; i--)
        fprintf(stderr, "%lu", (hash_fingerprint >> i) & 1);
    fprintf(stderr, "\n");
    fflush(stderr);
#endif /* DEBUG */

	if (GET_NO_LOCK(flags) != QF_NO_LOCK) {
		if (!qf_lock(qf, hash_bucket_index, /*small*/ true, flags))
			return QF_COULDNT_LOCK;
	}

    uint64_t runend_index = run_end(qf, hash_bucket_index);
    uint64_t runstart_index = hash_bucket_index == 0 ? 0 
                                : run_end(qf, hash_bucket_index - 1) + 1;
    uint64_t insert_index;
    if (is_occupied(qf, hash_bucket_index)) {
        int64_t fingerprint_pos = runstart_index;
        bool add_to_sorted_list = false;
        fingerprint_pos = next_matching_fingerprint_in_run(qf, fingerprint_pos,
                                                            hash_fingerprint);
        if (fingerprint_pos >= 0 && hash_fingerprint) {
            add_to_sorted_list = true;
            insert_index = fingerprint_pos;
        }

        if (add_to_sorted_list) {
            // Matching sorted list with a complete fingerprint target 
            res = add_memento_to_sorted_list(qf, hash_bucket_index, insert_index,
                                                                        memento);

            if (res < 0)
                return res;
            res = insert_index - hash_bucket_index;
        }
        else {
            // No fully matching fingerprints found
            insert_index = upper_bound_fingerprint_in_run(qf, runstart_index,
                                                            hash_fingerprint);
            const uint64_t next_empty_slot = find_first_empty_slot(qf, hash_bucket_index);
#ifdef DEBUG
            assert(next_empty_slot >= insert_index);
#endif /* DEBUG */

            if (insert_index < next_empty_slot) {
                shift_slots(qf, insert_index, next_empty_slot - 1, 1);
                shift_runends(qf, insert_index, next_empty_slot - 1, 1);
            }
            for (uint32_t i = hash_bucket_index / QF_SLOTS_PER_BLOCK + 1; 
                    i <= next_empty_slot / QF_SLOTS_PER_BLOCK; i++) {
                if (get_block(qf, i)->offset + 1
                                <= BITMASK(8 * sizeof(qf->blocks[0].offset)))
                    get_block(qf, i)->offset++;
            }
            set_slot(qf, insert_index, (hash_fingerprint << qf->metadata->memento_bits) 
                                        | memento);
            METADATA_WORD(qf, runends, runend_index) &= ~(1ULL << 
                    ((runend_index % QF_SLOTS_PER_BLOCK) % 64));
            METADATA_WORD(qf, runends, runend_index + 1) |= 1ULL << 
                    (((runend_index + 1) % QF_SLOTS_PER_BLOCK) % 64);
            modify_metadata(qf, &qf->metadata->ndistinct_elts, 1);
            modify_metadata(qf, &qf->metadata->noccupied_slots, 1);
            res = insert_index - hash_bucket_index;
        }
    }
    else {
        const uint64_t next_empty_slot = find_first_empty_slot(qf, hash_bucket_index);
#ifdef DEBUG
        assert(next_empty_slot >= hash_bucket_index);
#endif /* DEBUG */
        if (hash_bucket_index == next_empty_slot) {
            insert_index = hash_bucket_index;
        }
        else {
            insert_index = runend_index + 1;
            if (insert_index < next_empty_slot) {
                shift_slots(qf, insert_index, next_empty_slot - 1, 1);
                shift_runends(qf, insert_index, next_empty_slot - 1, 1);
            }
        }
        set_slot(qf, insert_index, (hash_fingerprint << qf->metadata->memento_bits)
                                                                    | memento);

        for (uint32_t i = hash_bucket_index / QF_SLOTS_PER_BLOCK + 1; 
                i <= next_empty_slot / QF_SLOTS_PER_BLOCK; i++) {
            if (get_block(qf, i)->offset + 1
                    <= BITMASK(8 * sizeof(qf->blocks[0].offset)))
                get_block(qf, i)->offset++;
        }
        METADATA_WORD(qf, runends, insert_index) |= 1ULL << 
                ((insert_index % QF_SLOTS_PER_BLOCK) % 64);
        METADATA_WORD(qf, occupieds, hash_bucket_index) |= 1ULL <<
                ((hash_bucket_index % QF_SLOTS_PER_BLOCK) % 64);
        modify_metadata(qf, &qf->metadata->ndistinct_elts, 1);
        modify_metadata(qf, &qf->metadata->noccupied_slots, 1);
        res = insert_index - hash_bucket_index;
    }

	if (GET_NO_LOCK(flags) != QF_NO_LOCK) {
		qf_unlock(qf, hash_bucket_index, /*small*/ true);
	}

    modify_metadata(qf, &qf->metadata->nelts, 1);
    return res;
}

void qf_bulk_load(QF *qf, uint64_t *sorted_hashes, uint64_t n, uint8_t flags)   // NEW IN MEMENTO
{
    assert(flags & QF_KEY_IS_HASH);

    const uint64_t fingerprint_mask = BITMASK(qf->metadata->fingerprint_bits);
    const uint64_t memento_mask = BITMASK(qf->metadata->memento_bits);

    uint64_t prefix = sorted_hashes[0] >> qf->metadata->memento_bits;
    uint64_t memento_list[10 * (1ULL << qf->metadata->memento_bits)];
    uint32_t prefix_set_size = 1;
    memento_list[0] = sorted_hashes[0] & memento_mask;
	uint64_t current_run = prefix >> qf->metadata->fingerprint_bits;
    uint64_t current_pos = current_run, old_pos = 0, next_run;
    uint64_t distinct_prefix_cnt = 0, total_slots_written = 0;
    for (uint64_t i = 1; i < n; i++) {
        const uint64_t new_prefix = sorted_hashes[i] >> qf->metadata->memento_bits;
        if (new_prefix == prefix)
            memento_list[prefix_set_size++] = sorted_hashes[i] & memento_mask;
        else {
            const uint32_t slots_written = write_prefix_set(qf, current_pos,
                                                prefix & fingerprint_mask, 
                                                memento_list, prefix_set_size);
            current_pos += slots_written;
            total_slots_written += slots_written;
            prefix = new_prefix;
            prefix_set_size = 1;
            memento_list[0] = sorted_hashes[i] & memento_mask;

            next_run = prefix >> qf->metadata->fingerprint_bits;
            if (current_run != next_run) {
                METADATA_WORD(qf, occupieds, current_run) |= 
                            (1ULL << ((current_run % QF_SLOTS_PER_BLOCK) % 64));
                METADATA_WORD(qf, runends, (current_pos - 1)) |= 
                            (1ULL << (((current_pos - 1) % QF_SLOTS_PER_BLOCK) % 64));
                for (uint64_t block_ind = current_run / QF_SLOTS_PER_BLOCK + 1;
                        block_ind <= (current_pos - 1) / QF_SLOTS_PER_BLOCK; block_ind++) {
                    const uint32_t cnt = current_pos - (block_ind * QF_SLOTS_PER_BLOCK < old_pos ? old_pos 
                                                                           : block_ind * QF_SLOTS_PER_BLOCK);
                    if (get_block(qf, block_ind)->offset + cnt
                            < BITMASK(8 * sizeof(qf->blocks[0].offset)))
                        get_block(qf, block_ind)->offset += cnt;
                    else
                        get_block(qf, block_ind)->offset = BITMASK(8 * sizeof(qf->blocks[0].offset));
                }
                current_run = next_run;
                old_pos = current_pos;
                current_pos = (current_pos < current_run ? current_run : current_pos);
            }
        }
    }
    const uint32_t slots_written = write_prefix_set(qf, current_pos,
                                                prefix & fingerprint_mask, 
                                                memento_list, prefix_set_size);
    current_pos += slots_written;
    total_slots_written += slots_written;
    METADATA_WORD(qf, occupieds, current_run) |= 
                        (1ULL << ((current_run % QF_SLOTS_PER_BLOCK) % 64));
    METADATA_WORD(qf, runends, (current_pos - 1)) |= 
                        (1ULL << (((current_pos - 1) % QF_SLOTS_PER_BLOCK) % 64));
    for (uint64_t block_ind = current_run / QF_SLOTS_PER_BLOCK + 1;
            block_ind <= (current_pos - 1) / QF_SLOTS_PER_BLOCK; block_ind++) {
        const uint32_t cnt = current_pos - (block_ind * QF_SLOTS_PER_BLOCK < old_pos ? old_pos 
                                                            : block_ind * QF_SLOTS_PER_BLOCK);
        if (get_block(qf, block_ind)->offset + cnt
                < BITMASK(8 * sizeof(qf->blocks[0].offset)))
            get_block(qf, block_ind)->offset += cnt;
        else
            get_block(qf, block_ind)->offset = BITMASK(8 * sizeof(qf->blocks[0].offset));
    }

    modify_metadata(qf, &qf->metadata->ndistinct_elts, distinct_prefix_cnt);
    modify_metadata(qf, &qf->metadata->noccupied_slots, total_slots_written);
    modify_metadata(qf, &qf->metadata->nelts, n);
}

int32_t qf_delete_single(QF *qf, uint64_t key, uint64_t memento, uint8_t flags)     // NEW IN MEMENTO
{
#ifdef DEBUG
    fprintf(stderr, "DELETING SINGLE MEMENTO %lu\n", memento);
#endif /* DEBUG */

	if (GET_KEY_HASH(flags) != QF_KEY_IS_HASH) {
		if (qf->metadata->hash_mode == QF_HASH_DEFAULT)
			key = MurmurHash64A(((void *)&key), sizeof(key), qf->metadata->seed);
		else if (qf->metadata->hash_mode == QF_HASH_INVERTIBLE)
            // Large hash!
			key = hash_64(key, BITMASK(63));
	}
    const uint32_t bucket_index_hash_size = qf->metadata->key_bits - qf->metadata->fingerprint_bits;
    const uint32_t orig_quotient_size = qf->metadata->original_quotient_bits;
    const uint64_t orig_nslots = qf->metadata->nslots >> (qf->metadata->key_bits 
                                                        - qf->metadata->fingerprint_bits 
                                                        - qf->metadata->original_quotient_bits);
    const uint64_t fast_reduced_part = fast_reduce(((key & BITMASK(orig_quotient_size)) 
                                << (32 - orig_quotient_size)), orig_nslots);
    key &= ~(BITMASK(qf->metadata->original_quotient_bits));
    key |= fast_reduced_part;
	uint64_t hash = key;
	const uint64_t hash_bucket_index = (fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                        | ((hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
	const uint64_t hash_fingerprint = (hash >> bucket_index_hash_size) & BITMASK(qf->metadata->fingerprint_bits); 

	if (GET_NO_LOCK(flags) != QF_NO_LOCK) {
		if (!qf_lock(qf, hash_bucket_index, /*small*/ true, flags))
			return QF_COULDNT_LOCK;
	}

    int64_t runstart_index = hash_bucket_index == 0 ? 0 
                                    : run_end(qf, hash_bucket_index - 1) + 1;
    uint64_t fingerprint_pos = runstart_index;
    uint64_t sorted_positions[50], ind = 0;
    while (true) {
        fingerprint_pos = next_matching_fingerprint_in_run(qf, fingerprint_pos,
                                                            hash_fingerprint);
        if (fingerprint_pos < 0) {
            // Matching fingerprints exhausted
            break;
        }
        sorted_positions[ind++] = fingerprint_pos;
        const uint64_t current_fingerprint = GET_FINGERPRINT(qf, fingerprint_pos);
        const uint64_t next_fingerprint = GET_FINGERPRINT(qf, fingerprint_pos + 1);
        if (!is_runend(qf, fingerprint_pos) && 
                current_fingerprint > next_fingerprint) {
            const uint64_t m1 = GET_MEMENTO(qf, fingerprint_pos);
            const uint64_t m2 = GET_MEMENTO(qf, fingerprint_pos + 1);
            fingerprint_pos += 2;
            if (m1 >= m2)
                fingerprint_pos += number_of_slots_used_for_memento_list(qf, fingerprint_pos);
        }
        else {
            fingerprint_pos++;
        }

        if (is_runend(qf, fingerprint_pos - 1))
            break;
    }

    bool handled = false;
    for (int32_t i = ind - 1; i >= 0; i--) {
        int32_t old_slot_count, new_slot_count;
        remove_mementos_from_prefix_set(qf, sorted_positions[i], &memento,
                            &handled, 1, &new_slot_count, &old_slot_count);
        if (handled) {
            if (new_slot_count < old_slot_count) {
                int32_t operation = ((new_slot_count == 0) && 
                                    (run_end(qf, hash_bucket_index) - runstart_index + 1 == old_slot_count));
                remove_replace_slots_and_shift_remainders_and_runends_and_offsets(qf, 
                        operation, hash_bucket_index, sorted_positions[i] + new_slot_count,
                        NULL, 0, old_slot_count - new_slot_count);
            }
            break;
        }
    }

	if (GET_NO_LOCK(flags) != QF_NO_LOCK) {
		qf_unlock(qf, hash_bucket_index, /*small*/ true);
	}

    return handled ? 0 : QF_DOESNT_EXIST;
}

// Assumes that the fingerprint has been extended using mementos. If there is no 
// upper bound for the target, Returns the maximum memento, which is smaller 
// than it.
__attribute__((always_inline))
static inline uint64_t lower_bound_mementos_for_fingerprint(const QF *qf, uint64_t pos,
                                                            uint64_t target_memento)    // NEW IN MEMENTO
{
    uint64_t current_memento = GET_MEMENTO(qf, pos);
    uint64_t next_memento = GET_MEMENTO(qf, pos + 1);
    if (current_memento < next_memento) {
        if (target_memento <= current_memento)
            return current_memento;
        else
            return next_memento;
    }
    else {
        // Mementos encoded as a sorted list
        if (target_memento <= next_memento)
            return next_memento;
        uint64_t max_memento = current_memento;
        if (max_memento <= target_memento)
            return max_memento;
        
        pos += 2;
        const uint64_t max_memento_value = (1ULL << qf->metadata->memento_bits) - 1;
        uint64_t current_slot = get_slot(qf, pos);
        uint64_t mementos_left = (current_slot & BITMASK(qf->metadata->memento_bits));
        current_slot >>= qf->metadata->memento_bits;
        uint32_t current_full_bits = qf->metadata->bits_per_slot - qf->metadata->memento_bits;

        // Check for an extended memento counter
        if (mementos_left == max_memento_value) {
            // Rarely every executes, as slot counts rarely exceed the maximum
            // value that a memento can hold
            uint64_t length = 2, pw = 1;
            mementos_left = 0;
            pos++;
            while (length > 0) {
                if (current_full_bits < qf->metadata->memento_bits) {
                    current_slot |= get_slot(qf, pos) << current_full_bits;
                    current_full_bits += qf->metadata->bits_per_slot;
                    pos++;
                }
                uint64_t current_part = current_slot & max_memento_value;
                if (current_part == max_memento_value) {
                    length++;
                }
                else {
                    mementos_left += pw * current_part;
                    pw *= max_memento_value;
                    length--;
                }
                current_slot >>= qf->metadata->memento_bits;
                current_full_bits -= qf->metadata->memento_bits;
            }
        }

        do {
            if (current_full_bits < qf->metadata->memento_bits) {
                pos++;
                current_slot |= get_slot(qf, pos) << current_full_bits;
                current_full_bits += qf->metadata->bits_per_slot;
            }
            current_memento = current_slot & BITMASK(qf->metadata->memento_bits);
            current_slot >>= qf->metadata->memento_bits;
            current_full_bits -= qf->metadata->memento_bits;
            if (target_memento <= current_memento)
                return current_memento;
            mementos_left--;
        } while (mementos_left);
        return max_memento;
    }
}

int qf_point_query(const QF *qf, uint64_t key, uint64_t memento, uint8_t flags)     // NEW IN MEMENTO
{
	if (GET_KEY_HASH(flags) != QF_KEY_IS_HASH) {
		if (qf->metadata->hash_mode == QF_HASH_DEFAULT)
			key = MurmurHash64A(((void *)&key), sizeof(key), qf->metadata->seed);
		else if (qf->metadata->hash_mode == QF_HASH_INVERTIBLE)
			key = hash_64(key, BITMASK(63));
	}
	const uint64_t hash = key;
    const uint32_t bucket_index_hash_size = qf->metadata->key_bits - \
                                            qf->metadata->fingerprint_bits;
    const uint32_t orig_quotient_size = qf->metadata->original_quotient_bits;
    const uint64_t orig_nslots = qf->metadata->nslots >> (qf->metadata->key_bits 
                                                        - qf->metadata->fingerprint_bits 
                                                        - qf->metadata->original_quotient_bits);
    const uint64_t fast_reduced_part = fast_reduce(((hash & BITMASK(qf->metadata->original_quotient_bits)) 
                                << (32 - qf->metadata->original_quotient_bits)), orig_nslots);
	const uint64_t hash_bucket_index = (fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                        | ((hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));

#ifdef DEBUG
    fprintf(stderr, "POINT QUERY: bucket_index=%lu fingerprint=", hash_bucket_index);
    for (int i = 2 * qf->metadata->fingerprint_bits - 1; i >= 0; i--) {
        fprintf(stderr, "%lu", (hash >> (i + bucket_index_hash_size)) & 1);
    }
    fprintf(stderr, " memento=%lu\n", memento);
#endif /* DEBUG */

	if (!is_occupied(qf, hash_bucket_index))
		return false;

    const uint64_t hash_fingerprint = (hash >> bucket_index_hash_size) & BITMASK(qf->metadata->fingerprint_bits);
#ifdef DEBUG
    PRINT_WORD_BITS(hash_fingerprint);
#endif /* DEBUG */

    int64_t runstart_index = hash_bucket_index == 0 ? 0 
                                    : run_end(qf, hash_bucket_index - 1) + 1;
	if (runstart_index < hash_bucket_index)
		runstart_index = hash_bucket_index;
    
    // Find the shortest matching fingerprint that gives a positive
    int64_t fingerprint_pos = runstart_index;
    while (true) {
#ifdef DEBUG
        fprintf(stderr, "WELP fingerprint_pos=%lu\n", fingerprint_pos);
#endif /* DEBUG */
        fingerprint_pos = next_matching_fingerprint_in_run(qf, fingerprint_pos,
                                                            hash_fingerprint);
        if (fingerprint_pos < 0) {
            // Matching fingerprints exhausted
            break;
        }
        
#ifdef DEBUG
        fprintf(stderr, "MATCHING fingerprint_pos=%lu\n", fingerprint_pos);
#endif /* DEBUG */

        const uint64_t current_fingerprint = GET_FINGERPRINT(qf, fingerprint_pos);
        const uint64_t next_fingerprint = GET_FINGERPRINT(qf, fingerprint_pos + 1);
        const int positive_res = (highbit_position(current_fingerprint) == qf->metadata->fingerprint_bits 
                                    ? 1 : 2);
        if (!is_runend(qf, fingerprint_pos) && 
                current_fingerprint > next_fingerprint) {
            if (lower_bound_mementos_for_fingerprint(qf, fingerprint_pos, memento) == memento)
                return positive_res;

            const uint64_t m1 = GET_MEMENTO(qf, fingerprint_pos);
            const uint64_t m2 = GET_MEMENTO(qf, fingerprint_pos + 1);
            fingerprint_pos += 2;
            if (m1 >= m2)
                fingerprint_pos += number_of_slots_used_for_memento_list(qf, fingerprint_pos);
        }
        else {
            if (GET_MEMENTO(qf, fingerprint_pos) == memento)
                return positive_res;
            fingerprint_pos++;
        }

        if (is_runend(qf, fingerprint_pos - 1))
            break;
    }

    return 0;
}

int qf_range_query(const QF *qf, uint64_t l_key, uint64_t l_memento,
                                  uint64_t r_key, uint64_t r_memento, uint8_t flags)    // NEW IN MEMENTO
{
    const uint64_t orig_l_key = l_key;
    const uint64_t orig_r_key = r_key;
	if (GET_KEY_HASH(flags) != QF_KEY_IS_HASH) {
		if (qf->metadata->hash_mode == QF_HASH_DEFAULT) {
			l_key = MurmurHash64A(((void *) &l_key), sizeof(l_key), qf->metadata->seed);
			r_key = MurmurHash64A(((void *) &r_key), sizeof(r_key), qf->metadata->seed);
        }
		else if (qf->metadata->hash_mode == QF_HASH_INVERTIBLE) {
			l_key = hash_64(l_key, BITMASK(63));
			r_key = hash_64(r_key, BITMASK(63));
        }
	}
    const uint32_t bucket_index_hash_size = qf->metadata->key_bits - \
                                            qf->metadata->fingerprint_bits;
    const uint32_t orig_quotient_size = qf->metadata->original_quotient_bits;
    const uint64_t orig_nslots = qf->metadata->nslots >> (qf->metadata->key_bits 
                                                        - qf->metadata->fingerprint_bits 
                                                        - qf->metadata->original_quotient_bits);

	const uint64_t l_hash = l_key;
    const uint64_t l_fast_reduced_part = fast_reduce(((l_hash & BITMASK(qf->metadata->original_quotient_bits)) 
                                << (32 - qf->metadata->original_quotient_bits)), orig_nslots);
	const uint64_t l_hash_bucket_index = (l_fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                        | ((l_hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
	const uint64_t l_hash_fingerprint = (l_hash >> bucket_index_hash_size) & BITMASK(qf->metadata->fingerprint_bits);

	const uint64_t r_hash = r_key;
    const uint64_t r_fast_reduced_part = fast_reduce(((r_hash & BITMASK(qf->metadata->original_quotient_bits)) 
                                << (32 - qf->metadata->original_quotient_bits)), orig_nslots);
	const uint64_t r_hash_bucket_index = (r_fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                        | ((r_hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
	const uint64_t r_hash_fingerprint = (r_hash >> bucket_index_hash_size) & BITMASK(qf->metadata->fingerprint_bits);

    uint64_t candidate_memento;
    if (l_hash == r_hash) { // Range contained in a single prefix.
#ifdef DEBUG
        perror("RANGE QUERY: SINGLE PREFIX");
#endif /* DEBUG */
        if (!is_occupied(qf, l_hash_bucket_index)) {
            return 0;
        }

        int64_t runstart_index = l_hash_bucket_index == 0 ? 0 
            : run_end(qf, l_hash_bucket_index - 1) + 1;
        if (runstart_index < l_hash_bucket_index)
            runstart_index = l_hash_bucket_index;

        // Find the shortest matching fingerprint that gives a positive
        int64_t fingerprint_pos = runstart_index;
        while (true) {
            fingerprint_pos = next_matching_fingerprint_in_run(qf, fingerprint_pos,
                                                            l_hash_fingerprint);
            if (fingerprint_pos < 0) {
                // Matching fingerprints exhausted
                break;
            }

            const uint64_t current_fingerprint = GET_FINGERPRINT(qf, fingerprint_pos);
            const uint64_t next_fingerprint = GET_FINGERPRINT(qf, fingerprint_pos + 1);
            const int positive_res = (highbit_position(current_fingerprint) == qf->metadata->fingerprint_bits 
                                        ? 1 : 2);
            if (!is_runend(qf, fingerprint_pos) && 
                    current_fingerprint > next_fingerprint) {
                candidate_memento = lower_bound_mementos_for_fingerprint(qf, 
                                                    fingerprint_pos, l_memento);
                if (l_memento <= candidate_memento && candidate_memento <= r_memento)
                    return positive_res;

                const uint64_t m1 = GET_MEMENTO(qf, fingerprint_pos);
                const uint64_t m2 = GET_MEMENTO(qf, fingerprint_pos + 1);
                fingerprint_pos += 2;
                if (m1 >= m2)
                    fingerprint_pos += number_of_slots_used_for_memento_list(qf,
                                                                fingerprint_pos);
            }
            else {
                candidate_memento = GET_MEMENTO(qf, fingerprint_pos);
                if (l_memento <= candidate_memento && candidate_memento <= r_memento)
                    return positive_res;
                fingerprint_pos++;
            }

            if (is_runend(qf, fingerprint_pos - 1))
                break;
        }
        return 0;
    }
    else {  // Range intersects two prefixes
#ifdef DEBUG
        perror("RANGE QUERY: TWO PREFIX");
#endif /* DEBUG */
        uint64_t l_runstart_index, r_runstart_index;
        if (!is_occupied(qf, l_hash_bucket_index))
            l_runstart_index = qf->metadata->xnslots + 100;
        else {
            l_runstart_index = l_hash_bucket_index == 0 ? 0 
                : run_end(qf, l_hash_bucket_index - 1) + 1;
            if (l_runstart_index < l_hash_bucket_index)
                l_runstart_index = l_hash_bucket_index;
        }
        if (!is_occupied(qf, r_hash_bucket_index))
            r_runstart_index = qf->metadata->xnslots + 100;
        else {
            r_runstart_index = r_hash_bucket_index == 0 ? 0 
                : run_end(qf, r_hash_bucket_index - 1) + 1;
            if (r_runstart_index < r_hash_bucket_index)
                r_runstart_index = r_hash_bucket_index;
        }

        // Check the left prefix
        if (l_runstart_index < qf->metadata->xnslots) {
            // Find the shortest matching fingerprint that gives a positive
            int64_t fingerprint_pos = l_runstart_index;
            while (true) {
                fingerprint_pos = next_matching_fingerprint_in_run(qf, fingerprint_pos,
                                                                l_hash_fingerprint);
                if (fingerprint_pos < 0) {
                    // Matching fingerprints exhausted
                    break;
                }

                const uint64_t current_fingerprint = GET_FINGERPRINT(qf, fingerprint_pos);
                const uint64_t next_fingerprint = GET_FINGERPRINT(qf, fingerprint_pos + 1);
                const int positive_res = (highbit_position(current_fingerprint) == qf->metadata->fingerprint_bits 
                                            ? 1 : 2);
                if (!is_runend(qf, fingerprint_pos) && 
                        current_fingerprint > next_fingerprint) {
                    uint64_t m1 = GET_MEMENTO(qf, fingerprint_pos);
                    uint64_t m2 = GET_MEMENTO(qf, fingerprint_pos + 1);

                    bool has_sorted_list = m1 >= m2;
                    if (has_sorted_list && l_memento <= m1)
                        return positive_res;
                    if (!has_sorted_list && l_memento <= m2)
                        return positive_res;

                    fingerprint_pos += 2;
                    if (has_sorted_list)
                        fingerprint_pos += number_of_slots_used_for_memento_list(qf,
                                                                    fingerprint_pos);
                }
                else {
                    if (l_memento <= GET_MEMENTO(qf, fingerprint_pos))
                        return positive_res;
                    fingerprint_pos++;
                }

                if (is_runend(qf, fingerprint_pos - 1))
                    break;
            }
        }

        // Check middle prefixes, if they exist
        for (uint64_t mid_key = orig_l_key + 1; mid_key < orig_r_key; mid_key++) {
            uint64_t mid_hash = mid_key;
            if (GET_KEY_HASH(flags) != QF_KEY_IS_HASH) {
                if (qf->metadata->hash_mode == QF_HASH_DEFAULT) {
                    mid_hash = MurmurHash64A(((void *) &mid_hash), sizeof(mid_hash), qf->metadata->seed);
                }
                else if (qf->metadata->hash_mode == QF_HASH_INVERTIBLE) {
                    mid_hash = hash_64(mid_hash, BITMASK(63));
                }
            }
            const uint64_t mid_fast_reduced_part = fast_reduce(((mid_hash & BITMASK(qf->metadata->original_quotient_bits)) 
                        << (32 - qf->metadata->original_quotient_bits)), orig_nslots);
            const uint64_t mid_hash_bucket_index = (mid_fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                | ((mid_hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
            const uint64_t mid_hash_fingerprint = (mid_hash >> bucket_index_hash_size) & BITMASK(qf->metadata->fingerprint_bits);

            if (!is_occupied(qf, mid_hash_bucket_index))
                continue;
            uint64_t mid_runstart_index = mid_hash_bucket_index == 0 ? 0 
                : run_end(qf, mid_hash_bucket_index - 1) + 1;
            if (mid_runstart_index < mid_hash_bucket_index)
                mid_runstart_index = mid_hash_bucket_index;

            // Check the current middle prefix
            if (mid_runstart_index < qf->metadata->xnslots) {
                // Find a matching fingerprint
                int64_t fingerprint_pos = next_matching_fingerprint_in_run(qf, mid_runstart_index, mid_hash_fingerprint);
                if (fingerprint_pos >= 0) {
                    // A matching fingerprint exists
                    return true;
                }
            }
        }

        // Check the right prefix
        if (r_runstart_index < qf->metadata->xnslots) {
            // Find the shortest matching fingerprint that gives a positive
            int64_t fingerprint_pos = r_runstart_index;
            while (true) {
                fingerprint_pos = next_matching_fingerprint_in_run(qf, fingerprint_pos,
                                                                r_hash_fingerprint);
                if (fingerprint_pos < 0) {
                    // Matching fingerprints exhausted
                    break;
                }

                const uint64_t current_fingerprint = GET_FINGERPRINT(qf, fingerprint_pos);
                const uint64_t next_fingerprint = GET_FINGERPRINT(qf, fingerprint_pos + 1);
                const int positive_res = (highbit_position(current_fingerprint) == qf->metadata->fingerprint_bits 
                                            ? 1 : 2);
                if (!is_runend(qf, fingerprint_pos) && 
                        current_fingerprint > next_fingerprint) {
                    uint64_t m1 = GET_MEMENTO(qf, fingerprint_pos);
                    uint64_t m2 = GET_MEMENTO(qf, fingerprint_pos + 1);
                    bool has_sorted_list = m1 >= m2;

                    if (has_sorted_list && m2 <= r_memento)
                        return positive_res;
                    if (!has_sorted_list && m1 <= r_memento)
                        return positive_res;

                    fingerprint_pos += 2;
                    if (has_sorted_list)
                        fingerprint_pos += number_of_slots_used_for_memento_list(qf,
                                                                    fingerprint_pos);
                }
                else {
                    if (GET_MEMENTO(qf, fingerprint_pos) <= r_memento)
                        return positive_res;
                    fingerprint_pos++;
                }

                if (is_runend(qf, fingerprint_pos - 1))
                    break;
            }
        }

        return false;
    }
}

/* Getters */
enum qf_hashmode qf_get_hashmode(const QF *qf) {
	return qf->metadata->hash_mode;
}
uint64_t qf_get_hash_seed(const QF *qf) {
	return qf->metadata->seed;
}
__uint128_t qf_get_hash_range(const QF *qf) {
	return qf->metadata->range;
}

bool qf_is_auto_resize_enabled(const QF *qf) {
	if (qf->metadata->auto_resize == 1)
		return true;
	return false;
}
uint64_t qf_get_total_size_in_bytes(const QF *qf) {
	return qf->metadata->total_size_in_bytes;
}
uint64_t qf_get_nslots(const QF *qf) {
	return qf->metadata->nslots;
}
uint64_t qf_get_num_occupied_slots(const QF *qf) {
	return qf->metadata->noccupied_slots;
}

uint64_t qf_get_num_key_bits(const QF *qf) {
	return qf->metadata->key_bits;
}
uint64_t qf_get_num_memento_bits(const QF *qf) {
	return qf->metadata->memento_bits;
}
uint64_t qf_get_num_key_fingerprint_bits(const QF *qf) {
	return qf->metadata->fingerprint_bits;
}
uint64_t qf_get_bits_per_slot(const QF *qf) {
	return qf->metadata->bits_per_slot;
}

uint64_t qf_get_sum_of_counts(const QF *qf) {
	return qf->metadata->nelts;
}
uint64_t qf_get_num_distinct_key_value_pairs(const QF *qf) {
	return qf->metadata->ndistinct_elts;
}

/* Initialize the iterator at the run corresponding to the position index. */
int64_t qf_iterator_from_position(const QF *qf, QFi *qfi, uint64_t position)
{
	if (position == 0xffffffffffffffff) {
		qfi->current = 0xffffffffffffffff;
		qfi->qf = qf;
		return QFI_INVALID;
	}
	assert(position < qf->metadata->nslots);
	if (!is_occupied(qf, position)) {
		uint64_t block_index = position;
		uint64_t idx = bitselect(get_block(qf, block_index)->occupieds[0], 0);
		if (idx == 64) {
			while (idx == 64 && block_index < qf->metadata->nblocks) {
				block_index++;
				idx = bitselect(get_block(qf, block_index)->occupieds[0], 0);
			}
		}
		position = block_index * QF_SLOTS_PER_BLOCK + idx;
	}

	qfi->qf = qf;
	qfi->num_clusters = 0;
	qfi->run = position;
	qfi->current = position == 0 ? 0 : run_end(qfi->qf, position-1) + 1;
	if (qfi->current < position)
		qfi->current = position;

#ifdef LOG_CLUSTER_LENGTH
	qfi->c_info = (cluster_data* )calloc(qf->metadata->nslots/32,
																			 sizeof(cluster_data));
	if (qfi->c_info == NULL) {
		perror("Couldn't allocate memory for c_info.");
		exit(EXIT_FAILURE);
	}
	qfi->cur_start_index = position;
	qfi->cur_length = 1;
#endif

	if (qfi->current >= qf->metadata->nslots)
		return QFI_INVALID;
	return qfi->current;
}

int64_t qf_iterator_by_key(const QF *qf, QFi *qfi, uint64_t key, uint8_t flags)     // NEW IN MEMENTO
{
	if (key >= qf->metadata->range) {
		qfi->current = 0xffffffffffffffff;
		qfi->qf = qf;
		return QFI_INVALID;
	}

	qfi->qf = qf;
	qfi->num_clusters = 0;

	if (GET_KEY_HASH(flags) != QF_KEY_IS_HASH) {
		if (qf->metadata->hash_mode == QF_HASH_DEFAULT)
			key = MurmurHash64A(((void *)&key), sizeof(key), qf->metadata->seed);
		else if (qf->metadata->hash_mode == QF_HASH_INVERTIBLE)
			key = hash_64(key, BITMASK(63));
	}
	uint64_t hash = key;

    const uint32_t bucket_index_hash_size = qf->metadata->key_bits - \
                                            qf->metadata->fingerprint_bits;
    const uint32_t orig_quotient_size = qf->metadata->original_quotient_bits;
    const uint64_t orig_nslots = qf->metadata->nslots >> (qf->metadata->key_bits 
                                                        - qf->metadata->fingerprint_bits 
                                                        - qf->metadata->original_quotient_bits);
	const uint64_t fast_reduced_part = fast_reduce(((hash & BITMASK(orig_quotient_size)) 
                                                    << (32 - orig_quotient_size)), orig_nslots);
	const uint64_t hash_bucket_index = (fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                        | ((hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
	const uint64_t hash_fingerprint = ((hash >> bucket_index_hash_size) & BITMASK(qf->metadata->fingerprint_bits))
                                        | (1ULL << qf->metadata->fingerprint_bits);
    
    bool target_found = false;
	// If a run starts at "position" move the iterator to point it to the
	// smallest key greater than or equal to "hash."
	if (is_occupied(qf, hash_bucket_index)) {
		uint64_t runstart_index = hash_bucket_index == 0 ? 0 : 
                                        run_end(qf, hash_bucket_index - 1) + 1;
		if (runstart_index < hash_bucket_index)
			runstart_index = hash_bucket_index;
        int64_t fingerprint_pos = next_matching_fingerprint_in_run(qf, 
                                        runstart_index, hash_fingerprint);
        if (fingerprint_pos < 0)
            fingerprint_pos = lower_bound_fingerprint_in_run(qf, runstart_index, 
                                                            hash_fingerprint);
		// found something matching `hash`, or smallest key greater than `hash`
        // in this run.
        target_found = fingerprint_pos <= run_end(qf, hash_bucket_index);
		if (target_found) {
			qfi->run = hash_bucket_index;
			qfi->current = runstart_index;
		}
	}
	// If a run doesn't start at `position` or the largest key in the run
	// starting at `position` is smaller than `hash` then find the start of the
	// next run.
	if (!is_occupied(qf, hash_bucket_index) || !target_found) {
		uint64_t position = hash_bucket_index;
		assert(position < qf->metadata->nslots);
		uint64_t block_index = position / QF_SLOTS_PER_BLOCK;
		uint64_t idx = bitselect(get_block(qf, block_index)->occupieds[0], 0);
		if (idx == 64) {
			while(idx == 64 && block_index < qf->metadata->nblocks) {
				block_index++;
				idx = bitselect(get_block(qf, block_index)->occupieds[0], 0);
			}
		}
		position = block_index * QF_SLOTS_PER_BLOCK + idx;
		qfi->run = position;
		qfi->current = position == 0 ? 0 : run_end(qfi->qf, position-1) + 1;
		if (qfi->current < position)
			qfi->current = position;
	}

	if (qfi->current >= qf->metadata->nslots)
		return QFI_INVALID;
	return qfi->current;
}

static int qfi_get(const QFi *qfi, uint64_t *key, uint64_t *mementos)   // NEW IN MEMENTO
{
	if (qfi_end(qfi))
		return QFI_INVALID;

    const QF *qf = qfi->qf;
    int32_t res = 0;

	uint64_t f1, f2, m1, m2;
    f1 = GET_FINGERPRINT(qf, qfi->current);
#ifdef DEBUG
    if (f1 == 0) {
        fprintf(stderr, "JAHAN-DARA! I run=%lu current=%lu\n", qfi->run, qfi->current);
    }
    assert(f1 > 0);
#endif /* DEBUG */
    f2 = GET_FINGERPRINT(qf, qfi->current + 1);
    if (!is_runend(qf, qfi->current) && f1 > f2) {
        m1 = GET_MEMENTO(qf, qfi->current);
        m2 = GET_MEMENTO(qf, qfi->current + 1);
        if (m1 < m2) {
            mementos[res++] = m1;
            mementos[res++] = m2;
        }
        else {
            // Mementos stored as sorted list
            const uint64_t memento_bits = qf->metadata->memento_bits;
            const uint64_t max_memento_value = (1ULL << memento_bits) - 1;

            mementos[res++] = m2;
            const uint64_t pos = qfi->current + 2;
            uint64_t data = 0;
            int32_t filled_bits = 0;
            int64_t data_bit_pos = (pos % QF_SLOTS_PER_BLOCK) * qf->metadata->bits_per_slot;
            uint64_t data_block_ind = pos / QF_SLOTS_PER_BLOCK;
            GET_NEXT_DATA_WORD_IF_EMPTY(qf, data, filled_bits, memento_bits,
                                        data_bit_pos, data_block_ind);

            uint64_t memento_count = data & max_memento_value;
            data >>= memento_bits;
            filled_bits -= memento_bits;
            if (memento_count == max_memento_value) {
                uint64_t length = 2, pw = 1;
                memento_count = 0;
                while (length) {
                    GET_NEXT_DATA_WORD_IF_EMPTY(qf, data, filled_bits, memento_bits,
                                                data_bit_pos, data_block_ind);
                    const uint64_t current_fragment = data & max_memento_value;
                    if (current_fragment == max_memento_value) {
                        length++;
                    }
                    else {
                        length--;
                        memento_count += pw * current_fragment;
                        pw *= max_memento_value;
                    }
                    data >>= memento_bits;
                    filled_bits -= memento_bits;
                }
            }
            for (uint32_t i = 0; i < memento_count; i++) {
                GET_NEXT_DATA_WORD_IF_EMPTY(qf, data, filled_bits, memento_bits,
                                            data_bit_pos, data_block_ind);
                mementos[res++] = data & max_memento_value;
                data >>= memento_bits;
                filled_bits -= memento_bits;
            }
            mementos[res++] = m1;
        }
    }
    else {
        mementos[res++] = GET_MEMENTO(qf, qfi->current);
    }
    const uint32_t bucket_index_hash_size = qf->metadata->key_bits - qf->metadata->fingerprint_bits;
    const uint32_t original_quotient_bits = qf->metadata->original_quotient_bits;
    const uint64_t original_bucket_index = qfi->run >> (bucket_index_hash_size - original_quotient_bits);
    const uint64_t bucket_extension = ((qfi->run & BITMASK(bucket_index_hash_size - original_quotient_bits)) 
                                            << original_quotient_bits);
    *key = original_bucket_index | (f1 << bucket_index_hash_size) | bucket_extension;
	return res;
}

int qfi_get_key(const QFi *qfi, uint64_t *key, uint64_t *mementos)  // NEW IN MEMENTO
{
	*key = 0;
	int ret = qfi_get(qfi, key, mementos);
	if (ret == 0) {
        if (qfi->qf->metadata->hash_mode == QF_HASH_DEFAULT) {
            *key = 0;
            return QF_INVALID;
        } 
        else if (qfi->qf->metadata->hash_mode == QF_HASH_INVERTIBLE) {
            *key = hash_64i(*key, BITMASK(63));
        }
    }

	return ret;
}

int qfi_get_hash(const QFi *qfi, uint64_t *key, uint64_t *mementos)     // NEW IN MEMENTO
{
	*key = 0;
	return qfi_get(qfi, key, mementos);
}

int qfi_next(QFi *qfi)  // NEW IN MEMENTO
{
	if (qfi_end(qfi))
		return QFI_INVALID;
	else {
		/* move to the end of the memento list */
		if (!is_runend(qfi->qf, qfi->current)) {
            if (GET_FINGERPRINT(qfi->qf, qfi->current)
                    > GET_FINGERPRINT(qfi->qf, qfi->current + 1)) {
                uint64_t current_memento = GET_MEMENTO(qfi->qf, qfi->current);
                uint64_t next_memento = GET_MEMENTO(qfi->qf, qfi->current + 1);
                if (current_memento < next_memento) {
                    qfi->current++;
                }
                else {
                    // Mementos encoded as a sroted list
#ifdef DEBUG
                    const uint64_t tmp = qfi->current;
#endif /* DEBUG */
                    qfi->current += number_of_slots_used_for_memento_list(qfi->qf,
                                                                qfi->current + 2) + 1;
#ifdef DEBUG
                    for (uint64_t i = tmp; i < qfi->current; i++) {
                        if (is_runend(qfi->qf, i)) {
                            qf_dump(qfi->qf);
                            fprintf(stderr, "I AM THE BONE OF MY SWORD run=%lu current=%lu --- i=%lu\n", qfi->run, tmp, i);
                        }
                        assert(!is_runend(qfi->qf, i));
                    }
#endif /* DEBUG */
                }
            }
        }

		if (!is_runend(qfi->qf, qfi->current)) {
			qfi->current++;
#ifdef LOG_CLUSTER_LENGTH
			qfi->cur_length++;
#endif

			if (qfi_end(qfi))
				return QFI_INVALID;
			return 0;
		} else {
#ifdef LOG_CLUSTER_LENGTH
			/* save to check if the new current is the new cluster. */
			uint64_t old_current = qfi->current;
#endif
			uint64_t block_index = qfi->run / QF_SLOTS_PER_BLOCK;
			uint64_t rank = bitrank(get_block(qfi->qf, block_index)->occupieds[0],
                                    qfi->run % QF_SLOTS_PER_BLOCK);
            uint64_t next_run = bitselect(get_block(qfi->qf, block_index)
                                            ->occupieds[0], rank);
			if (next_run == 64) {
				rank = 0;
				while (next_run == 64 && block_index < qfi->qf->metadata->nblocks) {
					block_index++;
					next_run = bitselect(get_block(qfi->qf, block_index)->occupieds[0],
															 rank);
				}
			}
			if (block_index == qfi->qf->metadata->nblocks) {
				/* set the index values to max. */
				qfi->run = qfi->current = qfi->qf->metadata->xnslots;
				return QFI_INVALID;
			}
			qfi->run = block_index * QF_SLOTS_PER_BLOCK + next_run;
			qfi->current++;
			if (qfi->current < qfi->run)
				qfi->current = qfi->run;
#ifdef LOG_CLUSTER_LENGTH
			if (qfi->current > old_current + 1) { /* new cluster. */
				if (qfi->cur_length > 10) {
					qfi->c_info[qfi->num_clusters].start_index = qfi->cur_start_index;
					qfi->c_info[qfi->num_clusters].length = qfi->cur_length;
					qfi->num_clusters++;
				}
				qfi->cur_start_index = qfi->run;
				qfi->cur_length = 1;
			} else {
				qfi->cur_length++;
			}
#endif

#ifdef DEBUG
            if (GET_FINGERPRINT(qfi->qf, qfi->current) == 0) {
                fprintf(stderr, "JAHAN-DARA! III run=%lu current=%lu\n", qfi->run, qfi->current);
                qf_dump_block(qfi->qf, qfi->current / QF_SLOTS_PER_BLOCK);
                fflush(stderr);
            }
            assert(GET_FINGERPRINT(qfi->qf, qfi->current) > 0);
#endif /* DEBUG */
            
			return 0;
		}
	}
}

bool qfi_end(const QFi *qfi)
{
	if (qfi->current >= qfi->qf->metadata->xnslots /*&& is_runend(qfi->qf, qfi->current)*/)
		return true;
	return false;
}

#ifdef QF_ITERATOR
/*
 * Merge qfa and qfb into qfc
 */
/*
 * iterate over both qf (qfa and qfb)
 * simultaneously
 * for each index i
 * min(get_value(qfa, ia) < get_value(qfb, ib))
 * insert(min, ic)
 * increment either ia or ib, whichever is minimum.
 */
void qf_merge(const QF *qfa, const QF *qfb, QF *qfc)
{
	QFi qfia, qfib;
	qf_iterator_from_position(qfa, &qfia, 0);
	qf_iterator_from_position(qfb, &qfib, 0);

	if (qfa->metadata->hash_mode != qfc->metadata->hash_mode &&
			qfa->metadata->seed != qfc->metadata->seed &&
			qfb->metadata->hash_mode  != qfc->metadata->hash_mode &&
			qfb->metadata->seed  != qfc->metadata->seed) {
		fprintf(stderr, "Output QF and input QFs do not have the same hash mode or seed.\n");
		exit(1);
	}

	uint64_t keya, valuea, counta, keyb, valueb, countb;
	qfi_get_hash(&qfia, &keya, &valuea, &counta);
	qfi_get_hash(&qfib, &keyb, &valueb, &countb);
	do {
		if (keya < keyb) {
			qf_insert(qfc, keya, valuea, counta, QF_NO_LOCK | QF_KEY_IS_HASH);
			qfi_next(&qfia);
			qfi_get_hash(&qfia, &keya, &valuea, &counta);
		}
		else {
			qf_insert(qfc, keyb, valueb, countb, QF_NO_LOCK | QF_KEY_IS_HASH);
			qfi_next(&qfib);
			qfi_get_hash(&qfib, &keyb, &valueb, &countb);
		}
	} while(!qfi_end(&qfia) && !qfi_end(&qfib));

	if (!qfi_end(&qfia)) {
		do {
			qfi_get_hash(&qfia, &keya, &valuea, &counta);
			qf_insert(qfc, keya, valuea, counta, QF_NO_LOCK | QF_KEY_IS_HASH);
		} while(!qfi_next(&qfia));
	}
	if (!qfi_end(&qfib)) {
		do {
			qfi_get_hash(&qfib, &keyb, &valueb, &countb);
			qf_insert(qfc, keyb, valueb, countb, QF_NO_LOCK | QF_KEY_IS_HASH);
		} while(!qfi_next(&qfib));
	}
}

/*
 * Merge an array of qfs into the resultant QF
 */
void qf_multi_merge(const QF *qf_arr[], int nqf, QF *qfr)
{
	int i;
	QFi qfi_arr[nqf];
	int smallest_idx = 0;
	uint64_t smallest_key = UINT64_MAX;
	for (i=0; i<nqf; i++) {
		if (qf_arr[i]->metadata->hash_mode != qfr->metadata->hash_mode &&
				qf_arr[i]->metadata->seed != qfr->metadata->seed) {
			fprintf(stderr, "Output QF and input QFs do not have the same hash mode or seed.\n");
			exit(1);
		}
		qf_iterator_from_position(qf_arr[i], &qfi_arr[i], 0);
	}

	DEBUG_CQF("Merging %d CQFs\n", nqf);
	for (i=0; i<nqf; i++) {
		DEBUG_CQF("CQF %d\n", i);
		DEBUG_DUMP(qf_arr[i]);
	}

	while (nqf > 1) {
		uint64_t keys[nqf];
		uint64_t values[nqf];
		uint64_t counts[nqf];
		for (i=0; i<nqf; i++)
			qfi_get_hash(&qfi_arr[i], &keys[i], &values[i], &counts[i]);

		do {
			smallest_key = UINT64_MAX;
			for (i=0; i<nqf; i++) {
				if (keys[i] < smallest_key) {
					smallest_key = keys[i]; smallest_idx = i;
				}
			}
			qf_insert(qfr, keys[smallest_idx], values[smallest_idx],
								counts[smallest_idx], QF_NO_LOCK | QF_KEY_IS_HASH);
			qfi_next(&qfi_arr[smallest_idx]);
			qfi_get_hash(&qfi_arr[smallest_idx], &keys[smallest_idx],
									 &values[smallest_idx],
							&counts[smallest_idx]);
		} while(!qfi_end(&qfi_arr[smallest_idx]));

		/* remove the qf that is exhausted from the array */
		if (smallest_idx < nqf-1)
			memmove(&qfi_arr[smallest_idx], &qfi_arr[smallest_idx+1],
							(nqf-smallest_idx-1)*sizeof(qfi_arr[0]));
		nqf--;
	}
	if (!qfi_end(&qfi_arr[0])) {
		uint64_t iters = 0;
		do {
			uint64_t key, value, count;
			qfi_get_hash(&qfi_arr[0], &key, &value, &count);
			qf_insert(qfr, key, value, count, QF_NO_LOCK | QF_KEY_IS_HASH);
			qfi_next(&qfi_arr[0]);
			iters++;
		} while(!qfi_end(&qfi_arr[0]));
		DEBUG_CQF("Num of iterations: %" PRIx64 "\n", iters);
	}

	DEBUG_CQF("%s", "Final CQF after merging.\n");
	DEBUG_DUMP(qfr);

	return;
}

/* find cosine similarity between two QFs. */
uint64_t qf_inner_product(const QF *qfa, const QF *qfb)
{
	uint64_t acc = 0;
	QFi qfi;
	const QF *qf_mem, *qf_disk;

	if (qfa->metadata->hash_mode != qfb->metadata->hash_mode &&
			qfa->metadata->seed != qfb->metadata->seed) {
		fprintf(stderr, "Input QFs do not have the same hash mode or seed.\n");
		exit(1);
	}

	// create the iterator on the larger QF.
	if (qfa->metadata->total_size_in_bytes > qfb->metadata->total_size_in_bytes)
	{
		qf_mem = qfb;
		qf_disk = qfa;
	} else {
		qf_mem = qfa;
		qf_disk = qfb;
	}

	qf_iterator_from_position(qf_disk, &qfi, 0);
	do {
		uint64_t key = 0, value = 0, count = 0;
		uint64_t count_mem;
		qfi_get_hash(&qfi, &key, &value, &count);
		if ((count_mem = qf_count_key_value(qf_mem, key, 0, QF_KEY_IS_HASH)) > 0) {
			acc += count*count_mem;
		}
	} while (!qfi_next(&qfi));

	return acc;
}

/* find cosine similarity between two QFs. */
void qf_intersect(const QF *qfa, const QF *qfb, QF *qfr)
{
	QFi qfi;
	const QF *qf_mem, *qf_disk;

	if (qfa->metadata->hash_mode != qfr->metadata->hash_mode &&
			qfa->metadata->seed != qfr->metadata->seed &&
			qfb->metadata->hash_mode  != qfr->metadata->hash_mode &&
			qfb->metadata->seed  != qfr->metadata->seed) {
		fprintf(stderr, "Output QF and input QFs do not have the same hash mode or seed.\n");
		exit(1);
	}

	// create the iterator on the larger QF.
	if (qfa->metadata->total_size_in_bytes > qfb->metadata->total_size_in_bytes)
	{
		qf_mem = qfb;
		qf_disk = qfa;
	} else {
		qf_mem = qfa;
		qf_disk = qfb;
	}

	qf_iterator_from_position(qf_disk, &qfi, 0);
	do {
		uint64_t key = 0, value = 0, count = 0;
		qfi_get_hash(&qfi, &key, &value, &count);
		if (qf_count_key_value(qf_mem, key, 0, QF_KEY_IS_HASH) > 0)
			qf_insert(qfr, key, value, count, QF_NO_LOCK | QF_KEY_IS_HASH);
	} while (!qfi_next(&qfi));
}

/* magnitude of a QF. */
uint64_t qf_magnitude(const QF *qf)
{
	return sqrt(qf_inner_product(qf, qf));
}
#endif /* QF_ITERATOR */




