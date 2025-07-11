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

#pragma once

#include <cstdint>
#include <iostream>
#include <argparse/argparse.hpp>
#include <string>
#include "bench_utils.hpp"

/**
 * This file contains a template class for running and measuring benchmarks of range filters.
 */
#define pass_fun(f) ([](auto... args){ return f(args...); })
#define pass_ref(fun) ([](auto& f, auto... args){ return fun(f, args...); })

#define start_timer(t) \
    auto t_start_##t = timer::now(); \

#define stop_timer(t) \
    auto t_end_##t = timer::now(); \
    test_out.add_measure(#t, std::chrono::duration_cast<std::chrono::milliseconds>(t_end_##t - t_start_##t).count());

const uint32_t expansion_count = 6;
const uint32_t expansion_timeout = 360000;

auto test_out = TestOutput();

auto test_verbose = true;
bool print_csv = false;
std::string csv_file = "";

template <typename InitFun, typename RangeFun, typename SizeFun, typename InsertFun, typename key_type, typename... Args>
void experiment(InitFun init_f, RangeFun range_f, InsertFun insert_f, SizeFun size_f, 
                const double param, InputKeys<key_type> &keys, Workload<key_type> &queries,
                Args... args)
{
    const uint64_t N = keys.size();
    const uint64_t n_queries = queries.size() / (expansion_count + 1);
    uint64_t current_dataset_size = N >> expansion_count;
    auto f = init_f(keys.begin(), keys.begin() + current_dataset_size, param, args...);

    std::cout << "[+] data structure constructed in " << test_out["build_time"] << "ms, starting queries" << std::endl;

    {
        std::string expansion_str = std::to_string(0);
        auto size = size_f(f);
        std::string name = "size_";
        name += expansion_str;
        test_out.add_measure(name, size);
        name = "bpk_";
        name += expansion_str;
        test_out.add_measure(name, TO_BPK(size, current_dataset_size));

        auto fp = 0, fn = 0;
        auto t_start_query_time = timer::now();
        std::cerr << "START QUERY PROCESS" << std::endl;
        for (uint64_t i = 0; i < n_queries; i++) {
            const auto [left, right, original_result] = queries[i];

            bool query_result = range_f(f, left, right);
            if (query_result && !original_result)
                fp++;
            else if (!query_result && original_result)
            {
                std::cerr << "[!] alert, found false negative!" << std::endl;
                fn++;
            }
        }
        auto t_end_query_time = timer::now();
        std::cerr << "DONE WITH QUERY PROCESS" << std::endl;
        name = "query_time_";
        name += expansion_str;
        test_out.add_measure(name,
                std::chrono::duration_cast<std::chrono::milliseconds>(t_end_query_time - t_start_query_time).count());

        name = "fpr_";
        name += expansion_str;
        test_out.add_measure(name, ((double)fp / n_queries));
        name = "false_neg_";
        name += expansion_str;
        test_out.add_measure(name, fn);
        name = "n_keys_";
        name += expansion_str;
        test_out.add_measure(name, current_dataset_size);
        name = "n_queries_";
        name += expansion_str;
        test_out.add_measure(name, n_queries);
        name = "false_positives_";
        name += expansion_str;
        test_out.add_measure(name, fp);
    }
    for (uint32_t expansion = 1; expansion <= expansion_count; expansion++) {
        std::string expansion_str = std::to_string(expansion);
        auto size = size_f(f);
        std::string name = "size_";
        name += expansion_str;
        test_out.add_measure(name, size);
        name = "bpk_";
        name += expansion_str;
        test_out.add_measure(name, TO_BPK(size, current_dataset_size));

        std::cerr << "START EXPANSION PROCESS" << std::endl;
        std::cerr << "current_dataset_size=" << current_dataset_size << std::endl;
        auto t_start_expansion_time = timer::now();
        for (uint32_t i = 0; i < current_dataset_size && i + current_dataset_size < N; i++) {
            //std::cerr << "current_dataset_size=" << current_dataset_size << " i=" << i << std::endl;
            insert_f(f, keys[current_dataset_size + i]);
            if (std::chrono::duration_cast<std::chrono::milliseconds>(timer::now() - t_start_expansion_time).count() > expansion_timeout) {
                std::cerr << "[!] expansion took too long (> " << expansion_timeout << " ms)" << std::endl;
                break;
            }
        }
        auto t_end_expansion_time = timer::now();
        name = "expansion_time_";
        name += expansion_str;
        auto time_measurement = std::chrono::duration_cast<std::chrono::milliseconds>(t_end_expansion_time - t_start_expansion_time).count();
        test_out.add_measure(name, time_measurement);
        if (time_measurement > expansion_timeout)
            break;
        current_dataset_size = (current_dataset_size * 2 < N ? current_dataset_size * 2 
                                                             : N);
        std::cerr << "DONE WITH EXPANSION PROCESS --- current_dataset_size=" << current_dataset_size << " vs. N=" << N << std::endl;

        auto fp = 0, fn = 0;
        auto t_start_query_time = timer::now();
        std::cerr << "START QUERY PROCESS" << std::endl;
        for (uint64_t i = expansion * n_queries; i < (expansion + 1) * n_queries; i++) {
            const auto [left, right, original_result] = queries[i];

            bool query_result = range_f(f, left, right);
            if (query_result && !original_result)
                fp++;
            else if (!query_result && original_result)
            {
                std::cerr << "[!] alert, found false negative!" << std::endl;
                fn++;
            }
        }
        std::cerr << "DONE WITH QUERY PROCESS" << std::endl;
        auto t_end_query_time = timer::now();
        name = "query_time_";
        name += expansion_str;
        test_out.add_measure(name,
                std::chrono::duration_cast<std::chrono::milliseconds>(t_end_query_time - t_start_query_time).count());

        name = "fpr_";
        name += expansion_str;
        test_out.add_measure(name, ((double)fp / n_queries));
        name = "false_neg_";
        name += expansion_str;
        test_out.add_measure(name, fn);
        name = "n_keys_";
        name += expansion_str;
        test_out.add_measure(name, current_dataset_size);
        name = "n_queries_";
        name += expansion_str;
        test_out.add_measure(name, n_queries);
        name = "false_positives_";
        name += expansion_str;
        test_out.add_measure(name, fp);
    }

    std::cout << "[+] test executed successfully, printing stats and closing." << std::endl;
}

argparse::ArgumentParser init_parser(const std::string &name)
{
    argparse::ArgumentParser parser(name);

    parser.add_argument("arg")
            .help("the main parameter of the ds (typically desired bpk o #suffix bits)")
            .scan<'g', double>();

    parser.add_argument("-w", "--workload")
            .help("pass the workload from file")
            .nargs(2, 3);

    parser.add_argument("-k", "--keys")
            .help("pass the keys from file")
            .nargs(1);

    parser.add_argument("--csv")
            .help("prints the output in csv")
            .nargs(1);

    parser.add_argument("--max-queries")
            .help("limits the maximum number of queries")
            .nargs(1)
            .scan<'i', int>();

    return parser;
}

std::tuple<InputKeys<uint64_t>, Workload<uint64_t>, double> read_parser_arguments(argparse::ArgumentParser &parser)
{
    auto arg = parser.get<double>("arg");

    auto keys_filename = parser.get<std::string>("keys");
    auto keys = (has_suffix(keys_filename, ".txt")) ? read_keys_from_file<uint64_t>(keys_filename)
                                                    : read_data_binary<uint64_t>(keys_filename);

    auto files = parser.get<std::vector<std::string>>("workload");

    Workload<uint64_t> queries;
    if (has_suffix(files[0], ".txt"))
        exit(0);
    else
    {
        auto left_q = read_data_binary<uint64_t>(files[0], false);
        auto right_q = read_data_binary<uint64_t>(files[1], false);

        if (files.size() == 3)
        {
            auto res_q = read_data_binary<int>(files[2], false);

            for (auto i = 0; i < left_q.size(); i++)
                queries.emplace_back(left_q[i], right_q[i], res_q[i]);
        }
        else
            for (auto i = 0; i < left_q.size(); i++)
                queries.emplace_back(left_q[i], right_q[i], false);
    }

    if (keys.empty())
        throw std::runtime_error("error, keys file is empty.");
    if (queries.empty())
        throw std::runtime_error("error, queries file is empty.");

    if (auto max_queries = parser.present<int>("--max-queries"))
    {
        if (*max_queries < queries.size())
            queries.resize(*max_queries);
    }

    if (auto arg_csv = parser.present<std::string>("--csv"))
    {
        print_csv = true;
        csv_file = *arg_csv;
    }

    std::cout << "[+] nkeys=" << keys.size() << ", nqueries=" << queries.size() << std::endl;
    std::cout << "[+] keys and queries loaded, starting test." << std::endl;
    return std::make_tuple(keys, queries, arg);
}

void print_test()
{
    if (test_verbose)
        test_out.print();

    if (print_csv)
    {
        std::cout << "[+] writing results in " << csv_file << std::endl;
        std::filesystem::path path_csv(csv_file);
        std::string s = (!std::filesystem::exists(path_csv) || std::filesystem::is_empty(path_csv))
                ? test_out.to_csv(true) : test_out.to_csv(false);
        std::ofstream outFile(path_csv, std::ios::app);
        outFile << s;
        outFile.close();
    }
}
