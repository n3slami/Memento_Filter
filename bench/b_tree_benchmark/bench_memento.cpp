/*
 * This file is part of Grafite <https://github.com/marcocosta97/grafite>.
 * Copyright (C) 2023 Marco Costa.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <boost/sort/sort.hpp>

#include "../bench_template_b_tree.hpp"
#include "memento.h"
#include "memento_int.h"

/**
 * This file contains the benchmark for Memento filter.
 */

inline uint64_t MurmurHash64A(const void * key, int len, unsigned int seed)
{
	const uint64_t m = 0xc6a4a7935bd1e995;
	const int r = 47;

	uint64_t h = seed ^ (len * m);

	const uint64_t * data = (const uint64_t *)key;
	const uint64_t * end = data + (len/8);

	while(data != end) {
		uint64_t k = *data++;

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	const unsigned char * data2 = (const unsigned char*)data;

	switch(len & 7) {
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

__attribute__((always_inline))
static inline uint32_t fast_reduce(uint32_t hash, uint32_t n) {
    // http://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
    return (uint32_t) (((uint64_t) hash * n) >> 32);
}

inline void check_iteration_validity(QF *qf, bool mode)
{
    QFi iter;
    qf_iterator_from_position(qf, &iter, 0);
    uint64_t hash_result, memento_result[256];
    uint64_t last_run = iter.run, last_fingerprint = 0, current_fingerprint;
    uint64_t cnt = 0;
    do {
        if (mode) {
            std::cerr << "run=" << iter.run << " current=" << iter.current << " vs. nslots=" << qf->metadata->nslots << std::endl;
            //qf_dump_block(qf, 2654 / QF_SLOTS_PER_BLOCK);
        }
        int result_length = qfi_get_hash(&iter, &hash_result, memento_result);
        current_fingerprint = hash_result >> (qf->metadata->key_bits - qf->metadata->fingerprint_bits);
        if (current_fingerprint == 0) {
            std::cerr << "WTF run=" << iter.run << " current=" << iter.current << " vs. nslots=" << qf->metadata->nslots << std::endl; 
            exit(1);
        }
        assert(current_fingerprint > 0);
        if (iter.run != last_run)
            last_run = iter.run;
        else {
            if (last_fingerprint > current_fingerprint) {
                std::cerr << "HMMM iter.run=" << iter.run << " iter.current=" << iter.current << std::endl;
            }
            assert(last_fingerprint <= current_fingerprint);
        }
        last_fingerprint = current_fingerprint;
        for (int i = 1; i < result_length; i++) {
            if (memento_result[i] < memento_result[i - 1]) {
                std::cerr << "run=" << iter.run << " current=" << iter.current << std::endl;
                for (int j = 0; j < result_length; j++)
                    std::cerr << memento_result[j] << ' ';
                std::cerr << std::endl;
            }
            assert(memento_result[i] >= memento_result[i - 1]);
        }

    } while (qfi_next(&iter) >= 0);
}

template <typename t_itr, typename... Args>
inline QF *init_memento(const t_itr begin, const t_itr end, const double bpk, Args... args)
{
    auto&& t = std::forward_as_tuple(args...);
    auto queries_temp = std::get<0>(t);
    auto query_lengths = std::vector<uint64_t>(queries_temp.size());
    std::transform(queries_temp.begin(), queries_temp.end(), query_lengths.begin(), [](auto x) {
        auto [left, right, result] = x;
        return right - left + 1;
    });
    const uint64_t n_items = std::distance(begin, end);
    //const uint64_t seed = std::chrono::steady_clock::now().time_since_epoch().count();
    const uint64_t seed = 1380;
    const uint64_t max_range_size = *std::max_element(query_lengths.begin(), query_lengths.end());
    const double load_factor = 0.94;
    const uint64_t n_slots = n_items / load_factor + std::sqrt(n_items);
    uint32_t memento_bits = 1;
    while ((1ULL << memento_bits) < max_range_size)
        memento_bits++;
    memento_bits = memento_bits < 2 ? 2 : memento_bits;
    const uint32_t fingerprint_size = round(bpk * load_factor - memento_bits - 3.125);
    uint32_t key_size = 0;
    while ((1ULL << key_size) <= n_slots)
        key_size++;
    key_size += fingerprint_size;
    std::cerr << "fingerprint_size=" << fingerprint_size << " memento_bits=" << memento_bits << std::endl;

    QF *qf = (QF *) malloc(sizeof(QF));
    qf_malloc(qf, n_slots, key_size, memento_bits, QF_HASH_DEFAULT, seed);
    qf_set_auto_resize(qf, true);

    start_timer(build_time);

    auto key_hashes = std::vector<uint64_t>(n_items);
    const uint64_t address_size = key_size - fingerprint_size;
    const uint64_t address_mask = (1ULL << address_size) - 1;
    const uint64_t memento_mask = (1ULL << memento_bits) - 1;
    const uint64_t hash_mask = (1ULL << key_size) - 1;
    SimpleBigInt key(key_len);
    std::transform(begin, end, key_hashes.begin(), [&](auto x) {
            auto y = x & ~(memento_mask);
            key = y;
            uint64_t hash = MurmurHash64A(((void *) key.num), key_len, seed) & hash_mask;
            const uint64_t address = fast_reduce((hash & address_mask) << (32 - address_size),
                                                    n_slots);
            hash = (hash >> address_size) | (address << fingerprint_size);
            return (hash << memento_bits) | (x & memento_mask);
            });
    /*
     * The following code uses the Boost library to sort the elements in a single thread, via spreadsort function.
     * This function is faster than std::sort and exploits the fact that the size of the maximum hash is bounded
     * via hybrid radix sort.
     */
    boost::sort::spreadsort::spreadsort(key_hashes.begin(), key_hashes.end());

    qf_bulk_load(qf, &key_hashes[0], key_hashes.size(), QF_NO_LOCK | QF_KEY_IS_HASH);

    stop_timer(build_time);

    check_iteration_validity(qf, false);

    return qf;
}

template <typename value_type>
inline void insert_memento(QF *f, const value_type key)
{
    char dup_key[key_len];
    memcpy(dup_key, key, key_len);
    uint64_t memento = 0;
    for (uint32_t i = 0; 8 * i < f->metadata->memento_bits; i++) {
        const uint64_t mask = ((1ULL << (f->metadata->memento_bits - 8 * i)) - 1);
        memento |= (dup_key[key_len - i - 1] & mask) << (8 * i);
        dup_key[key_len - i - 1] &= ~(mask);
    }
    uint64_t prefix_hash = MurmurHash64A((void *) dup_key, key_len, f->metadata->seed);

    qf_insert_single(f, prefix_hash, memento, QF_NO_LOCK | QF_KEY_IS_HASH);
}

template <typename value_type>
inline bool query_memento(QF *f, const value_type left, const value_type right)
{
    char dup_key[key_len];
    memcpy(dup_key, left, key_len);
    uint64_t l_memento = 0;
    for (uint32_t i = 0; 8 * i < f->metadata->memento_bits; i++) {
        const uint64_t mask = ((1ULL << (f->metadata->memento_bits - 8 * i)) - 1);
        l_memento |= (dup_key[key_len - i - 1] & mask) << (8 * i);
        dup_key[key_len - i - 1] &= ~(mask);
    }
    uint64_t l_hash = MurmurHash64A((void *) dup_key, key_len, f->metadata->seed);

    memcpy(dup_key, right, key_len);
    uint64_t r_memento = 0;
    for (uint32_t i = 0; 8 * i < f->metadata->memento_bits; i++) {
        const uint64_t mask = ((1ULL << (f->metadata->memento_bits - 8 * i)) - 1);
        r_memento |= (dup_key[key_len - i - 1] & mask) << (8 * i);
        dup_key[key_len - i - 1] &= ~(mask);
    }
    uint64_t r_hash = MurmurHash64A((void *) dup_key, key_len, f->metadata->seed);

    return qf_range_query(f, l_hash, l_memento, r_hash, r_memento, QF_NO_LOCK | QF_KEY_IS_HASH);
}

inline size_t size_memento(QF *f)
{
    return qf_get_total_size_in_bytes(f);
}

int main(int argc, char const *argv[])
{
    auto parser = init_parser("bench-memento");

    try
    {
        parser.parse_args(argc, argv);
    }
    catch (const std::runtime_error &err)
    {
        std::cerr << err.what() << std::endl;
        std::cerr << parser;
        std::exit(1);
    }

    auto [ keys, queries, arg ] = read_parser_arguments(parser);

    experiment(pass_fun(init_memento), pass_ref(query_memento), pass_ref(insert_memento),
                pass_ref(size_memento), arg, keys, queries, queries);

    print_test();

    return 0;
}



