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

#include "../bench_template_b_tree.hpp"
#include <algorithm>
#include <cstdint>
#include <iterator>

/**
 * This file contains the benchmark for the Grafite filter.
 */

template <typename t_itr, typename... Args>
inline void *init_none(const t_itr begin, const t_itr end, const double bpk, Args... args)
{
    start_timer(build_time);
    stop_timer(build_time);
    return nullptr;
}

template <typename value_type>
inline bool query_none(void *f, const value_type l, const value_type r)
{
    return true;
}

template <typename value_type>
inline void insert_none(void *f, const value_type key)
{
    return;
}


inline size_t size_none(void *f)
{
    return 0;
}

int main(int argc, char const *argv[])
{
    auto parser = init_parser("bench-b-tree-none");

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
    experiment(pass_fun(init_none), pass_ref(query_none), pass_ref(insert_none),
               pass_ref(size_none), arg, keys, queries);

    print_test();

    return 0;
}


