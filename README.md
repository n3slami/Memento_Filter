# Memento Filter

<p align="center">
Memento Filter is the first range filter to support dynamic datasets, constant
time operations, and a theoretically optimal false positive rate under any of
workload, all at the same time. 
</p>

## Quick Reproducibility
### Requirements
For ease of reproducibility, the `master` branch in the repository contains an
`evaluate.sh` script. This script downloads the required datasets, compiles
Memento filter, sets the benchmarks up, runs them, and plots the results. To
use this script, ensure that the following dependencies are satisfied:
- Boost >=1.67.0        (Used in some of the baselines, notably Grafite)
- Python >=3.7          (Used to plot the experimental results only)
  - Matplotlib >=3.2.1
  - Numpy >=1.18.2
  - Pandas >=1.0.3
We also advise using a Linux machine with at least 32GB of RAM. Then, running
```bash
git clone https://github.com/n3slami/Memento_Filter.git
cd Memento_Filter
bash evaluate.sh
```
will reproduce the results in the paper. The script may take several days to
run due to the large scale of the experiments. To conduct a smaller-scale
evaluation, the `evaluate.sh` script accepts the following parameters:
```bash 
tiny        # (Takes ~5 minutes to run. Works with datasets of size 200K.)
small       # (Takes ~1 hour to run. Works with datasets of size 2M.)
medium      # (Takes ~1 day to run. Works with datasets of size 20M.)
large       # (The default. Takes multiple days to run. Works with datasets of size 200M.)
```
For example, executing `bash evaluate.sh tiny` runs the experimental suite with
datasets that are 1000 times smaller than the original.

This script creates a `paper_results` directory with the datasets and workloads
and places it next to the cloned repository. The generated figures are placed
in `paper_results/figures`, where each is numbered similarly to the paper.

The [reproducibility.md](bench/reproducibility.md) file explains the details of
how the `evaluate.sh` script works internally, as well as how one can manually
emulate the process.

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
This repository has the following branches:
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
as described above to reproduce the results in the paper, and see
[reproducibility.md](bench/reproducibility.md) for details on its inner
workings.

## License

This project is licensed under the GPLv3 License - see the [LICENSE](LICENSE)
file for details.


