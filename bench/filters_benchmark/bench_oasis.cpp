/*
 * This file is part of Memento filter <>.
 * Copyright (C) 2023 Navid Eslami.
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
#include "../include/Oasis-RangeFilter/src/include/oasis_plus.h"
/**
 * This file contains the benchmark for the Oasis filter.
 */

const int block_size = 150;

template <typename t_itr>
inline oasis_plus::OasisPlus init_oasis(const t_itr begin, const t_itr end, const double bpk)
{
    std::vector<typename t_itr::value_type> keys(begin, end);
    start_timer(build_time);
    oasis_plus::OasisPlus f(bpk, block_size, keys);
    stop_timer(build_time);
    return f;
}

template <typename value_type>
inline bool query_oasis(oasis_plus::OasisPlus &f, const value_type left, const value_type right)
{
    return f.query(left, right);
}

template <typename value_type>
inline size_t size_oasis(oasis_plus::OasisPlus &f)
{
    return f.size();
}

int main(int argc, char const *argv[])
{
    auto parser = init_parser("bench-oasis");
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
    experiment(pass_fun(init_oasis),pass_ref(query_oasis),
               pass_ref(size_oasis<uint64_t>), arg, keys, queries);
    print_test();

    return 0;
}
