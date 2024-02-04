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

#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>

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
    //qf_dump_metadata(qf);
    //qf_dump(qf);
    //perror("#######################################################################");

    mementos[0] = 5;
    mementos[1] = 7;
    mementos[2] = 25;
    mementos[3] = 31;
    qf_insert_mementos(qf, 0xFFFFE10A, mementos, 4, QF_NO_LOCK | QF_KEY_IS_HASH);
    //qf_dump_metadata(qf);
    //qf_dump(qf);
    //perror("#######################################################################");

    for (int i = 0; i < 20; i++)
        mementos[i] = i + 1;
    qf_insert_mementos(qf, 0xFFFFE81D, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    //qf_dump_metadata(qf);
    //qf_dump(qf);
    //perror("#######################################################################");
    
    qf_insert_mementos(qf, 0x3333F70F, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    //qf_dump_metadata(qf);
    //qf_dump(qf);
    //perror("#######################################################################");

    qf_insert_mementos(qf, 0x33330640, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    //qf_dump_metadata(qf);
    //qf_dump(qf);
    //perror("#######################################################################");

    qf_insert_mementos(qf, 0x333F6940, mementos, 20, QF_NO_LOCK | QF_KEY_IS_HASH);
    //qf_dump_metadata(qf);
    //qf_dump(qf);
    //perror("#######################################################################");

    for (int i = 0; i < 30; i++)
        mementos[i] = i;
    qf_insert_mementos(qf, 0x3333A100, mementos, 30, QF_NO_LOCK | QF_KEY_IS_HASH);
    //qf_dump_metadata(qf);
    //qf_dump(qf);
    //perror("#######################################################################");

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
    //qf_dump_metadata(qf);
    //qf_dump(qf);

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

int main()
{
    std::cout << "HI MOM AND DAD!" << std::endl;
    test_without_hashing();

    return 0;
}
