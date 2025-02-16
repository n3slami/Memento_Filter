/**
 * @file Tests for Memento filter's C++ implementation
 * @author ---
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <ostream>
#include <random>
#include <set>
#include <tuple>
#include <vector>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "memento.hpp"
#include "doctest.h"

namespace memento {
void assert_memento_contents(std::multiset<std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>>& hash_set,
                             Memento<false>& filter) {
    std::multiset<std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>> filter_hash_set;

    auto filter_it = filter.hash_begin();
    uint64_t key, memento_count;
    std::vector<uint64_t> mementos;
    std::vector<uint64_t> payloads;
    mementos.reserve(1024);
    payloads.reserve(1024);
    for (; filter_it != filter.hash_end(); filter_it++) {
        // reset the vectors
        mementos.clear();
        payloads.clear();
        memento_count = filter_it.get(key, &mementos, &payloads);
        const uint64_t fingerprint = key >> filter.get_bucket_index_hash_size();
        const uint64_t bucket_index = key & BITMASK(filter.get_bucket_index_hash_size());
        for (int32_t i = 0; i < memento_count; i++)
            if (filter.get_payload_bits() > 0)
                filter_hash_set.insert({bucket_index, fingerprint, mementos[i], payloads[i]});
            else
                filter_hash_set.insert({bucket_index, fingerprint, mementos[i], 0});
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

    void assert_memento_contents(std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>>& hash_set,
                                 Memento<false>& filter) {
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> filter_hash_set;

        auto filter_it = filter.hash_begin();
        uint64_t key, memento_count;
        std::vector<uint64_t> mementos;
        std::vector<uint64_t> payloads;
        mementos.reserve(1024);
        payloads.reserve(1024);
        for (; filter_it != filter.hash_end(); filter_it++) {
            // clear existing vectors
            mementos.clear();
            payloads.clear();
            memento_count = filter_it.get(key, &mementos, &payloads);
            for (int32_t i = 0; i < memento_count; i++) {
                if (filter.get_payload_bits() > 0) {
                    filter_hash_set.insert({(1ULL << filter.get_num_key_bits()), mementos[i], payloads[i]});
                } else {
                    filter_hash_set.insert({(1ULL << filter.get_num_key_bits()), mementos[i], 0});
                }
            }
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


void assert_memento_contents(std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>>& hash_set,
                             Memento<true>& filter) {
    std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> filter_hash_set;

    auto filter_it = filter.hash_begin();
    uint64_t key, memento_count;
    std::vector<uint64_t> mementos;
    std::vector<uint64_t> payloads;
    mementos.reserve(1024);
    payloads.reserve(1024);
    for (; filter_it != filter.hash_end(); filter_it++) {
        // clear existing vectors
        mementos.clear();
        payloads.clear();
        memento_count = filter_it.get(key, &mementos, &payloads);
        for (int32_t i = 0; i < memento_count; i++) {
            if (filter.get_payload_bits() > 0) {
                filter_hash_set.insert({key, mementos[i], payloads[i]});
            } else {
                filter_hash_set.insert({key, mementos[i], 0});
            }
        }
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
    const uint32_t seed = 12345;

    TEST_CASE("allocation") {
        const uint32_t n_slots = 1000;
        const uint32_t key_bits = 20;
        const uint32_t memento_bits = 5;

        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};
    }

    TEST_CASE("allocation with payload") {
        const uint32_t n_slots = 1000;
        const uint32_t key_bits = 20;
        const uint32_t memento_bits = 5;
        const uint32_t payload_bits = 50;

        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::None, seed, 0, payload_bits};
    }

    TEST_CASE("inserts") {
        const uint32_t n_elements = 1000000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;

        SUBCASE("monte-carlo no hashing") {
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::None, seed, 0, 50};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_key_is_hash | Memento<false>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix_hash = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix_hash, elem_memento, flags), 0);

                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                check_set.insert({bucket_index, fingerprint, elem_memento,  0});
            }
            assert_memento_contents(check_set, memento);
        }

        SUBCASE("monte-carlo no hashing with payload") {
            const uint32_t payload_bits = 50;
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::None, seed, 0, payload_bits};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_key_is_hash | Memento<false>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix_hash = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix_hash, elem_memento, flags, i), 0);

                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size())
                                                & BITMASK(memento.get_num_fingerprint_bits());
                check_set.insert({bucket_index, fingerprint, elem_memento, i});
            }
            assert_memento_contents(check_set, memento);
        }


        SUBCASE("monte-carlo single insert") {
            const uint32_t payload_bits = 50;
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed, 0, payload_bits};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags, i), 0);

                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size())
                                                & BITMASK(memento.get_num_fingerprint_bits());
                check_set.insert({bucket_index, fingerprint, elem_memento, i});
            }
            assert_memento_contents(check_set, memento);
        }

        SUBCASE("monte-carlo single insert") {
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags), 0);

                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size())
                                                & BITMASK(memento.get_num_fingerprint_bits());
                check_set.insert({bucket_index, fingerprint, elem_memento, 0});
            }
            assert_memento_contents(check_set, memento);
        }

        SUBCASE("monte-carlo single insert with payload") {
            const uint32_t payload_bits = 50;
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed, 0, payload_bits};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags, i), 0);

                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size())
                                                & BITMASK(memento.get_num_fingerprint_bits());
                check_set.insert({bucket_index, fingerprint, elem_memento, i});
            }
            assert_memento_contents(check_set, memento);
        }


        SUBCASE("monte-carlo list insert") {
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_no_lock;

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

                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                for (uint64_t memento : memento_list)
                    check_set.insert({bucket_index, fingerprint, memento, 0});
            }
            assert_memento_contents(check_set, memento);
        }
    }


    TEST_CASE("deletes") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_deletes = 500000;
        const uint32_t delete_insert_ratio = 5;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;
        const uint32_t geometric_modulo = 3;

        SUBCASE("monte-carlo deletes and inserts") {
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_no_lock;
            std::vector<uint64_t> existing_keys, deleted_keys;
            while (existing_keys.size() < n_elements) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> quotient_bits) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                while (rng() % geometric_modulo == 0) {
                    const uint64_t elem_memento = rng() & BITMASK(memento_bits);
                    REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags), 0);
                    existing_keys.push_back((elem_prefix << memento_bits) | elem_memento);
                    check_set.insert({bucket_index, fingerprint, elem_memento, 0});
                }
            }
            assert_memento_contents(check_set, memento);

            uint64_t deletes_done = 0;
            while (deletes_done < n_deletes) {
                const uint32_t action = rng() % delete_insert_ratio;
                if (action > 0 || deleted_keys.empty()) { // Delete
                    const uint32_t delete_rank = rng() % existing_keys.size();
                    const uint64_t elem = existing_keys[delete_rank];
                    const uint64_t elem_prefix = elem >> memento_bits;
                    const uint64_t elem_memento = elem & BITMASK(memento_bits);
                    REQUIRE_GE(memento.delete_single(elem_prefix, elem_memento, flags), 0);
                    std::swap(existing_keys[existing_keys.size() - 1], existing_keys[delete_rank]);
                    existing_keys.pop_back();
                    deleted_keys.push_back(elem);
                    deletes_done++;

                    const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                    const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                              n_slots);
                    const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                    & BITMASK(memento.get_num_fingerprint_bits());
                    auto search_it = check_set.find({bucket_index, fingerprint, elem_memento, 0});
                    REQUIRE(search_it != check_set.end());
                    check_set.erase(search_it);
                }
                else { // Insert
                    const uint32_t insert_rank = rng() % deleted_keys.size();
                    const uint64_t elem = deleted_keys[insert_rank];
                    const uint64_t elem_prefix = elem >> memento_bits;
                    const uint64_t elem_memento = elem & BITMASK(memento_bits);
                    REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags), 0);
                    existing_keys.push_back(elem);
                    std::swap(deleted_keys[deleted_keys.size() - 1], deleted_keys[insert_rank]);
                    deleted_keys.pop_back();

                    const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                    const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                              n_slots);
                    const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size())
                                                    & BITMASK(memento.get_num_fingerprint_bits());
                    check_set.insert({bucket_index, fingerprint, elem_memento, 0});
                }
            }
            assert_memento_contents(check_set, memento);
        }

        SUBCASE("monte-carlo deletes and inserts with payload") {
            const uint32_t payload_bits = 50;
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed, 0, payload_bits};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_no_lock;
            std::vector<uint64_t> existing_keys, deleted_keys;
            while (existing_keys.size() < n_elements) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> quotient_bits)
                                                & BITMASK(memento.get_num_fingerprint_bits());
                while (rng() % geometric_modulo == 0) {
                    const uint64_t elem_memento = rng() & BITMASK(memento_bits);
                    REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags, elem_memento), 0);
                    existing_keys.push_back((elem_prefix << memento_bits) | elem_memento);
                    check_set.insert({bucket_index, fingerprint, elem_memento, elem_memento});
                }
            }
            assert_memento_contents(check_set, memento);

            uint64_t deletes_done = 0;
            while (deletes_done < n_deletes) {
                const uint32_t action = rng() % delete_insert_ratio;
                if (action > 0 || deleted_keys.empty()) { // Delete
                    const uint32_t delete_rank = rng() % existing_keys.size();
                    const uint64_t elem = existing_keys[delete_rank];
                    const uint64_t elem_prefix = elem >> memento_bits;
                    const uint64_t elem_memento = elem & BITMASK(memento_bits);
                    REQUIRE_GE(memento.delete_single(elem_prefix, elem_memento, flags), 0);
                    std::swap(existing_keys[existing_keys.size() - 1], existing_keys[delete_rank]);
                    existing_keys.pop_back();
                    deleted_keys.push_back(elem);
                    deletes_done++;

                    const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                    const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                              n_slots);
                    const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size())
                                                    & BITMASK(memento.get_num_fingerprint_bits());
                    auto search_it = check_set.find({bucket_index, fingerprint, elem_memento, elem_memento});
                    REQUIRE(search_it != check_set.end());
                    check_set.erase(search_it);
                }
                else { // Insert
                    const uint32_t insert_rank = rng() % deleted_keys.size();
                    const uint64_t elem = deleted_keys[insert_rank];
                    const uint64_t elem_prefix = elem >> memento_bits;
                    const uint64_t elem_memento = elem & BITMASK(memento_bits);
                    REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags, elem_memento), 0);
                    existing_keys.push_back(elem);
                    std::swap(deleted_keys[deleted_keys.size() - 1], deleted_keys[insert_rank]);
                    deleted_keys.pop_back();

                    const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                    const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                              n_slots);
                    const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size())
                                                    & BITMASK(memento.get_num_fingerprint_bits());
                    check_set.insert({bucket_index, fingerprint, elem_memento, elem_memento});
                }
            }
            assert_memento_contents(check_set, memento);
        }
    }


    TEST_CASE("queries") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_queries = 1000000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;
        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};

        const float expected_point_fpr = std::pow(2, -memento.get_num_fingerprint_bits());
        const float expected_range_fpr = 2 * expected_point_fpr;

        SUBCASE("point: no false negatives") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                keys.push_back(key);
                memento.insert(key_prefix, key_memento, Memento<false>::flag_no_lock);
            }
            for (uint64_t key : keys) {
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                REQUIRE(memento.point_query(key_prefix, key_memento, Memento<false>::flag_no_lock));
            }
        }


        SUBCASE("point: false positive rate") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                keys.push_back(key);
                memento.insert(key_prefix, key_memento, Memento<false>::flag_no_lock);
            }
            std::sort(keys.begin(), keys.end());

            uint64_t fp_count = 0;
            for (int32_t i = 0; i < n_queries; i++) {
                const uint64_t key = rng();
                if (std::binary_search(keys.begin(), keys.end(), key)) {
                    i--;
                    continue;
                }
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                fp_count += memento.point_query(key_prefix, key_memento, Memento<false>::flag_no_lock);
            }
            REQUIRE_LE(static_cast<float>(fp_count) / n_queries, 1.1 * expected_point_fpr);
        }


        SUBCASE("range: no false negatives") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                keys.push_back(key);
                memento.insert(key_prefix, key_memento, Memento<false>::flag_no_lock);
            }
            std::sort(keys.begin(), keys.end());

            std::vector<std::pair<uint64_t, uint64_t>> queries;
            while (queries.size() < n_queries) {
                uint64_t l = rng(), r = rng();
                if (l > r)
                    std::swap(l, r);
                auto lower_bound_elem = std::lower_bound(keys.begin(), keys.end(), l);
                if (lower_bound_elem == keys.end() || *lower_bound_elem > r)
                    continue;
                queries.push_back({l, r});
            }

            for (auto [l, r] : queries) {
                const uint64_t l_prefix = l >> memento.get_num_memento_bits();
                const uint64_t l_memento = l & BITMASK(memento.get_num_memento_bits());
                const uint64_t r_prefix = r >> memento.get_num_memento_bits();
                const uint64_t r_memento = r & BITMASK(memento.get_num_memento_bits());
                REQUIRE(memento.range_query(l_prefix, l_memento, r_prefix, r_memento, Memento<false>::flag_no_lock));
            }
        }


        SUBCASE("range: false positive rate") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                keys.push_back(key);
                memento.insert(key_prefix, key_memento, Memento<false>::flag_no_lock);
            }
            std::sort(keys.begin(), keys.end());

            std::vector<std::pair<uint64_t, uint64_t>> queries;
            for (int32_t i = 0; i < n_queries; i++) {
                const uint32_t pos = rng() % keys.size();
                const uint64_t sample_range_l = keys[pos];
                const uint64_t sample_range_r = pos == keys.size() - 1 ? keys[pos + 1] : std::numeric_limits<uint64_t>::max();
                uint64_t l = sample_range_l + rng() % (sample_range_r - sample_range_l);
                uint64_t r = sample_range_l + rng() % (sample_range_r - sample_range_l);
                if (l > r)
                    std::swap(l, r);
                queries.push_back({l, r});
            }

            uint64_t fp_count = 0;
            for (auto [l, r] : queries) {
                const uint64_t l_prefix = l >> memento.get_num_memento_bits();
                const uint64_t l_memento = l & BITMASK(memento.get_num_memento_bits());
                const uint64_t r_prefix = r >> memento.get_num_memento_bits();
                const uint64_t r_memento = r & BITMASK(memento.get_num_memento_bits());
                fp_count += memento.range_query(l_prefix, l_memento, r_prefix, r_memento, Memento<false>::flag_no_lock);
            }
            REQUIRE_LE(static_cast<float>(fp_count) / n_queries, 1.1 * expected_range_fpr);
        }
    }

    TEST_CASE("queries with payload") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_queries = 1000000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        const uint32_t payload_bits = 50;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;
        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed, 0, payload_bits};

        const float expected_point_fpr = std::pow(2, -memento.get_num_fingerprint_bits());
        const float expected_range_fpr = 2 * expected_point_fpr;

        SUBCASE("point: no false negatives") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                keys.push_back(key);
                memento.insert(key_prefix, key_memento, Memento<false>::flag_no_lock, key_memento);
            }
            for (uint64_t key : keys) {
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                uint64_t payload;
                REQUIRE(memento.point_query(key_prefix, key_memento, Memento<false>::flag_no_lock, &payload));
                REQUIRE_EQ(payload, key_memento);
            }
        }


        SUBCASE("point: false positive rate") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                keys.push_back(key);
                memento.insert(key_prefix, key_memento, Memento<false>::flag_no_lock, key_memento);
            }
            std::sort(keys.begin(), keys.end());

            uint64_t fp_count = 0;
            for (int32_t i = 0; i < n_queries; i++) {
                const uint64_t key = rng();
                if (std::binary_search(keys.begin(), keys.end(), key)) {
                    i--;
                    continue;
                }
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                fp_count += memento.point_query(key_prefix, key_memento, Memento<false>::flag_no_lock);
            }
            REQUIRE_LE(static_cast<float>(fp_count) / n_queries, 1.1 * expected_point_fpr);
        }


        SUBCASE("range: no false negatives") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                keys.push_back(key);
                memento.insert(key_prefix, key_memento, Memento<false>::flag_no_lock, key_memento);
            }
            std::sort(keys.begin(), keys.end());

            std::vector<std::pair<uint64_t, uint64_t>> queries;
            while (queries.size() < n_queries) {
                uint64_t l = rng(), r = rng();
                if (l > r)
                    std::swap(l, r);
                auto lower_bound_elem = std::lower_bound(keys.begin(), keys.end(), l);
                if (lower_bound_elem == keys.end() || *lower_bound_elem > r)
                    continue;
                queries.push_back({l, r});
            }

            for (auto [l, r] : queries) {
                const uint64_t l_prefix = l >> memento.get_num_memento_bits();
                const uint64_t l_memento = l & BITMASK(memento.get_num_memento_bits());
                const uint64_t r_prefix = r >> memento.get_num_memento_bits();
                const uint64_t r_memento = r & BITMASK(memento.get_num_memento_bits());
                REQUIRE(memento.range_query(l_prefix, l_memento, r_prefix, r_memento, Memento<false>::flag_no_lock));
            }
        }


        SUBCASE("range: false positive rate") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                const uint64_t key_prefix = key >> memento.get_num_memento_bits();
                const uint64_t key_memento = key & BITMASK(memento.get_num_memento_bits());
                keys.push_back(key);
                memento.insert(key_prefix, key_memento, Memento<false>::flag_no_lock, key_memento);
            }
            std::sort(keys.begin(), keys.end());

            std::vector<std::pair<uint64_t, uint64_t>> queries;
            for (int32_t i = 0; i < n_queries; i++) {
                const uint32_t pos = rng() % keys.size();
                const uint64_t sample_range_l = keys[pos];
                const uint64_t sample_range_r = pos == keys.size() - 1 ? keys[pos + 1] : std::numeric_limits<uint64_t>::max();
                uint64_t l = sample_range_l + rng() % (sample_range_r - sample_range_l);
                uint64_t r = sample_range_l + rng() % (sample_range_r - sample_range_l);
                if (l > r)
                    std::swap(l, r);
                queries.push_back({l, r});
            }

            uint64_t fp_count = 0;
            for (auto [l, r] : queries) {
                const uint64_t l_prefix = l >> memento.get_num_memento_bits();
                const uint64_t l_memento = l & BITMASK(memento.get_num_memento_bits());
                const uint64_t r_prefix = r >> memento.get_num_memento_bits();
                const uint64_t r_memento = r & BITMASK(memento.get_num_memento_bits());
                fp_count += memento.range_query(l_prefix, l_memento, r_prefix, r_memento, Memento<false>::flag_no_lock);
            }
            REQUIRE_LE(static_cast<float>(fp_count) / n_queries, 1.1 * expected_range_fpr);
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
        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};

        uint64_t key_prefix = 100;
        std::vector<uint64_t> mementos {1, 2, 3, 4, 5, 6, 7};
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento<false>::flag_no_lock);
        memento.insert(key_prefix, 20, Memento<false>::flag_no_lock);
        mementos.clear();
        for (int32_t i = 25; i < 31; i++)
            mementos.push_back(i);
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento<false>::flag_no_lock);

        SUBCASE("single partition") {
            const uint64_t check_keys[14] = {3201, 3202, 3203, 3204, 3205,
                3206, 3207, 3220, 3225, 3226, 3227, 3228, 3229, 3230};
            const uint64_t l = (100ULL << memento_bits) | 0ULL;
            const uint64_t r = (100ULL << memento_bits) | BITMASK(memento_bits);
            auto it = memento.begin(l, r, Memento<false>::flag_no_lock);
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
            auto it = memento.begin(l, r, Memento<false>::flag_no_lock);
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
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento<false>::flag_no_lock);
        mementos.clear();
        for (int32_t i = 0; i < 30; i += 2)
            mementos.push_back(i);
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento<false>::flag_no_lock);

        SUBCASE("two partitions") {
            const uint64_t check_keys[43] = {3201, 3202, 3203, 3204, 3205,
                3206, 3207, 3220, 3225, 3226, 3227, 3228, 3229, 3230, 3232,
                3234, 3236, 3238, 3240, 3242, 3242, 3243, 3244, 3244, 3245,
                3246, 3246, 3247, 3248, 3248, 3249, 3250, 3250, 3251, 3252,
                3252, 3253, 3254, 3254, 3255, 3256, 3258, 3260};
            const uint64_t l = (100ULL << memento_bits) | 0ULL;
            const uint64_t r = (101ULL << memento_bits) | BITMASK(memento_bits);
            auto it = memento.begin(l, r, Memento<false>::flag_no_lock);
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
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento<false>::flag_no_lock);

        SUBCASE("five partitions with two gaps") {
            const uint64_t check_keys[54] = {3201, 3202, 3203, 3204, 3205,
                3206, 3207, 3220, 3225, 3226, 3227, 3228, 3229, 3230, 3232,
                3234, 3236, 3238, 3240, 3242, 3242, 3243, 3244, 3244, 3245,
                3246, 3246, 3247, 3248, 3248, 3249, 3250, 3250, 3251, 3252,
                3252, 3253, 3254, 3254, 3255, 3256, 3258, 3260, 3297, 3300,
                3303, 3306, 3309, 3312, 3315, 3318, 3321, 3324, 3327};
            const uint64_t l = (100ULL << memento_bits) | 0ULL;
            const uint64_t r = (103ULL << memento_bits) | BITMASK(memento_bits);
            auto it = memento.begin(l, r, Memento<false>::flag_no_lock);
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
                memento.insert(key_prefix, i, Memento<false>::flag_no_lock);

                const uint64_t l = (key_prefix << memento_bits) | 0ULL;
                const uint64_t r = (key_prefix << memento_bits) | BITMASK(memento_bits);
                auto it = memento.begin(l, r, Memento<false>::flag_no_lock);
                for (int32_t i = 0; i < check_keys.size(); i++) {
                    REQUIRE_NE(it, memento.end());
                    REQUIRE_EQ(*it, check_keys[i]);
                    ++it;
                }
                REQUIRE_EQ(it, memento.end());
            }
        }
    }

    TEST_CASE("iterator with payload") {
        const uint32_t n_elements = 1000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        const uint32_t payload_bits = 50;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;
        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed, 0, payload_bits};

        uint64_t key_prefix = 100;
        std::vector<uint64_t> mementos {1, 2, 3, 4, 5, 6, 7};
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento<false>::flag_no_lock, mementos.data());
        memento.insert(key_prefix, 20, Memento<false>::flag_no_lock);
        mementos.clear();
        for (int32_t i = 25; i < 31; i++)
            mementos.push_back(i);
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento<false>::flag_no_lock, mementos.data());

        SUBCASE("single partition") {
            const uint64_t check_keys[14] = {3201, 3202, 3203, 3204, 3205,
                3206, 3207, 3220, 3225, 3226, 3227, 3228, 3229, 3230};
            const uint64_t l = (100ULL << memento_bits) | 0ULL;
            const uint64_t r = (100ULL << memento_bits) | BITMASK(memento_bits);
            auto it = memento.begin(l, r, Memento<false>::flag_no_lock);
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
            auto it = memento.begin(l, r, Memento<false>::flag_no_lock);
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
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento<false>::flag_no_lock, mementos.data());
        mementos.clear();
        for (int32_t i = 0; i < 30; i += 2)
            mementos.push_back(i);
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento<false>::flag_no_lock, mementos.data());

        // commented out since we don't support multiple keepsake boxes with the same fingerprint (we avoid sorting mementos and payloads together)
//        SUBCASE("two partitions") {
//            const uint64_t check_keys[43] = {3201, 3202, 3203, 3204, 3205,
//                3206, 3207, 3220, 3225, 3226, 3227, 3228, 3229, 3230, 3232,
//                3234, 3236, 3238, 3240, 3242, 3242, 3243, 3244, 3244, 3245,
//                3246, 3246, 3247, 3248, 3248, 3249, 3250, 3250, 3251, 3252,
//                3252, 3253, 3254, 3254, 3255, 3256, 3258, 3260};
//            const uint64_t l = (100ULL << memento_bits) | 0ULL;
//            const uint64_t r = (101ULL << memento_bits) | BITMASK(memento_bits);
//            auto it = memento.begin(l, r);
//            for (int32_t i = 0; i < 43; i++) {
//                REQUIRE_NE(it, memento.end());
//                REQUIRE_EQ(*it, check_keys[i]);
//                ++it;
//            }
//            REQUIRE_EQ(it, memento.end());
//        }

        key_prefix = 103;
        mementos.clear();
        for (int32_t i = 1; i < 32; i += 3)
            mementos.push_back(i);
        memento.insert_mementos(key_prefix, mementos.data(), mementos.size(), Memento<false>::flag_no_lock, mementos.data());

        // commented out since we don't support multiple keepsake boxes with the same fingerprint (we avoid sorting mementos and payloads together)
//        SUBCASE("five partitions with two gaps") {
//            const uint64_t check_keys[54] = {3201, 3202, 3203, 3204, 3205,
//                3206, 3207, 3220, 3225, 3226, 3227, 3228, 3229, 3230, 3232,
//                3234, 3236, 3238, 3240, 3242, 3242, 3243, 3244, 3244, 3245,
//                3246, 3246, 3247, 3248, 3248, 3249, 3250, 3250, 3251, 3252,
//                3252, 3253, 3254, 3254, 3255, 3256, 3258, 3260, 3297, 3300,
//                3303, 3306, 3309, 3312, 3315, 3318, 3321, 3324, 3327};
//            const uint64_t l = (100ULL << memento_bits) | 0ULL;
//            const uint64_t r = (103ULL << memento_bits) | BITMASK(memento_bits);
//            auto it = memento.begin(l, r);
//            for (int32_t i = 0; i < 54; i++) {
//                REQUIRE_NE(it, memento.end());
//                REQUIRE_EQ(*it, check_keys[i]);
//                ++it;
//            }
//            REQUIRE_EQ(it, memento.end());
//        }

        SUBCASE("insert") {
            key_prefix = 200;
            std::vector<uint64_t> check_keys;
            for (int32_t i = 31; i > 0; i -= 3) {
                const uint64_t key = (key_prefix << memento_bits) | i;
                check_keys.insert(check_keys.begin(), key);
                memento.insert(key_prefix, i, Memento<false>::flag_no_lock);

                const uint64_t l = (key_prefix << memento_bits) | 0ULL;
                const uint64_t r = (key_prefix << memento_bits) | BITMASK(memento_bits);
                auto it = memento.begin(l, r, Memento<false>::flag_no_lock);
                for (int32_t i = 0; i < check_keys.size(); i++) {
                    REQUIRE_NE(it, memento.end());
                    REQUIRE_EQ(*it, check_keys[i]);
                    ++it;
                }
                REQUIRE_EQ(it, memento.end());
            }
        }
    }

    TEST_CASE("large mementos") {
        const uint32_t n_elements = 1000;
        const uint32_t seed = 1;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 30;
        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};

        SUBCASE("insert") {
            uint64_t key_prefix = 200;
            std::vector<uint64_t> check_keys;
            for (int64_t i = BITMASK(memento_bits); i > 0; i -= BITMASK(memento_bits) >> 10) {
                const uint64_t key = (key_prefix << memento_bits) | i;
                check_keys.insert(check_keys.begin(), key);
                memento.insert(key_prefix, i, Memento<false>::flag_no_lock);

                const uint64_t l = (key_prefix << memento_bits) | 0ULL;
                const uint64_t r = (key_prefix << memento_bits) | BITMASK(memento_bits);
                auto it = memento.begin(l, r, Memento<false>::flag_no_lock);
                for (int32_t j = 0; j < check_keys.size(); j++) {
                    REQUIRE_NE(it, memento.end());
                    REQUIRE_EQ(*it, check_keys[j]);
                    ++it;
                }
                REQUIRE_EQ(it, memento.end());
            }
        }
    }

    TEST_CASE("large mementos with payload") {
        const uint32_t n_elements = 1000;
        const uint32_t seed = 1;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        const uint32_t payload_bits = 50;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor;
        const uint32_t key_bits = 32;
        // use a size here that doesn't overflow a slot of 64 bit (technically we would enforce <= 56)
        const uint32_t memento_bits = 30;
        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed, 0, payload_bits};

        SUBCASE("insert") {
            uint64_t key_prefix = 200;
            std::vector<uint64_t> check_keys;
            for (int64_t i = BITMASK(memento_bits); i > 0; i -= BITMASK(memento_bits) >> 10) {
                const uint64_t key = (key_prefix << memento_bits) | i;
                check_keys.insert(check_keys.begin(), key);
                memento.insert(key_prefix, i, Memento<false>::flag_no_lock, i);

                const uint64_t l = (key_prefix << memento_bits) | 0ULL;
                const uint64_t r = (key_prefix << memento_bits) | BITMASK(memento_bits);
                auto it = memento.begin(l, r, Memento<false>::flag_no_lock);
                for (int32_t j = 0; j < check_keys.size(); j++) {
                    REQUIRE_NE(it, memento.end());
                    REQUIRE_EQ(*it, check_keys[j]);
                    ++it;
                }
                REQUIRE_EQ(it, memento.end());
            }
        }
    }

    TEST_CASE("resizing") {
        const uint32_t n_elements = (1ULL << 12) * 0.95;
        const uint32_t rng_seed = 2;
        std::mt19937_64 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = 1ULL << 12;
        const uint32_t key_bits = 26;
        const uint32_t memento_bits = 32;

        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};
        memento.set_auto_resize(true);
        const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
        const uint8_t flags = Memento<false>::flag_no_lock;
        const uint32_t n_expansions = memento.get_num_fingerprint_bits();
        for (int32_t expansion = 0; expansion < n_expansions; expansion++) {
            for (int32_t i = 0; i < (n_elements << expansion); i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags), 0);

                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()),
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index)
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert({capped_hash, elem_memento, 0});
            }
            //assert_memento_contents(check_set, memento);
        }
    }

        TEST_CASE("resizing with payload") {
        const uint32_t n_elements = (1ULL << 12) * 0.95;
        const uint32_t rng_seed = 2;
        std::mt19937_64 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> check_set;
        const uint32_t payload_bits = 50;

        const float load_factor = 0.95;
        const uint32_t n_slots = 1ULL << 12;
        const uint32_t key_bits = 26;
        const uint32_t memento_bits = 32;

        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed, 0, payload_bits};
        memento.set_auto_resize(true);
        const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
        const uint8_t flags = Memento<false>::flag_no_lock;
        const uint32_t n_expansions = memento.get_num_fingerprint_bits();
        for (int32_t expansion = 0; expansion < n_expansions; expansion++) {
            for (int32_t i = 0; i < (n_elements << expansion); i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags, elem_memento), 0);

                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()),
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index)
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert({capped_hash, elem_memento, elem_memento});
            }
            //assert_memento_contents(check_set, memento);
        }
    }
}

TEST_SUITE("expandable memento") {
    const uint32_t seed = 1;

    TEST_CASE("allocation") {
        const uint32_t n_slots = 1000;
        const uint32_t key_bits = 20;
        const uint32_t memento_bits = 5;

        Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed};
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
            Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::None, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<true>::flag_key_is_hash | Memento<true>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix_hash = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix_hash, elem_memento, flags), 0);

                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()), 
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert({capped_hash, elem_memento, 0});
            }
            assert_memento_contents(check_set, memento);
        }


        SUBCASE("monte-carlo single insert") {
            Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<true>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags), 0);

                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()), 
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert({capped_hash, elem_memento, 0});
            }
            assert_memento_contents(check_set, memento);
        }


        SUBCASE("monte-carlo list insert") {
            Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<true>::flag_no_lock;

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

                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()), 
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                for (uint64_t memento : memento_list)
                    check_set.insert({capped_hash, memento, 0});
            }
            assert_memento_contents(check_set, memento);
        }
    }

    TEST_CASE("iterator") {
        const uint32_t n_elements = 100000;
        const uint32_t expansions = 4;
        const uint32_t rng_seed = 2;
        const uint32_t geometric_modulo = 3;
        std::mt19937 rng(rng_seed);
        std::multiset<std::pair<uint64_t, uint64_t>> check_set;
        std::map<uint64_t, std::vector<uint64_t>> prefix_sets;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;

        Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed};
        const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
        const uint8_t flags = Memento<true>::flag_no_lock;

        for (uint32_t expansion_count = 0; expansion_count < expansions; expansion_count++) {
            std::vector<uint64_t> current_prefix_pool;
            if (expansion_count > 0)
                for (auto it = prefix_sets.begin(); it != prefix_sets.end(); it++)
                    current_prefix_pool.push_back(it->first);
            for (uint32_t i = 0; i < n_elements; i++) {
                const uint64_t key_prefix = expansion_count > 0 ? current_prefix_pool[rng() % current_prefix_pool.size()]
                                                                : rng() & BITMASK(32);
                do {
                    const uint64_t key_memento = rng() & BITMASK(memento_bits);
                    prefix_sets[key_prefix].push_back(key_memento);
                    memento.insert(key_prefix, key_memento, flags);
                    i++;
                } while (rng() % geometric_modulo > 0);
                i--;
                std::sort(prefix_sets[key_prefix].begin(), prefix_sets[key_prefix].end());
            }

            for (auto it = prefix_sets.begin(); it != prefix_sets.end(); it++) {
                const uint64_t key_prefix = it->first;
                const uint64_t l_key = key_prefix << memento_bits;
                const uint64_t r_key = l_key | BITMASK(memento_bits);
                uint32_t check_ind = 0;
                auto memento_it = memento.begin(l_key, r_key);
                REQUIRE_NE(memento_it, memento.end());
                for (; memento_it != memento.end(); memento_it++) {
                    const uint64_t key = (key_prefix << memento_bits) | it->second[check_ind++];
                    REQUIRE_EQ(*memento_it, key);
                }
            }
        }
    }


    TEST_CASE("expansions") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_expansions = 4;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / (n_expansions * load_factor) + 5000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;

        Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed};
        const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
        const uint8_t flags = Memento<true>::flag_no_lock;
        for (int32_t i = 0; i < n_elements; i++) {
            const uint64_t elem = rng();
            const uint64_t elem_prefix = elem >> memento_bits;
            const uint64_t elem_memento = elem & BITMASK(memento_bits);
            REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags), 0);

            const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
            const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                << (32 - memento.get_original_quotient_bits()), 
                                                        n_slots);
            const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
            const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                            & BITMASK(hash_length)) | (1ULL << hash_length);
            check_set.insert({capped_hash, elem_memento, 0});
        }
        assert_memento_contents(check_set, memento);
    }
}

TEST_SUITE("expandable memento with payloads") {
    const uint32_t seed = 1;

    TEST_CASE("allocation") {
        const uint32_t n_slots = 1000;
        const uint32_t key_bits = 20;
        const uint32_t memento_bits = 5;
        const uint32_t payload_bits = 50;

        Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed, 0, payload_bits};
    }

    TEST_CASE("inserts") {
        const uint32_t n_elements = 1000000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> check_set;
        uint32_t payload_bits = 50;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;

        SUBCASE("monte-carlo no hashing") {
            Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::None, seed, 0, payload_bits};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_key_is_hash | Memento<false>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix_hash = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix_hash, elem_memento, flags, elem_memento), 0);

                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()),
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index)
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert({capped_hash, elem_memento, elem_memento});
            }
            assert_memento_contents(check_set, memento);
        }


        SUBCASE("monte-carlo single insert") {
            Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed, 0, payload_bits};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<true>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags, elem_memento), 0);

                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()),
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index)
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert({capped_hash, elem_memento, elem_memento});
            }
            assert_memento_contents(check_set, memento);
        }


        // currently not tested with payload since we don't support insert_mementos with payload
//        SUBCASE("monte-carlo list insert") {
//            Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed, 0, payload_bits};
//            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
//            const uint8_t flags = Memento<true>::flag_no_lock;
//
//            const uint32_t geometric_modulo = 5;
//            std::vector<uint64_t> memento_list;
//            uint32_t last_size = 0;
//            while (check_set.size() < n_elements) {
//                memento_list.clear();
//                const uint64_t elem_prefix = rng() >> memento_bits;
//                for (uint64_t memento = rng() % geometric_modulo; memento > 0; memento = rng() % geometric_modulo)
//                    memento_list.push_back(memento);
//                std::sort(memento_list.begin(), memento_list.end());
//                REQUIRE_GE(memento.insert_mementos(elem_prefix, memento_list.data(), memento_list.size(), flags), 0);
//
//                const uint64_t elem_prefix_hash = Memento<false>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
//                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
//                                                                    << (32 - memento.get_original_quotient_bits()),
//                                                            n_slots);
//                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
//                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index)
//                                                & BITMASK(hash_length)) | (1ULL << hash_length);
//                for (uint64_t memento : memento_list)
//                    check_set.insert({capped_hash, memento, 0});
//            }
//            assert_memento_contents(check_set, memento);
//        }
    }

    TEST_CASE("expansions") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_expansions = 4;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> check_set;
        const uint32_t payload_bits = 50;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / (n_expansions * load_factor) + 5000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;

        Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed, 0, payload_bits};
        const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
        const uint8_t flags = Memento<true>::flag_no_lock;
        for (int32_t i = 0; i < n_elements; i++) {
            const uint64_t elem = rng();
            const uint64_t elem_prefix = elem >> memento_bits;
            const uint64_t elem_memento = elem & BITMASK(memento_bits);
            REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags, elem_memento), 0);

            const uint64_t elem_prefix_hash = Memento<true>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
            const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                << (32 - memento.get_original_quotient_bits()),
                                                        n_slots);
            const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
            const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index)
                                            & BITMASK(hash_length)) | (1ULL << hash_length);
            check_set.insert({capped_hash, elem_memento, elem_memento});
        }
        assert_memento_contents(check_set, memento);
    }

    TEST_CASE("expansions with fingerprint duplication") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_expansions = 5;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / ((1ULL << n_expansions) * load_factor) + 5000;
        const uint32_t key_bits = 20;
        const uint32_t memento_bits = 5;

        Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed};
        const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
        const uint8_t flags = Memento<true>::flag_no_lock;
        for (int32_t i = 0; i < n_elements; i++) {
            const uint64_t elem = rng();
            const uint64_t elem_prefix = elem >> memento_bits;
            const uint64_t elem_memento = elem & BITMASK(memento_bits);
            REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags), 0);

            const uint64_t elem_prefix_hash = Memento<true>::MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
            const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                << (32 - memento.get_original_quotient_bits()),
                                                        n_slots);
            const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
            const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index)
                                            & BITMASK(hash_length)) | (1ULL << hash_length);
            check_set.insert({capped_hash, elem_memento, 0});
        }

        std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> extend_list;
        for (auto it : check_set)
            if (highbit_position(std::get<0>(it)) == key_bits)
                extend_list.push_back(it);
        for (auto it : extend_list) {
            check_set.erase(check_set.find(it));
            std::tuple<uint64_t, uint64_t, uint64_t> a = it;
            uint64_t a_first = std::get<0>(a);
            a_first &= BITMASK(key_bits);
            a_first |= 1ULL << (key_bits + 1);
            check_set.insert({a_first, std::get<1>(a), std::get<2>(a)});
            uint64_t b_first = a_first;
            b_first |= 1ULL << key_bits;
            check_set.insert({b_first, std::get<1>(a), std::get<2>(a)});
        }
        assert_memento_contents(check_set, memento);
    }
}

} // namespace memento


