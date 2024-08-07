/**
 * @file Tests for Memento filter's C++ implementation
 * @author ---
 */

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <set>
#include <tuple>
#include <utility>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "memento.hpp"
#include "doctest.h"


void assert_memento_contents(std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>>& hash_set,
                             Memento& filter) {
    std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> filter_hash_set;

    auto filter_it = filter.hash_begin();
    uint64_t key, memento_count, mementos[1024];
    for (; filter_it != filter.hash_end(); filter_it++) {
        memento_count = filter_it.get(key, mementos);
        const uint64_t fingerprint = key >> filter.get_bucket_index_hash_size();
        const uint64_t bucket_index = key & BITMASK(filter.get_bucket_index_hash_size());
        for (int32_t i = 0; i < memento_count; i++)
            filter_hash_set.insert({bucket_index, fingerprint, mementos[i]});
    }
    REQUIRE_EQ(filter_it, filter.hash_end());

    REQUIRE_EQ(hash_set.size(), filter_hash_set.size());
    auto hash_it = hash_set.begin();
    for (auto it = filter_hash_set.begin(); it != filter_hash_set.end(); ++it) {
        REQUIRE_EQ(*hash_it, *it);
        ++hash_it;
    }
    REQUIRE_EQ(hash_it, hash_set.end());
}


TEST_SUITE("standard memento") {
    const uint32_t seed = 1;

    TEST_CASE("allocation") {
        const uint32_t n_slots = 1000;
        const uint32_t key_bits = 20;
        const uint32_t memento_bits = 5;

        Memento memento(n_slots, key_bits, memento_bits, Memento::hashmode::Default, seed);
    }

    TEST_CASE("inserts") {
        const uint32_t n_elements = 1000000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;

        SUBCASE("monte-carlo no hashing") {
            Memento memento(n_slots, key_bits, memento_bits, Memento::hashmode::None, seed);
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento::flag_key_is_hash | Memento::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix_hash = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix_hash, elem_memento, flags), 0);

                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                check_set.insert({bucket_index, fingerprint, elem_memento});
            }
            assert_memento_contents(check_set, memento);
        }


        SUBCASE("monte-carlo single insert") {
            Memento memento(n_slots, key_bits, memento_bits, Memento::hashmode::Default, seed);
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags), 0);

                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                check_set.insert({bucket_index, fingerprint, elem_memento});
            }
            assert_memento_contents(check_set, memento);
        }


        SUBCASE("monte-carlo list insert") {
            Memento memento(n_slots, key_bits, memento_bits, Memento::hashmode::Default, seed);
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento::flag_no_lock;

            const uint32_t geometric_modulo = 5;
            std::vector<uint64_t> memento_list;
            uint32_t last_size = 0;
            while (check_set.size() < n_elements) {
                memento_list.clear();
                const uint64_t elem_prefix = rng() >> memento_bits;
                for (uint64_t memento = rng() % geometric_modulo; memento > 0; memento = rng() % geometric_modulo)
                    memento_list.push_back(memento);
                std::sort(memento_list.begin(), memento_list.end());
                REQUIRE_GE(memento.insert_mementos(elem_prefix, memento_list.data(), memento_list.size(), flags), 0);

                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                for (uint64_t memento : memento_list)
                    check_set.insert({bucket_index, fingerprint, memento});
            }
            assert_memento_contents(check_set, memento);
        }
    }
}


