# Memento Filter

<p align="center">
Memento Filter is the first range filter to support dynamic datasets, constant
time operations, and a theoretically optimal false positive rate under any of
workload, all at the same time. 
</p>

## Quick Reproducibility
The `master` branch in the repository contains an `evaluate.sh` script. This
script downloads the required datasets, compiles Memento filter, sets the
benchmarks up, runs them, and plots the results. To ease the reproducibility
process, this branch also provides a `Dockerfile` that spins up a Ubuntu Docker
container with the requirements installed and runs `evaluate.sh` to gather the
results. One may choose to run the experiments in Docker, or to run the
experiments natively. We provide instructions on how to do both. We advise
using a Linux machine with at least 32GB of RAM.

### Docker Route
Simply run the following commands:
```bash
git clone https://github.com/n3slami/Memento_Filter.git
cd Memento_Filter
docker build -t memento_eval .
mkdir -p ../paper_results/figures/ && docker run -v ../paper_results/figures:/usr/local/paper_results/figures -it memento_eval
```
These commands first clone the repository and builds the required Docker image.
Then, it creates a directory **next to the cloned repository** where the
generated figures will be placed and spins up a Docker container to run the
experiments. One the container is done running, the generated figures will be
placed in the `paper_results/figures` directory. The figures will be numbered
to match the paper.

**Important:** By default, this Docker container takes multiple days to finish
running. To run the experiments on a smaller scale that completes in ~10
minutes, comment out the final line in the `Dockerfile` and repeat the build
and run process above.

### Native Route
To use the `evaluate.sh` script, ensure that the following dependencies are
satisfied:
- CMake >=3.30.8        (Used to compile the baselines)
- Boost >=1.67.0        (Used in some of the baselines, notably Grafite)
- libssl-dev >=1:3.2.4  (Used in WiredTiger)
- python3-dev >=3.13.5  (Used in WiredTiger)
- python3-env >=3.13.5  (Used to plot the experimental results only)
- Python 3 >=3.7        (Used to plot the experimental results only)
- Latex full            (Used to plot the experimental results only)
Then, running
```bash
git clone https://github.com/n3slami/Memento_Filter.git
cd Memento_Filter
bash evaluate.sh -s large
```
will reproduce the results in the paper, creating a `paper_results` directory
**next to the cloned repository** with a `figures` subdirectory containing the
generated figures. The figures will be numbered to match the paper. The script
may take several days to run due to the large scale of the experiments. To
conduct a smaller-scale evaluation, the `evaluate.sh` script accepts the `-s`
position argument with the following options:
```bash 
tiny        # (The default. Takes ~10 minutes to run. Works with datasets of size 200K.)
small       # (Takes ~1 hour to run. Works with datasets of size 2M.)
medium      # (Takes ~1 day to run. Works with datasets of size 20M.)
large       # (Takes multiple days to run. Works with datasets of size 200M.)
```
For example, executing `bash evaluate.sh -s tiny` runs the experimental suite
with datasets that are 1000 times smaller than the original. This option is the
default so that one can ensure the suite is working properly without having to
wait a long amount of time for the execution to finish.

The `evaluate.sh` also allows one to only generate a subset of the figures
using the `-f` option. Running `bash evaluate.sh -h` provides more information
on how to use this script.

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


