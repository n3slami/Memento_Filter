# Memento Filter

<p align="center">
Memento Filter is the first range filter to support dynamic datasets, constant
time operations, and a theoretically optimal false positive rate. It also
provides strong guarantees on the false positive rate under any kind of
workload, be it static or dynamic. 
</p>

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
integers with Memento filter.

## Repository Structure
This repository has the following two branches:
- The `master` branch hosts the dynamic only implementation of Memento filter,
  and does not contain the test suite for the expandability and B-Tree
  experiments.
- The `expandable` branch hosts the expandable implementation of Memento
  filter, as well as the test suite for the expandability and B-Tree
  experiments.

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


