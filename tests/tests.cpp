// Copyright (c) 2024, Navid Eslami
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * ============================================================================
 *
 *        Author:  Navid Eslami <navideslami@cs.toronto.edu>
 *
 * ============================================================================
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <openssl/rand.h>

#include "memento.h"
#include "memento_int.h"

#define BUFFER_LEN 100000
#define SEED 1

// For pretty print.
static const char *k_green = "\033[0;32m";
static const char *k_white = "\033[0;0m";
static const char *k_red = "\033[31m";

const uint64_t nslots = 200;
const uint64_t key_bits = 13;
const uint64_t memento_bits = 5;
void *buffer;

void test_with_hashing(QF *qf) {
    uint64_t mementos[256];
    for (int i = 0; i < 30; i++)
        mementos[i] = i;
    qf_insert_mementos(qf, 69, mementos, 30, QF_NO_LOCK);

    qf_dump_metadata(qf);
    qf_dump(qf);

    fprintf(stderr, "%s#######################################################################%s\n",
                    k_green, k_white);

    mementos[0] = 5;
    mementos[1] = 7;
    mementos[2] = 25;
    mementos[3] = 31;
    qf_insert_mementos(qf, 59, mementos, 4, QF_NO_LOCK);

    qf_dump_metadata(qf);
    qf_dump(qf);

    fprintf(stderr, "%s#######################################################################%s\n",
                    k_green, k_white);
    for (int i = 0; i < 20; i++)
        mementos[i] = i + 1;
    qf_insert_mementos(qf, 1, mementos, 20, QF_NO_LOCK);

    qf_dump_metadata(qf);
    qf_dump(qf);

    fprintf(stderr, "%s#######################################################################%s\n",
                    k_green, k_white);
    for (int i = 0; i < 20; i++)
        mementos[i] = i + 1;
    qf_insert_mementos(qf, 9, mementos, 20, QF_NO_LOCK);

    qf_dump_metadata(qf);
    qf_dump(qf);

    
    fprintf(stderr, "%s#######################################################################%s\n",
                    k_green, k_white);
    fprintf(stderr, "%s#######################################################################%s\n",
                    k_green, k_white);
    fprintf(stderr, "%s#######################################################################%s\n",
                    k_green, k_white);
    fprintf(stderr, (qf_point_query(qf, 9, 1, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 9, 15, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 9, 20, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 9, 0, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 9, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));

    fprintf(stderr, (qf_point_query(qf, 4, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 100, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));

    fprintf(stderr, (qf_point_query(qf, 1, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 1, 20, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 1, 4, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 1, 1, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 1, 0, QF_NO_LOCK) ? "YES\n" : "NO\n"));

    fprintf(stderr, (qf_point_query(qf, 59, 31, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 59, 5, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 59, 6, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 59, 7, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 59, 25, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 59, 28, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 59, 1, QF_NO_LOCK) ? "YES\n" : "NO\n"));

    fprintf(stderr, (qf_point_query(qf, 69, 0, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 69, 1, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 69, 8, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 69, 29, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 69, 28, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 69, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 69, 31, QF_NO_LOCK) ? "YES\n" : "NO\n"));

    fprintf(stderr, (qf_point_query(qf, 45, 31, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 51, 5, QF_NO_LOCK) ? "YES\n" : "NO\n"));


    for (int i = 0; i < 20; i++)
        mementos[i] = i + 1;
    qf_insert_mementos(qf, 45, mementos, 20, QF_NO_LOCK);
    qf_dump_metadata(qf);
    qf_dump(qf);
    fprintf(stderr, (qf_point_query(qf, 45, 31, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 45, 20, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 45, 1, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 45, 4, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 45, 0, QF_NO_LOCK) ? "YES\n" : "NO\n"));

    fprintf(stderr, (qf_point_query(qf, 69, 0, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 69, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 69, 29, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_point_query(qf, 69, 27, QF_NO_LOCK) ? "YES\n" : "NO\n"));


    fprintf(stderr, "%s#######################################################################%s\n",
                    k_green, k_white);
    fprintf(stderr, "%s#######################################################################%s\n",
                    k_green, k_white);
    fprintf(stderr, "%s#######################################################################%s\n",
                    k_green, k_white);
    // Query contained in single prefix
    fprintf(stderr, (qf_range_query(qf, 0, 4, 0, 6, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 4, 59, 6, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 0, 59, 4, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 0, 59, 5, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 6, 59, 7, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 9, 59, 19, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 9, 59, 24, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 9, 59, 25, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 26, 59, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 26, 59, 31, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 0, 59, 31, QF_NO_LOCK) ? "YES\n" : "NO\n"));

    fprintf(stderr, (qf_range_query(qf, 45, 0, 45, 5, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 45, 0, 45, 1, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 45, 0, 45, 0, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 45, 20, 45, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 45, 23, 45, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    
    // Query contained in two prefixes
    fprintf(stderr, (qf_range_query(qf, 59, 23, 60, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 59, 31, 60, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 58, 31, 59, 30, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 58, 31, 59, 5, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 58, 31, 59, 4, QF_NO_LOCK) ? "YES\n" : "NO\n"));

    fprintf(stderr, (qf_range_query(qf, 45, 20, 46, 4, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 45, 21, 46, 4, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 45, 27, 46, 4, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 44, 27, 45, 4, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 44, 27, 45, 1, QF_NO_LOCK) ? "YES\n" : "NO\n"));
    fprintf(stderr, (qf_range_query(qf, 44, 27, 45, 0, QF_NO_LOCK) ? "YES\n" : "NO\n"));
}

void test_without_hashing() {
    buffer = malloc(BUFFER_LEN + sizeof(qfruntime));
    QF *qf = (QF *) malloc(sizeof(QF));
	qf->runtimedata = (qfruntime *)(malloc(sizeof(qfruntime)));
    qf_init(qf, nslots, key_bits, memento_bits, QF_HASH_DEFAULT, SEED,
            buffer, BUFFER_LEN);

    uint64_t mementos[256];

    fprintf(stderr, "%s######################### EXECUTING test_without_hashing ########################%s\n",
                                                            k_red, k_white);
    fprintf(stderr, "%s-------- INSERTING STUFF INTO THE FILTER --------%s\n",
                    k_green, k_white);
    for (int i = 0; i < 30; i++)
        mementos[i] = i;
    qf_insert_mementos(qf, 0xFFFFF502, mementos, 30, QF_NO_LOCK | QF_KEY_IS_HASH);

    mementos[0] = 5;
    mementos[1] = 7;
    mementos[2] = 25;
    mementos[3] = 31;
    qf_insert_mementos(qf, 0xFFFFE10A, mementos, 4, QF_NO_LOCK | QF_KEY_IS_HASH);

    for (int i = 0; i < 20; i++)
        mementos[i] = i + 1;
    qf_insert_mementos(qf, 0xFFFFE81D, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    
    qf_insert_mementos(qf, 0x3333F70F, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);

    qf_insert_mementos(qf, 0x33330640, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);

    qf_insert_mementos(qf, 0x333F6940, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);

    for (int i = 0; i < 30; i++)
        mementos[i] = i;
    qf_insert_mementos(qf, 0x3333A100, mementos, 30, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING POINT QUERY + COLLISIONS --------%s\n",
                    k_green, k_white);
    
    assert(qf_point_query(qf, 0x333F6940, 20, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0x333F6940, 19, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0x333F6940, 21, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0xFFFFE81D, 20, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0xFFFFE81D, 19, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0xFFFFE81D, 21, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0xFFFFE10A, 20, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0xFFFFE10A, 19, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0xFFFFE10A, 21, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(!qf_point_query(qf, 0xFFFFE10A, 21, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0xFFFFE10A, 25, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0xFFFFE10A, 31, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0xFFFFE10A, 5, QF_NO_LOCK | QF_KEY_IS_HASH));

    mementos[0] = 29;
    qf_insert_mementos(qf, 0x33330640, mementos, 1, QF_NO_LOCK | QF_KEY_IS_HASH);

    assert(qf_point_query(qf, 0x33330640, 29, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0x33330640, 30, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0x33330640, 1, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(!qf_point_query(qf, 0x3333A100, 31, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0x3333A100, 29, QF_NO_LOCK | QF_KEY_IS_HASH));

    fprintf(stderr, "%s-------- CHECKING RANGE QUERY WITH ONE PREFIX + COLLISIONS --------%s\n",
                    k_green, k_white);
    // Query contained in single prefix
    assert(!qf_range_query(qf, 0x33330640, 21, 0x33330640, 27, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0x33330640, 27, 0x33330640, 31, QF_NO_LOCK | QF_KEY_IS_HASH));


    assert(qf_range_query(qf, 0x33330640, 15, 0x33330640, 22, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_range_query(qf, 0xFFFFE10A, 15, 0xFFFFE10A, 22, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0x3333A100, 15, 0x3333A100, 29, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(!qf_range_query(qf, 0xFFFFE10A, 27, 0xFFFFE10A, 28, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0xFFFFE10A, 28, 0xFFFFE10A, 31, QF_NO_LOCK | QF_KEY_IS_HASH));

    // Query contained in two prefixes
    fprintf(stderr, "%s-------- CHECKING RANGE QUERY WITH TWO PREFIXES + COLLISIONS --------%s\n",
                    k_green, k_white);
    assert(qf_range_query(qf, 0x33330640, 21, 0xFFFFE10A, 4, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_range_query(qf, 0x33330640, 30, 0xFFFFE10A, 4, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0x33330640, 30, 0xFFFFE10A, 6, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(qf_range_query(qf, 0x3333A100, 15, 0xFFFFE10A, 4, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0x3333A100, 21, 0xFFFFE10A, 4, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_range_query(qf, 0x3333A100, 30, 0xFFFFE10A, 4, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_range_query(qf, 0x3333A100, 30, 0xFFFFE10A, 2, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0x3333A100, 30, 0xFFFFE10A, 30, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(qf_range_query(qf, 0x3333A100, 27, 0xFFFFE10A, 4, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0x3333A100, 28, 0xFFFFE10A, 4, QF_NO_LOCK | QF_KEY_IS_HASH));
    
    qf_free(qf);
}

void print_iterator_contents(QFi *qfi) {
    fprintf(stderr, "run=%lu - current=%lu - cur_start_index=%lu - cur_length=%u - ",
            qfi->run, qfi->current, qfi->cur_start_index, qfi->cur_length);
    fprintf(stderr, "num_clusters=%u\n", qfi->num_clusters);
}

void print_iterator_get_result(QFi *qfi) {
    uint64_t hash_result, memento_result[256];
    memset(memento_result, 0, sizeof(memento_result));
    int result_length = qfi_get_hash(qfi, &hash_result, memento_result);

    if (result_length < 0) {
        perror("Iterator depleted");
        return;
    }

    fprintf(stderr, "HASH RESULT=");
    for (int i = 8 * sizeof(hash_result) - 1; i >= 0; i--)
        fprintf(stderr, "%lu", (hash_result >> i) & 1);
    fprintf(stderr, " --- MEMENTO COUNT=%d : MEMENTOS=[", result_length);
    bool print_comma = false;
    for (uint32_t i = 0; i < result_length; i++) {
        fprintf(stderr, (print_comma ? ", %lu" : "%lu"), memento_result[i]);
        print_comma = true;
    }
    perror("]");
}

void test_iterators() {
    buffer = malloc(BUFFER_LEN + sizeof(qfruntime));
    QF *qf = (QF *) malloc(sizeof(QF));
	qf->runtimedata = (qfruntime *)(malloc(sizeof(qfruntime)));
    qf_init(qf, nslots, key_bits, memento_bits, QF_HASH_DEFAULT, SEED,
            buffer, BUFFER_LEN);

    uint64_t mementos[256];

    fprintf(stderr, "%s######################### EXECUTING test_iterators ########################%s\n",
                                                            k_red, k_white);
    fprintf(stderr, "%s-------- INSERTING STUFF INTO THE FILTER --------%s\n", k_green, k_white);
    for (int i = 0; i < 30; i++)
        mementos[i] = i;
    qf_insert_mementos(qf, 0xFFFFF502, mementos, 30, QF_NO_LOCK | QF_KEY_IS_HASH);
    mementos[0] = 5;
    mementos[1] = 7;
    mementos[2] = 25;
    mementos[3] = 31;
    qf_insert_mementos(qf, 0xFFFFE10A, mementos, 4, QF_NO_LOCK | QF_KEY_IS_HASH);
    for (int i = 0; i < 20; i++)
        mementos[i] = i + 1;
    qf_insert_mementos(qf, 0xFFFFE81D, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_mementos(qf, 0x3333F70F, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_mementos(qf, 0x33330640, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_mementos(qf, 0x333F6940, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    mementos[0] = 29;
    qf_insert_mementos(qf, 0x33330640, mementos, 1, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING ITERATORS --------%s\n", k_green, k_white);
    uint64_t hash_result, memento_result[256];
    memset(memento_result, 0, sizeof(memento_result));
    int result_length;

    QFi *iter = (QFi *) malloc(sizeof(QFi));
    qf_iterator_from_position(qf, iter, 0);
    assert(iter->run == 1);
    assert(iter->current == 1);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 30);
    for (uint64_t i = 0; i < 30; i++)
        assert(memento_result[i] == i);

    qfi_next(iter);
    assert(iter->run == 7);
    assert(iter->current == 18);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b0000000000100000111);
    assert(result_length == 4);
    assert(memento_result[0] == 5);
    assert(memento_result[1] == 7);
    assert(memento_result[2] == 25);
    assert(memento_result[3] == 31);

    qfi_next(iter);
    assert(iter->run == 11);
    assert(iter->current == 22);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b0000001011100001011);
    assert(result_length == 20);
    for (uint64_t i = 0; i < 20; i++)
        assert(memento_result[i] == i + 1);

    qfi_next(iter);
    assert(iter->run == 22);
    assert(iter->current == 34);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b0000000100000010110);
    assert(result_length == 20);
    for (uint64_t i = 0; i < 20; i++)
        assert(memento_result[i] == i + 1);

    qfi_next(iter);
    assert(iter->run == 50);
    assert(iter->current == 50);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b0000000011000110010);
    assert(result_length == 20);
    for (uint64_t i = 0; i < 20; i++)
        assert(memento_result[i] == i + 1);

    qfi_next(iter);
    assert(iter->run == 50);
    assert(iter->current == 62);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b0000000011000110010);
    assert(result_length == 1);
    assert(memento_result[0] == 29);

    qfi_next(iter);
    assert(iter->run == 50);
    assert(iter->current == 63);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b0000000100100110010);
    assert(result_length == 20);
    for (uint64_t i = 0; i < 20; i++)
        assert(memento_result[i] == i + 1);

    qfi_next(iter);
    
    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(result_length < 0); // Done iterating!

    fprintf(stderr, "%s-------- DONE --------%s\n", k_green, k_white);
    
    qf_free(qf);
}

void test_expansion() {
    buffer = malloc(BUFFER_LEN + sizeof(qfruntime));
    QF *qf = (QF *) malloc(sizeof(QF));
	qf->runtimedata = (qfruntime *)(malloc(sizeof(qfruntime)));
    qf_init(qf, nslots, key_bits, memento_bits, QF_HASH_DEFAULT, SEED,
            buffer, BUFFER_LEN);

    uint64_t mementos[256];

    fprintf(stderr, "%s######################### EXECUTING test_expansion ########################%s\n",
                                                            k_red, k_white);
    fprintf(stderr, "%s-------- INSERTING STUFF INTO THE FILTER --------%s\n", k_green, k_white);
    for (int i = 0; i < 30; i++)
        mementos[i] = i;
    qf_insert_mementos(qf, 0xFFFFF502, mementos, 30, QF_NO_LOCK | QF_KEY_IS_HASH);
    mementos[0] = 5;
    mementos[1] = 7;
    mementos[2] = 25;
    mementos[3] = 31;
    qf_insert_mementos(qf, 0xFFFFE10A, mementos, 4, QF_NO_LOCK | QF_KEY_IS_HASH);
    for (int i = 0; i < 20; i++)
        mementos[i] = i + 1;
    qf_insert_mementos(qf, 0xFFFFE81D, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_mementos(qf, 0x3333F70F, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_mementos(qf, 0x33330640, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_mementos(qf, 0x333F6940, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    mementos[0] = 29;
    qf_insert_mementos(qf, 0x33330640, mementos, 1, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- EXPANDING FILTER --------%s\n", k_green, k_white);
    qf_resize_malloc(qf, qf->metadata->nslots * 2);

    for (uint32_t i = 0; i < 10; i++)
        mementos[i] = (i + 1) * 3;
    qf_insert_mementos(qf, 0x11126C40, mementos, 10, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING QUERIES --------%s\n", k_green, k_white);
    assert(!qf_point_query(qf, 0x11126C40, 20, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0x11126C40, 21, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0x11126C40, 30, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0x11126C40, 29, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(qf_point_query(qf, 0x333F6940, 20, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0x333F6940, 19, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0x333F6940, 21, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0xFFFFE81D, 20, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0xFFFFE81D, 19, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0xFFFFE81D, 21, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0xFFFFE10A, 20, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0xFFFFE10A, 19, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0xFFFFE10A, 21, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(!qf_point_query(qf, 0xFFFFE10A, 21, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0xFFFFE10A, 25, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0xFFFFE10A, 31, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0xFFFFE10A, 5, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(qf_point_query(qf, 0x33330640, 29, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_point_query(qf, 0x33330640, 30, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_point_query(qf, 0x33330640, 1, QF_NO_LOCK | QF_KEY_IS_HASH));

    // Range query contained in single prefix
    assert(!qf_range_query(qf, 0x11126C40, 22, 0x11126C40, 23, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_range_query(qf, 0x11126C40, 0, 0x11126C40, 2, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0x11126C40, 10, 0x11126C40, 22, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(!qf_range_query(qf, 0x33330640, 21, 0x33330640, 27, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0x33330640, 27, 0x33330640, 31, QF_NO_LOCK | QF_KEY_IS_HASH));


    assert(qf_range_query(qf, 0x33330640, 15, 0x33330640, 22, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_range_query(qf, 0xFFFFE10A, 15, 0xFFFFE10A, 22, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(!qf_range_query(qf, 0xFFFFE10A, 27, 0xFFFFE10A, 28, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0xFFFFE10A, 28, 0xFFFFE10A, 31, QF_NO_LOCK | QF_KEY_IS_HASH));

    // Range query contained in two prefixes
    assert(!qf_range_query(qf, 0x33330640, 30, 0x11126C40, 2, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0x33330640, 30, 0x11126C40, 25, QF_NO_LOCK | QF_KEY_IS_HASH));

    assert(qf_range_query(qf, 0x33330640, 21, 0xFFFFE10A, 4, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(!qf_range_query(qf, 0x33330640, 30, 0xFFFFE10A, 4, QF_NO_LOCK | QF_KEY_IS_HASH));
    assert(qf_range_query(qf, 0x33330640, 30, 0xFFFFE10A, 6, QF_NO_LOCK | QF_KEY_IS_HASH));

    fprintf(stderr, "%s-------- CHECKING ITERATION --------%s\n", k_green, k_white);
    QFi *iter = (QFi *) malloc(sizeof(QFi));
    uint64_t hash_result, memento_result[256];
    memset(memento_result, 0, sizeof(memento_result));
    int result_length;

    qf_iterator_from_position(qf, iter, 0);
    assert(iter->run == 3);
    assert(iter->current == 3);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b000001010100000001);
    assert(result_length == 30);
    for (uint64_t i = 0; i < 30; i++)
        assert(memento_result[i] == i);

    qfi_next(iter);
    assert(iter->run == 15);
    assert(iter->current == 22);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b000000000100000111);
    assert(result_length == 1);
    assert(memento_result[0] == 5);

    qfi_next(iter);
    assert(iter->run == 15);
    assert(iter->current == 23);
    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b000000000100000111);
    assert(result_length == 1);
    assert(memento_result[0] == 7);

    qfi_next(iter);
    assert(iter->run == 15);
    assert(iter->current == 24);
    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b000000000100000111);
    assert(result_length == 1);
    assert(memento_result[0] == 25);

    qfi_next(iter);
    assert(iter->run == 15);
    assert(iter->current == 25);
    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b000000000100000111);
    assert(result_length == 1);
    assert(memento_result[0] == 31);

    qfi_next(iter);
    assert(iter->run == 23);
    assert(iter->current == 26);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b000001011100001011);
    assert(result_length == 20);
    for (uint64_t i = 0; i < 20; i++)
        assert(memento_result[i] == i + 1);

    qfi_next(iter);
    assert(iter->run == 44);
    assert(iter->current == 44);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b000000100000010110);
    assert(result_length == 20);
    for (uint64_t i = 0; i < 20; i++)
        assert(memento_result[i] == i + 1);

    qfi_next(iter);
    assert(iter->run == 100);
    assert(iter->current == 100);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b000000011000110010);
    assert(result_length == 20);
    for (uint64_t i = 0; i < 20; i++)
        assert(memento_result[i] == i + 1);

    qfi_next(iter);
    assert(iter->run == 100);
    assert(iter->current == 113);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b00011000110010);
    assert(result_length == 1);
    assert(memento_result[0] == 29);

    qfi_next(iter);
    assert(iter->run == 100);
    assert(iter->current == 114);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b00000000110000110010);
    assert(result_length == 10);
    for (uint64_t i = 0; i < 10; i++)
        assert(memento_result[i] == 3 * (i + 1));


    qfi_next(iter);
    assert(iter->run == 101);
    assert(iter->current == 121);

    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(hash_result == 0b000000100100110010);
    assert(result_length == 20);
    for (uint64_t i = 0; i < 20; i++)
        assert(memento_result[i] == i + 1);

    qfi_next(iter);
    
    result_length = qfi_get_hash(iter, &hash_result, memento_result);
    assert(result_length < 0); // Done iterating!

    fprintf(stderr, "%s-------- EXPANDING FILTER AGAIN --------%s\n", k_green, k_white);
    qf_resize_malloc(qf, qf->metadata->nslots * 2);
    
    qf_free(qf);
}

void test_insert_single() {
    buffer = malloc(BUFFER_LEN + sizeof(qfruntime));
    QF *qf = (QF *) malloc(sizeof(QF));
	qf->runtimedata = (qfruntime *)(malloc(sizeof(qfruntime)));
    qf_init(qf, nslots, key_bits, memento_bits, QF_HASH_DEFAULT, SEED,
            buffer, BUFFER_LEN);

    uint64_t mementos[256];
    uint64_t hash_result, result_mementos[256];
    int32_t result_length;

    fprintf(stderr, "%s######################### EXECUTING test_insert_single ########################%s\n",
                                                            k_red, k_white);
    fprintf(stderr, "%s-------- INSERTING STUFF INTO THE FILTER --------%s\n", k_green, k_white);
    for (int i = 0; i < 10; i++)
        mementos[i] = 3 * (i + 1);
    qf_insert_mementos(qf, 0xFFFFF502, mementos, 10, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_single(qf, 0xFFFFF502, 20, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);
    QFi iter;
    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 11);
    for (uint32_t i = 0; i < 6; i++)
        assert(result_mementos[i] == 3 * (i + 1));
    assert(result_mementos[6] == 20);
    for (uint32_t i = 7; i < 11; i++)
        assert(result_mementos[i] == 3 * i);
    
    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);

    mementos[0] = 5;
    mementos[1] = 7;
    mementos[2] = 20;
    mementos[3] = 25;
    mementos[4] = 31;
    qf_insert_mementos(qf, 0xFFFFE00B, mementos, 5, QF_NO_LOCK | QF_KEY_IS_HASH);

    qf_insert_single(qf, 0xFFFFF502, 19, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_single(qf, 0xFFFFF502, 1, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_single(qf, 0xFFFFF502, 31, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);
    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 14);
    assert(result_mementos[0] == 1);
    for (uint32_t i = 1; i < 7; i++)
        assert(result_mementos[i] == 3 * i);
    assert(result_mementos[7] == 19);
    assert(result_mementos[8] == 20);
    for (uint32_t i = 9; i < 13; i++)
        assert(result_mementos[i] == 3 * (i - 2));
    assert(result_mementos[13] == 31);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 10);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 5);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 11);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 7);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 12);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 20);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 13);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 25);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 14);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 31);

    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);

    for (uint32_t i = 0; i < 18; i++) {
        qf_insert_single(qf, 0xFFFFF502, 1, QF_NO_LOCK | QF_KEY_IS_HASH);
    }

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);

    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 32);
    for (uint32_t i = 0; i < 19; i++)
        assert(result_mementos[i] == 1);
    for (uint32_t i = 0; i < 6; i++)
        assert(result_mementos[19 + i] == 3 * (i + 1));
    assert(result_mementos[25] == 19);
    assert(result_mementos[26] == 20);
    for (uint32_t i = 6; i < 10; i++)
        assert(result_mementos[27 - 6 + i] == 3 * (i + 1));
    assert(result_mementos[31] == 31);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 19);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 5);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 20);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 7);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 21);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 20);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 22);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 25);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 23);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 31);

    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);

    qf_insert_single(qf, 0xFFFFF502, 0, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_single(qf, 0xFFFFF502, 31, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_single(qf, 0xFFFFF502, 30, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);

    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 35);
    assert(result_mementos[0] == 0);
    for (uint32_t i = 0; i < 19; i++)
        assert(result_mementos[i + 1] == 1);
    for (uint32_t i = 0; i < 6; i++)
        assert(result_mementos[20 + i] == 3 * (i + 1));
    assert(result_mementos[26] == 19);
    assert(result_mementos[27] == 20);
    for (uint32_t i = 6; i < 10; i++)
        assert(result_mementos[28 - 6 + i] == 3 * (i + 1));
    assert(result_mementos[32] == 30);
    assert(result_mementos[33] == 31);
    assert(result_mementos[34] == 31);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 21);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 5);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 22);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 7);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 23);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 20);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 24);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 25);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 25);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 31);

    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);

    mementos[0] = 7;
    mementos[1] = 14;
    mementos[2] = 21;
    mementos[3] = 28;
    mementos[4] = 31;
    qf_insert_mementos(qf, 0xFFFFE14A, mementos, 5, QF_NO_LOCK | QF_KEY_IS_HASH);
    for (uint32_t i = 0; i < 18; i++) {
        qf_insert_single(qf, 0xFFFFE14A, 1, QF_NO_LOCK | QF_KEY_IS_HASH);
    }

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);

    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 35);
    assert(result_mementos[0] == 0);
    for (uint32_t i = 0; i < 19; i++)
        assert(result_mementos[i + 1] == 1);
    for (uint32_t i = 0; i < 6; i++)
        assert(result_mementos[20 + i] == 3 * (i + 1));
    assert(result_mementos[26] == 19);
    assert(result_mementos[27] == 20);
    for (uint32_t i = 6; i < 10; i++)
        assert(result_mementos[28 - 6 + i] == 3 * (i + 1));
    assert(result_mementos[32] == 30);
    assert(result_mementos[33] == 31);
    assert(result_mementos[34] == 31);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 21);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 5);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 22);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 7);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 23);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 20);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 24);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 25);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 25);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 31);

    qfi_next(&iter);
    assert(iter.run == 57);
    assert(iter.current == 57);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000100111001);
    assert(result_length == 23);
    for (uint32_t i = 0; i < 18; i++)
        assert(result_mementos[i] == 1);
    for (uint32_t i = 0; i < 4; i++)
        assert(result_mementos[18 + i] == 7 * (i + 1));
    assert(result_mementos[22] == 31);

    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);


    qf_insert_single(qf, 0x5555554A, 21, QF_NO_LOCK | QF_KEY_IS_HASH);
    assert(qf_insert_single(qf, 0x5555554A, 22, QF_NO_LOCK | QF_KEY_IS_HASH) >= 0);
    qf_insert_single(qf, 0x5555550B, 21, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_single(qf, 0x3333330B, 21, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_single(qf, 0x3333330B, 21, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_single(qf, 0x3333330B, 21, QF_NO_LOCK | QF_KEY_IS_HASH);
    qf_insert_single(qf, 0x33333308, 16, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);

    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 35);
    assert(result_mementos[0] == 0);
    for (uint32_t i = 0; i < 19; i++)
        assert(result_mementos[i + 1] == 1);
    for (uint32_t i = 0; i < 6; i++)
        assert(result_mementos[20 + i] == 3 * (i + 1));
    assert(result_mementos[26] == 19);
    assert(result_mementos[27] == 20);
    for (uint32_t i = 6; i < 10; i++)
        assert(result_mementos[28 - 6 + i] == 3 * (i + 1));
    assert(result_mementos[32] == 30);
    assert(result_mementos[33] == 31);
    assert(result_mementos[34] == 31);

    qfi_next(&iter);
    assert(iter.run == 6);
    assert(iter.current == 21);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001001100000110);
    assert(result_length == 1);
    assert(result_mementos[0] == 16);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 22);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 5);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 23);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 7);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 24);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 20);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 25);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 25);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 26);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000000001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 31);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 27);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b00000001001100001000);
    assert(result_length == 3);
    assert(result_mementos[0] == 21);
    assert(result_mementos[1] == 21);
    assert(result_mementos[2] == 21);

    qfi_next(&iter);
    assert(iter.run == 8);
    assert(iter.current == 30);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b00000001010100001000);
    assert(result_length == 1);
    assert(result_mementos[0] == 21);

    qfi_next(&iter);
    assert(iter.run == 57);
    assert(iter.current == 57);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000000000100111001);
    assert(result_length == 23);
    for (uint32_t i = 0; i < 18; i++)
        assert(result_mementos[i] == 1);
    for (uint32_t i = 0; i < 4; i++)
        assert(result_mementos[18 + i] == 7 * (i + 1));
    assert(result_mementos[22] == 31);

    qfi_next(&iter);
    assert(iter.run == 57);
    assert(iter.current == 70);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100111001);
    assert(result_length == 2);
    assert(result_mementos[0] == 21);
    assert(result_mementos[1] == 22);

    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);
    
    qf_free(qf);
}

void test_delete_single() {
    buffer = malloc(BUFFER_LEN + sizeof(qfruntime));
    QF *qf = (QF *) malloc(sizeof(QF));
	qf->runtimedata = (qfruntime *)(malloc(sizeof(qfruntime)));
    qf_init(qf, nslots, key_bits, memento_bits, QF_HASH_DEFAULT, SEED,
            buffer, BUFFER_LEN);

    uint64_t mementos[256];
    uint64_t hash_result, result_mementos[256];
    int32_t result_length;

    fprintf(stderr, "%s######################### EXECUTING test_delete_single ########################%s\n",
                                                            k_red, k_white);
    fprintf(stderr, "%s-------- INSERTING STUFF INTO THE FILTER --------%s\n", k_green, k_white);
    for (int i = 0; i < 5; i++)
        mementos[i] = 1;
    for (int i = 0; i < 10; i++)
        mementos[5 + i] = 3 * (i + 1);
    for (int i = 0; i < 10; i++)
        mementos[15 + i] = 31;
    qf_insert_mementos(qf, 0xFFFFF502, mementos, 25, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);

    QFi iter;
    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 25);
    for (uint32_t i = 0; i < 5; i++)
        assert(result_mementos[i] == 1);
    for (uint32_t i = 0; i < 10; i++)
        assert(result_mementos[5 + i] == 3 * (i + 1));
    for (uint32_t i = 0; i < 10; i++)
        assert(result_mementos[15 + 1] == 31);
    
    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);

    fprintf(stderr, "%s-------- REMOVING STUFF FROM THE PREFIX SET --------%s\n", k_green, k_white);
    qf_delete_single(qf, 0xFFFFF502, 3, QF_NO_LOCK | QF_KEY_IS_HASH);
    for (uint32_t i = 0; i < 10; i++)
        qf_delete_single(qf, 0xFFFFF502, 31, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);
    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 14);
    for (uint32_t i = 0; i < 5; i++)
        assert(result_mementos[i] == 1);
    for (uint32_t i = 0; i < 9; i++)
        assert(result_mementos[5 + i] == 3 * (i + 2));
    
    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);

    fprintf(stderr, "%s-------- INSERTING STUFF INTO THE FILTER --------%s\n", k_green, k_white);
    mementos[0] = 10;
    mementos[1] = 20;
    qf_insert_mementos(qf, 0x33333332, mementos, 2, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);
    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 14);
    for (uint32_t i = 0; i < 5; i++)
        assert(result_mementos[i] == 1);
    for (uint32_t i = 0; i < 9; i++)
        assert(result_mementos[5 + i] == 3 * (i + 2));

    qfi_next(&iter);
    assert(iter.run == 39);
    assert(iter.current == 39);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001001100100111);
    assert(result_length == 2);
    assert(result_mementos[0] == 10);
    assert(result_mementos[1] == 20);
    
    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);

    fprintf(stderr, "%s-------- REMOVING STUFF FROM THE PREFIX SET --------%s\n", k_green, k_white);
    qf_delete_single(qf, 0x33333332, 10, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);
    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 14);
    for (uint32_t i = 0; i < 5; i++)
        assert(result_mementos[i] == 1);
    for (uint32_t i = 0; i < 9; i++)
        assert(result_mementos[5 + i] == 3 * (i + 2));

    qfi_next(&iter);
    assert(iter.run == 39);
    assert(iter.current == 39);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001001100100111);
    assert(result_length == 1);
    assert(result_mementos[0] == 20);
    
    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);

    fprintf(stderr, "%s-------- REMOVING STUFF FROM THE PREFIX SET --------%s\n", k_green, k_white);
    qf_delete_single(qf, 0x33333332, 20, QF_NO_LOCK | QF_KEY_IS_HASH);

    fprintf(stderr, "%s-------- CHECKING RESULTS --------%s\n", k_green, k_white);
    qf_iterator_from_position(qf, &iter, 0);
    assert(iter.run == 1);
    assert(iter.current == 1);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(hash_result == 0b0000001010100000001);
    assert(result_length == 14);
    for (uint32_t i = 0; i < 5; i++)
        assert(result_mementos[i] == 1);
    for (uint32_t i = 0; i < 9; i++)
        assert(result_mementos[5 + i] == 3 * (i + 2));

    qfi_next(&iter);
    result_length = qfi_get_hash(&iter, &hash_result, result_mementos);
    assert(result_length < 0);
    fprintf(stderr, "%s-------- STATUS: OK --------%s\n", k_green, k_white);
    
    qf_free(qf);
}

void test_uniform_distribution(QF *qf) {
    srand(5);

    for (uint64_t i = 0; i < (uint64_t) (nslots * 0.85); i++) {
        uint64_t key = rand();
        while ((key & ((1ULL << memento_bits) - 1)) == 0)
            key = rand();
        fprintf(stderr, "%sINSERTING key=%lu%s\n", k_green, key, k_white);
        uint64_t mementos[1] = {key & ((1 << memento_bits) - 1)};
        qf_insert_mementos(qf, key >> memento_bits, mementos, 1, QF_NO_LOCK | QF_KEY_IS_HASH);

        fprintf(stderr, "%s#######################################################################%s\n",
                        k_green, k_white);
        qf_dump(qf);
    }
}

// Courtesy of https://phoxis.org/2013/05/04/generating-random-numbers-from-normal-distribution-in-c/
static double rand_normal(double mu, double sigma) {
    double U1, U2, W, mult;
    static double X1, X2;
    static int call = 0;

    if (call == 1) {
        call = !call;
        return (mu + sigma * (double) X2);
    }

    do
    {
        U1 = -1 + ((double) rand () / RAND_MAX) * 2;
        U2 = -1 + ((double) rand () / RAND_MAX) * 2;
        W = pow (U1, 2) + pow (U2, 2);
    } while (W >= 1 || W == 0);

    mult = sqrt ((-2 * log (W)) / W);
    X1 = U1 * mult;
    X2 = U2 * mult;

    call = !call;

    return (mu + sigma * (double) X1);
}

int compare_uint64_t(const void * elem1, const void * elem2) 
{
    int f = *((uint64_t *) elem1);
    int s = *((uint64_t *) elem2);
    if (f > s) return  1;
    if (f < s) return -1;
    return 0;
}

void test_normal_distribution(QF *qf) {
    srand(5);
    double mu = (1ULL << 18);
    double sigma = 500;

    const uint64_t workload_size = 85;
    uint64_t keys[workload_size];
    for (uint64_t i = 0; i < workload_size; i++) {
        keys[i] = rand_normal(mu, sigma);
        while ((keys[i] & ((1ULL << memento_bits) - 1)) == 0)
            keys[i] = rand_normal(mu, sigma);
    }
    qsort(keys, workload_size, sizeof(uint64_t), compare_uint64_t);
    
    uint64_t prefix = keys[0] >> memento_bits;
    uint64_t memento_list[256];
    uint64_t prefix_set_size = 1;
    memento_list[0] = keys[0] & ((1ULL << memento_bits) - 1);
    for (uint64_t i = 1; i < workload_size; i++) {
        uint64_t current_prefix = keys[i] >> memento_bits;
        if (prefix != current_prefix) {
            fprintf(stderr, "%sINSERTING key=%lu mementos=", k_green, prefix);
            for (uint64_t j = 0; j < prefix_set_size; j++)
                fprintf(stderr, "%lu ", memento_list[j]);
            fprintf(stderr, "%s\n", k_white);

            qf_insert_mementos(qf, prefix, memento_list, prefix_set_size,
                                QF_NO_LOCK);
            prefix = current_prefix;
            prefix_set_size = 0;

            qf_dump(qf);
            fprintf(stderr, "%s#######################################################################%s\n",
                            k_green, k_white);
        }
        memento_list[prefix_set_size] = keys[i] & ((1ULL << memento_bits) - 1);
        prefix_set_size++;
    }
}

//===========================================================================
//=  Function to generate Zipf (power law) distributed random variables     =
//=    - Input: alpha and N                                                 =
//=    - Output: Returns with Zipf distributed random variable              =
//===========================================================================
int rand_zipf(double alpha, int n)
{
  static int first = true;      // Static first time flag
  static double c = 0;          // Normalization constant
  double z;                     // Uniform random number (0 < z < 1)
  double sum_prob;              // Sum of probabilities
  double zipf_value = 0;        // Computed exponential value to be returned
  int    i;                     // Loop counter

  // Compute normalization constant on first call only
  if (first == true)
  {
    for (i=1; i<=n; i++)
      c = c + (1.0 / pow((double) i, alpha));
    c = 1.0 / c;
    first = false;
  }

  // Pull a uniform random number (0 < z < 1)
  do
  {
    z = 1.0 * rand() / RAND_MAX ;
  }
  while ((z == 0) || (z >= 1));

  // Map z to the value
  sum_prob = 0;
  for (i=1; i<=n; i++)
  {
    sum_prob = sum_prob + c / pow((double) i, alpha);
    if (sum_prob >= z)
    {
      zipf_value = i;
      break;
    }
  }

  // Assert that zipf_value is between 1 and N
  assert((zipf_value >=1) && (zipf_value <= n));

  return(zipf_value);
}

void test_zipf_distribution(QF *qf) {
    srand(5);
    double alpha = 1;

    const uint64_t workload_size = 85;
    uint64_t keys[workload_size];
    for (uint64_t i = 0; i < workload_size; i++) {
        keys[i] = rand_zipf(alpha, 1000000) + 1;
        while ((keys[i] & ((1ULL << memento_bits) - 1)) == 0)
            keys[i] = rand_zipf(alpha, 256);
    }
    qsort(keys, workload_size, sizeof(uint64_t), compare_uint64_t);

    for (uint64_t i = 0; i < workload_size; i++)
        fprintf(stderr, "%lu, ", keys[i]);
    fprintf(stderr, "\n");
    
    uint64_t prefix = keys[0] >> memento_bits;
    uint64_t memento_list[256];
    uint64_t prefix_set_size = 1;
    memento_list[0] = keys[0] & ((1ULL << memento_bits) - 1);
    for (uint64_t i = 1; i < workload_size; i++) {
        uint64_t current_prefix = keys[i] >> memento_bits;
        if (prefix != current_prefix) {
            fprintf(stderr, "%sINSERTING key=%lu mementos=", k_green, prefix);
            for (uint64_t j = 0; j < prefix_set_size; j++)
                fprintf(stderr, "%lu ", memento_list[j]);
            fprintf(stderr, "%s\n", k_white);

            qf_insert_mementos(qf, prefix, memento_list, prefix_set_size,
                                QF_NO_LOCK);
            prefix = current_prefix;
            prefix_set_size = 0;

            qf_dump(qf);
            fprintf(stderr, "%s#######################################################################%s\n",
                            k_green, k_white);
        }
        memento_list[prefix_set_size] = keys[i] & ((1ULL << memento_bits) - 1);
        prefix_set_size++;
    }
}

void test_skew(QF *qf) {
    const uint64_t workload_size = 150;
    uint64_t keys[workload_size];
    for (uint64_t i = 0; i < workload_size; i++) {
        keys[i] = i * 3;
    }

    for (uint64_t i = 0; i < workload_size; i++)
        fprintf(stderr, "%lu, ", keys[i]);
    fprintf(stderr, "\n");
    
    uint64_t prefix = keys[0] >> memento_bits;
    uint64_t memento_list[256];
    uint64_t prefix_set_size = 1;
    memento_list[0] = keys[0] & ((1ULL << memento_bits) - 1);
    for (uint64_t i = 1; i < workload_size; i++) {
        uint64_t current_prefix = keys[i] >> memento_bits;
        if (prefix != current_prefix) {
            fprintf(stderr, "%sINSERTING key=%lu mementos=", k_green, prefix);
            for (uint64_t j = 0; j < prefix_set_size; j++)
                fprintf(stderr, "%lu ", memento_list[j]);
            fprintf(stderr, "%s\n", k_white);

            qf_insert_mementos(qf, prefix, memento_list, prefix_set_size,
                                QF_NO_LOCK);
            prefix = current_prefix;
            prefix_set_size = 0;

            qf_dump(qf);
            fprintf(stderr, "%s#######################################################################%s\n",
                            k_green, k_white);
        }
        memento_list[prefix_set_size] = keys[i] & ((1ULL << memento_bits) - 1);
        prefix_set_size++;
    }
}

int main(int argc, char **argv) {
    test_without_hashing();
    test_iterators();
    test_expansion();
    test_insert_single();
    test_delete_single();
}

