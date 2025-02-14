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

// TODO: Missing reverse iterators. Also, should I do something about __SSE4_2_?
// TODO: Issue in the original code with insert_mementos when there are two
// mementos and the smallest and largest mementos are the same: uses 3 slots,
// which is incorrect.
// TODO: Original code uses the expandable implementation in the
// qf_iterator_by_key function.
// TODO: Original code writes two mementos in a single word. Won't work for
// large mementos.
// TODO: Remove the if that halves the fingerprint size if there are multiple
// mementos in the keepsake box during expansion in the expandable branch.

#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utility>
#include <immintrin.h>
#include <vector>

namespace memento {
/******************************************************************
 * Hash functions used in Memento filter.                         *
 ******************************************************************/

/**
 * MurmurHash2, 64-bit versions, by Austin Appleby.
 * The same caveats as 32-bit MurmurHash2 apply here - beware of alignment and
 * endian-ness issues if used across multiple platforms.
 */
static uint64_t MurmurHash64A(const void *key, int32_t len, uint32_t seed) {
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t * data = (const uint64_t *)key;
    const uint64_t * end = data + (len / 8);

    while(data != end)
    {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char * data2 = (const unsigned char*)data;

    switch(len & 7)
    {
        case 7: h ^= (uint64_t)data2[6] << 48; do {} while (0);  /* fallthrough */
        case 6: h ^= (uint64_t)data2[5] << 40; do {} while (0);  /* fallthrough */
        case 5: h ^= (uint64_t)data2[4] << 32; do {} while (0);  /* fallthrough */
        case 4: h ^= (uint64_t)data2[3] << 24; do {} while (0);  /* fallthrough */
        case 3: h ^= (uint64_t)data2[2] << 16; do {} while (0);  /* fallthrough */
        case 2: h ^= (uint64_t)data2[1] << 8; do {} while (0); /* fallthrough */
        case 1: h ^= (uint64_t)data2[0];
                h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

/** Thomas Wang's integer hash functions. See
* <https://gist.github.com/lh3/59882d6b96166dfc3d8d> for a snapshot.
*/
static uint64_t hash_64(uint64_t key, uint64_t mask) {
    key = (~key + (key << 21)) & mask; // key = (key << 21) - key - 1;
    key = key ^ key >> 24;
    key = ((key + (key << 3)) + (key << 8)) & mask; // key * 265
    key = key ^ key >> 14;
    key = ((key + (key << 2)) + (key << 4)) & mask; // key * 21
    key = key ^ key >> 28;
    key = (key + (key << 31)) & mask;
    return key;
}

/******************************************************************
 * Code for managing the metadata bits and slots w/o interpreting *
 * the content of the slots.                                      *
 ******************************************************************/

#define MAX_VALUE(nbits) ((1ULL << (nbits)) - 1)
#define BITMASK(nbits) ((nbits) == 64 ? 0XFFFFFFFFFFFFFFFF : MAX_VALUE(nbits))
#define METADATA_WORD(field, slot_index) (get_block((slot_index) / \
             slots_per_block_)->field[((slot_index) % slots_per_block_) / 64])

#define GET_NO_LOCK(flag) (flag & flag_no_lock)
#define GET_TRY_ONCE_LOCK(flag) (flag & flag_try_once_lock)
#define GET_WAIT_FOR_LOCK(flag) (flag & flag_wait_for_lock)
#define GET_KEY_HASH(flag) (flag & flag_key_is_hash)

#define REMAINDER_WORD(i) (reinterpret_cast<uint64_t *>(&(get_block((i) / metadata_->bits_per_slot)->slots[8 * ((i) % metadata_->bits_per_slot)])))

#define CMP_MASK_FINGERPRINT(a, b, mask) ((((a) ^ (b)) & (mask)) == 0)

/**
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
#define GET_NEXT_DATA_WORD_IF_EMPTY(data, filled_bits, bit_cnt, bit_pos, block_ind) \
    { \
    while (filled_bits < (bit_cnt)) { \
        const uint64_t byte_pos = bit_pos / 8; \
        uint64_t *p = reinterpret_cast<uint64_t *>(&get_block((block_ind))->slots[byte_pos]); \
        uint64_t tmp; \
        memcpy(&tmp, p, sizeof(tmp)); \
        tmp >>= (bit_pos % 8); \
        data |= tmp << filled_bits; \
        const uint64_t bits_per_block = slots_per_block_ * metadata_->bits_per_slot; \
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
#define GET_NEXT_DATA_WORD_IF_EMPTY_ITERATOR(filter, data, filled_bits, bit_cnt, bit_pos, block_ind) \
    { \
    while (filled_bits < (bit_cnt)) { \
        const uint64_t byte_pos = bit_pos / 8; \
        uint64_t *p = reinterpret_cast<uint64_t *>(&filter.get_block((block_ind))->slots[byte_pos]); \
        uint64_t tmp; \
        memcpy(&tmp, p, sizeof(tmp)); \
        tmp >>= (bit_pos % 8); \
        data |= tmp << filled_bits; \
        const uint64_t bits_per_block = slots_per_block_ * filter.metadata_->bits_per_slot; \
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
#define INIT_PAYLOAD_WORD(payload, filled_bits, bit_pos, block_ind) \
    { \
    uint64_t byte_pos = bit_pos / 8; \
    uint64_t *p = reinterpret_cast<uint64_t *>(&get_block((block_ind))->slots[byte_pos]); \
    memcpy(&payload, p, sizeof(payload)); \
    filled_bits = bit_pos % 8; \
    bit_pos -= bit_pos % 8; \
    }
#define APPEND_WRITE_PAYLOAD_WORD(payload, filled_bits, val, val_len, bit_pos, block_ind) \
    { \
    const uint64_t bits_per_block = slots_per_block_ * metadata_->bits_per_slot; \
    uint64_t val_copy = (val); \
    uint32_t val_bit_cnt = (val_len); \
    uint32_t max_filled_bits = (bits_per_block - bit_pos < 64 ? \
                                bits_per_block - bit_pos : 64); \
    while (filled_bits + val_bit_cnt > max_filled_bits) { \
        const uint64_t mask = BITMASK(max_filled_bits - filled_bits); \
        payload &= ~(mask << filled_bits); \
        payload |= (val_copy & mask) << filled_bits; \
        uint64_t byte_pos = bit_pos / 8; \
        uint64_t *p = reinterpret_cast<uint64_t *>(&get_block((block_ind))->slots[byte_pos]); \
        memcpy(p, &payload, sizeof(payload)); \
        bit_pos += max_filled_bits; \
        if (bit_pos >= bits_per_block) { \
            bit_pos = 0; \
            block_ind++; \
        } \
        val_copy >>= max_filled_bits - filled_bits; \
        val_bit_cnt -= max_filled_bits - filled_bits; \
        filled_bits = 0; \
        max_filled_bits = (bits_per_block - bit_pos < 64 ? \
                           bits_per_block - bit_pos : 64); \
        p = reinterpret_cast<uint64_t *>(&get_block((block_ind))->slots[bit_pos / 8]); \
        memcpy(&payload, p, sizeof(payload)); \
    } \
    if (filled_bits + val_bit_cnt <= max_filled_bits) { \
        payload &= ~(BITMASK(val_bit_cnt) << filled_bits); \
        payload |= val_copy << filled_bits; \
        filled_bits += val_bit_cnt; \
    } \
    }
#define FLUSH_PAYLOAD_WORD(payload, filled_bits, bit_pos, block_ind) \
    { \
    uint64_t byte_pos = bit_pos / 8; \
    uint64_t *p = reinterpret_cast<uint64_t *>(&get_block((block_ind))->slots[byte_pos]); \
    memcpy(p, &payload, sizeof(payload)); \
    }

/**
 * A fast replacement for modulo operations. Credit to Daniel Lemiere.
 * See http://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/ 
 *
 * @param hash - The dividend hash value.
 * @param n - The divisor.
 * @returns The modulus of the operation.
 */
__attribute__((always_inline))
static inline uint32_t fast_reduce(uint32_t hash, uint32_t n) {
    return static_cast<uint32_t>(((uint64_t) hash * n) >> 32);
}

/**
 * @param val - The value to count the set bits from.
 * @param ignore - The number of bits to ignore.
 * @returns the number of set bits in `val` ignoring the `ignore`
 * least-significant bits.
 */
static inline int popcntv(const uint64_t val, int32_t ignore) {
	if (ignore % 64)
		return __builtin_popcountll(val & ~BITMASK(ignore % 64));
	else
		return __builtin_popcountll(val);
}

/**
 * Returns the number of 1s in `val` up to (and including) the `pos`'th bit.
 *
 * @param val - The value being processed.
 * @param pos - The target position.
 * @returns The rank of the target position.
 */
static inline int bitrank(uint64_t val, int32_t pos) {
    return __builtin_popcountll(val & ((2ULL << pos) - 1));
}

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

/**
 * Returns the position of the `k`-th 1 in the 64-bit word x. `k` is 0-based,
 * so `k`=0 returns the position of the first 1.
 *
 * Uses the broadword selection algorithm by Vigna [1], improved by Gog and
 * Petri [2] and Vigna [3].
 *
 * [1] Sebastiano Vigna. Broadword Implementation of Rank/Select Queries. WEA,
 * 2008
 *
 * [2] Simon Gog, Matthias Petri. Optimized succinct data structures for
 * massive data. Softw. Pract. Exper., 2014
 *
 * [3] Sebastiano Vigna. MG4J 5.2.1. http://mg4j.di.unimi.it/
 * The following code is taken from
 * https://github.com/facebook/folly/blob/b28186247104f8b90cfbe094d289c91f9e413317/folly/experimental/Select64.h
 *
 * @param x - The mask to select from.
 * @param k - The desired bit rank.
 * @returns The position of the `k`-th set bit.
 */
static inline uint64_t _select64(uint64_t x, int k)
{
	if (k >= __builtin_popcountll(x)) { return 64; }

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
	uint64_t place = __builtin_popcountll(geqKStep8) * 8;
	uint64_t byteRank = k - (((byteSums << 8) >> place) & (uint64_t)(0xFF));
	return place + kSelectInByte[((x >> place) & 0xFF) | (byteRank << 8)];
}

/**
 * @param val - The mask to select from.
 * @param rank - The rank of the target bit.
 * @returns The position of the `rank`-th 1.  (`rank` = 0 returns the 1st 1).
 * Returns 64 if there are fewer than `rank` + 1 1s.
 */
static inline uint64_t bitselect(uint64_t val, int rank) {
#ifdef __SSE4_2_
    uint64_t tmp = 1ULL << rank;
    tmp = _pdep_u64(tmp, val);
    return __builtin_ia32_tzcnt_u64(tmp);
#else
	return _select64(val, rank);
#endif
}

/**
 * @param val - The value to extract the position of the lowbit from.
 * @returns The position of the lowest-order set bit of `val`. Returns 64
 * if there are zero set bits.
 */
static inline uint64_t lowbit_position(uint64_t val) {
#ifdef __SSE4_2_
    return __builtin_ia32_tzcnt_u64(val);
#else
	return _select64(val, 0);
#endif
}

/**
 * @param val - The value to extract the position of the highbit from.
 * @returns The position of the highest-order set bit of `val`. Returns 64
 * if there are zero set bits.
 */
static inline uint64_t highbit_position(uint64_t val) {
#ifdef __SSE4_2_
    return 8 * sizeof(val) - __builtin_ia32_lzcnt_u64(val) - 1;
#else
    if (val == 0)
        return 64;
    return val == 0 ? 64 : 8 * sizeof(val) - __builtin_clzll(val) - 1;
#endif
}

/**
 * @param val - The value to count the set bits from.
 * @param ignore - The number of bits to ignore.
 * @returns The position of the `rank`-th set bit of `val`, ignoring the
 * least-significant `ignore` bits. Returns 64 if there are zero set bits.
 */
static inline uint64_t bitselectv(const uint64_t val, int ignore, int rank) {
	return bitselect(val & ~BITMASK(ignore % 64), rank);
}

/**
 * Shifts the `amount` bits from `a` starting from the position `bstart` into
 * `b`, blocking the shift at position `bend`.
 *
 * @param a, b - The values to be shifted.
 * @param bstart, bend - The start and end positions of shifting.
 * @param amount - The number of bits to shift.
 */
static inline uint64_t shift_into_b(const uint64_t a, const uint64_t b,
                                    const int bstart, const int bend,
                                    const int amount) {
	const uint64_t a_component = bstart == 0 ? (a >> (64 - amount)) : 0;
	const uint64_t b_shifted_mask = BITMASK(bend - bstart) << bstart;
	const uint64_t b_shifted = ((b_shifted_mask & b) << amount) & b_shifted_mask;
	const uint64_t b_mask = ~b_shifted_mask;
	return a_component | b_shifted | (b & b_mask);
}


template <bool expandable=false>
class Memento {
    friend class iterator;
    friend class hash_iterator;

public:
	/** Memento filter supports two hashing modes:
     *
     *   - `Default` uses a hash that may introduce false positives, but this
     *   can be useful when inserting large keys that need to be hashed down to
     *   a small fingerprint. With this type of hash, you can iterate over the
     *   hash values of all the keys in the filter, but you cannot iterate over
     *   the keys themselves.
     *
     *   - `Invertible` has no false positives, but the size of the hash output
     *   must be the same as the size of the hash input, e.g. 17-bit keys
     *   hashed to 17-bit outputs. So this mode is generally only useful when
     *   storing small keys in the filter. With this hashing mode, you can use
     *   iterators to enumerate both all the hashes in Memento filter, or all
     *   the keys.
     *
     *   - `None`, for when you've done the hashing yourself. WARNING: Memento
     *   filter can exhibit very poor performance if you insert a skewed
     *   distribution of intputs.
	 */
	enum class hashmode {
		Default,
        Invertible,
		None
	};

    /** Like the RSQF, Memento filter supports concurrent insertions and 
     * queries. Only the portion of the filter being examined or modified is
     * locked, so it supports high throughput even with many threads.
     *
	 * The RSQF and Memento filter operations support 3 locking modes:
     *
	 *   - `flag_no_lock`: for single-threaded applications or applications
     *                     that do their own concurrency management.
     *
	 *   - `flag_wait_for_lock`: Spin until you get the lock, then do the query
     *                           or update.
     *
	 *   - `flag_try_once_lock`: If you can't grab the lock on the first try,
     *                           return with an error code.
     *
     * DISCLAIMER: These concurrency features have not been thoroughly tested,
     * as they lie outside of the main scope of Memento filter. However, they
     * should working pretty well already.
	 */
    static constexpr uint32_t flag_no_lock = 0x01;
    static constexpr uint32_t flag_try_once_lock = 0x02;
    static constexpr uint32_t flag_wait_for_lock = 0x04;
    static constexpr uint32_t flag_key_is_hash = 0x08; // It is sometimes useful to insert a key that has already been hashed.

private:
    /** Must be >= 6.  6 seems fastest. */
    static constexpr uint32_t block_offset_bits_ = 6;

    static constexpr uint32_t slots_per_block_ = 1ULL << block_offset_bits_;
    static constexpr uint32_t metadata_words_per_block_ = (slots_per_block_ + 63) / 64;

    static constexpr uint64_t num_slots_to_lock_ = 1ULL << 16;
    static constexpr uint64_t cluster_size_ = 1ULL << 14;

    static constexpr uint32_t distance_from_home_slot_cutoff_ = 1000;
    static constexpr uint32_t billion_ = 1000000000ULL;
    static constexpr uint64_t magic_number_ = 1018874902021329732;

    struct __attribute__ ((__packed__)) qfblock {
        uint8_t offset; // Also works with uint16_t, uint32_t, etc., but uint8_t seems just as fast
        uint64_t occupieds[metadata_words_per_block_];
        uint64_t runends[metadata_words_per_block_];
        uint8_t slots[1];
    };

    /**
     * The below struct is used to instrument the code.
     * It is not used in normal operations of Memento filter.
     */
    struct wait_time_data {
        uint64_t total_time_single;
        uint64_t total_time_spinning;
        uint64_t locks_taken;
        uint64_t locks_acquired_single_attempt;
    };

    struct qfruntime {
        uint64_t num_locks;
        volatile int metadata_lock;
        volatile int *locks;
        wait_time_data *wait_times;
    };

    struct qfmetadata {
        uint64_t magic_endian_number;
        hashmode hash_mode;
        uint32_t auto_resize;
        uint64_t total_size_in_bytes;
        uint32_t seed;
        uint64_t nslots;
        uint64_t xnslots;
        uint64_t key_bits;
        uint64_t original_quotient_bits;
        uint64_t memento_bits;
        uint64_t fingerprint_bits;
        uint64_t bits_per_slot;
        __uint128_t range;
        uint64_t nblocks;
        uint64_t nelts;
        uint64_t ndistinct_elts;
        uint64_t noccupied_slots;
    };

    /**
     * The below struct is used to instrument the code.
     * It is not used in normal operations of Memento filter.
     */
    struct cluster_data {
        uint64_t start_index;
        uint16_t length;
    };

public:
	/**
     * An RSQF defines low-level constructor and destructor operations that are
     * designed to enable the application to manage the memory used by the
     * RSQF. 
     *
     * As Memento filter is based on the RQSF, it follows the same general code
     * structure and reuses its relevant code segments.
     */

    explicit Memento(uint64_t nslots, uint64_t key_bits, uint64_t memento_bits,
                     hashmode hash_mode, uint32_t seed, const uint64_t orig_quotient_bit_cnt=0);
    ~Memento();
    Memento(const Memento& other);
    Memento(Memento&& other) noexcept;
    Memento& operator=(const Memento& other);
    Memento& operator=(Memento&& other) noexcept;

    void debug_dump_block(uint64_t i) const;

    void debug_dump_metadata() const;

    void debug_dump() const {
        debug_dump_metadata();
        for (uint32_t i = 0; i < metadata_->nblocks; i++) {
            debug_dump_block(i);
        }
    }

	/* 
     * Resize the Memento filter instance to the specified number of slots.
     * Uses malloc() to obtain the new memory, and calls free() on the old
     * memory. Return value:
	 *    >= 0: number of keys copied during resizing.
     * Note that `nslots` must be twice the number of slots in `qf`.
	 */
	int64_t resize(uint64_t nslots);

    /**
     * Turn on/off automatic resizing. Resizing is performed by calling
     * `resize`, so the Memento filter instance must meet the requirements of
     * that function.
     *
     * @param enabled - Turns automatic resizing on if `true` and off if
     * `false`.
     */
	void set_auto_resize(bool enabled) {
		metadata_->auto_resize = enabled;
    }

	/**
     * Insert a sorted list of mementos as specified in the `mementos` array,
     * all having `key` as their prefix. 
     *
     * @param key - The prefix of the keys.
     * @param mementos - The sorted list of mementos bmementoeing inserted.
     * @param memento_count - The number of mementos in the sorted list.
     * @param flags - Flags determining the filter's behavior under
     * concurrency, as well as if the prefix is already hashed or not. If
     * `flag_wait_for_lock` is set to 1, the thread spins. Otherwise, it tries to
     * acquire the lock once.
     * @returns >= 0: Distance from the home slot to the slot in which the key
     *                is inserted.
	 *          == `err_no_space`: The filter has reached capacity.
     *          == `err_couldnt_lock`: `flag_try_once_lock` has failed to acquire the
     *                                lock.
	 */
	int32_t insert_mementos(uint64_t key, uint64_t mementos[], uint64_t memento_count,
                            uint8_t flags);

    /**
     * Insert a single key into the filter, merging with a fully rejuvenated
     * memento list, if it exists. If no fingerprint matches the fingerprint of
     * the key being inserted, it is inserted as a stand-alone
     * fingerprint/memento pair. If none of these two cases hold, it returns
     * `-pos`, where `pos` is the position this key would have been inserted if
     * it were to be a stand-alone fingerprint/memento pair.
     *
     * @param key - The input key's prefix.
     * @param memento - The input key's memento.
     * @param flags - Flags determining the filter's behavior under
     * concurrency, as well as if the prefix is already hashed or not. If
     * `flag_wait_for_lock` is set to 1, the thread spins. Otherwise, it tries to
     * acquire the lock once.
     * @returns >= 0 if successful and `-pos` if the provided key may be merged
     * with a keepsake box with a partially complete fingerprint. May also
     * return `err_no_space` if the filter is out of space, and
     * `err_couldnt_lock` if `flag_try_once_lock` failed to acquire the lock.
     */
	int64_t insert(uint64_t key, uint64_t memento, uint8_t flags);

    /**
     * Bulk load a set of keys into the filter. The list `sorted_hashes` must 
     * be a list of key hashes sorted in increasing order of (1) their slot
     * addresses, (2) fingerprints, and (3) mementos. That is, the highest
     * order bits of these values must be the slot addresses, the lower order
     * bits just after them the fingerprints, and the remaining lowest order
     * bits, the mementos.
     *
     * @param sorted_hashes - A well-structured list of sorted hashes and
     * mementos to load into the filter.
     * @param n - The number of elements to insert.
     * @param flags - Flags determining the filter's behavior under
     * concurrency, as well as if the prefix is already hashed or not. If
     * `flag_wait_for_lock` is set to 1, the thread spins. Otherwise, it tries to
     * acquire the lock once.
     */
	void bulk_load(uint64_t *sorted_hashes, uint64_t n, uint8_t flags);

    /**
     * Delete a single key from Memento filter. The provided key is deleted
     * from the keepsake box with the longest matching fingerprint to avoid
     * false-positives. 
     *
     * @param key - The input key's prefix.
     * @param key - The input key's memento.
     * @param flags - Flags determining the filter's behavior under
     * concurrency, as well as if the prefix is already hashed or not. If
     * `flag_wait_for_lock` is set to 1, the thread spins. Otherwise, it tries to
     * acquire the lock once.
     * @returns == 0: the key was successfully deleted.
     *          == `err_doesnt_exist`: there was no matching key in the filter.
     *          == `err_couldnt_lock`: `flag_try_once_lock` has failed to acquire
     *                                 the lock.
     */
    int32_t delete_single(uint64_t key, uint64_t memento, uint8_t flags);

    /** 
     * Checks Memento filter for the existence of the point corresponding to
     * the provided prefix and memento. 
     *
     * @param key - The input key's prefix.
     * @param memento - The input key's memento.
     * @param flags - Flags determining the filter's behavior under
     * concurrency, as well as if the prefix is already hashed or not. If
     * `flag_wait_for_lock` is set to 1, the thread spins. Otherwise, it tries to
     * acquire the lock once.
     * @returns 0 if the query results in a negative. Returns 1 if the result
     * is a positive, but rejuvenation is not needed. Return 2 if the result is
     * a positive, but the corresponding fingerprint can be rejuvenated. May
     * return `err_couldnt_lock` if called with `QF_TRY_LOCK`.
     */
    int32_t point_query(uint64_t key, uint64_t memento, uint8_t flags) const;

    /** 
     * Checks the memento filter for the existence of any point in the range
     * denoted by the left and right prefix keys and mementos. 
     *
     * @param l_key, r_key - The prefixes of the left and right endpoints of
     * the range.
     * @param l_memento, r_memento - The mementos of the left and right
     * endpoints of the range.
     * @param flags - Flags determining the filter's behavior under
     * concurrency, as well as if the prefixes are already hashed or not. If
     * `flag_wait_for_lock` is set to 1, the thread spins. Otherwise, it tries to
     * acquire the lock once.
     * @returns 0 if the query results in a negative. Returns 1 if the result
     * is a positive, but rejuvenation is unnecessary. Return 2 if the result
     * is a positive, but at least one of the corresponding fingerprints can be
     * rejuvenated. May return `err_couldnt_lock` if called with `QF_TRY_LOCK`.  
     */
    int32_t range_query(uint64_t l_key, uint64_t l_memento,
                        uint64_t r_key, uint64_t r_memento, uint8_t flags) const;

	/** Reset the Memento filter instance to an empty filter. */
	void reset();

	/* Hashing info */
	hashmode get_hashmode() const {
        return metadata_->hash_mode;
    }
	uint64_t get_hash_seed() const {
        return metadata_->seed;
    }
	__uint128_t get_hash_range() const {
        return metadata_->range;
    }

	/* Space usage info. */
	bool is_auto_resize_enabled() const {
        return metadata_->auto_resize;
    }
	uint64_t size_in_bytes() const {
        return metadata_->total_size_in_bytes;
    }
	uint64_t count_slots() const {
        return metadata_->nslots;
    }
	uint64_t count_occupied_slots() const {
        return metadata_->noccupied_slots;
    }

	/* Bit-sizes info. */
	uint64_t get_num_key_bits() const {
        return metadata_->key_bits;
    }
	uint64_t get_num_memento_bits() const {
        return metadata_->memento_bits;
    }
	uint64_t get_num_fingerprint_bits() const {
        return metadata_->fingerprint_bits;
    }
	uint64_t get_bits_per_slot() const {
        return metadata_->bits_per_slot;
    }
	uint64_t get_bucket_index_hash_size() const {
        return metadata_->key_bits - metadata_->fingerprint_bits;
    }
	uint64_t get_original_quotient_bits() const {
        return metadata_->original_quotient_bits;
    }

	/* Number of (distinct) key-value pairs. */
	uint64_t count_keys() const {
        return metadata_->nelts;
    }
	uint64_t count_distinct_prefixes() const {
        return metadata_->ndistinct_elts;
    }

    /* Status Codes */
    static constexpr int32_t err_no_space = -1;
    static constexpr int32_t err_couldnt_lock = -2;
    static constexpr int32_t err_doesnt_exist = -3;

    class hash_iterator {
        friend class Memento;
    public:
        hash_iterator(const Memento &filter): 
            filter_{filter} {};
        hash_iterator(const hash_iterator& other) = default;
        hash_iterator(hash_iterator&& other) noexcept;
        hash_iterator& operator=(const hash_iterator& other);
        hash_iterator& operator=(hash_iterator&& other) noexcept;
        hash_iterator& operator++();
        hash_iterator operator++(int);
        ~hash_iterator();

        bool operator==(const hash_iterator& rhs) const;
        bool operator!=(const hash_iterator& rhs) const;
        int32_t get(uint64_t& key, uint64_t *mementos=nullptr) const;

    private:
        bool is_at_runend() const;

        const Memento &filter_;
        uint64_t run_ = 0;                   /**< The canonical slot of the current run. */
        uint64_t current_ = 0;               /**< The current slot in the run. */
        uint64_t cur_start_index_ = 0;       /**< The starting slot of the current cluster. */
        uint16_t cur_length_ = 0;            /**< The length of the current cluster. */
        uint32_t num_clusters_ = 0;          /**< The number of traversed clusters. */
        cluster_data *c_info_ = nullptr;     /**< A list with information about the filter's clusters. */
    };

    class iterator {
        friend class Memento;
    public:
        iterator(const Memento& filter, const uint64_t l_key, const uint64_t r_key);
        iterator(const Memento& filter):
            filter_{filter},
            cur_prefix_{std::numeric_limits<uint64_t>::max()},
            it_{filter.hash_end()} {}
        iterator(const iterator& other);
        iterator& operator=(const iterator &other);
        uint64_t operator*();
        iterator& operator++();
        iterator operator++(int);
        bool operator==(const iterator& rhs) const;
        bool operator!=(const iterator& rhs) const;

    private:
        void fetch_matching_prefix_mementos(bool reinit_hash_it=true);

        const Memento& filter_;
        uint64_t l_key_;
        uint64_t r_key_;
        uint64_t cur_prefix_ = 0;
        Memento::hash_iterator it_;
        uint64_t cur_ind_ = 0;
        std::vector<uint64_t> mementos_;
    };

    /**
     * Initialize a key iterator returning keys in the range from `l_key` to
     * `r_key`, inclusive.
     *
     * @param l_key - The left end-point of the iteration range.
     * @param r_key - The right end-point of the iteration range.
     * @returns The corresponding iterator.
     */
	iterator begin(uint64_t l_key=0,
                   uint64_t r_key=std::numeric_limits<uint64_t>::max()) const;

	iterator end() const;


    /**
     * Initialize an iterator starting at the first run after `position`.
     *
     * @param position - The position in the filter to initialize the iterator from.
     * @returns The corresponding iterator.
     */
	hash_iterator hash_begin(uint64_t position=0) const;

	/**
     * Initialize an iterator and position it at the smallest index containing
     * a keepsake box whose prefix hash is greater than or equal to the
     * specified key pair.
     *
	 * @param key - The prefix.
     * @param flags - Flags determining the filter's behavior under
     * concurrency, as well as if `key` is already hashed or not.
     * @returns The corresponding iterator.
	 */
    hash_iterator hash_begin(uint64_t key, uint8_t flags) const;

    hash_iterator hash_end() const;

private:
    qfruntime *runtimedata_;
    qfmetadata *metadata_;
    qfblock *blocks_;

    /**
     * Try to acquire a lock once and return even if the lock is busy. If spin
     * flag is set, then spin until the lock is available.
     *
     * @param lock - The lock to acquire.
     * @param flags - Flags determining the filter's behavior under
     * concurrency, as well as if `key` is already hashed or not. If
     * `flag_wait_for_lock` is set to 1, the thread spins. Otherwise, it tries to
     * acquire the lock once.
     * @returns `true` if `lock` was successfully acquired and `false`
     * otherwise.
     */
    bool spin_lock(volatile int *lock, uint8_t flag);

    /**
     * Unlock the acquired lock.
     *
     * @param lock - The lock to unlock.
     */
    void spin_unlock(volatile int *lock) {
        __sync_lock_release(lock);
        return;
    }

    /**
     * Lock the portion of the filter indicated by `hash_bucket_index`.
     *
     * @param hash_bucket_index - The bucket indicating the target portion of
     * the filter.
     * @param small - idk
     * @param runtime_lock - idk
     * @returns `true` if the portion was was successfully locked and `false`
     * otherwise.
     */
    bool memento_lock(uint64_t hash_bucket_index, bool small, uint8_t runtime_lock);

    /**
     * Unlock the portion of the filter indicated by `hash_bucket_index`.
     *
     * @param hash_bucket_index - The bucket indicating the target portion of
     * the filter.
     * @param small - idk
     */
    void memento_unlock(uint64_t hash_bucket_index, bool small);


    /**
     * Add `cnt` to a metadata value.
     *
     * @param metadata - The metadata value to update.
     * @param cnt - The amount to update the metadata value by.
     */
    void modify_metadata(uint64_t *metadata, int32_t cnt);


    qfblock *get_block(uint64_t block_index) const {
        uint8_t *byte_ptr = reinterpret_cast<uint8_t *>(blocks_);
        const uint32_t byte_offset = block_index 
                * (sizeof(qfblock) - sizeof(qfblock::slots) + slots_per_block_ * metadata_->bits_per_slot / 8);
        return reinterpret_cast<qfblock *>(byte_ptr + byte_offset);
    }


    bool is_runend(uint64_t index) const {
        return (METADATA_WORD(runends, index) >> ((index % slots_per_block_) % 64)) & 1ULL;
    }


    bool is_occupied(uint64_t index) const {
        return (METADATA_WORD(occupieds, index) >> ((index % slots_per_block_) % 64)) & 1ULL;
    }

    /** Only works for little-endian machines. */
    uint64_t get_slot(uint64_t index) const;

    __attribute__((always_inline))
    uint64_t get_fingerprint(uint64_t index) const {
        return get_slot(index) >> metadata_->memento_bits;
    }

    __attribute__((always_inline))
    uint64_t get_memento(uint64_t index) const {
        return get_slot(index) & BITMASK(metadata_->memento_bits);
    }

    /** Only works for little-endian machines. */
    void set_slot(uint64_t index, uint64_t value);

    uint64_t run_end(uint64_t hash_bucket_index) const;

    uint64_t block_offset(uint64_t blockidx) const;

    int32_t offset_lower_bound(uint64_t slot_index) const;

    bool is_empty(uint64_t slot_index) const {
        return offset_lower_bound(slot_index) == 0;
    }

    bool might_be_empty(uint64_t slot_index) const {
        return !is_occupied(slot_index) && !is_runend(slot_index);
    }

    bool probably_is_empty(uint64_t slot_index) const {
        return get_slot(slot_index) == 0 && !is_occupied(slot_index) && !is_runend(slot_index);
    }

    uint64_t find_first_empty_slot(uint64_t from) const;

    uint64_t get_number_of_consecutive_empty_slots(uint64_t first_empty, uint64_t goal_slots) const;

    void shift_remainders(const uint64_t start_index, const uint64_t empty_index);

    void find_next_n_empty_slots(uint64_t from, uint64_t n, uint64_t *indices) const;

    /**
     * Returns pairs of the form `(pos, len)` denoting ranges where empty slots
     * start and how many slots after them are empty.
     *
     * @param[in] from - The position to searching from.
     * @param[in] from - The number of empty slots to find.
     * @param[out] indices - A list to be filled with the `(pos, len)` pairs.
     * For `i >= 0`, `indices[2i]` represents a `pos` value of the `i`-th pair,
     * while `indices[2i + 1]` represents its `len` value.
     * @returns The number of elements filled in `indices`, including both
     * slots positions and empty segment lengths.
     */
    uint32_t find_next_empty_slot_runs_of_size_n(uint64_t from, uint64_t n, uint64_t *indices) const;

    /**
     * Shifts the filter's slots to the right starting from `first` to `last`,
     * inclusive, by `distance` slots. Slots shifter over will be overwritten,
     * while the empty slots resulting from the shift will be zeroed out.
     *
     * @param first, last - The range of slots to shift. Both endpoints are
     * included in the range.
     * @param distance - The number of slots to shift to the right by.
     */
    void shift_slots(int64_t first, uint64_t last, uint64_t distance);

    /**
     * Shifts the filter's `runends` bitmap to the right starting from `first` to `last`,
     * inclusive, by `distance` bits. Bits shifter over will be overwritten,
     * while the empty bits resulting from the shift will be zeroed out.
     *
     * @param first, last - The range of bits to shift. Both endpoints are
     * included in the range.
     * @param distance - The number of bits to shift to the right by.
     */
    void shift_runends(int64_t first, uint64_t last, uint64_t distance);

    /**
     * Removes `remove_length` slots starting at `remove_index`. If
     * `only_item_in_run` is set to `true`, the corresponding `occupieds` bits
     * in the filter will be set to zero if the run becomes empty.
     *
     * @param only_item_in_run - If `true`, the `occupieds` of the run will be
     * reset and the run becomes empty.
     * @param bucket_index - The bucket whose run is being modified.
     * @param remove_index - The slot in the filter where removals will happen
     * from.
     * @param remove_length - The number of slots being removed.
     */
    int32_t remove_slots_and_shift_remainders_and_runends_and_offsets(bool only_item_in_run,
                                                                      uint64_t bucket_index, 
                                                                      uint64_t remove_index,
                                                                      uint64_t remove_length);

    /** 
     * Creates an empty slot for a list of mementos. The indexing here is
     * slightly different from the case where we just want a simple empty slot.
     * This function updates the relevant metadata as well.
     *
     * @param bucket_index - The bucket whose memento list is requesting extra
     * space.
     * @param pos - The position to make the empty slot in.
     * @returns A status code signaling if the operation was successful or if
     * the filter has ran out of space.
     */
    int32_t make_empty_slot_for_memento_list(uint64_t bucket_index, uint64_t pos);


    /** 
     * Creates `n` empty slot for a list of mementos by shifting other slots.
     * The indexing here is slightly different from the case where we just want
     * some empty slot. This function updates the relevant metadata as well.
     *
     * @param bucket_index - The bucket whose memento list is requesting extra
     * space.
     * @param pos - The position to make the empty slots.
     * @param n - The number of empty slots to create.
     * @returns A status code signaling if the operation was successful or if
     * the filter has ran out of space.
     */
    int32_t make_n_empty_slots_for_memento_list(uint64_t bucket_index, uint64_t pos, uint32_t n);

    /** 
     * Writes the keepsake box with a fingerprint of `fingerprint` and a given
     * list of mementos.
     *
     * @param pos - The slots position to write the keepsake box from.
     * @param fingerprint - The keepsake box's fingerprint.
     * @param mementos - The list of mementos to include in the keepsake box.
     * @param memento_cnt - The length of the list of mementos.
     * @returns The number of mementos successfully written to the filter.
     */
    int32_t write_prefix_set(const uint64_t pos, const uint64_t fingerprint, 
                             const uint64_t *mementos, const uint64_t memento_cnt);

    /** 
     * Removes the mementos in the list `mementos` from the keepsake box store
     * at `pos`. A in/out parameter of a boolean list is also passed to
     * indicate which mementos must be removed and which have already been
     * removed. This is included to simplify the prioritized removal of
     * mementos from multiple different keepsake boxes, with the ones with
     * longer fingerprints having higher priority.
     *
     * @param pos[in] - The position of the keepsake box, i.e., the slot where it's
     * fingerprint is stored in.
     * @param mementos[in] - The list of mementos to remove.
     * @param handled[in, out] - A boolean list indicating which mementos of
     * the list have been previously removed and which remain. This list is
     * further updated for future calls to this function. The `i`-th boolean in
     * this list corresponds to the `i`-th memento in `mementos`.
     * @param memento_cnt - The length of the list of mementos.
     * @param new_slot_count - The number of slots the keepsake box uses after
     * the removal of mementos.
     * @param old_slot_count - The number of slots the keepsake box used before
     * the removal of mementos.
     * @returns The number of mementos successfully removed from the keepsake
     * box.
     */
    int32_t remove_mementos_from_prefix_set(const uint64_t pos, const uint64_t *mementos,
                                            bool *handled, const uint32_t memento_cnt,
                                            int32_t *new_slot_count, int32_t *old_slot_count);


    /** 
     * Add a memento to the keepsake box starting at slot `pos`, i.e., the
     * keepsake box's fingerprint is store in the `pos`-th slot of the filter.
     *
     * @param bucket_index - The bucket of the keepsake box.
     * @param pos - The position of the keepsake box being modified.
     * @param new_memento - The memento being added to the keepsake box.
     * @returns A status code signaling if the operation was successful or if
     * the filter has ran out of space.
     */
    int32_t add_memento_to_sorted_list(const uint64_t bucket_index, const uint64_t pos, uint64_t new_memento);

    /**
     * Counts the number of slots occupied by a memento list stored at the
     * `pos`-th slot of the filter. Used to skip over long keepsake boxes.
     * `pos` must point to the slot that is the start of the actual memento
     * list, not any of the slots containing fingerprints.
     *
     * @param pos - The starting slot of the memento list considered. It must
     * not point to the slot storing the fingerprint of the keepsake box, but
     * the slot storing actual mementos.
     * @returns The number of slots the memento list occupies.
     */
    __attribute__((always_inline))
    uint64_t number_of_slots_used_for_memento_list(uint64_t pos) const;

    /**
     * Finds the position of the slot containing the next keepsake box with a
     * matching fingerprint in a run. Note that if the slot at `pos` matches
     * the fingerprint, it is returned. Used to iterate over the matching
     * keepsake boxes in a run during a query.
     *
     * @param pos - The position to start the search for a matching keepsake
     * box.
     * @param fingerprint - The fingerprint to search for.
     * @returns The position of the slot containing the found matching
     * fingerprint, or -1 if not found.
     */
    __attribute__((always_inline))
    int64_t next_matching_fingerprint_in_run(uint64_t pos, const uint64_t fingerprint) const;

    /**
     * Finds the position of the slot containing the next keepsake box's
     * fingerprint that is at least as large as `fingerprint`. Used to search
     * for search for a relevant keepsake box in the iterator.
     *
     * @param pos - The position to start the search.
     * @param fingerprint - The lower bounding value of the fingerprint being
     * searched for.
     * @returns The position of the slot containing a fingerprint that is at
     * least as large as `fingerprint`. If not found, returns a position beyond
     * the end of the run.
     */
    uint64_t lower_bound_fingerprint_in_run(uint64_t pos, uint64_t fingerprint) const;

    /**
     * Finds the position of the slot containing the next keepsake box's
     * fingerprint that is strictly larger than `fingerprint`. Used to maintain
     * the fingerprint order invariant by shifting the keepsake boxes.
     *
     * @param pos - The position to start the search.
     * @param fingerprint - The lower bounding value of the fingerprint being
     * searched for.
     * @returns The position of the slot containing a fingerprint that is
     * strictly larger than `fingerprint`. If not found, returns a position
     * beyond the end of the run.
     */
    uint64_t upper_bound_fingerprint_in_run(uint64_t pos, uint64_t fingerprint) const;

    /**
     * Inserts the list of mementos in `mementos` with the shared prefix hash
     * `hash`. The size of the fingerprints being inserted is controlled by
     * `actual_fingerprint_size`. This is used to handle variable-length
     * fingerprints in an expandable memento filter, where the slots may be
     * wider than the fingerprints.
     *
     * @param hash - The shared prefix hash of the keys.
     * @param mementos - The list of mementos being inserted.
     * @param memento_count - The number of mementos in the list.
     * @param actual_fingerprint_size - The size of the fingerprint being
     * inserted.
     * @param runtime_lock - The lock guarding the runtime data of the filter.
     * @returns The distance of the position the mementos were inserted from
     * their canonical slot. A negative status code is returned if insertion
     * fails.
     */
    int32_t insert_mementos(const __uint128_t hash, const uint64_t mementos[], const uint64_t memento_count, 
                            const uint32_t actual_fingerprint_size, uint8_t runtime_lock);

    /**
     * Retrieves the smallest memento in the keepsake box stored in the `pos`-th
     * slot that is at least as large as `target_memento`. If there is no such
     * memento, the largest memento of the keepsake box is returned. `pos`
     * points to the slot storing the keepsake box's fingerprint. Assumes that
     * the keepsake box encoding has a sorted list of mementos.
     *
     * @param pos - The position of the target keepsake box, i.e., the position
     * of the slot storing its fingerprint.
     * @param target_memento - The memento used for comparison.
     * @returns The smallest memento in the keepsake box that is at least as
     * large as `target_memento`. If not found, returns the largest memento in
     * the keepsake box.
     */
    __attribute__((always_inline))
    uint64_t lower_bound_mementos_for_fingerprint(uint64_t pos, uint64_t target_memento) const;
};


template <bool expandable>
inline bool Memento<expandable>::spin_lock(volatile int *lock, uint8_t flag) {
    if (GET_WAIT_FOR_LOCK(flag) != flag_wait_for_lock) {
        return !__sync_lock_test_and_set(lock, 1);
    } else {
        while (__sync_lock_test_and_set(lock, 1))
            while (*lock);
        return true;
    }
    return false;
}


template <bool expandable>
inline bool Memento<expandable>::memento_lock(uint64_t hash_bucket_index, bool small, uint8_t runtime_lock) {
    uint64_t hash_bucket_lock_offset  = hash_bucket_index % num_slots_to_lock_;
    if (small) {
#ifdef LOG_WAIT_TIME
        if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock],
                    hash_bucket_index / num_slots_to_lock, runtime_lock))
            return false;
        if (num_slots_to_lock - hash_bucket_lock_offset <= cluster_size) {
            if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock + 1],
                        hash_bucket_index / num_slots_to_lock + 1, runtime_lock)) {
                spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock]);
                return false;
            }
        }
#else
        if (!spin_lock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_], runtime_lock))
            return false;
        if (num_slots_to_lock_ - hash_bucket_lock_offset <= cluster_size_) {
            if (!spin_lock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_ + 1], runtime_lock)) {
                spin_unlock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_]);
                return false;
            }
        }
#endif
    } else {
#ifdef LOG_WAIT_TIME
        if (hash_bucket_index >= num_slots_to_lock && hash_bucket_lock_offset <= cluster_size) {
            if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock - 1], runtime_lock))
                return false;
        }
        if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock], runtime_lock)) {
            if (hash_bucket_index >= num_slots_to_lock && hash_bucket_lock_offset <= cluster_size)
                spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock - 1]);
            return false;
        }
        if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock + 1], runtime_lock)) {
            spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock]);
            if (hash_bucket_index >= num_slots_to_lock && hash_bucket_lock_offset <= cluster_size)
                spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock - 1]);
            return false;
        }
#else
        if (hash_bucket_index >= num_slots_to_lock_ && hash_bucket_lock_offset <= cluster_size_) {
            if (!spin_lock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_ - 1], runtime_lock))
                return false;
        }
        if (!spin_lock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_], runtime_lock)) {
            if (hash_bucket_index >= num_slots_to_lock_ && hash_bucket_lock_offset <= cluster_size_)
                spin_unlock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_ - 1]);
            return false;
        }
        if (!spin_lock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_ + 1], runtime_lock)) {
            spin_unlock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_]);
            if (hash_bucket_index >= num_slots_to_lock_ && hash_bucket_lock_offset <= cluster_size_)
                spin_unlock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_ - 1]);
            return false;
        }
#endif
    }
    return true;
}


template <bool expandable>
inline void Memento<expandable>::memento_unlock(uint64_t hash_bucket_index, bool small) {
    uint64_t hash_bucket_lock_offset  = hash_bucket_index % num_slots_to_lock_;
    if (small) {
        if (num_slots_to_lock_ - hash_bucket_lock_offset <= cluster_size_) {
            spin_unlock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_ + 1]);
        }
        spin_unlock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_]);
    } else {
        spin_unlock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_ + 1]);
        spin_unlock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_]);
        if (hash_bucket_index >= num_slots_to_lock_ && hash_bucket_lock_offset <= cluster_size_)
            spin_unlock(&runtimedata_->locks[hash_bucket_index / num_slots_to_lock_ - 1]);
    }
}


template <bool expandable>
inline void Memento<expandable>::modify_metadata(uint64_t *metadata, int32_t cnt) {
#ifdef LOG_WAIT_TIME
    spin_lock(&runtimedata->metadata_lock, runtimedata->num_locks, flag_wait_for_lock);
#else
    spin_lock(&runtimedata_->metadata_lock, flag_wait_for_lock);
#endif
    *metadata = *metadata + cnt;
    spin_unlock(&runtimedata_->metadata_lock);
    return;
}


template <bool expandable>
inline uint64_t Memento<expandable>::get_slot(uint64_t index) const {
    assert(index < metadata_->xnslots);
    /* Should use __uint128_t to support up to 64-bit remainders, but gcc seems
     * to generate buggy code.  :/  */
    uint64_t *p = reinterpret_cast<uint64_t *>(&get_block(index / slots_per_block_)->slots[(index %
                                                    slots_per_block_) * metadata_->bits_per_slot / 8]);
    // you cannot just do *p to get the value, undefined behavior
    uint64_t pvalue;
    memcpy(&pvalue, p, sizeof(pvalue));
    return (pvalue >> (((index % slots_per_block_) * metadata_->bits_per_slot) % 8)) &
                                BITMASK(metadata_->bits_per_slot);
}


template <bool expandable>
inline void Memento<expandable>::set_slot(uint64_t index, uint64_t value) {
    assert(index < metadata_->xnslots);
    /* Should use __uint128_t to support up to 64-bit remainders, but gcc seems
     * to generate buggy code.  :/  */
    uint64_t *p = reinterpret_cast<uint64_t *>(&get_block(index / slots_per_block_) ->slots[(index % 
                                                    slots_per_block_) * metadata_->bits_per_slot / 8]);
    // This results in undefined behavior:
    // uint64_t t = *p;
    uint64_t t;
    memcpy(&t, p, sizeof(t));
    uint64_t mask = BITMASK(metadata_->bits_per_slot);
    uint64_t v = value;
    int32_t shift = ((index % slots_per_block_) * metadata_->bits_per_slot) % 8;
    mask <<= shift;
    v <<= shift;
    t &= ~mask;
    t |= v;
    // This results in undefined behavior:
    // *p = t;
    memcpy(p, &t, sizeof(t));
}


template <bool expandable>
inline uint64_t Memento<expandable>::block_offset(uint64_t blockidx) const {
	/* If we have extended counters and a 16-bit (or larger) offset
		 field, then we can safely ignore the possibility of overflowing
		 that field. */
	if (sizeof(blocks_[0].offset) > 1 ||
			get_block(blockidx)->offset < BITMASK(8 * sizeof(blocks_[0].offset)))
		return get_block(blockidx)->offset;
	return run_end(slots_per_block_ * blockidx - 1) - slots_per_block_ * blockidx + 1;
}


template <bool expandable>
inline uint64_t Memento<expandable>::run_end(uint64_t hash_bucket_index) const {
	uint64_t bucket_block_index = hash_bucket_index / slots_per_block_;
	uint64_t bucket_intrablock_offset = hash_bucket_index % slots_per_block_;
	uint64_t bucket_blocks_offset = block_offset(bucket_block_index);

    uint64_t bucket_intrablock_rank = bitrank(get_block(bucket_block_index)->occupieds[0],
                                              bucket_intrablock_offset);

	if (bucket_intrablock_rank == 0) {
		if (bucket_blocks_offset <= bucket_intrablock_offset)
			return hash_bucket_index;
		else
			return slots_per_block_ * bucket_block_index + bucket_blocks_offset - 1;
	}

	uint64_t runend_block_index  = bucket_block_index + bucket_blocks_offset /
                                                            slots_per_block_;
	uint64_t runend_ignore_bits = bucket_blocks_offset % slots_per_block_;
	uint64_t runend_rank = bucket_intrablock_rank - 1;
    uint64_t runend_block_offset = bitselectv(get_block(runend_block_index)->runends[0],
                                              runend_ignore_bits, runend_rank);
	if (runend_block_offset == slots_per_block_) {
        if (bucket_blocks_offset == 0 && bucket_intrablock_rank == 0) {
            /* The block begins in empty space, and this bucket is in that
             * region of empty space */
            return hash_bucket_index;
        } else {
            do {
                runend_rank -= popcntv(get_block(runend_block_index)->runends[0],
                                        runend_ignore_bits);
                runend_block_index++;
                runend_ignore_bits = 0;
                runend_block_offset = bitselectv(get_block(runend_block_index)->runends[0],
                                                runend_ignore_bits, runend_rank);
            } while (runend_block_offset == slots_per_block_);
        }
    }

    uint64_t runend_index = slots_per_block_ * runend_block_index + runend_block_offset;
    if (runend_index < hash_bucket_index)
        return hash_bucket_index;
    else
		return runend_index;
}


template <bool expandable>
inline int32_t Memento<expandable>::offset_lower_bound(uint64_t slot_index) const {
    const qfblock *b = get_block(slot_index / slots_per_block_);
    const uint64_t slot_offset = slot_index % slots_per_block_;
    const uint64_t boffset = b->offset;
    const uint64_t occupieds = b->occupieds[0] & BITMASK(slot_offset + 1);
    assert(slots_per_block_ == 64);
    if (boffset <= slot_offset) {
        const uint64_t runends = (b->runends[0] & BITMASK(slot_offset)) >> boffset;
        return __builtin_popcountll(occupieds) - __builtin_popcountll(runends);
    }
    return boffset - slot_offset + __builtin_popcountll(occupieds);
}


template <bool expandable>
inline uint64_t Memento<expandable>::find_first_empty_slot(uint64_t from) const {
    do {
        int32_t t = offset_lower_bound(from);
        assert(t >= 0);
        if (t == 0)
            break;
        from = from + t;
    } while(1);
    assert(!is_occupied(from) && !is_runend(from));
    return from;
}


template <bool expandable>
inline uint64_t Memento<expandable>::get_number_of_consecutive_empty_slots(uint64_t first_empty, uint64_t goal_slots) const {
    uint64_t inter_block_offset = first_empty % slots_per_block_;
    uint64_t occupieds = METADATA_WORD(occupieds, first_empty) & (~BITMASK(inter_block_offset));
    
    uint64_t res = 0;
    while (true) {
        uint64_t empty_bits = lowbit_position(occupieds);
        res += empty_bits - inter_block_offset;

        if (empty_bits < 64 || res >= goal_slots)
            break;

        inter_block_offset = 0;
        first_empty += slots_per_block_ - first_empty % slots_per_block_;
        occupieds = METADATA_WORD(occupieds, first_empty);
    }
    return res < goal_slots ? res : goal_slots;
}


template <bool expandable>
inline void Memento<expandable>::shift_remainders(const uint64_t start_index, const uint64_t empty_index) {
	uint64_t last_word = (empty_index + 1) * metadata_->bits_per_slot / 64;
	const uint64_t first_word = start_index * metadata_->bits_per_slot / 64;
	int bend = ((empty_index + 1) * metadata_->bits_per_slot) % 64;
	const int bstart = (start_index * metadata_->bits_per_slot) % 64;

    assert(first_word <= last_word);
	while (last_word != first_word) {
		*REMAINDER_WORD(last_word) = shift_into_b(*REMAINDER_WORD(last_word - 1),
                                                  *REMAINDER_WORD(last_word),
                                                  0, bend, metadata_->bits_per_slot);
		last_word--;
		bend = 64;
	}
    *REMAINDER_WORD(last_word) = shift_into_b(0, *REMAINDER_WORD(last_word),
                                              bstart, bend, metadata_->bits_per_slot);
}


template <bool expandable>
inline void Memento<expandable>::find_next_n_empty_slots(uint64_t from, uint64_t n, uint64_t *indices) const {
	while (n) {
		indices[--n] = find_first_empty_slot(from);
		from = indices[n] + 1;
	}
}


template <bool expandable>
inline uint32_t Memento<expandable>::find_next_empty_slot_runs_of_size_n(uint64_t from, uint64_t n,
                                                                         uint64_t *indices) const {
    uint32_t ind = 0;
    while (n > 0) {
        indices[ind++] = find_first_empty_slot(from);
        indices[ind] = get_number_of_consecutive_empty_slots(indices[ind - 1], n);
        from = indices[ind - 1] + indices[ind];
        n -= indices[ind];
        ind++;

        if (from >= metadata_->xnslots) {
            indices[ind++] = from;
            indices[ind++] = 1;
            return ind;
        }
    }
    return ind;
}


template <bool expandable>
inline void Memento<expandable>::shift_slots(int64_t first, uint64_t last, uint64_t distance) {
    if (distance == 1) {
        shift_remainders(first, last + 1);
        return;
    }

    /*
    if (metadata_->memento_bits > 32) {
        for (int64_t i = last; i >= first; i--)
            set_slot(i + 1, get_slot(i));
    }
    */

    int64_t i, j;
    // Simple implementation:
    // for (i = last; i >= first; i--)
    //     set_slot(i + distance, get_slot(i));

    // Faster implementation? It follows a very similar logic as the macros
    // implemented at the start of this file. More specifically, it moves
    // the data one word at a time as opposed to one slot at a time. It's a
    // bit more optimized for the specific use case of shifting slots by a
    // large distance.
    const uint32_t bits_in_block = metadata_->bits_per_slot * slots_per_block_;

    int64_t x_last_block = last / slots_per_block_;
    int64_t x_bits_in_prev_block = x_last_block * bits_in_block;
    int64_t x_bits_in_block;

    int64_t y_last_block = (last + distance) / slots_per_block_;
    int64_t y_bits_in_prev_block = y_last_block * bits_in_block;
    int64_t y_bits_in_block;

    int64_t x_first_bit = first * metadata_->bits_per_slot;
    int64_t x_last_bit = last * metadata_->bits_per_slot + metadata_->bits_per_slot - 1;
    int64_t y_last_bit = x_last_bit + distance * metadata_->bits_per_slot;

    
    int32_t x_extra, y_extra;
    int32_t x_prefix, y_prefix;
    uint64_t w_i, w_j, payload;
    do {
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

        uint8_t *dest = get_block(y_last_block)->slots + j;
        uint8_t *src = get_block(x_last_block)->slots + i;
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


template <bool expandable>
inline void Memento<expandable>::shift_runends(int64_t first, uint64_t last, uint64_t distance) {
    assert(last < metadata_->xnslots && distance < 64);
    uint64_t first_word = first / 64;
    uint64_t bstart = first % 64;
    uint64_t last_word = (last + distance + 1) / 64;
    uint64_t bend = (last + distance + 1) % 64;

    if (last_word != first_word) {
        // The code in the original RSQF implementation had a weird issue with
        // overwriting parts of the bitmap that it shouldn't have touched. The
        // issue came up when `distance > 1`, and is fixed now.
        const uint64_t first_runends_replacement = METADATA_WORD(runends, first) & (~BITMASK(bstart));
        METADATA_WORD(runends, 64*last_word) = shift_into_b(last_word == first_word + 1 ? first_runends_replacement
                                                                                        : METADATA_WORD(runends, 64 * (last_word - 1)),
                                                            METADATA_WORD(runends, 64 * last_word),
                                                            0, bend, distance);
        bend = 64;
        last_word--;
        while (last_word != first_word) {
            METADATA_WORD(runends, 64 * last_word) = shift_into_b(last_word == first_word + 1 ? first_runends_replacement
                                                                                              : METADATA_WORD(runends, 64 * (last_word - 1)),
                                                                  METADATA_WORD(runends, 64 * last_word),
                                                                  0, bend, distance);
            last_word--;
        }
    }
    METADATA_WORD(runends, 64 * last_word) = shift_into_b(0LL, METADATA_WORD(runends, 64 * last_word),
                                                            bstart, bend, distance);
}

template <bool expandable>
inline int32_t Memento<expandable>::remove_slots_and_shift_remainders_and_runends_and_offsets(bool only_item_in_run,
                                                                                              uint64_t bucket_index, 
                                                                                              uint64_t remove_index,
                                                                                              uint64_t remove_length) {
    // If this is the last thing in its run, then we may need to set a new runend bit
    const bool was_runend = is_runend(remove_index + remove_length - 1);
    if (was_runend && !only_item_in_run)
        METADATA_WORD(runends, remove_index - 1) |= 1ULL << ((remove_index - 1) % 64);

    // shift slots back one run at a time
    uint64_t original_bucket = bucket_index;
    uint64_t current_bucket = bucket_index;
    uint64_t current_slot = remove_index;
    uint64_t current_distance = remove_length;
    int64_t last_slot_in_initial_cluster = -1;
    int ret_current_distance = current_distance;

    while (current_distance > 0) { // every iteration of this loop deletes one slot from the item and shifts the cluster accordingly
                                   // start with an occupied-runend pair
        current_bucket = bucket_index;
        current_slot = remove_index + current_distance - 1;

        if (!was_runend) 
            while (!is_runend(current_slot))
                current_slot++; // step to the end of the run
        do {
            current_bucket++;
        } while (current_bucket <= current_slot && !is_occupied(current_bucket)); // step to the next occupied bucket
                                                                                  // current_slot should now be on the last slot in the run and current_bucket should be on the bucket for the next run

        while (current_bucket <= current_slot) { // until we find the end of the cluster,
                                                 // find the last slot in the run
            current_slot++; // step into the next run
            while (!is_runend(current_slot))
                current_slot++; // find the last slot in this run
                                // find the next bucket
            do {
                current_bucket++;
            } while (current_bucket <= current_slot && !is_occupied(current_bucket));
        }

        if (last_slot_in_initial_cluster == -1)
            last_slot_in_initial_cluster = current_slot;

        // now that we've found the last slot in the cluster, we can shift the whole cluster over by 1
        uint64_t i;
        for (i = remove_index; i < current_slot; i++) {
            set_slot(i, get_slot(i + 1));
            if (is_runend(i) != is_runend(i + 1))
                METADATA_WORD(runends, i) ^= 1ULL << (i % 64);
        }
        set_slot(i, 0);
        METADATA_WORD(runends, i) &= ~(1ULL << (i % 64));

        current_distance--;
    }

    // reset the occupied bit of the hash bucket index if the hash is the
    // only item in the run and is removed completely.
    if (only_item_in_run)
        METADATA_WORD(occupieds, bucket_index) &= ~(1ULL << (bucket_index % 64));

    // update the offset bits.
    // find the number of occupied slots in the original_bucket block.
    // Then find the runend slot corresponding to the last run in the
    // original_bucket block.
    // Update the offset of the block to which it belongs.
    uint64_t original_block = original_bucket / slots_per_block_;
    if (remove_length > 0) {	// we only update offsets if we shift/delete anything
        while (original_block < last_slot_in_initial_cluster / slots_per_block_) {
            uint64_t last_occupieds_hash_index = slots_per_block_ * original_block + (slots_per_block_ - 1);
            uint64_t runend_index = run_end(last_occupieds_hash_index);
            // runend spans across the block
            // update the offset of the next block
            if (runend_index / slots_per_block_ == original_block) { // if the run ends in the same block
                get_block(original_block + 1)->offset = 0;
            } else { // if the last run spans across the block
                const uint32_t max_offset = (uint32_t) BITMASK(8 * sizeof(blocks_[0].offset));
                const uint32_t new_offset = runend_index - last_occupieds_hash_index;
                get_block(original_block + 1)->offset = new_offset < max_offset ? new_offset : max_offset;
            }
            original_block++;
        }
    }

    modify_metadata(&metadata_->noccupied_slots, -((int32_t) remove_length));
    return ret_current_distance;
}


template <bool expandable>
inline int32_t Memento<expandable>::make_empty_slot_for_memento_list(uint64_t bucket_index, uint64_t pos) {
    const uint64_t next_empty = find_first_empty_slot(pos);
    if (next_empty >= metadata_->xnslots) {  // Check that the new data fits
        return err_no_space;
    }
    if (pos < next_empty)
        shift_slots(pos, next_empty - 1, 1);
    shift_runends(pos - 1, next_empty - 1, 1);
    for (uint32_t i = bucket_index / slots_per_block_ + 1;
            i <= next_empty / slots_per_block_; i++) {
        if (get_block(i)->offset + 1ULL <= BITMASK(8 * sizeof(blocks_[0].offset)))
            get_block(i)->offset++;
    }
    modify_metadata(&metadata_->noccupied_slots, 1);
    return 0;
}


template <bool expandable>
inline int32_t Memento<expandable>::make_n_empty_slots_for_memento_list(uint64_t bucket_index, uint64_t pos, uint32_t n) {
    uint64_t empty_runs[2 * n];
    uint64_t empty_runs_ind = find_next_empty_slot_runs_of_size_n(pos, n, empty_runs);
    if (empty_runs[empty_runs_ind - 2] + empty_runs[empty_runs_ind - 1] - 1 >= metadata_->xnslots) {
        // Check that the new data fits
        return err_no_space;
    }

    uint64_t shift_distance = 0;
    for (int i = empty_runs_ind - 2; i >= 2; i -= 2) {
        shift_distance += empty_runs[i + 1];
        shift_slots(empty_runs[i - 2] + empty_runs[i - 1], empty_runs[i] - 1,
                shift_distance);
        shift_runends(empty_runs[i - 2] + empty_runs[i - 1], empty_runs[i] - 1,
                shift_distance);
    }
    if (pos < empty_runs[0])
        shift_slots(pos, empty_runs[0] - 1, n);
    shift_runends(pos - 1, empty_runs[0] - 1, n);
    // Update offsets
    uint64_t npreceding_empties = 0;
    uint32_t empty_iter = 0;
    uint32_t last_block_to_update_offset = (empty_runs[empty_runs_ind - 2] + 
                                            empty_runs[empty_runs_ind - 1] - 1) / slots_per_block_;
    for (uint64_t i = bucket_index / slots_per_block_ + 1;
            i <= last_block_to_update_offset; i++) {
        while (npreceding_empties < n) {
            uint64_t r = i * slots_per_block_;
            uint64_t l = r - slots_per_block_;
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
        if (get_block(i)->offset + n - npreceding_empties < BITMASK(8 * sizeof(blocks_[0].offset)))
            get_block(i)->offset += n - npreceding_empties;
        else
            get_block(i)->offset = static_cast<uint8_t>(BITMASK(8 * sizeof(blocks_[0].offset)));
    }
    modify_metadata(&metadata_->noccupied_slots, n);

    return 0;
}


template <bool expandable>
inline int32_t Memento<expandable>::write_prefix_set(const uint64_t pos, const uint64_t fingerprint, 
                                                     const uint64_t *mementos, const uint64_t memento_cnt) {
    if (memento_cnt == 1) {
        set_slot(pos, (fingerprint << metadata_->memento_bits) | mementos[0]);
        return 1;
    }

    if (fingerprint == 0) {     // Can't use a void fingerprint
        uint64_t payload = 0, current_full_prefix = 0;
        uint64_t dest_bit_pos = (pos % slots_per_block_) * metadata_->bits_per_slot;
        uint64_t dest_block_ind = pos / slots_per_block_;
        INIT_PAYLOAD_WORD(payload, current_full_prefix, dest_bit_pos, dest_block_ind);
        for (uint32_t i = 0; i < memento_cnt; i++) {
            uint64_t val = mementos[i];
            APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, val,
                                      metadata_->bits_per_slot, dest_bit_pos, dest_block_ind);
        }
        if (current_full_prefix) {
            FLUSH_PAYLOAD_WORD(payload, current_full_prefix, dest_bit_pos, dest_block_ind);
        }
        return memento_cnt;
    }

    uint32_t res = 2;
    uint64_t payload = 0, current_full_prefix = 0;
    uint64_t dest_bit_pos = (pos % slots_per_block_) * metadata_->bits_per_slot;
    uint64_t dest_block_ind = pos / slots_per_block_;
    INIT_PAYLOAD_WORD(payload, current_full_prefix, dest_bit_pos, dest_block_ind);
    if (memento_cnt == 2) {
        uint64_t val = (fingerprint << metadata_->memento_bits) | mementos[0];
        APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, val,
                                  metadata_->bits_per_slot, dest_bit_pos, dest_block_ind);
        val = mementos[1];
        APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, val,
                                  metadata_->bits_per_slot, dest_bit_pos, dest_block_ind);
        if (mementos[0] == mementos[1]) {
            APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, 0LL,
                                    metadata_->bits_per_slot, dest_bit_pos, dest_block_ind);
            res++;
        }
    }
    else {
        uint64_t val = (fingerprint << metadata_->memento_bits) | mementos[memento_cnt - 1];
        APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, val, metadata_->bits_per_slot,
                                  dest_bit_pos, dest_block_ind);
        val = mementos[0];
        APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, val, metadata_->bits_per_slot,
                                  dest_bit_pos, dest_block_ind);
    }
    if (memento_cnt > 2) {
        const uint32_t memento_bits = metadata_->memento_bits;
        const uint64_t max_memento_value = (1ULL << metadata_->memento_bits) - 1;
        const uint64_t list_len = memento_cnt - 2;
        int32_t written_bits = memento_bits;
        if (list_len >= max_memento_value) {
            uint64_t fragments[5], frag_cnt = 0;
            for (uint32_t cnt = list_len; cnt; cnt /= max_memento_value) {
                fragments[frag_cnt++] = cnt % max_memento_value;
            }
            for (uint32_t i = 0; i < frag_cnt - 1; i++) {
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, max_memento_value,
                                          memento_bits, dest_bit_pos, dest_block_ind);
            }
            for (uint32_t i = 0; i < frag_cnt; i++) {
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, fragments[i],
                                          memento_bits, dest_bit_pos, dest_block_ind);
            }
            written_bits += 2 * (frag_cnt - 1) * memento_bits;
        }
        else {
            APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, list_len,
                                      memento_bits, dest_bit_pos, dest_block_ind);
        }
        for (uint32_t i = 1; i < memento_cnt - 1; i++) {
            written_bits += memento_bits;
            APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, mementos[i],
                                      memento_bits, dest_bit_pos, dest_block_ind);
        }
        while (written_bits > 0) {
            res++;
            written_bits -= metadata_->bits_per_slot;
        }
        
        // Optional, can be removed
        if (written_bits < 0) {
            APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, 0LL, -written_bits,
                                      dest_bit_pos, dest_block_ind);
        }
    }
    if (current_full_prefix) {
        FLUSH_PAYLOAD_WORD(payload, current_full_prefix, dest_bit_pos, dest_block_ind);
    }
    return res;
}


template <bool expandable>
inline int32_t Memento<expandable>::remove_mementos_from_prefix_set(const uint64_t pos, const uint64_t *mementos,
                                                                    bool *handled, const uint32_t memento_cnt,
                                                                    int32_t *new_slot_count, int32_t *old_slot_count) {
    const uint64_t f1 = get_fingerprint(pos);
    const uint64_t m1 = get_memento(pos);
    const uint64_t f2 = get_fingerprint(pos + 1);
    const uint64_t m2 = get_memento(pos + 1);
    const uint64_t memento_bits = metadata_->memento_bits;
    const uint64_t max_memento_value = BITMASK(memento_bits);

    if (f1 <= f2 || is_runend(pos)) {
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
    uint64_t filled_bits = 0;
    uint64_t data_bit_pos = ((pos + 2) % slots_per_block_) * metadata_->bits_per_slot;
    uint64_t data_block_ind = (pos + 2) / slots_per_block_;
    GET_NEXT_DATA_WORD_IF_EMPTY(data, filled_bits, memento_bits,
                                data_bit_pos, data_block_ind);
    if (m1 >= m2) {
        *old_slot_count = number_of_slots_used_for_memento_list(pos + 2) + 2;
        old_memento_cnt += data & max_memento_value;
        data >>= memento_bits;
        filled_bits -= memento_bits;
        if (old_memento_cnt == max_memento_value + 2) {
            uint64_t length = 2, pw = 1;
            old_memento_cnt = 2;
            old_unary_cnt = 1;
            while (length) {
                GET_NEXT_DATA_WORD_IF_EMPTY(data, filled_bits, memento_bits,
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
    }
    else {
        res_mementos[res_cnt++] = val;
    }
    // Handle the actual list
    for (uint32_t i = 1; i < old_memento_cnt - 1; i++) {
        GET_NEXT_DATA_WORD_IF_EMPTY(data, filled_bits, memento_bits,
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
        }
        else {
            res_mementos[res_cnt++] = val;
        }
    }
    // Handle the maximum
    val = (m1 < m2 ? m2 : m1);
    while (cmp_ind < memento_cnt && (handled[cmp_ind] || mementos[cmp_ind] < val)) {
        cmp_ind++;
    }
    if (cmp_ind < memento_cnt && mementos[cmp_ind] == val) {
        handled[cmp_ind++] = true;
        newly_handled_cnt++;
    }
    else {
        res_mementos[res_cnt++] = val;
    }

    if (res_cnt != old_memento_cnt) {
        // Something changed
        *new_slot_count = res_cnt ? write_prefix_set(pos, f1, res_mementos, res_cnt) : 0;
    }
    else {
        // Nothing changed
        *new_slot_count = -1;
    }

    return newly_handled_cnt;
}

template <bool expandable>
inline int32_t Memento<expandable>::add_memento_to_sorted_list(const uint64_t bucket_index,
                                                               const uint64_t pos,
                                                               uint64_t new_memento) {
    const uint64_t f1 = get_fingerprint(pos);
    const uint64_t m1 = get_memento(pos);
    const uint64_t f2 = get_fingerprint(pos + 1);
    const uint64_t m2 = get_memento(pos + 1);
    const uint64_t memento_bits = metadata_->memento_bits;

    const bool singleton_prefix_set = (is_runend(pos) || f1 <= f2);
    if (singleton_prefix_set) {
        if (new_memento == m1) {
            int32_t err = make_n_empty_slots_for_memento_list(bucket_index, pos + 1, 2);
            if (err < 0)    // Check that the new data fits
                return err;

            if (new_memento < m1) {
                set_slot(pos, (f1 << memento_bits) | new_memento);
                set_slot(pos + 1, m1);
            }
            else {
                set_slot(pos + 1, new_memento);
            }
            set_slot(pos + 2, 0);
        }
        else {
            int32_t err = make_empty_slot_for_memento_list(bucket_index, pos + 1);
            if (err < 0)    // Check that the new data fits
                return err;

            if (new_memento < m1) {
                set_slot(pos, (f1 << memento_bits) | new_memento);
                set_slot(pos + 1, m1);
            }
            else {
                set_slot(pos, (f1 << memento_bits) | m1);
                set_slot(pos + 1, new_memento);
            }
        }
        return 0;
    }

    const bool size_two_prefix_set = (m1 < m2);
    if (size_two_prefix_set) {
        if (metadata_->bits_per_slot < 2 * metadata_->memento_bits) {
            int32_t err = make_n_empty_slots_for_memento_list(bucket_index, pos + 1, 2);
            if (err < 0)    // Check that the new data fits
                return err;

            uint64_t payload = 0, dest_pos = pos;
            uint64_t dest_bit_pos = (dest_pos % slots_per_block_) * metadata_->bits_per_slot;
            uint64_t dest_block_ind = dest_pos / slots_per_block_;
            uint32_t current_full_prefix = 0;
            INIT_PAYLOAD_WORD(payload, current_full_prefix, dest_bit_pos, dest_block_ind);
            uint64_t value;
            if (new_memento < m1) {
                value = (f1 << memento_bits) | m2;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          metadata_->bits_per_slot, dest_bit_pos,
                                          dest_block_ind);
                value = (f2 << memento_bits) | new_memento;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          metadata_->bits_per_slot, dest_bit_pos,
                                          dest_block_ind);
                value = 1ULL;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          memento_bits, dest_bit_pos, dest_block_ind);
                value = m1;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          memento_bits, dest_bit_pos, dest_block_ind);
            }
            else if (m2 < new_memento) {
                value = (f1 << memento_bits) | new_memento;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          metadata_->bits_per_slot, dest_bit_pos,
                                          dest_block_ind);
                value = (f2 << memento_bits) | m1;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          metadata_->bits_per_slot, dest_bit_pos,
                                          dest_block_ind);
                value = 1ULL;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          memento_bits, dest_bit_pos, dest_block_ind);
                value = m2;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          memento_bits, dest_bit_pos, dest_block_ind);
            }
            else {
                value = (f1 << memento_bits) | m2;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          metadata_->bits_per_slot, dest_bit_pos,
                                          dest_block_ind);
                value = (f2 << memento_bits) | m1;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value, 
                                          metadata_->bits_per_slot, dest_bit_pos,
                                          dest_block_ind);
                value = 1ULL;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          memento_bits, dest_bit_pos, dest_block_ind);
                value = new_memento;
                APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, value,
                                          memento_bits, dest_bit_pos, dest_block_ind);
            }

            if (current_full_prefix)
                FLUSH_PAYLOAD_WORD(payload, current_full_prefix, dest_bit_pos, dest_block_ind);
        }
        else {
            int32_t err = make_empty_slot_for_memento_list(bucket_index, pos + 2);
            if (err < 0)    // Check that the new data fits
                return err;

            if (new_memento < m1) {
                set_slot(pos, (f1 << memento_bits) | m2);
                set_slot(pos + 1, (f2 << memento_bits) | new_memento);
                set_slot(pos + 2, (m1 << memento_bits) | 1ULL);
            }
            else if (m2 < new_memento) {
                set_slot(pos, (f1 << memento_bits) | new_memento);
                set_slot(pos + 1, (f2 << memento_bits) | m1);
                set_slot(pos + 2, (m2 << memento_bits) | 1ULL);
            }
            else {
                set_slot(pos, (f1 << memento_bits) | m2);
                set_slot(pos + 1, (f2 << memento_bits) | m1);
                set_slot(pos + 2, (new_memento << memento_bits) | 1ULL);
            }
        }
        return 0;
    }

    if (new_memento < m2) {
        set_slot(pos + 1, (f2 << memento_bits) | new_memento);
        new_memento = m2;
    }
    else if (m1 < new_memento) {
        set_slot(pos, (f1 << memento_bits) | new_memento);
        new_memento = m1;
    }

    const uint64_t max_memento_value = BITMASK(memento_bits);
    uint64_t ind = pos + 2, ind_cnt = 0;
    uint64_t data = 0;
    uint64_t filled_bits = 0;
    uint64_t data_bit_pos = (ind % slots_per_block_) * metadata_->bits_per_slot;
    uint64_t data_block_ind = ind / slots_per_block_;
    GET_NEXT_DATA_WORD_IF_EMPTY(data, filled_bits, memento_bits, data_bit_pos, data_block_ind);

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
            GET_NEXT_DATA_WORD_IF_EMPTY(data, filled_bits, memento_bits,
                                        data_bit_pos, data_block_ind);
            const uint64_t current_fragment = data & max_memento_value;
            ind_cnt += memento_bits;
            if (ind_cnt >= metadata_->bits_per_slot) {
                ind++;
                ind_cnt -= metadata_->bits_per_slot;
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

    uint64_t mementos[memento_count + 1];
    uint32_t cnt = 0;
    while (cnt < memento_count) {
        GET_NEXT_DATA_WORD_IF_EMPTY(data, filled_bits, memento_bits, data_bit_pos, data_block_ind);
        mementos[cnt] = data & max_memento_value;
        ind_cnt += memento_bits;
        if (ind_cnt >= metadata_->bits_per_slot) {
            ind++;
            ind_cnt -= metadata_->bits_per_slot;
        }
        data >>= memento_bits;
        filled_bits -= memento_bits;
        cnt++;
    }

    int32_t extra_bits = (2 * unary_count + memento_count + 1) * memento_bits;
    while (extra_bits > 0)
        extra_bits -= metadata_->bits_per_slot;
    int32_t extra_slots = 0;
    extra_bits += memento_bits + counter_overflow * 2 * memento_bits;
    while (extra_bits > 0) {
        extra_bits -= metadata_->bits_per_slot;
        extra_slots++;
    }

    if (extra_slots) {
        // Find empty slots and shift everything to fit the new mementos
        ind++;
        make_n_empty_slots_for_memento_list(bucket_index, ind, extra_slots);
    }

    // Update the actual list 
    memento_count++;
    uint64_t payload = 0, dest_pos = pos + 2;
    uint64_t dest_bit_pos = (dest_pos % slots_per_block_) * metadata_->bits_per_slot;
    uint64_t dest_block_ind = dest_pos / slots_per_block_;
    uint32_t current_full_prefix = 0;
    INIT_PAYLOAD_WORD(payload, current_full_prefix, dest_bit_pos, dest_block_ind);
    unary_count += counter_overflow;
    if (unary_count) {
        while (unary_count) {
            APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, max_memento_value,
                                      memento_bits, dest_bit_pos, dest_block_ind);
            unary_count--;
        }
        for (cnt = memento_count; cnt; cnt /= max_memento_value) {
            const uint64_t appendee = cnt % max_memento_value;
            APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, appendee,
                                      memento_bits, dest_bit_pos, dest_block_ind);
        }
    }
    else {
        APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, memento_count,
                                  memento_bits, dest_bit_pos, dest_block_ind);
    }
    bool written_new_memento = false;
    for (uint32_t i = 0; i < memento_count - 1; i++) {
        if (!written_new_memento && mementos[i] > new_memento) {
            written_new_memento = true;
            APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, new_memento,
                                      memento_bits, dest_bit_pos, dest_block_ind);
        }
        const uint64_t memento = mementos[i];
        APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, memento,
                                  memento_bits, dest_bit_pos, dest_block_ind);
    }
    if (!written_new_memento) {
        APPEND_WRITE_PAYLOAD_WORD(payload, current_full_prefix, new_memento,
                                  memento_bits, dest_bit_pos, dest_block_ind);
    }
    if (current_full_prefix) {
        FLUSH_PAYLOAD_WORD(payload, current_full_prefix, dest_bit_pos, dest_block_ind);
    }

    return 0;
}


template <bool expandable>
inline uint64_t Memento<expandable>::number_of_slots_used_for_memento_list(uint64_t pos) const {
    const uint64_t max_memento = ((1ULL << metadata_->memento_bits) - 1);
    uint64_t data = get_slot(pos);
    uint64_t memento_count = (data & max_memento) + 1;
    if (memento_count == max_memento + 1) {
        // This is very unlikely to execute
        uint64_t length = 2, pw = 1;
        uint64_t bits_left = metadata_->bits_per_slot - metadata_->memento_bits;
        data >>= metadata_->memento_bits;
        memento_count = 1;
        pos++;
        while (length > 0) {
            if (bits_left < metadata_->memento_bits) {
                data |= get_slot(pos) << bits_left;
                bits_left += metadata_->bits_per_slot;
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
            data >>= metadata_->memento_bits;
            bits_left -= metadata_->memento_bits;
        }
    }

    int64_t bits_left = memento_count * metadata_->memento_bits;
    uint64_t res = 0;
    // Slight optimization for doing this division?
    const int64_t step = metadata_->bits_per_slot * 16;
    while (bits_left >= step) {
        bits_left -= step;
        res += 16;
    }
    while (bits_left > 0) {
        bits_left -= metadata_->bits_per_slot;
        res++;
    }
    return res;
}


template <bool expandable>
inline int64_t Memento<expandable>::next_matching_fingerprint_in_run(uint64_t pos, const uint64_t fingerprint) const {
    uint64_t current_fingerprint, current_memento;
    uint64_t next_fingerprint, next_memento;
    while (true) {
        current_fingerprint = get_fingerprint(pos);
        current_memento = get_memento(pos);
        if (fingerprint < current_fingerprint)
            return -1;

        pos++;
        if constexpr (expandable) {
            const uint64_t current_highbit_pos = highbit_position(current_fingerprint);
            const uint64_t cmp_mask = BITMASK(current_highbit_pos);
            const bool match = CMP_MASK_FINGERPRINT(current_fingerprint, fingerprint, cmp_mask);
            if (match)
                return pos - 1;
            else if (is_runend(pos - 1))
                return -1;
        }
        else {
            if (fingerprint == current_fingerprint)
                return pos - 1;
            else if (is_runend(pos - 1))
                return -1;
        }

        next_fingerprint = get_fingerprint(pos);
        if (current_fingerprint > next_fingerprint) {
            next_memento = get_memento(pos);
            pos++;
            if (current_memento >= next_memento) {
                // Mementos encoded as a sorted list
                pos += number_of_slots_used_for_memento_list(pos);
            }
            if (is_runend(pos - 1)) {
                return -1;
            }
        }
    }
    return pos;
}


template <bool expandable>
inline uint64_t Memento<expandable>::lower_bound_fingerprint_in_run(uint64_t pos, uint64_t fingerprint) const {
    uint64_t current_fingerprint, current_memento;
    uint64_t next_fingerprint, next_memento;
    do {
        current_fingerprint = get_fingerprint(pos);
        current_memento = get_memento(pos);
        if (fingerprint <= current_fingerprint) {
            break;
        }

        pos++;
        if (is_runend(pos - 1))
            break;

        next_fingerprint = get_fingerprint(pos);
        if (next_fingerprint < current_fingerprint) {
            next_memento = get_memento(pos);
            if (current_memento < next_memento)
                pos++;
            else {
                // Mementos encoded as a sorted list
                pos++;
                pos += number_of_slots_used_for_memento_list(pos);
            }
        }
    } while (!is_runend(pos - 1));
    return pos;
}


template <bool expandable>
inline uint64_t Memento<expandable>::upper_bound_fingerprint_in_run(uint64_t pos, uint64_t fingerprint) const {
    uint64_t current_fingerprint, current_memento;
    do {
        current_fingerprint = get_fingerprint(pos);
        current_memento = get_memento(pos);
        if (fingerprint < current_fingerprint) {
            break;
        }

        pos++;
        if (is_runend(pos - 1))
            break;

        if (get_fingerprint(pos) < current_fingerprint) {
            if (current_memento < get_memento(pos))
                pos++;
            else {
                // Mementos encoded as a sorted list
                pos++;
                pos += number_of_slots_used_for_memento_list(pos);
            }
        }
    } while (!is_runend(pos - 1));
    return pos;
}


template <bool expandable>
inline int32_t Memento<expandable>::insert_mementos(const __uint128_t hash, const uint64_t mementos[],
                                                    const uint64_t memento_count, const uint32_t actual_fingerprint_size,
                                                    uint8_t runtime_lock) {
    int ret_distance = 0;
    const uint32_t bucket_index_hash_size = get_bucket_index_hash_size();
    const uint64_t hash_fingerprint = (hash >> bucket_index_hash_size) & BITMASK(actual_fingerprint_size)
                                    | (static_cast<uint64_t>(expandable) << actual_fingerprint_size);
    const uint32_t orig_quotient_size = metadata_->original_quotient_bits;
    const uint64_t hash_bucket_index = ((hash & BITMASK(orig_quotient_size)) << (bucket_index_hash_size - orig_quotient_size))
                                    | ((hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));


    uint32_t new_slot_count = memento_count, memento_unary_count = 0;
    const uint64_t max_memento_value = (1ULL << metadata_->memento_bits) - 1;
    if (hash_fingerprint && memento_count > 2) {
        new_slot_count = 0;
        int32_t total_new_bits = 2 * metadata_->bits_per_slot +
            (memento_count - 2 + (metadata_->memento_bits > 2)) * metadata_->memento_bits;

        if (memento_count - 2 >= max_memento_value) {
            // Must take into account the extra length of the memento counter.
            // This will rarely execute
            uint32_t val = max_memento_value - 1;
            for (uint32_t tmp_cnt = val; tmp_cnt < memento_count - 2; tmp_cnt += val) {
                val *= max_memento_value;
                memento_unary_count++;
            }
            total_new_bits += 2 * memento_unary_count * metadata_->memento_bits;
        }

        while (total_new_bits > 0) {
            total_new_bits -= metadata_->bits_per_slot;
            new_slot_count++;
        }   // Result of new_slot_count same as using normal division: `total_new_bits / metadata->bits_per_slot`
            // Hopefully this is a bit faster to calculate.
    }
    else if (hash_fingerprint != 0 && (memento_count == 2 && mementos[0] == mementos[1])) {
        new_slot_count = 3;
    }

    if (GET_NO_LOCK(runtime_lock) != flag_no_lock) {
        if (!memento_lock(hash_bucket_index, /*small*/ true, runtime_lock))
            return err_couldnt_lock;
    }

    // Find empty slots and shift everything to fit the new mementos
    uint64_t empty_runs[65];
    uint64_t empty_runs_ind = find_next_empty_slot_runs_of_size_n(hash_bucket_index,
                                                            new_slot_count, empty_runs);
    if (empty_runs[empty_runs_ind - 2] + empty_runs[empty_runs_ind - 1] - 1 >= metadata_->xnslots) {
        // Check that the new data fits
        if (GET_NO_LOCK(runtime_lock) != flag_no_lock) {
            memento_unlock(hash_bucket_index, /*small*/ true);
        }
        return err_no_space;
    }

    uint64_t shift_distance = 0;
    for (int i = empty_runs_ind - 2; i >= 2; i -= 2) {
        shift_distance += empty_runs[i + 1];
        shift_slots(empty_runs[i - 2] + empty_runs[i - 1], empty_runs[i] - 1, shift_distance);
        shift_runends(empty_runs[i - 2] + empty_runs[i - 1], empty_runs[i] - 1, shift_distance);
    }

    // Update offsets
    uint64_t npreceding_empties = 0;
    uint32_t empty_iter = 0;
    uint32_t last_block_to_update_offset = (empty_runs[empty_runs_ind - 2] + 
                                            empty_runs[empty_runs_ind - 1] - 1) 
                                                / slots_per_block_;
    for (uint64_t i = hash_bucket_index / slots_per_block_ + 1;
                i <= last_block_to_update_offset; i++) {
        while (npreceding_empties < new_slot_count) {
            uint64_t r = i * slots_per_block_;
            uint64_t l = r - slots_per_block_;
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

        if (get_block(i)->offset + new_slot_count - npreceding_empties < BITMASK(8 * sizeof(blocks_[0].offset)))
            get_block(i)->offset += new_slot_count - npreceding_empties;
        else
            get_block(i)->offset = BITMASK(8 * sizeof(blocks_[0].offset));
    }

    uint64_t runend_index = run_end(hash_bucket_index);
    uint64_t runstart_index = hash_bucket_index == 0 ? 0 
                                : run_end(hash_bucket_index - 1) + 1;
    uint64_t insert_index;
    if (is_occupied(hash_bucket_index)) {
        insert_index = upper_bound_fingerprint_in_run(runstart_index, hash_fingerprint);

        if (insert_index < empty_runs[0]) {
            shift_slots(insert_index, empty_runs[0] - 1, new_slot_count);
            shift_runends(insert_index, empty_runs[0] - 1, new_slot_count);
        }
        METADATA_WORD(runends, runend_index) &= ~(1ULL << ((runend_index % slots_per_block_) % 64));
        METADATA_WORD(runends, runend_index + new_slot_count) |= 1ULL << (((runend_index + new_slot_count) % slots_per_block_) % 64);
    }
    else {
        if (hash_bucket_index == empty_runs[0]) {
            insert_index = hash_bucket_index;
        }
        else {
            insert_index = runend_index + 1;
            if (insert_index < empty_runs[0]) {
                shift_slots(insert_index, empty_runs[0] - 1, new_slot_count);
                shift_runends(insert_index, empty_runs[0] - 1, new_slot_count);
            }
        }
        METADATA_WORD(runends, insert_index + new_slot_count - 1) |= 1ULL << (((insert_index + new_slot_count - 1) % slots_per_block_) % 64);
        METADATA_WORD(occupieds, hash_bucket_index) |= 1ULL << ((hash_bucket_index % slots_per_block_) % 64);
    }

    // Move in the payload!
    write_prefix_set(insert_index, hash_fingerprint, mementos, memento_count);

    modify_metadata(&metadata_->ndistinct_elts, 1);
    modify_metadata(&metadata_->noccupied_slots, new_slot_count);
    modify_metadata(&metadata_->nelts, memento_count);

    if (GET_NO_LOCK(runtime_lock) != flag_no_lock) {
        memento_unlock(hash_bucket_index, /*small*/ true);
    }

    return ret_distance;
}


template <bool expandable>
inline Memento<expandable>::Memento(uint64_t nslots, uint64_t key_bits, uint64_t memento_bits,
                                    hashmode hash_mode, uint32_t seed, const uint64_t orig_quotient_bit_cnt) {
    uint64_t num_slots, xnslots, nblocks;
    uint64_t fingerprint_bits, bits_per_slot;
    uint64_t size;
    uint64_t total_num_bytes;

    /* nslots can be any number now, as opposed to just being able to be a power of 2! */
    num_slots = nslots;
    xnslots = nslots + 10 * sqrt((double) nslots);
    nblocks = (xnslots + slots_per_block_ - 1) / slots_per_block_;
    fingerprint_bits = key_bits;
    while (nslots > 1) {
        assert(fingerprint_bits > 0);
        fingerprint_bits--;
        nslots >>= 1;
    }
    fingerprint_bits -= (__builtin_popcountll(num_slots) > 1);

    bits_per_slot = fingerprint_bits + memento_bits + expandable;
    assert(bits_per_slot > 1);
    size = nblocks * (sizeof(qfblock) - sizeof(uint8_t) + slots_per_block_ * bits_per_slot / 8);

    total_num_bytes = sizeof(qfmetadata) + size;
    uint8_t *buffer = new uint8_t[total_num_bytes] {};
    metadata_ = reinterpret_cast<qfmetadata *>(buffer);
    blocks_ = reinterpret_cast<qfblock *>(metadata_ + 1);

    metadata_->magic_endian_number = magic_number_;
    metadata_->auto_resize = expandable;
    metadata_->hash_mode = hash_mode;
    metadata_->total_size_in_bytes = size;
    metadata_->seed = seed;
    metadata_->nslots = num_slots;
    metadata_->xnslots = xnslots;
    metadata_->key_bits = key_bits;
    metadata_->original_quotient_bits = (orig_quotient_bit_cnt ?
                                          orig_quotient_bit_cnt 
                                        : key_bits - fingerprint_bits);
    metadata_->memento_bits = memento_bits;
    metadata_->fingerprint_bits = fingerprint_bits;
    metadata_->bits_per_slot = bits_per_slot;

    metadata_->range = metadata_->nslots;
    metadata_->range <<= metadata_->fingerprint_bits + metadata_->memento_bits;
    metadata_->nblocks = (metadata_->xnslots + slots_per_block_ - 1) / slots_per_block_;
    metadata_->nelts = 0;
    metadata_->ndistinct_elts = 0;
    metadata_->noccupied_slots = 0;

    runtimedata_ = new qfruntime;
    runtimedata_->num_locks = (metadata_->xnslots / num_slots_to_lock_) + 2;

    /* initialize all the locks to 0 */
    runtimedata_->metadata_lock = 0;
    runtimedata_->locks = reinterpret_cast<volatile int32_t *>(new volatile int32_t[runtimedata_->num_locks]);
#ifdef LOG_WAIT_TIME
	runtimedata->wait_times = reinterpret_cast<wait_time_data *>(new wait_time_data[runtimedata->num_locks + 1]);
#endif
}


template <bool expandable>
inline Memento<expandable>::~Memento() {
    if (runtimedata_) {
        delete[] runtimedata_->locks;
#ifdef LOG_WAIT_TIME
        delete[] runtimedata_->wait_times;
#endif
    }
    delete[] runtimedata_;
    delete[] metadata_;
}


template <bool expandable>
inline Memento<expandable>::Memento(const Memento<expandable>& other) {
	const uint32_t total_num_bytes = sizeof(qfmetadata) + other.size_in_bytes();
    uint8_t *buffer = new uint8_t[total_num_bytes];
    memcpy(buffer, other.metadata_, total_num_bytes);
    metadata_ = reinterpret_cast<qfmetadata *>(buffer);
	blocks_ = reinterpret_cast<qfblock *>(metadata_ + 1);

	runtimedata_ = new qfruntime;
    memcpy(runtimedata_, other.runtimedata_, sizeof(qfruntime));

    runtimedata_->locks = reinterpret_cast<volatile int32_t *>(new volatile int32_t[other.runtimedata_->num_locks]);
    for (uint64_t i = 0; i < other.runtimedata_->num_locks; i++)
      runtimedata_->locks[i] = other.runtimedata_->locks[i];

#ifdef LOG_WAIT_TIME
	runtimedata->wait_times = reinterpret_cast<wait_time_data *>(new wait_time_data[runtimedata->num_locks + 1]);
    memcpy(metadata->wait_times, memento.metadata->wait_times, sizeof(wait_time_data) * (runtimedata->num_locks + 1));
#endif
}


template <bool expandable>
inline Memento<expandable>::Memento(Memento<expandable>&& other) noexcept {
    metadata_ = other.metadata_;
    blocks_ = other.blocks_;
    runtimedata_ = other.runtimedata_;
    other.metadata_ = nullptr;
    other.blocks_ = nullptr;
    other.runtimedata_ = nullptr;
}


template <bool expandable>
inline Memento<expandable>& Memento<expandable>::operator=(const Memento<expandable>& other) {
    delete[] runtimedata_->locks;
    delete[] runtimedata_;
#ifdef LOG_WAIT_TIME
    delete[] runtimedata->wait_times;
#endif
    delete[] metadata_;

	const uint32_t total_num_bytes = sizeof(qfmetadata) + other.size_in_bytes();
    uint8_t *buffer = new uint8_t[total_num_bytes];
    memcpy(buffer, other.metadata_, total_num_bytes);
    metadata_ = reinterpret_cast<qfmetadata *>(buffer);
	blocks_ = reinterpret_cast<qfblock *>(metadata_ + 1);

        runtimedata_ = new qfruntime;
    memcpy(runtimedata_, other.runtimedata_, sizeof(qfruntime));

    runtimedata_->locks = reinterpret_cast<volatile int32_t *>(new volatile int32_t[other.runtimedata_->num_locks]);
    for (uint64_t i = 0; i < other.runtimedata_->num_locks; i++)
      runtimedata_->locks[i] = other.runtimedata_->locks[i];

#ifdef LOG_WAIT_TIME
	runtimedata->wait_times = reinterpret_cast<wait_time_data *>(new wait_time_data[runtimedata->num_locks + 1]);
    memcpy(metadata->wait_times, memento.metadata->wait_times, sizeof(wait_time_data) * (runtimedata->num_locks + 1));
#endif
    return *this;
}


template <bool expandable>
inline Memento<expandable>& Memento<expandable>::operator=(Memento<expandable>&& other) noexcept {
    metadata_ = other.metadata_;
    blocks_ = other.blocks_;
    runtimedata_ = other.runtimedata_;
    other.metadata_ = nullptr;
    other.blocks_ = nullptr;
    other.runtimedata_ = nullptr;
    return *this;
}


template <bool expandable>
inline void Memento<expandable>::reset() {
        metadata_->nelts = 0;
        metadata_->ndistinct_elts = 0;
        metadata_->noccupied_slots = 0;

#ifdef LOG_WAIT_TIME
	memset(wait_times, 0, (runtimedata->num_locks + 1) * sizeof(wait_time_data));
#endif
	memset(blocks_, 0, metadata_->nblocks *
            (sizeof(qfblock) + slots_per_block_ * metadata_->bits_per_slot / 8));
}


template <bool expandable>
inline int64_t Memento<expandable>::resize(uint64_t nslots) {
    Memento new_memento(nslots, metadata_->key_bits + expandable, metadata_->memento_bits,
                      metadata_->hash_mode, metadata_->seed, metadata_->original_quotient_bits);
    new_memento.set_auto_resize(metadata_->auto_resize);

	// Copy keys from this filter into the new filter
	int64_t ret_numkeys = 0;
    uint64_t key, memento_count, mementos[1024];
    for (auto it = hash_begin(); it != hash_end(); ++it) {
		memento_count = it.get(key, mementos);

        int ret;
        if constexpr (expandable) {
            const int64_t new_fingerprint_size = highbit_position(key) - metadata_->key_bits + metadata_->fingerprint_bits - 1;
            if (new_fingerprint_size < 0) {
                const uint32_t bucket_index_hash_size = new_memento.get_bucket_index_hash_size();
                const uint64_t key_1 = (key & BITMASK(bucket_index_hash_size - 1)) | (1ULL << bucket_index_hash_size);
                const uint64_t key_2 = key_1 | (1ULL << (bucket_index_hash_size - 1));
                ret = new_memento.insert_mementos(key_1,
                                                  mementos,
                                                  memento_count,
                                                  0,
                                                  flag_no_lock | flag_key_is_hash);
                ret = new_memento.insert_mementos(key_2,
                                                  mementos,
                                                  memento_count,
                                                  0,
                                                  flag_no_lock | flag_key_is_hash);
            }
            else {
                ret = new_memento.insert_mementos(key,
                                                  mementos,
                                                  memento_count, 
                                                  new_fingerprint_size,
                                                  flag_no_lock | flag_key_is_hash);
            }
        }
        else {
            const int64_t new_fingerprint_size = new_memento.metadata_->fingerprint_bits;
            ret = new_memento.insert_mementos(key,
                                              mementos,
                                              memento_count, 
                                              new_fingerprint_size,
                                              flag_no_lock | flag_key_is_hash);
        }

		if (ret < 0)
			return ret;
		ret_numkeys += memento_count;
    }

    delete[] runtimedata_->locks;
    delete[] runtimedata_;
#ifdef LOG_WAIT_TIME
    delete[] runtimedata->wait_times;
#endif
    delete[] metadata_;
    *this = std::move(new_memento);

	return ret_numkeys;
}


template <bool expandable>
inline int32_t Memento<expandable>::insert_mementos(uint64_t key, uint64_t mementos[],
                                                    uint64_t memento_count, uint8_t flags) {
    uint32_t new_slot_count = 1 + (memento_count + 1) / 2;
	// We fill up the CQF up to 95% load factor.
	// This is a very conservative check.
	if (metadata_->noccupied_slots >= metadata_->nslots * 0.95 ||
        metadata_->noccupied_slots + new_slot_count >= metadata_->nslots) {
		if (metadata_->auto_resize)
			resize(metadata_->nslots * 2);
		else
			return err_no_space;
	}
	if (memento_count == 0)
		return 0;

	if (GET_KEY_HASH(flags) != flag_key_is_hash) {
		if (metadata_->hash_mode == hashmode::Default)
			key = MurmurHash64A(&key, sizeof(key), metadata_->seed);
		else if (metadata_->hash_mode == hashmode::Invertible) // Large hash!
			key = hash_64(key, BITMASK(63));
	}
    const uint64_t orig_nslots = metadata_->nslots >> (metadata_->key_bits
                                                      - metadata_->fingerprint_bits
                                                      - metadata_->original_quotient_bits);
    uint64_t fast_reduced_part = fast_reduce(((key & BITMASK(metadata_->original_quotient_bits))
                                << (32 - metadata_->original_quotient_bits)), orig_nslots);
    key &= ~BITMASK(metadata_->original_quotient_bits);
    key |= fast_reduced_part;
	uint64_t hash = key;
	int32_t ret = insert_mementos(hash, mementos, memento_count, metadata_->fingerprint_bits, flags);

    // Check for fullness based on the distance from the home slot to the slot
    // in which the key is inserted
	if (ret > (int32_t) distance_from_home_slot_cutoff_ && metadata_->auto_resize) {
        resize(metadata_->nslots * 2);
	}
	return ret;
}


template <bool expandable>
inline int64_t Memento<expandable>::insert(uint64_t key, uint64_t memento, uint8_t flags) {
    // We fill up the CQF up to 95% load factor.
    // This is a very conservative check.
    if (metadata_->noccupied_slots >= metadata_->nslots * 0.95 ||
            metadata_->noccupied_slots + 1 >= metadata_->nslots) {
        if (metadata_->auto_resize)
            resize(metadata_->nslots * 2);
        else
            return err_no_space;
    }

    if (GET_KEY_HASH(flags) != flag_key_is_hash) {
        if (metadata_->hash_mode == hashmode::Default)
            key = MurmurHash64A(&key, sizeof(key), metadata_->seed);
        else if (metadata_->hash_mode == hashmode::Invertible) // Large hash!
            key = hash_64(key, BITMASK(63));
    }
    const uint64_t orig_nslots = metadata_->nslots >> (metadata_->key_bits
                                                       - metadata_->fingerprint_bits
                                                       - metadata_->original_quotient_bits);
    const uint64_t fast_reduced_part = fast_reduce(((key & BITMASK(metadata_->original_quotient_bits))
                                << (32 - metadata_->original_quotient_bits)), orig_nslots);
    key &= ~BITMASK(metadata_->original_quotient_bits);
    key |= fast_reduced_part;
    uint64_t hash = key;

    int64_t res = 0;
    const uint32_t bucket_index_hash_size = get_bucket_index_hash_size();
    const uint64_t hash_fingerprint = (hash >> bucket_index_hash_size) & BITMASK(metadata_->fingerprint_bits)
                                    | (static_cast<uint64_t>(expandable) << metadata_->fingerprint_bits);
    const uint32_t orig_quotient_size = metadata_->original_quotient_bits;
    const uint64_t hash_bucket_index = (fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
        | ((hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));

    if (GET_NO_LOCK(flags) != flag_no_lock) {
        if (!memento_lock(hash_bucket_index, /*small*/ true, flags))
            return err_couldnt_lock;
    }

    uint64_t runend_index = run_end(hash_bucket_index);
    uint64_t runstart_index = hash_bucket_index == 0 ? 0 : run_end(hash_bucket_index - 1) + 1;
    uint64_t insert_index;
    if (is_occupied(hash_bucket_index)) {
        int64_t fingerprint_pos = runstart_index;
        bool add_to_sorted_list = false;
        if constexpr (expandable) {
            while (true) {
                fingerprint_pos = next_matching_fingerprint_in_run(fingerprint_pos, hash_fingerprint);
                if (fingerprint_pos < 0) {
                    // Matching fingerprints exhausted
                    break;
                }

                const uint64_t current_fingerprint = get_fingerprint(fingerprint_pos);
                const uint64_t next_fingerprint = get_fingerprint(fingerprint_pos + 1);
                if (highbit_position(current_fingerprint) == metadata_->fingerprint_bits) {
                    // Should add to this sorted list
                    add_to_sorted_list = true;
                    insert_index = fingerprint_pos;
                    break;
                }

                if (!is_runend(fingerprint_pos) && current_fingerprint > next_fingerprint) {
                    const uint64_t m1 = get_memento(fingerprint_pos);
                    const uint64_t m2 = get_memento(fingerprint_pos + 1);
                    fingerprint_pos += 2;
                    if (m1 >= m2)
                        fingerprint_pos += number_of_slots_used_for_memento_list(fingerprint_pos);
                }
                else {
                    fingerprint_pos++;
                }

                if (is_runend(fingerprint_pos - 1))
                    break;
            }
        }
        else {
            fingerprint_pos = next_matching_fingerprint_in_run(fingerprint_pos, hash_fingerprint);
            if (fingerprint_pos >= 0 && hash_fingerprint) {
                add_to_sorted_list = true;
                insert_index = fingerprint_pos;
            }
        }

        if (add_to_sorted_list) {
            // Matching sorted list with a complete fingerprint target 
            res = add_memento_to_sorted_list(hash_bucket_index, insert_index, memento);

            if (res < 0)
                return res;
            res = insert_index - hash_bucket_index;
        }
        else {
            // No fully matching fingerprints found
            insert_index = upper_bound_fingerprint_in_run(runstart_index, hash_fingerprint);
            const uint64_t next_empty_slot = find_first_empty_slot(hash_bucket_index);
            assert(next_empty_slot >= insert_index);

            if (insert_index < next_empty_slot) {
                shift_slots(insert_index, next_empty_slot - 1, 1);
                shift_runends(insert_index, next_empty_slot - 1, 1);
            }
            for (uint32_t i = hash_bucket_index / slots_per_block_ + 1; i <= next_empty_slot / slots_per_block_; i++) {
                if (get_block(i)->offset + 1 <= BITMASK(8 * sizeof(blocks_[0].offset)))
                    get_block(i)->offset++;
            }
            set_slot(insert_index, (hash_fingerprint << metadata_->memento_bits) | memento);
            METADATA_WORD(runends, runend_index) &= ~(1ULL << ((runend_index % slots_per_block_) % 64));
            METADATA_WORD(runends, runend_index + 1) |= 1ULL << (((runend_index + 1) % slots_per_block_) % 64);
            modify_metadata(&metadata_->ndistinct_elts, 1);
            modify_metadata(&metadata_->noccupied_slots, 1);
            res = insert_index - hash_bucket_index;
        }
    }
    else {
        const uint64_t next_empty_slot = find_first_empty_slot(hash_bucket_index);
        assert(next_empty_slot >= hash_bucket_index);
        if (hash_bucket_index == next_empty_slot) {
            insert_index = hash_bucket_index;
        }
        else {
            insert_index = runend_index + 1;
            if (insert_index < next_empty_slot) {
                shift_slots(insert_index, next_empty_slot - 1, 1);
                shift_runends(insert_index, next_empty_slot - 1, 1);
            }
        }
        set_slot(insert_index, (hash_fingerprint << metadata_->memento_bits) | memento);

        for (uint32_t i = hash_bucket_index / slots_per_block_ + 1;
                i <= next_empty_slot / slots_per_block_; i++) {
            if (get_block(i)->offset + 1ULL <= BITMASK(8 * sizeof(blocks_[0].offset)))
                get_block(i)->offset++;
        }
        METADATA_WORD(runends, insert_index) |= 1ULL << ((insert_index % slots_per_block_) % 64);
        METADATA_WORD(occupieds, hash_bucket_index) |= 1ULL << ((hash_bucket_index % slots_per_block_) % 64);
        modify_metadata(&metadata_->ndistinct_elts, 1);
        modify_metadata(&metadata_->noccupied_slots, 1);
        res = insert_index - hash_bucket_index;
    }

    if (GET_NO_LOCK(flags) != flag_no_lock)
        memento_unlock(hash_bucket_index, /*small*/ true);

    modify_metadata(&metadata_->nelts, 1);
    return res;
}

template <bool expandable>
inline void Memento<expandable>::bulk_load(uint64_t *sorted_hashes, uint64_t n, uint8_t flags) {
    assert(flags & flag_key_is_hash);

    const uint64_t fingerprint_mask = BITMASK(metadata_->fingerprint_bits);
    const uint64_t memento_mask = BITMASK(metadata_->memento_bits);

    uint64_t prefix = sorted_hashes[0] >> metadata_->memento_bits;
    uint64_t memento_list[10 * (1ULL << metadata_->memento_bits)];
    uint32_t prefix_set_size = 1;
    memento_list[0] = sorted_hashes[0] & memento_mask;
    uint64_t current_run = prefix >> metadata_->fingerprint_bits;
    uint64_t current_pos = current_run, old_pos = 0, next_run;
    uint64_t distinct_prefix_cnt = 0, total_slots_written = 0;
    for (uint64_t i = 1; i < n; i++) {
        const uint64_t new_prefix = sorted_hashes[i] >> metadata_->memento_bits;
        if (new_prefix == prefix)
            memento_list[prefix_set_size++] = sorted_hashes[i] & memento_mask;
        else {
            const uint32_t slots_written = write_prefix_set(current_pos, (static_cast<uint64_t>(expandable) << metadata_->fingerprint_bits) 
                                                                                | prefix & fingerprint_mask, 
                                                            memento_list, prefix_set_size);
            current_pos += slots_written;
            total_slots_written += slots_written;
            prefix = new_prefix;
            prefix_set_size = 1;
            memento_list[0] = sorted_hashes[i] & memento_mask;

            next_run = prefix >> metadata_->fingerprint_bits;
            if (current_run != next_run) {
                METADATA_WORD(occupieds, current_run) |= (1ULL << ((current_run % slots_per_block_) % 64));
                METADATA_WORD(runends, (current_pos - 1)) |= (1ULL << (((current_pos - 1) % slots_per_block_) % 64));
                for (uint64_t block_ind = current_run / slots_per_block_ + 1; block_ind <= (current_pos - 1) / slots_per_block_; block_ind++) {
                    const uint32_t cnt = current_pos - std::max(old_pos, block_ind * slots_per_block_);
                    if (get_block(block_ind)->offset + cnt < BITMASK(8 * sizeof(blocks_[0].offset)))
                        get_block(block_ind)->offset += cnt;
                    else
                        get_block(block_ind)->offset = static_cast<uint8_t>(BITMASK(8 * sizeof(blocks_[0].offset)));
                }
                current_run = next_run;
                old_pos = current_pos;
                current_pos = std::max(current_run, current_pos);
            }
        }
    }
    const uint32_t slots_written = write_prefix_set(current_pos, (static_cast<uint64_t>(expandable) << metadata_->fingerprint_bits) 
                                                                        | prefix & fingerprint_mask, 
                                                    memento_list, prefix_set_size);
    current_pos += slots_written;
    total_slots_written += slots_written;
    METADATA_WORD(occupieds, current_run) |= (1ULL << ((current_run % slots_per_block_) % 64));
    METADATA_WORD(runends, (current_pos - 1)) |= (1ULL << (((current_pos - 1) % slots_per_block_) % 64));
    for (uint64_t block_ind = current_run / slots_per_block_ + 1; block_ind <= (current_pos - 1) / slots_per_block_; block_ind++) {
        const uint32_t cnt = current_pos - std::max(old_pos, block_ind * slots_per_block_);
        if (get_block(block_ind)->offset + cnt < BITMASK(8 * sizeof(blocks_[0].offset)))
            get_block(block_ind)->offset += cnt;
        else
            get_block(block_ind)->offset = static_cast<uint8_t>(BITMASK(8 * sizeof(blocks_[0].offset)));
    }

    modify_metadata(&metadata_->ndistinct_elts, distinct_prefix_cnt);
    modify_metadata(&metadata_->noccupied_slots, total_slots_written);
    modify_metadata(&metadata_->nelts, n);
}

template <bool expandable>
inline int32_t Memento<expandable>::delete_single(uint64_t key, uint64_t memento, uint8_t flags) {
    if (GET_KEY_HASH(flags) != flag_key_is_hash) {
        if (metadata_->hash_mode == hashmode::Default)
            key = MurmurHash64A(&key, sizeof(key), metadata_->seed);
        else if (metadata_->hash_mode == hashmode::Invertible) // Large hash!
            key = hash_64(key, BITMASK(63));
    }
    const uint32_t bucket_index_hash_size = get_bucket_index_hash_size();
    const uint32_t orig_quotient_size = metadata_->original_quotient_bits;
    const uint64_t orig_nslots = metadata_->nslots >> (metadata_->key_bits
                                 - metadata_->fingerprint_bits
                                 - metadata_->original_quotient_bits);
    const uint64_t fast_reduced_part = fast_reduce(((key & BITMASK(orig_quotient_size)) 
                                                    << (32 - orig_quotient_size)), orig_nslots);
    key &= ~(BITMASK(metadata_->original_quotient_bits));
    key |= fast_reduced_part;
    uint64_t hash = key;
    const uint64_t hash_bucket_index = (fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                                        | ((hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
    const uint64_t hash_fingerprint = (hash >> bucket_index_hash_size) & BITMASK(metadata_->fingerprint_bits) 
                                    | (static_cast<uint64_t>(expandable) << metadata_->fingerprint_bits);

    if (GET_NO_LOCK(flags) != flag_no_lock) {
        if (!memento_lock(hash_bucket_index, /*small*/ true, flags))
            return err_couldnt_lock;
    }

    bool handled = false;
    if constexpr (expandable) {
        const int64_t runstart_index = hash_bucket_index == 0 ? 0 : run_end(hash_bucket_index - 1) + 1;
        int64_t fingerprint_pos = runstart_index;
        uint64_t matching_position[10], match_amount[10], ind = 0;
        uint8_t matching_cnt[metadata_->fingerprint_bits + 1];
        memset(matching_cnt, 0, metadata_->fingerprint_bits + 1);
        while (true) {
            fingerprint_pos = next_matching_fingerprint_in_run(fingerprint_pos, hash_fingerprint);
            if (fingerprint_pos < 0) {
                // Matching fingerprints exhausted
                break;
            }
            matching_position[ind] = fingerprint_pos;
            const uint64_t current_fingerprint = get_fingerprint(fingerprint_pos);
            const uint64_t next_fingerprint = get_fingerprint(fingerprint_pos + 1);
            match_amount[ind++] = highbit_position(current_fingerprint);
            matching_cnt[match_amount[ind - 1]]++;
            if (!is_runend(fingerprint_pos) && current_fingerprint > next_fingerprint) {
                const uint64_t m1 = get_memento(fingerprint_pos);
                const uint64_t m2 = get_memento(fingerprint_pos + 1);
                fingerprint_pos += 2;
                if (m1 >= m2)
                    fingerprint_pos += number_of_slots_used_for_memento_list(fingerprint_pos);
            }
            else {
                fingerprint_pos++;
            }

            if (is_runend(fingerprint_pos - 1))
                break;
        }
        for (uint32_t i = 1; i <= metadata_->fingerprint_bits; i++)
            matching_cnt[i] += matching_cnt[i - 1];

        uint64_t sorted_positions[50];
        for (uint32_t i = 0; i < ind; i++) {
            sorted_positions[--matching_cnt[match_amount[i]]] = matching_position[i];
        }
        for (int32_t i = ind - 1; i >= 0; i--) {
            int32_t old_slot_count, new_slot_count;
            remove_mementos_from_prefix_set(sorted_positions[i], &memento, &handled,
                                            1, &new_slot_count, &old_slot_count);
            if (handled) {
                if (new_slot_count < old_slot_count) {
                    const bool only_item_in_run = is_runend(runstart_index);
                    remove_slots_and_shift_remainders_and_runends_and_offsets(only_item_in_run,
                                                                              hash_bucket_index,
                                                                              sorted_positions[i] + new_slot_count,
                                                                              old_slot_count - new_slot_count);
                }
                break;
            }
        }
    }
    else {
        const int64_t runstart_index = hash_bucket_index == 0 ? 0 : run_end(hash_bucket_index - 1) + 1;
        int64_t fingerprint_pos = runstart_index;
        uint64_t sorted_positions[50], ind = 0;
        while (true) {
            fingerprint_pos = next_matching_fingerprint_in_run(fingerprint_pos, hash_fingerprint);
            if (fingerprint_pos < 0) {
                // Matching fingerprints exhausted
                break;
            }
            sorted_positions[ind++] = fingerprint_pos;
            const uint64_t current_fingerprint = get_fingerprint(fingerprint_pos);
            const uint64_t next_fingerprint = get_fingerprint(fingerprint_pos + 1);
            if (!is_runend(fingerprint_pos) && 
                    current_fingerprint > next_fingerprint) {
                const uint64_t m1 = get_memento(fingerprint_pos);
                const uint64_t m2 = get_memento(fingerprint_pos + 1);
                fingerprint_pos += 2;
                if (m1 >= m2)
                    fingerprint_pos += number_of_slots_used_for_memento_list(fingerprint_pos);
            }
            else {
                fingerprint_pos++;
            }

            if (is_runend(fingerprint_pos - 1))
                break;
        }

        for (int32_t i = ind - 1; i >= 0; i--) {
            int32_t old_slot_count, new_slot_count;
            remove_mementos_from_prefix_set(sorted_positions[i], &memento, &handled,
                                            1, &new_slot_count, &old_slot_count);
            if (handled) {
                if (new_slot_count < old_slot_count) {
                    const bool only_item_in_run = is_runend(runstart_index);
                    remove_slots_and_shift_remainders_and_runends_and_offsets(only_item_in_run,
                                                                              hash_bucket_index,
                                                                              sorted_positions[i] + new_slot_count,
                                                                              old_slot_count - new_slot_count);
                }
                break;
            }
        }
    }

    if (handled)
        modify_metadata(&metadata_->nelts, -1);

    if (GET_NO_LOCK(flags) != flag_no_lock) {
        memento_unlock(hash_bucket_index, /*small*/ true);
    }

    return handled ? 0 : err_doesnt_exist;
}


template <bool expandable>
inline uint64_t Memento<expandable>::lower_bound_mementos_for_fingerprint(uint64_t pos, uint64_t target_memento) const {
    uint64_t current_memento = get_memento(pos);
    uint64_t next_memento = get_memento(pos + 1);
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
        const uint64_t max_memento_value = (1ULL << metadata_->memento_bits) - 1;
        uint64_t current_slot = get_slot(pos);
        uint64_t mementos_left = (current_slot & BITMASK(metadata_->memento_bits));
        current_slot >>= metadata_->memento_bits;
        uint32_t current_full_bits = metadata_->bits_per_slot - metadata_->memento_bits;

        // Check for an extended memento counter
        if (mementos_left == max_memento_value) {
            // Rarely executes, as slot counts rarely exceed the maximum value
            // that a memento can hold
            uint64_t length = 2, pw = 1;
            mementos_left = 0;
            pos++;
            while (length > 0) {
                if (current_full_bits < metadata_->memento_bits) {
                    current_slot |= get_slot(pos) << current_full_bits;
                    current_full_bits += metadata_->bits_per_slot;
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
                current_slot >>= metadata_->memento_bits;
                current_full_bits -= metadata_->memento_bits;
            }
        }

        do {
            if (current_full_bits < metadata_->memento_bits) {
                pos++;
                current_slot |= get_slot(pos) << current_full_bits;
                current_full_bits += metadata_->bits_per_slot;
            }
            current_memento = current_slot & BITMASK(metadata_->memento_bits);
            current_slot >>= metadata_->memento_bits;
            current_full_bits -= metadata_->memento_bits;
            if (target_memento <= current_memento)
                return current_memento;
            mementos_left--;
        } while (mementos_left);
        return max_memento;
    }
}


template <bool expandable>
inline int32_t Memento<expandable>::point_query(uint64_t key, uint64_t memento, uint8_t flags) const {
	if (GET_KEY_HASH(flags) != flag_key_is_hash) {
		if (metadata_->hash_mode == hashmode::Default)
			key = MurmurHash64A(&key, sizeof(key), metadata_->seed);
		else if (metadata_->hash_mode == hashmode::Invertible)
			key = hash_64(key, BITMASK(63));
	}
	const uint64_t hash = key;
    const uint32_t bucket_index_hash_size = get_bucket_index_hash_size();
    const uint32_t orig_quotient_size = metadata_->original_quotient_bits;
    const uint64_t orig_nslots = metadata_->nslots >> (metadata_->key_bits
                                                      - metadata_->fingerprint_bits
                                                      - metadata_->original_quotient_bits);
    const uint64_t fast_reduced_part = fast_reduce(((hash & BITMASK(metadata_->original_quotient_bits))
                                                    << (32 - metadata_->original_quotient_bits)), orig_nslots);
	const uint64_t hash_bucket_index = (fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                        | ((hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));

	if (!is_occupied(hash_bucket_index))
		return false;

    const uint64_t hash_fingerprint = (hash >> bucket_index_hash_size) & BITMASK(metadata_->fingerprint_bits)
                                    | (static_cast<uint64_t>(expandable) << metadata_->fingerprint_bits);

    uint64_t runstart_index = hash_bucket_index == 0 ? 0 : run_end(hash_bucket_index - 1) + 1;
	if (runstart_index < hash_bucket_index)
		runstart_index = hash_bucket_index;
    
    // Find the shortest matching fingerprint that gives a positive
    int64_t fingerprint_pos = runstart_index;
    while (true) {
        fingerprint_pos = next_matching_fingerprint_in_run(fingerprint_pos, hash_fingerprint);
        if (fingerprint_pos < 0) // Matching fingerprints exhausted
            break;
        
        const uint64_t current_fingerprint = get_fingerprint(fingerprint_pos);
        const uint64_t next_fingerprint = get_fingerprint(fingerprint_pos + 1);
        const int positive_res = expandable ? (highbit_position(current_fingerprint) == metadata_->fingerprint_bits ? 1 : 2)
                                            : 1;
        if (!is_runend(fingerprint_pos) && current_fingerprint > next_fingerprint) {
            if (lower_bound_mementos_for_fingerprint(fingerprint_pos, memento) == memento)
                return positive_res;

            const uint64_t m1 = get_memento(fingerprint_pos);
            const uint64_t m2 = get_memento(fingerprint_pos + 1);
            fingerprint_pos += 2;
            if (m1 >= m2)
                fingerprint_pos += number_of_slots_used_for_memento_list(fingerprint_pos);
        }
        else {
            if (get_memento(fingerprint_pos) == memento)
                return positive_res;
            fingerprint_pos++;
        }

        if (is_runend(fingerprint_pos - 1))
            break;
    }

    return 0;
}


template <bool expandable>
inline int32_t Memento<expandable>::range_query(uint64_t l_key, uint64_t l_memento,
                                                uint64_t r_key, uint64_t r_memento, 
                                                uint8_t flags) const {
    const uint64_t orig_l_key = l_key;
    const uint64_t orig_r_key = r_key;
	if (GET_KEY_HASH(flags) != flag_key_is_hash) {
		if (metadata_->hash_mode == hashmode::Default) {
			l_key = MurmurHash64A(&l_key, sizeof(l_key), metadata_->seed);
			r_key = MurmurHash64A(&r_key, sizeof(r_key), metadata_->seed);
        }
		else if (metadata_->hash_mode == hashmode::Invertible) {
			l_key = hash_64(l_key, BITMASK(63));
			r_key = hash_64(r_key, BITMASK(63));
        }
	}
    const uint32_t bucket_index_hash_size = get_bucket_index_hash_size();
    const uint32_t orig_quotient_size = metadata_->original_quotient_bits;
    const uint64_t orig_nslots = metadata_->nslots >> (metadata_->key_bits
                                                      - metadata_->fingerprint_bits
                                                      - metadata_->original_quotient_bits);

	const uint64_t l_hash = l_key;
    const uint64_t l_fast_reduced_part = fast_reduce(((l_hash & BITMASK(metadata_->original_quotient_bits))
                                                     << (32 - metadata_->original_quotient_bits)), orig_nslots);
	const uint64_t l_hash_bucket_index = (l_fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                        | ((l_hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
	const uint64_t l_hash_fingerprint = (l_hash >> bucket_index_hash_size) & BITMASK(metadata_->fingerprint_bits)
                                        | (static_cast<uint64_t>(expandable) << metadata_->fingerprint_bits);

	const uint64_t r_hash = r_key;
    const uint64_t r_fast_reduced_part = fast_reduce(((r_hash & BITMASK(metadata_->original_quotient_bits))
                                << (32 - metadata_->original_quotient_bits)), orig_nslots);
	const uint64_t r_hash_bucket_index = (r_fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                        | ((r_hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
	const uint64_t r_hash_fingerprint = (r_hash >> bucket_index_hash_size) & BITMASK(metadata_->fingerprint_bits)
                                        | (static_cast<uint64_t>(expandable) << metadata_->fingerprint_bits);

    uint64_t candidate_memento;
    if (l_hash == r_hash) { // Range contained in a single prefix.
        if (!is_occupied(l_hash_bucket_index)) {
            return 0;
        }

        uint64_t runstart_index = l_hash_bucket_index == 0 ? 0 : run_end(l_hash_bucket_index - 1) + 1;
        if (runstart_index < l_hash_bucket_index)
            runstart_index = l_hash_bucket_index;

        // Find the shortest matching fingerprint that gives a positive
        int64_t fingerprint_pos = runstart_index;
        while (true) {
            fingerprint_pos = next_matching_fingerprint_in_run(fingerprint_pos, l_hash_fingerprint);
            if (fingerprint_pos < 0) {
                // Matching fingerprints exhausted
                break;
            }

            const uint64_t current_fingerprint = get_fingerprint(fingerprint_pos);
            const uint64_t next_fingerprint = get_fingerprint(fingerprint_pos + 1);
            const int positive_res = expandable ? (highbit_position(current_fingerprint) == metadata_->fingerprint_bits ? 1 : 2)
                                                : 1;
            if (!is_runend(fingerprint_pos) && current_fingerprint > next_fingerprint) {
                candidate_memento = lower_bound_mementos_for_fingerprint(fingerprint_pos, l_memento);
                if (l_memento <= candidate_memento && candidate_memento <= r_memento)
                    return positive_res;

                const uint64_t m1 = get_memento(fingerprint_pos);
                const uint64_t m2 = get_memento(fingerprint_pos + 1);
                fingerprint_pos += 2;
                if (m1 >= m2)
                    fingerprint_pos += number_of_slots_used_for_memento_list(fingerprint_pos);
            }
            else {
                candidate_memento = get_memento(fingerprint_pos);
                if (l_memento <= candidate_memento && candidate_memento <= r_memento)
                    return positive_res;
                fingerprint_pos++;
            }

            if (is_runend(fingerprint_pos - 1))
                break;
        }
        return 0;
    }
    else {  // Range intersects two prefixes
        uint64_t l_runstart_index, r_runstart_index;
        if (!is_occupied(l_hash_bucket_index))
            l_runstart_index = metadata_->xnslots + 100;
        else {
            l_runstart_index = l_hash_bucket_index == 0 ? 0 : run_end(l_hash_bucket_index - 1) + 1;
            if (l_runstart_index < l_hash_bucket_index)
                l_runstart_index = l_hash_bucket_index;
        }
        if (!is_occupied(r_hash_bucket_index))
            r_runstart_index = metadata_->xnslots + 100;
        else {
            r_runstart_index = r_hash_bucket_index == 0 ? 0 
                : run_end(r_hash_bucket_index - 1) + 1;
            if (r_runstart_index < r_hash_bucket_index)
                r_runstart_index = r_hash_bucket_index;
        }

        // Check the left prefix
        if (l_runstart_index < metadata_->xnslots) {
            // Find the shortest matching fingerprint that gives a positive
            int64_t fingerprint_pos = l_runstart_index;
            while (true) {
                fingerprint_pos = next_matching_fingerprint_in_run(fingerprint_pos, l_hash_fingerprint);
                if (fingerprint_pos < 0) {
                    // Matching fingerprints exhausted
                    break;
                }

                const uint64_t current_fingerprint = get_fingerprint(fingerprint_pos);
                const uint64_t next_fingerprint = get_fingerprint(fingerprint_pos + 1);
                const int positive_res = expandable ? (highbit_position(current_fingerprint) == metadata_->fingerprint_bits ? 1 : 2)
                                                    : 1;
                if (!is_runend(fingerprint_pos) && current_fingerprint > next_fingerprint) {
                    uint64_t m1 = get_memento(fingerprint_pos);
                    uint64_t m2 = get_memento(fingerprint_pos + 1);

                    bool has_sorted_list = m1 >= m2;
                    if (has_sorted_list && l_memento <= m1)
                        return positive_res;
                    if (!has_sorted_list && l_memento <= m2)
                        return positive_res;

                    fingerprint_pos += 2;
                    if (has_sorted_list)
                        fingerprint_pos += number_of_slots_used_for_memento_list(fingerprint_pos);
                }
                else {
                    if (l_memento <= get_memento(fingerprint_pos))
                        return positive_res;
                    fingerprint_pos++;
                }

                if (is_runend(fingerprint_pos - 1))
                    break;
            }
        }

        // Check middle prefixes, if they exist
        for (uint64_t mid_key = orig_l_key + 1; mid_key < orig_r_key; mid_key++) {
            uint64_t mid_hash = mid_key;
            if (GET_KEY_HASH(flags) != flag_key_is_hash) {
                if (metadata_->hash_mode == hashmode::Default) {
                    mid_hash = MurmurHash64A(&mid_hash, sizeof(mid_hash), metadata_->seed);
                }
                else if (metadata_->hash_mode == hashmode::Invertible) {
                    mid_hash = hash_64(mid_hash, BITMASK(63));
                }
            }
            const uint64_t mid_fast_reduced_part = fast_reduce(((mid_hash & BITMASK(metadata_->original_quotient_bits))
                                                                << (32 - metadata_->original_quotient_bits)), orig_nslots);
            const uint64_t mid_hash_bucket_index = (mid_fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                | ((mid_hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
            const uint64_t mid_hash_fingerprint = (mid_hash >> bucket_index_hash_size) & BITMASK(metadata_->fingerprint_bits)
                                                    | (static_cast<uint64_t>(expandable) << metadata_->fingerprint_bits);

            if (!is_occupied(mid_hash_bucket_index))
                continue;
            uint64_t mid_runstart_index = mid_hash_bucket_index == 0 ? 0 
                : run_end(mid_hash_bucket_index - 1) + 1;
            if (mid_runstart_index < mid_hash_bucket_index)
                mid_runstart_index = mid_hash_bucket_index;

            // Check the current middle prefix
            if (mid_runstart_index < metadata_->xnslots) {
                // Find a matching fingerprint
                int64_t fingerprint_pos = next_matching_fingerprint_in_run(mid_runstart_index, mid_hash_fingerprint);
                if (fingerprint_pos >= 0) {
                    // A matching fingerprint exists
                    return true;
                }
            }
        }

        // Check the right prefix
        if (r_runstart_index < metadata_->xnslots) {
            // Find the shortest matching fingerprint that gives a positive
            int64_t fingerprint_pos = r_runstart_index;
            while (true) {
                fingerprint_pos = next_matching_fingerprint_in_run(fingerprint_pos, r_hash_fingerprint);
                if (fingerprint_pos < 0) {
                    // Matching fingerprints exhausted
                    break;
                }

                const uint64_t current_fingerprint = get_fingerprint(fingerprint_pos);
                const uint64_t next_fingerprint = get_fingerprint(fingerprint_pos + 1);
                const int positive_res = expandable ? (highbit_position(current_fingerprint) == metadata_->fingerprint_bits ? 1 : 2)
                                                    : 1;
                if (!is_runend(fingerprint_pos) && current_fingerprint > next_fingerprint) {
                    uint64_t m1 = get_memento(fingerprint_pos);
                    uint64_t m2 = get_memento(fingerprint_pos + 1);
                    bool has_sorted_list = m1 >= m2;

                    if (has_sorted_list && m2 <= r_memento)
                        return positive_res;
                    if (!has_sorted_list && m1 <= r_memento)
                        return positive_res;

                    fingerprint_pos += 2;
                    if (has_sorted_list)
                        fingerprint_pos += number_of_slots_used_for_memento_list(fingerprint_pos);
                }
                else {
                    if (get_memento(fingerprint_pos) <= r_memento)
                        return positive_res;
                    fingerprint_pos++;
                }

                if (is_runend(fingerprint_pos - 1))
                    break;
            }
        }

        return false;
    }
}


template <bool expandable>
inline typename Memento<expandable>::iterator Memento<expandable>::begin(uint64_t l_key, uint64_t r_key) const {
    return iterator(*this, l_key, r_key);
}


template <bool expandable>
inline typename Memento<expandable>::iterator Memento<expandable>::end() const {
    return iterator(*this);
}


template <bool expandable>
inline Memento<expandable>::iterator::iterator(const Memento& filter, const uint64_t l_key, const uint64_t r_key):
        filter_{filter},
        l_key_{l_key},
        r_key_{r_key},
        cur_prefix_{(l_key >> filter.get_num_memento_bits()) - 1},
        it_{filter},
        cur_ind_{0} {
    const uint64_t l_memento = l_key & BITMASK(filter.get_num_memento_bits());
    const uint64_t r_prefix = r_key >> filter.get_num_memento_bits();
    const uint64_t r_memento = r_key & BITMASK(filter.get_num_memento_bits());

    uint64_t hash_bucket_index;
    do {
        cur_prefix_++;
        if (cur_prefix_ > r_prefix) {
            cur_prefix_ = std::numeric_limits<uint64_t>::max();
            return;
        }
        uint64_t cur_prefix_hash;
        if (filter.metadata_->hash_mode == hashmode::Default)
            cur_prefix_hash = MurmurHash64A(&cur_prefix_, sizeof(cur_prefix_), filter.metadata_->seed);
        else if (filter.metadata_->hash_mode == hashmode::Invertible)
            cur_prefix_hash = hash_64(cur_prefix_hash, BITMASK(63));

        const uint32_t bucket_index_hash_size = filter.get_bucket_index_hash_size();
        const uint32_t orig_quotient_size = filter.metadata_->original_quotient_bits;
        const uint64_t orig_nslots = filter.metadata_->nslots >> (filter.metadata_->key_bits
                                                                - filter.metadata_->fingerprint_bits
                                                                - filter.metadata_->original_quotient_bits);
        const uint64_t fast_reduced_part = fast_reduce(((cur_prefix_hash & BITMASK(orig_quotient_size)) 
                                                        << (32 - orig_quotient_size)), orig_nslots);
        hash_bucket_index = (fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                    | ((cur_prefix_hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
    } while (!filter.is_occupied(hash_bucket_index));

    it_ = filter.hash_begin(hash_bucket_index);

    fetch_matching_prefix_mementos(false);
    cur_ind_ = std::lower_bound(mementos_.begin(), mementos_.end(), l_memento) - mementos_.begin();
    while (cur_ind_ == mementos_.size() && cur_prefix_ <= r_prefix) {
        cur_prefix_++;
        fetch_matching_prefix_mementos();
        cur_ind_ = 0;
    }
    if (cur_prefix_ > r_prefix ||
            (cur_prefix_ <= r_prefix && (cur_ind_ < mementos_.size() && mementos_[cur_ind_] > r_memento)))
        cur_prefix_ = std::numeric_limits<uint64_t>::max();
}


template <bool expandable>
inline typename Memento<expandable>::iterator& Memento<expandable>::iterator::operator=(const iterator &other) {
    assert(&filter_ == &other.filter_);
    l_key_ = other.l_key_;
    r_key_ = other.r_key_;
    cur_prefix_ = other.cur_prefix_;
    it_ = other.it_;
    cur_ind_ = other.cur_ind_;
    mementos_ = other.mementos_;
    return *this;
}


template <bool expandable>
inline void Memento<expandable>::iterator::fetch_matching_prefix_mementos(bool reinit_hash_it) {
    if (reinit_hash_it)
        it_ = filter_.hash_begin(cur_prefix_, Memento::flag_no_lock);
    uint64_t cur_prefix_hash = MurmurHash64A(&cur_prefix_, sizeof(cur_prefix_), filter_.get_hash_seed());
    const uint32_t bucket_index_hash_size = filter_.get_bucket_index_hash_size();
    const uint32_t orig_quotient_size = filter_.metadata_->original_quotient_bits;
    const uint64_t orig_nslots = filter_.metadata_->nslots >> (filter_.metadata_->key_bits
                                                             - filter_.metadata_->fingerprint_bits
                                                             - filter_.metadata_->original_quotient_bits);
	const uint64_t fast_reduced_part = fast_reduce(((cur_prefix_hash & BITMASK(orig_quotient_size)) 
                                                    << (32 - orig_quotient_size)), orig_nslots);
	const uint64_t hash_bucket_index = (fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
                        | ((cur_prefix_hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
	const uint64_t hash_fingerprint = (cur_prefix_hash >> bucket_index_hash_size) & BITMASK(filter_.metadata_->fingerprint_bits)
                                        | (static_cast<uint64_t>(expandable) << filter_.metadata_->fingerprint_bits);
    const uint64_t orig_prefix_hash = ((fast_reduced_part | (cur_prefix_hash & (~BITMASK(orig_quotient_size))))
                                        & BITMASK(filter_.metadata_->key_bits))
                                        | (static_cast<uint64_t>(expandable) << filter_.metadata_->key_bits);
    cur_prefix_hash = (hash_fingerprint << bucket_index_hash_size) | hash_bucket_index;
    
    mementos_.clear();
    uint64_t it_hash;
    while (it_ != filter_.hash_end()) {
        const uint32_t memento_count = it_.get(it_hash);
        if constexpr (expandable) {
            const uint64_t compare_mask = BITMASK(highbit_position(it_hash));
            if (!CMP_MASK_FINGERPRINT(it_hash, orig_prefix_hash, compare_mask)) {
                if (it_.is_at_runend())
                    break;
                ++it_;
                continue;
            }
        }
        else {
            if (it_hash != orig_prefix_hash) {
                if (it_.is_at_runend())
                    break;
                ++it_;
                continue;
            }
        }

        const uint32_t old_list_length = mementos_.size();
        mementos_.resize(old_list_length + memento_count);
        it_.get(it_hash, mementos_.data() + old_list_length);
        if (it_.is_at_runend())
            break;
        ++it_;
    }
    std::sort(mementos_.begin(), mementos_.end());
}


template <bool expandable>
inline uint64_t Memento<expandable>::iterator::operator*() {
    assert(cur_ind_ < mementos_.size());
    return (cur_prefix_ << filter_.get_num_memento_bits()) | mementos_[cur_ind_];
}


template <bool expandable>
inline typename Memento<expandable>::iterator& Memento<expandable>::iterator::operator++() {
    if (cur_ind_ < mementos_.size() - 1) {
        cur_ind_++;
        const uint64_t cur_key = (cur_prefix_ << filter_.get_num_memento_bits())
                                    | mementos_[cur_ind_];
        if (cur_key > r_key_)
          cur_prefix_ = std::numeric_limits<uint64_t>::max();
    }
    else {
        const uint64_t r_prefix = r_key_ >> filter_.get_num_memento_bits();
        const uint64_t r_memento = r_key_ & BITMASK(filter_.get_num_memento_bits());
        cur_ind_ = 0;
        do {
          cur_prefix_++;
            if (cur_prefix_ > r_prefix)
                break;
            fetch_matching_prefix_mementos();
        } while (cur_ind_ == mementos_.size());
        if (cur_prefix_ > r_prefix ||
                (cur_prefix_ <= r_prefix && (cur_ind_ < mementos_.size() && mementos_[cur_ind_] > r_memento)))
            cur_prefix_ = std::numeric_limits<uint64_t>::max();
    }
    return *this;
}


template <bool expandable>
inline typename Memento<expandable>::iterator Memento<expandable>::iterator::operator++(int) {
    auto old = *this;
    operator++();
    return old;
}

template <bool expandable>
inline Memento<expandable>::iterator::iterator(const iterator& other):
        filter_{other.filter_},
        l_key_{other.l_key_},
        r_key_{other.r_key_},
        cur_prefix_{other.cur_prefix_},
        it_{other.it_},
        cur_ind_{other.cur_ind_},
        mementos_{other.mementos_} {
}


template <bool expandable>
inline bool Memento<expandable>::iterator::operator==(const iterator& rhs) const {
    if (&filter_ != &rhs.filter_)
        return false;
    if (cur_prefix_ == std::numeric_limits<uint64_t>::max() && rhs.cur_prefix_ == std::numeric_limits<uint64_t>::max())
        return true;
    return cur_prefix_ == rhs.cur_prefix_ && it_ == rhs.it_;
}


template <bool expandable>
inline bool Memento<expandable>::iterator::operator!=(const iterator& rhs) const {
    return !(*this == rhs);
}


template <bool expandable>
inline typename Memento<expandable>::hash_iterator Memento<expandable>::hash_begin(uint64_t position) const {
    hash_iterator it(*this);
	if (position >= metadata_->nslots) {
		it.current_ = 0XFFFFFFFFFFFFFFFF;
        return it;
	}
	assert(position < metadata_->nslots);
	if (!is_occupied(position)) {
		uint64_t block_index = position / slots_per_block_;
		uint64_t idx = lowbit_position(get_block(block_index)->occupieds[0] & (~BITMASK(position % slots_per_block_)));
		if (idx == 64) {
			while (idx == 64 && block_index < metadata_->nblocks) {
				block_index++;
				idx = lowbit_position(get_block(block_index)->occupieds[0]);
			}
		}
		position = block_index * slots_per_block_ + idx;
	}

	it.num_clusters_ = 0;
	it.run_ = position;
	it.current_ = std::max(position == 0 ? 0 : run_end(position - 1) + 1, position);

#ifdef LOG_CLUSTER_LENGTH
    it.c_info = new cluster_data[metadata->nslots / 32];
	it.cur_start_index = position;
	it.cur_length = 1;
#endif

	if (it.current_ >= metadata_->nslots)
		it.current_ = 0XFFFFFFFFFFFFFFFF;
	return it;
}


template <bool expandable>
inline typename Memento<expandable>::hash_iterator Memento<expandable>::hash_begin(uint64_t key, uint8_t flags) const {
    hash_iterator it(*this);
    if (key >= metadata_->range) {
        it.current_ = 0XFFFFFFFFFFFFFFFF;
        return it;
    }

    it.num_clusters_ = 0;

    if (GET_KEY_HASH(flags) != flag_key_is_hash) {
        if (metadata_->hash_mode == hashmode::Default)
            key = MurmurHash64A(&key, sizeof(key), metadata_->seed);
        else if (metadata_->hash_mode == hashmode::Invertible)
            key = hash_64(key, BITMASK(63));
    }
    uint64_t hash = key;

    const uint32_t bucket_index_hash_size = get_bucket_index_hash_size();
    const uint32_t orig_quotient_size = metadata_->original_quotient_bits;
    const uint64_t orig_nslots = metadata_->nslots >> (metadata_->key_bits
            - metadata_->fingerprint_bits
            - metadata_->original_quotient_bits);
    const uint64_t fast_reduced_part = fast_reduce(((hash & BITMASK(orig_quotient_size)) 
                << (32 - orig_quotient_size)), orig_nslots);
    const uint64_t hash_bucket_index = (fast_reduced_part << (bucket_index_hash_size - orig_quotient_size))
        | ((hash >> orig_quotient_size) & BITMASK(bucket_index_hash_size - orig_quotient_size));
    const uint64_t hash_fingerprint = (hash >> bucket_index_hash_size) & BITMASK(metadata_->fingerprint_bits);

    bool target_found = false;
    // If a run starts at "position" move the iterator to point it to the
    // smallest key greater than or equal to "hash."
    if (is_occupied(hash_bucket_index)) {
        uint64_t runstart_index = hash_bucket_index == 0 ? 0 : run_end(hash_bucket_index - 1) + 1;
        if (runstart_index < hash_bucket_index)
            runstart_index = hash_bucket_index;
        int64_t fingerprint_pos = next_matching_fingerprint_in_run(runstart_index, hash_fingerprint);
        if (fingerprint_pos < 0)
            fingerprint_pos = lower_bound_fingerprint_in_run(runstart_index, hash_fingerprint);
        // Found something matching `hash`, or smallest key greater than `hash`
        // in this run.
        target_found = (uint64_t) fingerprint_pos <= run_end(hash_bucket_index);
        if (target_found) {
            it.run_ = hash_bucket_index;
            it.current_ = fingerprint_pos;
        }
    }
    // If a run doesn't start at `position` or the largest key in the run
    // starting at `position` is smaller than `hash` then find the start of the
    // next run.
    if (!is_occupied(hash_bucket_index) || !target_found) {
        uint64_t position = hash_bucket_index;
        assert(position < metadata_->nslots);
        uint64_t block_index = position / slots_per_block_;
        uint64_t idx = lowbit_position(get_block(block_index)->occupieds[0] & (~BITMASK((position % slots_per_block_) + 1)));
        if (idx == 64) {
            while(idx == 64 && block_index < metadata_->nblocks) {
                block_index++;
                idx = lowbit_position(get_block(block_index)->occupieds[0]);
            }
        }
        position = block_index * slots_per_block_ + idx;
        it.run_ = position;
        it.current_ = std::max(position == 0 ? 0 : run_end(position - 1) + 1, position);
    }

    if (it.current_ >= metadata_->xnslots)
        it.current_ = 0XFFFFFFFFFFFFFFFF;
    return it;
}


template <bool expandable>
inline typename Memento<expandable>::hash_iterator Memento<expandable>::hash_end() const {
    hash_iterator it(*this);
    it.current_ = 0XFFFFFFFFFFFFFFFF;
    return it;
}


template <bool expandable>
inline int32_t Memento<expandable>::hash_iterator::get(uint64_t& key, uint64_t *mementos) const {
	if (*this == filter_.hash_end())
		return -1;

    int32_t res = 0;
	uint64_t f1, f2, m1, m2;
    f1 = filter_.get_fingerprint(current_);
    f2 = filter_.get_fingerprint(current_ + 1);
    if (!filter_.is_runend(current_) && f1 > f2) {
        m1 = filter_.get_memento(current_);
        m2 = filter_.get_memento(current_ + 1);
        if (m1 < m2) {
            if (mementos != nullptr) {
                mementos[res++] = m1;
                mementos[res++] = m2;
            }
            else
                res += 2;
        }
        else {
            // Mementos stored as sorted list
            const uint64_t memento_bits = filter_.metadata_->memento_bits;
            const uint64_t max_memento_value = (1ULL << memento_bits) - 1;

            if (mementos != nullptr)
                mementos[res] = m2;
            res++;
            const uint64_t pos = current_ + 2;
            uint64_t data = 0;
            uint64_t filled_bits = 0;
            uint64_t data_bit_pos = (pos % slots_per_block_) * filter_.metadata_->bits_per_slot;
            uint64_t data_block_ind = pos / slots_per_block_;
            GET_NEXT_DATA_WORD_IF_EMPTY_ITERATOR(filter_, data, filled_bits, memento_bits,
                                                 data_bit_pos, data_block_ind);

            uint64_t memento_count = data & max_memento_value;
            data >>= memento_bits;
            filled_bits -= memento_bits;
            if (memento_count == max_memento_value) {
                uint64_t length = 2, pw = 1;
                memento_count = 0;
                while (length) {
                    GET_NEXT_DATA_WORD_IF_EMPTY_ITERATOR(filter_, data, filled_bits, memento_bits,
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
                GET_NEXT_DATA_WORD_IF_EMPTY_ITERATOR(filter_, data, filled_bits, memento_bits,
                                                     data_bit_pos, data_block_ind);
                if (mementos != nullptr)
                    mementos[res] = data & max_memento_value;
                res++;
                data >>= memento_bits;
                filled_bits -= memento_bits;
            }
            if (mementos != nullptr)
                mementos[res] = m1;
            res++;
        }
    }
    else {
        if (mementos != nullptr)
            mementos[res] = filter_.get_memento(current_);
        res++;
    }

    const uint32_t bucket_index_hash_size = filter_.get_bucket_index_hash_size();
    const uint32_t original_quotient_bits = filter_.metadata_->original_quotient_bits;
    const uint64_t original_bucket_index = run_ >> (bucket_index_hash_size - original_quotient_bits);
    const uint64_t bucket_extension = ((run_ & BITMASK(bucket_index_hash_size - original_quotient_bits)) << original_quotient_bits);
    key = original_bucket_index | (f1 << bucket_index_hash_size) | bucket_extension;
	return res;
}


template <bool expandable>
inline Memento<expandable>::hash_iterator::hash_iterator(hash_iterator&& other) noexcept:
        filter_{other.filter_},
        run_{other.run_},
        current_{other.current_} {
#ifdef LOG_CLUSTER_LENGTH
    cur_start_index = other.cur_start_index;
    cur_length = other.cur_length;
    c_info = other.c_info;
    other.c_info = nullptr;
#endif
}

template <bool expandable>
inline typename Memento<expandable>::hash_iterator& Memento<expandable>::hash_iterator::operator=(const hash_iterator& other) {
    assert(&filter_ == &other.filter_);
    run_ = other.run_;
    current_ = other.current_;
#ifdef LOG_CLUSTER_LENGTH
    cur_start_index = other.cur_start_index;
    cur_length = other.cur_length;
    if (c_info == nullptr && other.c_info != nullptr) {
        c_info = new cluster_data[filter.metadata->nslots / 32];
        memcpy(c_info, other.c_info, sizeof(cluster_data) * (filter.metadata->nslots / 32));
    }
    else if (c_info != nullptr && other.c_info == nullptr) {
        delete[] c_info;
        c_info = nullptr;
    }
    else if (c_info != nullptr && other.c_info != nullptr)
        memcpy(c_info, other.c_info, sizeof(cluster_data) * (filter.metadata->nslots / 32));
#endif
    return *this;
}

template <bool expandable>
inline typename Memento<expandable>::hash_iterator& Memento<expandable>::hash_iterator::operator=(hash_iterator&& other) noexcept {
    assert(&filter_ == &other.filter_);
    run_ = other.run_;
    current_ = other.current_;
#ifdef LOG_CLUSTER_LENGTH
    cur_start_index = other.cur_start_index;
    cur_length = other.cur_length;
    if (c_info != nullptr)
        delete c_info;
    c_info = other.c_info;
    other.c_info = nullptr;
#endif
    return *this;
}

template <bool expandable>
inline Memento<expandable>::hash_iterator::~hash_iterator() {
#ifdef LOG_CLUSTER_LENGTH
    delete[] c_info;
#endif
}


template <bool expandable>
inline typename Memento<expandable>::hash_iterator& Memento<expandable>::hash_iterator::operator++() {
	if (*this == filter_.hash_end())
		return *this;
	else {
		// Move to the end of the memento list
		if (!filter_.is_runend(current_)) {
            if (filter_.get_fingerprint(current_) > filter_.get_fingerprint(current_ + 1)) {
                uint64_t current_memento = filter_.get_memento(current_);
                uint64_t next_memento = filter_.get_memento(current_ + 1);
                if (current_memento < next_memento)
                  current_++;
                else // Mementos encoded as a sroted list
                  current_ += filter_.number_of_slots_used_for_memento_list(current_ + 2) + 1;
            }
        }

		if (!filter_.is_runend(current_)) {
			current_++;
#ifdef LOG_CLUSTER_LENGTH
			cur_length++;
#endif
		} else {
#ifdef LOG_CLUSTER_LENGTH
			// Save to check if the new current is the new cluster.
			uint64_t old_current = current;
#endif
			uint64_t block_index = run_ / slots_per_block_;
			uint64_t rank = bitrank(filter_.get_block(block_index)->occupieds[0], run_ % slots_per_block_);
            uint64_t next_run = bitselect(filter_.get_block(block_index)->occupieds[0], rank);
			if (next_run == 64) {
				rank = 0;
				while (next_run == 64 && block_index < filter_.metadata_->nblocks) {
					block_index++;
					next_run = bitselect(filter_.get_block(block_index)->occupieds[0], rank);
				}
			}
			if (block_index == filter_.metadata_->nblocks) {
				/* set the index values to max. */
				run_ = current_ = 0XFFFFFFFFFFFFFFFF;
                return *this;
			}
            run_ = block_index * slots_per_block_ + next_run;
            current_++;
            if (current_ < run_)
                current_ = run_;
#ifdef LOG_CLUSTER_LENGTH
			if (current > old_current + 1) { /* new cluster. */
				if (cur_length > 10) {
					c_info[num_clusters].start_index = cur_start_index;
					c_info[num_clusters].length = cur_length;
					num_clusters++;
				}
				cur_start_index = run;
				cur_length = 1;
			} 
            else
				cur_length++;
#endif
		}
        return *this;
	}
}

template <bool expandable>
inline typename Memento<expandable>::hash_iterator Memento<expandable>::hash_iterator::operator++(int) {
    auto old = *this;
    operator++();
    return old;
}

template <bool expandable>
inline bool Memento<expandable>::hash_iterator::is_at_runend() const {
    if (filter_.is_runend(current_))
        return true;
    if (filter_.get_fingerprint(current_) <= filter_.get_fingerprint(current_ + 1))
        return false;
    uint64_t current_memento = filter_.get_memento(current_);
    uint64_t next_memento = filter_.get_memento(current_ + 1);
    if (current_memento < next_memento)
        return filter_.is_runend(current_ + 1);
    return filter_.is_runend(current_ + filter_.number_of_slots_used_for_memento_list(current_ + 2) + 1);
}


template <bool expandable>
inline bool Memento<expandable>::hash_iterator::operator==(const Memento<expandable>::hash_iterator& rhs) const {
    if (&filter_ != &rhs.filter_)
        return false;
    const int64_t lhs_current = current_ > filter_.metadata_->xnslots ? -1 : current_;
    const int64_t rhs_current = rhs.current_ > filter_.metadata_->xnslots ? -1 : rhs.current_;
    return lhs_current == rhs_current;
}


template <bool expandable>
inline bool Memento<expandable>::hash_iterator::operator!=(const Memento<expandable>::hash_iterator& rhs) const {
    return !(*this == rhs);
}



template <bool expandable>
inline void Memento<expandable>::debug_dump_block(uint64_t i) const {
    std::cerr << "============================= block " << i << " offset=" << +get_block(i)->offset << std::endl;
    for (uint32_t j = 0; j < slots_per_block_; j++) {
        const uint64_t ind = slots_per_block_ * i + j;
        if (ind >= metadata_->xnslots)
            break;
        const uint64_t slot = get_slot(ind);
        std::cerr << '@' << ind << '(' << j << "):" << is_occupied(ind) << ',' << is_runend(ind) << ',';
        for (int32_t k = metadata_->bits_per_slot - 1; k >= 0; k--)
            std::cerr << ((slot >> k) & 1);
        std::cerr << ' ';
    }
    std::cerr << std::endl << std::endl;
}

template <bool expandable>
inline void Memento<expandable>::debug_dump_metadata() const {
    std::cerr << "Slots: " << metadata_->nslots;
    std::cerr << " Blocks: " << metadata_->nblocks;
    std::cerr << " Occupied: " << metadata_->noccupied_slots;
    std::cerr << " Elements: " << metadata_->nelts;
    std::cerr << " Distinct: " << metadata_->ndistinct_elts << std::endl;
    std::cerr << "Key bits: " << metadata_->key_bits;
    std::cerr << " Fingerprint bits: " << metadata_->fingerprint_bits;
    std::cerr << " Memento bits: " << metadata_->memento_bits;
    std::cerr << " --- Bits per slot: " << metadata_->bits_per_slot << std::endl;
}


#undef GET_NO_LOCK
#undef GET_TRY_ONCE_LOCK
#undef GET_WAIT_FOR_LOCK
#undef GET_KEY_HASH
#undef REMAINDER_WORD
#undef CMP_MASK_FINGERPRINT
#undef GET_NEXT_DATA_WORD_IF_EMPTY
#undef GET_NEXT_DATA_WORD_IF_EMPTY_ITERATOR
#undef INIT_PAYLOAD_WORD
#undef APPEND_WRITE_PAYLOAD_WORD
#undef FLUSH_PAYLOAD_WORD
}
