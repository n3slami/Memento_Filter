# Memento Filter

<p align="center">
Memento Filter is the first range filter to support dynamic datasets, constant
time operations, and a theoretically optimal false positive rate. It also
provides strong guarantees on the false positive rate under any kind of
workload, be it static or dynamic. 
</p>

## Quick Reproducibility
For ease of reproducibility, the `master` branch in the repository contains an
`evaluate.sh` script. This script downloads the required datasets, compiles
Memento filter, sets the benchmarks up, runs them, and plots the results. To
use this script, ensure that the requirements stated in
[reproducibility.md](bench/reproducibility.md) are installed and then run
```bash
git clone https://github.com/n3slami/Memento_Filter.git
cd Memento_Filter
bash evaluate.sh
```
This will reproduce the results in the paper, and may take several days to run
due to the large scale of the experiments. Supplying the `small` option to
script, i.e., executing `bash evaluate.sh small`, runs a shorter and
lower-scale evaluation with 200 times smaller experiments.

This script creates a `paper_results` directory with the datasets and workloads
and places it next to the cloned repository. The generated figures are placed
in `paper_results/figures`, where each is numbered similarly to the paper.

## Development

This is a header-only library and does not need to be installed. Just clone the
repository with
```bash
git clone https://github.com/n3slami/Memento_Filter.git
cd Memento_Filter
```
and switch to the `header-only` branch. You can use Memento filter by including
`include/memento.cpp` in your project.

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
- The `header-only` branch hosts a C++ header file with a templated
  implementation of Memento filter in the `include` directory. The template
  allows one to choose between a standard dynamic Memento filter or an
  expandable one.

## Compiling Tests and Benchmarks

After cloning the repository and all its submodules with
```bash
git clone --recurse-submodules -j8 https://github.com/n3slami/Memento_Filter.git
cd Memento_Filter
```
build the project with CMAKE
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
```
The benchmarks will be placed in `build/bench/`. Use the `evaluate.sh` script
as described aboe to reproduce the results in the paper, and see
[reproducibility.md](bench/reproducibility.md) for details reproducibility.

## License

This project is licensed under the GPLv3 License - see the [LICENSE](LICENSE)
file for details.


