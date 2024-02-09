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

#include "../bench_template.hpp"
#include "memento.h"
#include "memento_int.h"
#include <algorithm>
#include <cstdint>
#include <iterator>

/**
 * This file contains the benchmark for the Grafite filter.
 */

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

    const uint64_t seed = std::chrono::steady_clock::now().time_since_epoch().count();
    const uint64_t max_range_size = *std::max_element(query_lengths.begin(), query_lengths.end());
    const uint64_t n_items = std::distance(begin, end);
    const double load_factor = 0.95;
    const uint64_t n_slots = n_items / load_factor;
    uint32_t memento_bits = 1;
    while ((1ULL << memento_bits) <= max_range_size)
        memento_bits++;
    const uint32_t fingerprint_size = round(bpk * load_factor - memento_bits - 2.125);
    uint32_t key_size = 0;
    while ((1ULL << key_size) <= n_slots)
        key_size++;
    key_size += fingerprint_size;

    QF *qf = (QF *) malloc(sizeof(QF));
    qf_malloc(qf, n_slots, key_size, memento_bits, QF_HASH_DEFAULT, seed);

    start_timer(build_time);

    const uint64_t memento_mask = (1ULL << memento_bits) - 1;
    uint64_t prefix = (*begin) >> memento_bits;
    uint64_t memento_list[256];
    uint32_t prefix_set_size = 1;
    memento_list[0] = (*begin) & memento_mask;
    for (t_itr it = begin + 1; it != end; it++) {
        const uint64_t new_prefix = (*it) >> memento_bits;
        if (new_prefix == prefix)
            memento_list[prefix_set_size++] = (*it) & memento_mask;
        else {
            qf_insert_mementos(qf, prefix, memento_list, prefix_set_size, QF_NO_LOCK);
            prefix = new_prefix;
            prefix_set_size = 1;
            memento_list[0] = (*it) & memento_mask;
        }
    }
    qf_insert_mementos(qf, prefix, memento_list, prefix_set_size, QF_NO_LOCK);

    stop_timer(build_time);

    return qf;
}

template <typename value_type>
inline bool query_memento(QF *f, const value_type left, const value_type right)
{
    value_type l_key = left >> f->metadata->memento_bits;
    value_type l_memento = left & ((1ULL << f->metadata->memento_bits) - 1);
    value_type r_key = right >> f->metadata->memento_bits;
    value_type r_memento = right & ((1ULL << f->metadata->memento_bits) - 1);
    return qf_range_query(f, l_key, l_memento, r_key, r_memento, QF_NO_LOCK);
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

    experiment(pass_fun(init_memento), pass_ref(query_memento),
                pass_ref(size_memento), arg, keys, queries, queries);

    print_test();

    return 0;
}


