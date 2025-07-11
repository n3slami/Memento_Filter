#! /bin/bash

#
# This file is part of Memento Filter <https://github.com/n3slami/Memento_Filter>.
# Copyright (C) 2024 Navid Eslami.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

if [[ "$#" -ne 2 ]] && [[ "$#" -ne 3 ]]; then
    echo "Invalid number of parameters, usage: generate_datasets.sh <memento_build_path> <real_datasets_path> [tiny|small|medium|large]"
    exit 1
fi
OPTIONS=("tiny" "small" "medium" "large")
if [[ "$#" -eq 3 ]] && ! printf "%s\n" "${OPTIONS[@]}" | grep -Fxq "$3"; then
    echo "Invalid parameters, usage: generate_datasets.sh <memento_build_path> <real_datasets_path> [tiny|small|medium|large]"
    exit 1
fi

N_KEYS=200000000
N_QUERIES=10000000
if [[ "$#" -eq 3 ]] && [ "$3" == "${OPTIONS[0]}" ]; then
    N_KEYS=200000
    N_QUERIES=10000
elif [[ "$#" -eq 3 ]] && [ "$3" == "${OPTIONS[1]}" ]; then
    N_KEYS=2000000
    N_QUERIES=100000
elif [[ "$#" -eq 3 ]] && [ "$3" == "${OPTIONS[2]}" ]; then
    N_KEYS=20000000
    N_QUERIES=1000000
fi

MEMENTO_BUILD_PATH=$(realpath $1)
if [ ! -d "$MEMENTO_BUILD_PATH" ]; then
  echo "Grafite build path does not exist"
  exit 1
fi
REAL_DATASETS_PATH=$(realpath $2)
if [ ! -d "$REAL_DATASETS_PATH" ]; then
  echo "Real datasets path does not exist"
  exit 1
fi

WORKLOAD_GEN_PATH=$(realpath $MEMENTO_BUILD_PATH/bench/workload_gen)
if [ ! -f "$WORKLOAD_GEN_PATH" ]; then
  echo "Workload generator does not exist"
  exit 1
fi

OUT_PATH=./workloads
generate_corr_test() {
  i=0
  x=0.0

  while [ $i -le 10 ]
  do
    $WORKLOAD_GEN_PATH -n ${N_KEYS} -q ${N_QUERIES} --mixed --kdist kuniform --qdist qcorrelated --corr-degree ${x}
    if [ -d "kuniform_${i}/" ]; then
        rm -rf kuniform_${i}/
    fi
    mv kuniform/ kuniform_${i}/
    x=$(echo $x + 0.1 | bc)
    i=$(($i + 1))
  done
}

generate_true_test() {
  $WORKLOAD_GEN_PATH -n ${N_KEYS} -q ${N_QUERIES} --kdist kuniform --mixed --qdist qtrue
  #$WORKLOAD_GEN_PATH -n ${N_KEYS} -q ${N_QUERIES} --kdist knormal --mixed --qdist qtrue
  #$WORKLOAD_GEN_PATH -n ${N_KEYS} -q ${N_QUERIES} --mixed --binary-keys $REAL_DATASETS_PATH/books_200M_uint64 $REAL_DATASETS_PATH/osm_cellids_200M_uint64 --qdist qtrue
}

generate_constr_time_test() {
  i=5
  x=100000
  if [[ $N_KEYS -ne 200000000 ]]; then
      x=10000
  fi
  while [ $i -le 8 ]
  do
    $WORKLOAD_GEN_PATH --kdist kuniform --qdist quniform --range-size 5 -n ${x} -q $(echo "($x * 0.1)/1" | bc)
    if [ -d "kuniform_${i}/" ]; then
        rm -rf kuniform_${i}/
    fi
    mv kuniform/ kuniform_${i}/
    x=$(echo "$x * 10" | bc)
    i=$(($i + 1))
  done
}

generate_memento_vary_test() {
  $WORKLOAD_GEN_PATH -n ${N_KEYS} -q ${N_QUERIES} --mixed --range-size 0 5 --binary-keys $REAL_DATASETS_PATH/books_200M_uint64 $REAL_DATASETS_PATH/osm_cellids_200M_uint64
  $WORKLOAD_GEN_PATH -n ${N_KEYS} -q ${N_QUERIES} --mixed --range-size 0 5 --kdist kuniform --qdist qcorrelated --corr-degree 0.8
}

mkdir -p $OUT_PATH/corr_test && cd $OUT_PATH/corr_test || exit 1
if ! generate_corr_test ; then
  echo "[!!] corr_test generation failed"
  exit 1
fi
echo "[!!] corr_test (figure 8) dataset generated"
mkdir -p ../fpr_test && cd ../fpr_test || exit 1
if ! $WORKLOAD_GEN_PATH -n ${N_KEYS} -q ${N_QUERIES} --mixed ; then
  echo "[!!] fpr_test generation failed"
  exit 1
fi
echo "[!!] fpr_test (figure 9) dataset generated"
mkdir -p ../fpr_real_test && cd ../fpr_real_test || exit 1
if ! $WORKLOAD_GEN_PATH -n ${N_KEYS} -q ${N_QUERIES} --mixed --binary-keys $REAL_DATASETS_PATH/books_200M_uint64 $REAL_DATASETS_PATH/fb_200M_uint64 $REAL_DATASETS_PATH/osm_cellids_200M_uint64 ; then
  echo "[!!] fpr_real_test generation failed"
  exit 1
fi
echo "[!!] fpr_real_test (figure 9) dataset generated"
mkdir -p ../true_test && cd ../true_test || exit 1
if ! generate_true_test ; then
  echo "[!!] true_test generation failed"
  exit 1
fi
echo "[!!] true_test (figure 10) dataset generated"
mkdir -p ../constr_time_test && cd ../constr_time_test || exit 1
if ! generate_constr_time_test ; then
  echo "[!!] constr_time_test generation failed"
  exit 1
fi
echo "[!!] constr_time_test (figure 11) dataset generated"
mkdir -p ../vary_memento_test && cd ../vary_memento_test || exit 1
if ! generate_memento_vary_test ; then
  echo "[!!] memento_vary_test generation failed"
  exit 1
fi
echo "[!!] memento_vary_test (figure 12) dataset generated"
echo "[!!] success, all datasets generated"

