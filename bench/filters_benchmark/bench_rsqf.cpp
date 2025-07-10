/*
 * This file is part of Memento Filter <https://github.com/n3slami/Memento_Filter>.
 * Copyright (C) 2024 Navid Eslami.
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
#include <iostream>
#include <iterator>
#include <boost/sort/sort.hpp>

#include "../bench_template.hpp"
#include "../include/cqf/include/gqf.h"
#include "../include/cqf/include/gqf_int.h"

/**
 * This file contains the benchmark for Memento filter.
 */

template <typename t_itr, typename... Args>
inline QF *init_rsqf(const t_itr begin, const t_itr end, const double bpk, Args... args)
{
    const uint64_t n_items = std::distance(begin, end);
    //const uint64_t seed = std::chrono::steady_clock::now().time_since_epoch().count();
    const uint64_t seed = 1380;
    const double load_factor = 0.95;
    uint64_t n_slots = n_items / load_factor + std::sqrt(n_items);
    const uint32_t fingerprint_size = round(bpk * load_factor - 2.125);
    uint32_t key_size = 0;
    while ((1ULL << key_size) <= n_slots)
        key_size++;
    n_slots = 1ULL << key_size;
    key_size += fingerprint_size;
    std::cerr << "fingerprint_size=" << fingerprint_size << std::endl;

    QF *qf = (QF *) malloc(sizeof(QF));
    qf_malloc(qf, n_slots, key_size, 0, QF_HASH_DEFAULT, seed);
    qf_set_auto_resize(qf, true);

    start_timer(build_time);

    for (auto it = begin; it != end; it++) {
        qf_insert(qf, *it, 0, 1, QF_HASH_DEFAULT);
    }

    stop_timer(build_time);

    return qf;
}

template <typename value_type>
inline bool query_rsqf(QF *f, const value_type left, const value_type right)
{
    if (left != right)
        exit(1);
    value_type res;
    return qf_query(f, left, &res, QF_HASH_DEFAULT);
}

inline size_t size_rsqf(QF *f)
{
    return qf_get_total_size_in_bytes(f);
}

int main(int argc, char const *argv[])
{
    auto parser = init_parser("bench-rsqf");

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

    experiment(pass_fun(init_rsqf), pass_ref(query_rsqf), 
                pass_ref(size_rsqf), arg, keys, queries);

    print_test();

    return 0;
}




