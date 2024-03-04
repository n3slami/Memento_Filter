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

#pragma once

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <argparse/argparse.hpp>
#include <wiredtiger.h>

#include "bench_utils.hpp"
#include "include/bigint.hpp"

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

const uint32_t expansion_count = 3;

auto test_out = TestOutput();

static const char *wt_home = "./wt_database_home";
const uint32_t max_schema_len = 128;
const uint32_t max_conn_config_len = 128;
const int default_key_len = 128, default_val_len = 128;
const std::string default_buffer_pool_size = "1GB";
uint64_t key_len, val_len;
std::string buffer_pool_size = default_buffer_pool_size;

static uint64_t optimizer_hack = 0;

bool test_verbose = true;
bool print_csv = false;
std::string csv_file = "";

static inline void error_check(int ret)
{
    if (ret != 0) {
        std::cerr << "WiredTiger Error: " << wiredtiger_strerror(ret) << std::endl;
        exit(ret);
    }
}

static inline void insert_kv(WT_CURSOR *cursor, char *key, char *value)
{
    cursor->set_key(cursor, key);
    cursor->set_value(cursor, value);
    error_check(cursor->insert(cursor));
}

static inline void fetch_range_from_db(WT_CURSOR *cursor, SimpleBigInt &l, SimpleBigInt &r)
{
    error_check(cursor->reset(cursor));
    cursor->set_key(cursor, (char *) l.num);
    error_check(cursor->bound(cursor, "action=set,bound=lower,inclusive=true"));
    cursor->set_key(cursor, (char *) r.num);
    error_check(cursor->bound(cursor, "action=set,bound=upper,inclusive=true"));

    uint32_t x = 1;
    while ((cursor->next(cursor)) == 0) {
        x ^= 1;
    }
    optimizer_hack += x;
}

template <typename InitFun, typename RangeFun, typename SizeFun, typename InsertFun, typename key_type, typename... Args>
void experiment(InitFun init_f, RangeFun range_f, InsertFun insert_f, SizeFun size_f, 
                const double param, InputKeys<key_type> &keys, Workload<key_type> &queries,
                Args... args)
{
    WT_CONNECTION *conn;
    WT_SESSION *session;
    WT_CURSOR *cursor;
    char table_schema[max_schema_len];
    char connection_config[max_conn_config_len];

    sprintf(table_schema, "key_format=%lds,value_format=%lds", key_len, val_len);
    sprintf(connection_config, "create,statistics=(all),direct_io=[data],cache_size=%s", buffer_pool_size.c_str());

    if (std::filesystem::exists(wt_home))
        std::filesystem::remove_all(wt_home);
    std::filesystem::create_directory(wt_home);

    error_check(wiredtiger_open(wt_home, NULL, connection_config, &conn));
    error_check(conn->open_session(conn, NULL, NULL, &session));
    error_check(session->create(session, "table:access", table_schema));
    error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
    std::cout << "[+] WiredTiger initialized" << std::endl;

    SimpleBigInt big_int_k(key_len), big_int_v(val_len);
    SimpleBigInt big_int_l(key_len), big_int_r(key_len);
    const uint64_t N = keys.size();
    const uint64_t n_queries = queries.size() / (expansion_count + 1);
    uint64_t current_dataset_size = N >> expansion_count;
    error_check(cursor->reset(cursor));
    for (uint64_t i = 0; i < current_dataset_size; i++) {
        big_int_k = keys[i];
        big_int_v.randomize();
        insert_kv(cursor, (char *) big_int_k.num, (char *) big_int_v.num);
    }
    auto f = init_f(keys.begin(), keys.begin() + current_dataset_size, param, args...);

    {
        std::string expansion_str = "0";
        auto size = size_f(f);
        std::string name = "size_";
        name += expansion_str;
        test_out.add_measure(name, size);
        name = "bpk";
        name += expansion_str;
        test_out.add_measure(name, TO_BPK(size, current_dataset_size));

        auto fp = 0, fn = 0;
        auto t_start_query_time = timer::now();
        std::cerr << "START QUERY PROCESS" << std::endl;
        for (uint64_t i = 0; i < n_queries; i++) {
            const auto [left, right, original_result] = queries[i];
            big_int_l = left;
            big_int_r = right;

            bool query_result = range_f(f, (char *) big_int_l.num, (char *) big_int_r.num);
            if (query_result) {
                fetch_range_from_db(cursor, big_int_l, big_int_r);
                fp += !original_result;
            }
            else if (!query_result && original_result)
            {
                std::cerr << "[!] alert, found false negative!" << std::endl;
                fn++;
            }
        }
        auto t_end_query_time = timer::now();
        std::cerr << "DONE WITH QUERY PROCESS " << std::chrono::duration_cast<std::chrono::milliseconds>(t_end_query_time - t_start_query_time).count() << std::endl;
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

    std::cout << "[+] data structure constructed in " << test_out["build_time"] << "ms, starting queries" << std::endl;
    for (uint32_t expansion = 1; expansion <= expansion_count; expansion++) {
        std::string expansion_str = std::to_string(expansion);
        auto size = size_f(f);
        std::string name = "size_";
        name += expansion_str;
        test_out.add_measure(name, size);
        name = "bpk";
        name += expansion_str;
        test_out.add_measure(name, TO_BPK(size, current_dataset_size));

        std::cerr << "START EXPANSION PROCESS" << std::endl;
        std::cerr << "current_dataset_size=" << current_dataset_size << std::endl;
        auto t_start_expansion_time = timer::now();
        error_check(cursor->reset(cursor));
        for (uint32_t i = 0; i < current_dataset_size && i + current_dataset_size < N; i++) {
            big_int_k = keys[current_dataset_size + i];
            big_int_v.randomize();
            insert_kv(cursor, (char *) big_int_k.num, (char *) big_int_v.num);

            insert_f(f, (char *) big_int_k.num);
        }
        auto t_end_expansion_time = timer::now();
        name = "expansion_time_";
        name += expansion_str;
        auto time_measurement = std::chrono::duration_cast<std::chrono::milliseconds>(t_end_expansion_time - t_start_expansion_time).count();
        test_out.add_measure(name, time_measurement);
        current_dataset_size = (current_dataset_size * 2 < N ? current_dataset_size * 2 
                                                             : N);
        std::cerr << "DONE WITH EXPANSION PROCESS --- current_dataset_size=" << current_dataset_size << " vs. N=" << N << std::endl;

        auto fp = 0, fn = 0;
        auto t_start_query_time = timer::now();
        std::cerr << "START QUERY PROCESS" << std::endl;
        for (uint64_t i = expansion * n_queries; i < (expansion + 1) * n_queries; i++) {
            const auto [left, right, original_result] = queries[i];
            big_int_l = left;
            big_int_r = right;

            bool query_result = range_f(f, (char *) big_int_l.num, (char *) big_int_r.num);
            if (query_result) {
                fetch_range_from_db(cursor, big_int_l, big_int_r);
                fp += !original_result;
            }
            else if (!query_result && original_result)
            {
                std::cerr << "[!] alert, found false negative!" << std::endl;
                fn++;
            }
        }
        auto t_end_query_time = timer::now();
        std::cerr << "DONE WITH QUERY PROCESS " << std::chrono::duration_cast<std::chrono::milliseconds>(t_end_query_time - t_start_query_time).count() << std::endl;
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

    error_check(conn->close(conn, NULL)); /* Close all handles. */
}

argparse::ArgumentParser init_parser(const std::string &name)
{
    argparse::ArgumentParser parser(name);

    parser.add_argument("arg")
            .help("the main parameter of the ds (typically desired bpk o #suffix bits)")
            .scan<'g', double>();

    parser.add_argument("-b", "--buffer_pool_size")
            .help("size of WiredTiger's buffer pool, in MB")
            .nargs(1)
            .required()
            .default_value(default_buffer_pool_size);

    parser.add_argument("--key_len")
            .help("length of WiredTiger's keys, in bytes")
            .nargs(1)
            .scan<'i', int>()
            .required()
            .default_value(default_key_len);

    parser.add_argument("--val_len")
            .help("length of WiredTiger's values, in bytes")
            .nargs(1)
            .scan<'i', int>()
            .required()
            .default_value(default_val_len);

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

    buffer_pool_size = parser.get<std::string>("buffer_pool_size");
    key_len = parser.get<int>("key_len");
    val_len = parser.get<int>("val_len");

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
    std::cout << "[+] buffer_pool_size=" << buffer_pool_size << ", klen=" << key_len << ", vlen=" << val_len << std::endl;
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
