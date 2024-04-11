# Memento Filter

<p align="center">Memento Filter is the first range filter to support dynamic
datasets, constant time operations, and a theoretically optimal false positive
rate. It also provides strong guarantees on the expected false positive rate
under any kind of workloads, while also maintaining a low false positive rate
for growing datasets. </p>

## Quickstart

This is a header-only library. It does not need to be installed. Just clone the
repo with

```bash
git clone [url]
cd Memento_Filter
```

and copy the `include` and `src` directories to your system's or project's
include path.

The `examples/simple.cpp` file shows how to index and query a vector of random
integers with Memento filter:

```cpp
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
    test_without_hashing();

    return 0;
}
```

## Repository Structure
This repository has the following two branches:
- The `master` branch hosts the dynamic only implementation of Memento filter,
  and does not contain the test suite for the expandability and B-Tree
  experiments.
- The `expandable` branch hosts the expandable implementation of Memento
  filter, as well as the test suite for the expandability and B-Tree
  experiments.

## Code Overview
~TO DO~

## Compiling Tests and Benchmarks

After cloning the repository and all its submodules with
```bash
git clone --recurse-submodules -j8 [url]
cd Memento_Filter
```

build the project with CMAKE
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
```

The benchmarks will be placed in `build/bench/`, see
[reproducibility.md](bench/reproducibility.md) for details on how to reproduce
the tests in the paper.

## License

This project is licensed under the GPLv3 License - see the [LICENSE](LICENSE)
file for details.

