/**
 * @file Tests for Memento filter's C++ implementation
 * @author Navid Eslami
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <tuple>
#include <vector>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "memento.hpp"
#include "doctest.h"

namespace memento {
void assert_memento_contents(std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>>& hash_set,
                             Memento<false>& filter) {
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


void assert_rsqf_contents(std::multiset<std::pair<uint64_t, uint64_t>>& hash_set,
                          Memento<false>& filter) {
    std::multiset<std::pair<uint64_t, uint64_t>> filter_hash_set;

    auto filter_it = filter.hash_begin();
    uint64_t key, mementos[1024];
    for (; filter_it != filter.hash_end(); filter_it++) {
        filter_it.get(key, mementos);
        const uint64_t fingerprint = key >> filter.get_bucket_index_hash_size();
        const uint64_t bucket_index = key & BITMASK(filter.get_bucket_index_hash_size());
        filter_hash_set.insert({bucket_index, fingerprint});
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


void assert_memento_contents(std::multiset<std::pair<uint64_t, uint64_t>>& hash_set,
                             Memento<false>& filter) {
    std::multiset<std::pair<uint64_t, uint64_t>> filter_hash_set;

    auto filter_it = filter.hash_begin();
    uint64_t key, memento_count, mementos[1024];
    for (; filter_it != filter.hash_end(); filter_it++) {
        memento_count = filter_it.get(key, mementos);
        for (int32_t i = 0; i < memento_count; i++)
            filter_hash_set.insert({key | (1ULL << filter.get_num_key_bits()), mementos[i]});
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


void assert_rsqf_contents(std::multiset<uint64_t>& hash_set, Memento<false>& filter) {
    std::multiset<uint64_t> filter_hash_set;

    auto filter_it = filter.hash_begin();
    uint64_t key, memento_count, mementos[1024];
    for (; filter_it != filter.hash_end(); filter_it++) {
        memento_count = filter_it.get(key, mementos);
        for (int32_t i = 0; i < memento_count; i++)
            filter_hash_set.insert(key | (1ULL << filter.get_num_key_bits()));
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


void assert_memento_contents(std::multiset<std::pair<uint64_t, uint64_t>>& hash_set,
                             Memento<true>& filter) {
    std::multiset<std::pair<uint64_t, uint64_t>> filter_hash_set;

    auto filter_it = filter.hash_begin();
    uint64_t key, memento_count, mementos[1024];
    for (; filter_it != filter.hash_end(); filter_it++) {
        memento_count = filter_it.get(key, mementos);
        for (int32_t i = 0; i < memento_count; i++)
            filter_hash_set.insert({key, mementos[i]});
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


void assert_rsqf_contents(std::multiset<uint64_t>& hash_set, Memento<true>& filter) {
    std::multiset<uint64_t> filter_hash_set;

    auto filter_it = filter.hash_begin();
    uint64_t key, memento_count, mementos[1024];
    for (; filter_it != filter.hash_end(); filter_it++) {
        memento_count = filter_it.get(key, mementos);
        for (int32_t i = 0; i < memento_count; i++)
            filter_hash_set.insert(key);
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
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::None, seed};
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
                check_set.insert({bucket_index, fingerprint, elem_memento});
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


    TEST_CASE("deletes") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_deletes = 500000;
        const uint32_t delete_insert_ratio = 5;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> check_set;

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
                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> quotient_bits) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                while (rng() % geometric_modulo == 0) {
                    const uint64_t elem_memento = rng() & BITMASK(memento_bits);
                    REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags), 0);
                    existing_keys.push_back((elem_prefix << memento_bits) | elem_memento);
                    check_set.insert({bucket_index, fingerprint, elem_memento});
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

                    const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                    const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                              n_slots);
                    const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                    & BITMASK(memento.get_num_fingerprint_bits());
                    auto search_it = check_set.find({bucket_index, fingerprint, elem_memento});
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

                    const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                    const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                              n_slots);
                    const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                    & BITMASK(memento.get_num_fingerprint_bits());
                    check_set.insert({bucket_index, fingerprint, elem_memento});
                }
            }
            assert_memento_contents(check_set, memento);
        }
    }


    TEST_CASE("updates") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_updates = 500000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::tuple<uint64_t, uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;
        const uint32_t geometric_modulo = 3;

        SUBCASE("monte-carlo inserts and updates") {
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_no_lock;
            std::vector<uint64_t> existing_keys;
            while (existing_keys.size() < n_elements) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> quotient_bits) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                while (rng() % geometric_modulo == 0) {
                    const uint64_t elem_memento = rng() & BITMASK(memento_bits);
                    REQUIRE_GE(memento.insert(elem_prefix, elem_memento, flags), 0);
                    existing_keys.push_back((elem_prefix << memento_bits) | elem_memento);
                    check_set.insert({bucket_index, fingerprint, elem_memento});
                }
            }
            assert_memento_contents(check_set, memento);

            for (uint32_t updates = 0; updates < n_updates; updates++) {
                const uint32_t update_rank = rng() % existing_keys.size();
                const uint64_t elem = existing_keys[update_rank];
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_memento = elem & BITMASK(memento_bits);
                uint64_t new_elem_memento = rng() & BITMASK(memento_bits);
                if (elem_memento == new_elem_memento)
                    new_elem_memento ^= 1;
                REQUIRE_EQ(memento.update_single(elem_prefix, elem_memento, new_elem_memento, flags), 0);
                existing_keys[update_rank] = (elem & (~BITMASK(memento_bits))) | new_elem_memento;

                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                auto search_it = check_set.find({bucket_index, fingerprint, elem_memento});
                REQUIRE(search_it != check_set.end());
                check_set.erase(search_it);
                check_set.insert({bucket_index, fingerprint, new_elem_memento});
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
            keys.resize(std::unique(keys.begin(), keys.end()) - keys.begin());

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
            keys.resize(std::unique(keys.begin(), keys.end()) - keys.begin());

            std::vector<std::pair<uint64_t, uint64_t>> queries;
            for (int32_t i = 0; i < n_queries; i++) {
                const uint32_t pos = rng() % keys.size();
                const uint64_t sample_range_l = keys[pos];
                const uint64_t sample_range_r = pos != keys.size() - 1 ? keys[pos + 1] : std::numeric_limits<uint64_t>::max();
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
                memento.insert(key_prefix, i, Memento<false>::flag_no_lock);

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

    TEST_CASE("large mementos") {
        const uint32_t n_elements = 1000;
        const uint32_t seed = 1;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 40;
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

    TEST_CASE("resizing") {
        const uint32_t n_elements = (1ULL << 12) * 0.95;
        const uint32_t rng_seed = 2;
        std::mt19937_64 rng(rng_seed);
        std::multiset<std::pair<uint64_t, uint64_t>> check_set;

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

                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()), 
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert({capped_hash, elem_memento});
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
        std::multiset<std::pair<uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 5;

        SUBCASE("monte-carlo no hashing") {
            Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::None, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_key_is_hash | Memento<false>::flag_no_lock;
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
                check_set.insert({capped_hash, elem_memento});
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

                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()), 
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert({capped_hash, elem_memento});
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

                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()), 
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                for (uint64_t memento : memento_list)
                    check_set.insert({capped_hash, memento});
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
        std::multiset<std::pair<uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / ((1ULL << n_expansions) * load_factor) + 5000;
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

            const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
            const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                << (32 - memento.get_original_quotient_bits()), 
                                                        n_slots);
            const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
            const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                            & BITMASK(hash_length)) | (1ULL << hash_length);
            check_set.insert({capped_hash, elem_memento});
        }
        assert_memento_contents(check_set, memento);
    }

    TEST_CASE("expansions with fingerprint duplication") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_expansions = 5;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::pair<uint64_t, uint64_t>> check_set;

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

            const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
            const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                << (32 - memento.get_original_quotient_bits()), 
                                                        n_slots);
            const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
            const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                            & BITMASK(hash_length)) | (1ULL << hash_length);
            check_set.insert({capped_hash, elem_memento});
        }

        std::vector<std::pair<uint64_t, uint64_t>> extend_list;
        for (auto it : check_set)
            if (highbit_position(it.first) == key_bits)
                extend_list.push_back(it);
        for (auto it : extend_list) {
            check_set.erase(check_set.find(it));
            std::pair<uint64_t, uint64_t> a = it;
            a.first &= BITMASK(key_bits);
            a.first |= 1ULL << (key_bits + 1);
            std::pair<uint64_t, uint64_t> b = a;
            b.first |= 1ULL << key_bits;
            check_set.insert(a);
            check_set.insert(b);
        }
        assert_memento_contents(check_set, memento);
    }


    TEST_CASE("updates") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_expansions = 4;
        const uint32_t n_updates = 500000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::pair<uint64_t, uint64_t>> check_set;
        std::vector<uint64_t> elems;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / ((1ULL << n_expansions) * load_factor) + 5000;
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

            const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
            const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                << (32 - memento.get_original_quotient_bits()), 
                                                        n_slots);
            const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
            const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                            & BITMASK(hash_length)) | (1ULL << hash_length);
            check_set.insert({capped_hash, elem_memento});
            elems.push_back(elem);
        }
        assert_memento_contents(check_set, memento);

        for (uint32_t updates = 0; updates < n_updates; updates++) {
            const uint32_t update_rank = rng() % elems.size();
            const uint64_t elem = elems[update_rank];
            const uint64_t elem_prefix = elem >> memento_bits;
            const uint64_t elem_memento = elem & BITMASK(memento_bits);
            uint64_t new_elem_memento = rng() & BITMASK(memento_bits);
            if (elem_memento == new_elem_memento)
                new_elem_memento ^= 1;
            REQUIRE_EQ(memento.update_single(elem_prefix, elem_memento, new_elem_memento, flags), 0);
            elems[update_rank] = (elem & (~BITMASK(memento_bits))) | new_elem_memento;

            const uint32_t max_hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
            bool found = false;
            for (uint32_t hash_length = max_hash_length; !found && hash_length > 0; hash_length--) {
                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                << (32 - memento.get_original_quotient_bits()), 
                                                            n_slots);
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                                            & BITMASK(hash_length)) | (1ULL << hash_length);
                auto search_it = check_set.find({capped_hash, elem_memento});
                if (search_it != check_set.end()) {
                    check_set.erase(search_it);
                    check_set.insert({capped_hash, new_elem_memento});
                    found = true;
                }
            }
            REQUIRE(found);
        }
        assert_memento_contents(check_set, memento);
    }
}


TEST_SUITE("no mementos memento") {
    const uint32_t seed = 12345;

    TEST_CASE("allocation") {
        const uint32_t n_slots = 1000;
        const uint32_t key_bits = 20;
        const uint32_t memento_bits = 0;

        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};
    }


    TEST_CASE("inserts") {
        const uint32_t n_elements = 1000000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::pair<uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 0;

        SUBCASE("monte-carlo no hashing") {
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::None, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_key_is_hash | Memento<false>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix_hash = elem >> memento_bits;
                REQUIRE_GE(memento.insert(elem_prefix_hash, 0, flags), 0);

                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                check_set.insert({bucket_index, fingerprint});
            }
            assert_rsqf_contents(check_set, memento);
        }


        SUBCASE("monte-carlo single insert") {
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                REQUIRE_GE(memento.insert(elem_prefix, 0, flags), 0);

                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                check_set.insert({bucket_index, fingerprint});
            }
            assert_rsqf_contents(check_set, memento);
        }
    }


    TEST_CASE("deletes") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_deletes = 500000;
        const uint32_t delete_insert_ratio = 5;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<std::pair<uint64_t, uint64_t>> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 0;

        SUBCASE("monte-carlo deletes and inserts") {
            Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_no_lock;
            std::vector<uint64_t> existing_keys, deleted_keys;
            while (existing_keys.size() < n_elements) {
                const uint64_t elem = rng();
                const uint64_t elem_prefix = elem >> memento_bits;
                const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                          n_slots);
                const uint64_t fingerprint = (elem_prefix_hash >> quotient_bits) 
                                                & BITMASK(memento.get_num_fingerprint_bits());
                REQUIRE_GE(memento.insert(elem_prefix, 0, flags), 0);
                existing_keys.push_back(elem_prefix);
                check_set.insert({bucket_index, fingerprint});
            }
            assert_rsqf_contents(check_set, memento);

            uint64_t deletes_done = 0;
            while (deletes_done < n_deletes) {
                const uint32_t action = rng() % delete_insert_ratio;
                if (action > 0 || deleted_keys.empty()) { // Delete
                    const uint32_t delete_rank = rng() % existing_keys.size();
                    const uint64_t elem = existing_keys[delete_rank];
                    const uint64_t elem_prefix = elem >> memento_bits;
                    REQUIRE_GE(memento.delete_single(elem_prefix, 0, flags), 0);
                    std::swap(existing_keys[existing_keys.size() - 1], existing_keys[delete_rank]);
                    existing_keys.pop_back();
                    deleted_keys.push_back(elem);
                    deletes_done++;

                    const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                    const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                              n_slots);
                    const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                    & BITMASK(memento.get_num_fingerprint_bits());
                    auto search_it = check_set.find({bucket_index, fingerprint});
                    REQUIRE(search_it != check_set.end());
                    check_set.erase(search_it);
                }
                else { // Insert
                    const uint32_t insert_rank = rng() % deleted_keys.size();
                    const uint64_t elem = deleted_keys[insert_rank];
                    const uint64_t elem_prefix = elem >> memento_bits;
                    REQUIRE_GE(memento.insert(elem_prefix, 0, flags), 0);
                    existing_keys.push_back(elem);
                    std::swap(deleted_keys[deleted_keys.size() - 1], deleted_keys[insert_rank]);
                    deleted_keys.pop_back();

                    const uint64_t elem_prefix_hash = MurmurHash64A(&elem_prefix, sizeof(elem_prefix), seed);
                    const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(quotient_bits)) << (32 - quotient_bits),
                                                              n_slots);
                    const uint64_t fingerprint = (elem_prefix_hash >> memento.get_bucket_index_hash_size()) 
                                                    & BITMASK(memento.get_num_fingerprint_bits());
                    check_set.insert({bucket_index, fingerprint});
                }
            }
            assert_rsqf_contents(check_set, memento);
        }
    }


    TEST_CASE("queries") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_queries = 1000000;
        const uint32_t rng_seed = 2;
        std::mt19937_64 rng(rng_seed);

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 0;
        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};

        const uint64_t max_range_size = 6;
        const float expected_point_fpr = std::pow(2, -memento.get_num_fingerprint_bits());
        const float expected_range_fpr = max_range_size * expected_point_fpr;

        SUBCASE("point: no false negatives") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                keys.push_back(key);
                memento.insert(key, 0, Memento<false>::flag_no_lock);
            }
            for (uint64_t key : keys)
                REQUIRE(memento.point_query(key, 0, Memento<false>::flag_no_lock));
        }


        SUBCASE("point: false positive rate") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                keys.push_back(key);
                memento.insert(key, 0, Memento<false>::flag_no_lock);
            }
            std::sort(keys.begin(), keys.end());
            keys.resize(std::unique(keys.begin(), keys.end()) - keys.begin());

            uint64_t fp_count = 0;
            for (int32_t i = 0; i < n_queries; i++) {
                const uint64_t key = rng();
                if (std::binary_search(keys.begin(), keys.end(), key)) {
                    i--;
                    continue;
                }
                fp_count += memento.point_query(key, 0, Memento<false>::flag_no_lock);
            }
            REQUIRE_LE(static_cast<float>(fp_count) / n_queries, 1.1 * expected_point_fpr);
        }


        SUBCASE("range: no false negatives") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                keys.push_back(key);
                memento.insert(key, 0, Memento<false>::flag_no_lock);
            }
            std::sort(keys.begin(), keys.end());
            keys.resize(std::unique(keys.begin(), keys.end()) - keys.begin());

            std::vector<std::pair<uint64_t, uint64_t>> queries;
            while (queries.size() < n_queries) {
                const uint64_t l_offset = rng() % max_range_size;
                const uint64_t r_offset = rng() % (max_range_size - l_offset);
                uint32_t pos;
                do {
                    pos = rng() % keys.size();
                } while (keys[pos] < l_offset || keys[pos] > std::numeric_limits<uint64_t>::max() - r_offset);
                const uint64_t l = keys[pos] - l_offset;
                const uint64_t r = keys[pos] + r_offset;
                queries.push_back({l, r});
            }

            for (auto [l, r] : queries)
                REQUIRE(memento.range_query(l, 0, r, 0, Memento<false>::flag_no_lock));
        }


        SUBCASE("range: false positive rate") {
            std::vector<uint64_t> keys;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t key = rng();
                keys.push_back(key);
                memento.insert(key, 0, Memento<false>::flag_no_lock);
            }
            std::sort(keys.begin(), keys.end());
            keys.resize(std::unique(keys.begin(), keys.end()) - keys.begin());

            std::vector<std::pair<uint64_t, uint64_t>> queries;
            for (int32_t i = 0; i < n_queries; i++) {
                const uint32_t pos = rng() % keys.size();
                const uint64_t sample_range_l = keys[pos];
                const uint64_t sample_range_r = pos != keys.size() - 1 ? keys[pos + 1] : std::numeric_limits<uint64_t>::max();
                uint64_t l = sample_range_l + rng() % (sample_range_r - sample_range_l);
                uint64_t r = l + rng() % max_range_size;
                if (l > r)
                    std::swap(l, r);
                queries.push_back({l, r});
            }

            uint64_t fp_count = 0;
            for (auto [l, r] : queries)
                fp_count += memento.range_query(l, 0, r, 0, Memento<false>::flag_no_lock);
            REQUIRE_LE(static_cast<float>(fp_count) / n_queries, 1.1 * expected_range_fpr);
        }
    }

    TEST_CASE("resizing") {
        const uint32_t n_elements = (1ULL << 12) * 0.95;
        const uint32_t rng_seed = 2;
        std::mt19937_64 rng(rng_seed);
        std::multiset<uint64_t> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = 1ULL << 12;
        const uint32_t key_bits = 26;
        const uint32_t memento_bits = 0;

        Memento<false> memento{n_slots, key_bits, memento_bits, Memento<false>::hashmode::Default, seed};
        memento.set_auto_resize(true);
        const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
        const uint8_t flags = Memento<false>::flag_no_lock;
        const uint32_t n_expansions = memento.get_num_fingerprint_bits() - 1;
        for (int32_t expansion = 0; expansion < n_expansions; expansion++) {
            for (int32_t i = 0; i < (n_elements << expansion); i++) {
                const uint64_t elem = rng();
                REQUIRE_GE(memento.insert(elem, 0, flags), 0);

                const uint64_t elem_hash = MurmurHash64A(&elem, sizeof(elem), seed);
                const uint64_t bucket_index = fast_reduce((elem_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()), 
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert(capped_hash);
            }
            assert_rsqf_contents(check_set, memento);
        }
    }
}


TEST_SUITE("no mementos expandable memento") {
    const uint32_t seed = 1;

    TEST_CASE("allocation") {
        const uint32_t n_slots = 1000;
        const uint32_t key_bits = 20;
        const uint32_t memento_bits = 0;

        Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed};
    }

    TEST_CASE("inserts") {
        const uint32_t n_elements = 1000000;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<uint64_t> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / load_factor + 10000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 0;

        SUBCASE("monte-carlo no hashing") {
            Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::None, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<false>::flag_key_is_hash | Memento<false>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem_prefix_hash = rng();
                REQUIRE_GE(memento.insert(elem_prefix_hash, 0, flags), 0);

                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()), 
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert(capped_hash);
            }
            assert_rsqf_contents(check_set, memento);
        }


        SUBCASE("monte-carlo single insert") {
            Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed};
            const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
            const uint8_t flags = Memento<true>::flag_no_lock;
            for (int32_t i = 0; i < n_elements; i++) {
                const uint64_t elem = rng();
                REQUIRE_GE(memento.insert(elem, 0, flags), 0);

                const uint64_t elem_prefix_hash = MurmurHash64A(&elem, sizeof(elem), seed);
                const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                    << (32 - memento.get_original_quotient_bits()), 
                                                            n_slots);
                const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
                const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                                & BITMASK(hash_length)) | (1ULL << hash_length);
                check_set.insert(capped_hash);
            }
            assert_rsqf_contents(check_set, memento);
        }
    }


    TEST_CASE("expansions") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_expansions = 4;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<uint64_t> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / ((1ULL << n_expansions) * load_factor) + 5000;
        const uint32_t key_bits = 32;
        const uint32_t memento_bits = 0;

        Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed};
        const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
        const uint8_t flags = Memento<true>::flag_no_lock;
        for (int32_t i = 0; i < n_elements; i++) {
            const uint64_t elem = rng();
            REQUIRE_GE(memento.insert(elem, 0, flags), 0);

            const uint64_t elem_prefix_hash = MurmurHash64A(&elem, sizeof(elem), seed);
            const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                << (32 - memento.get_original_quotient_bits()), 
                                                        n_slots);
            const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
            const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                            & BITMASK(hash_length)) | (1ULL << hash_length);
            check_set.insert(capped_hash);
        }
        assert_rsqf_contents(check_set, memento);
    }

    TEST_CASE("expansions with fingerprint duplication") {
        const uint32_t n_elements = 1000000;
        const uint32_t n_expansions = 5;
        const uint32_t rng_seed = 2;
        std::mt19937 rng(rng_seed);
        std::multiset<uint64_t> check_set;

        const float load_factor = 0.95;
        const uint32_t n_slots = n_elements / ((1ULL << n_expansions) * load_factor) + 5000;
        const uint32_t key_bits = 20;
        const uint32_t memento_bits = 0;

        Memento<true> memento{n_slots, key_bits, memento_bits, Memento<true>::hashmode::Default, seed};
        const uint32_t quotient_bits = memento.get_bucket_index_hash_size();
        const uint8_t flags = Memento<true>::flag_no_lock;
        for (int32_t i = 0; i < n_elements; i++) {
            const uint64_t elem = rng();
            REQUIRE_GE(memento.insert(elem, 0, flags), 0);

            const uint64_t elem_prefix_hash = MurmurHash64A(&elem, sizeof(elem), seed);
            const uint64_t bucket_index = fast_reduce((elem_prefix_hash & BITMASK(memento.get_original_quotient_bits()))
                                                                << (32 - memento.get_original_quotient_bits()), 
                                                        n_slots);
            const uint32_t hash_length = memento.get_bucket_index_hash_size() + memento.get_num_fingerprint_bits();
            const uint64_t capped_hash = (((elem_prefix_hash & ~BITMASK(memento.get_original_quotient_bits())) | bucket_index) 
                                            & BITMASK(hash_length)) | (1ULL << hash_length);
            check_set.insert(capped_hash);
        }

        std::vector<uint64_t> extend_list;
        for (auto it : check_set)
            if (highbit_position(it) == key_bits)
                extend_list.push_back(it);
        for (auto it : extend_list) {
            check_set.erase(check_set.find(it));
            uint64_t a = it;
            a &= BITMASK(key_bits);
            a |= 1ULL << (key_bits + 1);
            uint64_t b = a;
            b |= 1ULL << key_bits;
            check_set.insert(a);
            check_set.insert(b);
        }
        assert_rsqf_contents(check_set, memento);
    }
}

} // namespace memento


