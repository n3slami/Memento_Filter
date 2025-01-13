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
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "memento.hpp"
#include "doctest.h"

namespace memento {
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

        Memento memento{n_slots, key_bits, memento_bits, Memento::hashmode::Default, seed, 50};
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
            Memento memento{n_slots, key_bits, memento_bits, Memento::hashmode::None, seed, 50};
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
            Memento memento{n_slots, key_bits, memento_bits, Memento::hashmode::Default, seed};
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
            Memento memento{n_slots, key_bits, memento_bits, Memento::hashmode::Default, seed};
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


    TEST_CASE("queries") {
        const uint32_t n_elements = 1000000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;
        Memento memento{n_slots, key_bits, memento_bits, Memento::hashmode::Default, seed};

        SUBCASE("point: no false negatives") {
        }


        SUBCASE("point: false positive rate") {
        }


        SUBCASE("range: no false negatives") {
        }

        SUBCASE("range: false positive rate") {
        }
    }

    TEST_CASE("iterator") {
        const uint32_t n_elements = 1000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;
        Memento memento{n_slots, key_bits, memento_bits, Memento::hashmode::Default, seed};

        uint64_t key_prefix = 100;
        std::vector<uint64_t> mementos {1, 2, 3, 4, 5, 6, 7};
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento::flag_no_lock);
        memento.insert(key_prefix, 20, Memento::flag_no_lock);
        mementos.clear();
        for (int32_t i = 25; i < 31; i++)
            mementos.push_back(i);
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento::flag_no_lock);

        SUBCASE("single partition") {
            const uint64_t check_keys[14] = {3201, 3202, 3203, 3204, 3205,
                3206, 3207, 3220, 3225, 3226, 3227, 3228, 3229, 3230};
            const uint64_t l = (100ULL << memento_bits) | 0ULL;
            const uint64_t r = (100ULL << memento_bits) | BITMASK(memento_bits);
            auto it = memento.begin(l, r);
            for (int32_t i = 0; i < 14; i++) {
                REQUIRE_NE(it, memento.end());
                REQUIRE_EQ(*it, check_keys[i]);
                ++it;
            }
            REQUIRE_EQ(it, memento.end());
        }

        SUBCASE("single partition: skip first and last mementos") {
            const uint64_t check_keys[11] = {3203, 3204, 3205, 3206, 3207,
                3220, 3225, 3226, 3227, 3228, 3229};
            const uint64_t l = (100ULL << memento_bits) | 3ULL;
            const uint64_t r = (100ULL << memento_bits) | 29ULL;
            auto it = memento.begin(l, r);
            for (int32_t i = 0; i < 11; i++) {
                REQUIRE_NE(it, memento.end());
                REQUIRE_EQ(*it, check_keys[i]);
                ++it;
            }
            REQUIRE_EQ(it, memento.end());
        }

        key_prefix = 101;
        mementos.clear();
        for (int32_t i = 10; i < 24; i++)
            mementos.push_back(i);
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento::flag_no_lock);
        mementos.clear();
        for (int32_t i = 0; i < 30; i += 2)
            mementos.push_back(i);
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento::flag_no_lock);

        SUBCASE("two partitions") {
            const uint64_t check_keys[43] = {3201, 3202, 3203, 3204, 3205,
                3206, 3207, 3220, 3225, 3226, 3227, 3228, 3229, 3230, 3232,
                3234, 3236, 3238, 3240, 3242, 3242, 3243, 3244, 3244, 3245,
                3246, 3246, 3247, 3248, 3248, 3249, 3250, 3250, 3251, 3252,
                3252, 3253, 3254, 3254, 3255, 3256, 3258, 3260};
            const uint64_t l = (100ULL << memento_bits) | 0ULL;
            const uint64_t r = (101ULL << memento_bits) | BITMASK(memento_bits);
            auto it = memento.begin(l, r);
            for (int32_t i = 0; i < 43; i++) {
                REQUIRE_NE(it, memento.end());
                REQUIRE_EQ(*it, check_keys[i]);
                ++it;
            }
            REQUIRE_EQ(it, memento.end());
        }

        key_prefix = 103;
        mementos.clear();
        for (int32_t i = 1; i < 32; i += 3)
            mementos.push_back(i);
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento::flag_no_lock);

        SUBCASE("five partitions with two gaps") {
            const uint64_t check_keys[54] = {3201, 3202, 3203, 3204, 3205,
                3206, 3207, 3220, 3225, 3226, 3227, 3228, 3229, 3230, 3232,
                3234, 3236, 3238, 3240, 3242, 3242, 3243, 3244, 3244, 3245,
                3246, 3246, 3247, 3248, 3248, 3249, 3250, 3250, 3251, 3252,
                3252, 3253, 3254, 3254, 3255, 3256, 3258, 3260, 3297, 3300,
                3303, 3306, 3309, 3312, 3315, 3318, 3321, 3324, 3327};
            const uint64_t l = (100ULL << memento_bits) | 0ULL;
            const uint64_t r = (103ULL << memento_bits) | BITMASK(memento_bits);
            auto it = memento.begin(l, r);
            for (int32_t i = 0; i < 54; i++) {
                REQUIRE_NE(it, memento.end());
                REQUIRE_EQ(*it, check_keys[i]);
                ++it;
            }
            REQUIRE_EQ(it, memento.end());
        }

        SUBCASE("insert") {
            key_prefix = 200;
            std::vector<uint64_t> check_keys;
            for (int32_t i = 31; i > 0; i -= 3) {
                const uint64_t key = (key_prefix << memento_bits) | i;
                check_keys.insert(check_keys.begin(), key);
                memento.insert(key_prefix, i, Memento::flag_no_lock);

                const uint64_t l = (key_prefix << memento_bits) | 0ULL;
                const uint64_t r = (key_prefix << memento_bits) | BITMASK(memento_bits);
                auto it = memento.begin(l, r);
                for (int32_t i = 0; i < check_keys.size(); i++) {
                    REQUIRE_NE(it, memento.end());
                    REQUIRE_EQ(*it, check_keys[i]);
                    ++it;
                }
                REQUIRE_EQ(it, memento.end());
            }
        }
    }
}

TEST_SUITE("standard memento: large mementos") {
    const uint32_t seed = 1;

    TEST_CASE("iterator") {
        const uint32_t n_elements = 1000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 40;
        Memento memento{n_slots, key_bits, memento_bits, Memento::hashmode::Default, seed};

        SUBCASE("insert") {
            uint64_t key_prefix = 200;
            std::vector<uint64_t> check_keys;
            for (int64_t i = BITMASK(memento_bits); i > 0; i -= BITMASK(memento_bits) >> 10) {
                const uint64_t key = (key_prefix << memento_bits) | i;
                check_keys.insert(check_keys.begin(), key);
                memento.insert(key_prefix, i, Memento::flag_no_lock);

                const uint64_t l = (key_prefix << memento_bits) | 0ULL;
                const uint64_t r = (key_prefix << memento_bits) | BITMASK(memento_bits);
                auto it = memento.begin(l, r);
                for (int32_t j = 0; j < check_keys.size(); j++) {
                    REQUIRE_NE(it, memento.end());
                    REQUIRE_EQ(*it, check_keys[j]);
                    ++it;
                }
                REQUIRE_EQ(it, memento.end());
            }
        }
    }

    TEST_CASE("serialize & deserialize") {
        const uint32_t n_elements = 1000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;
        Memento memento{n_slots, key_bits, memento_bits, Memento::hashmode::Default, seed};

        // serialize
        char* serialized = memento.serialize();

        // deserialize
        Memento* memento_deserialized = new memento::Memento(serialized);

        // assert they are equal
        REQUIRE_EQ(memento.get_num_memento_bits(), memento_deserialized->get_num_memento_bits());
        REQUIRE_EQ(memento.get_num_fingerprint_bits(), memento_deserialized->get_num_fingerprint_bits());
        REQUIRE_EQ(memento.get_bucket_index_hash_size(), memento_deserialized->get_bucket_index_hash_size());
        REQUIRE_EQ(memento.count_keys(), memento_deserialized->count_keys());
        REQUIRE_EQ(memento.size_in_bytes(), memento_deserialized->size_in_bytes());
        REQUIRE_EQ(memento.count_distinct_prefixes(), memento_deserialized->count_distinct_prefixes());
        REQUIRE_EQ(memento.count_slots(), memento_deserialized->count_slots());
    }
}
} // namespace memento


