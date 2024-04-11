#! /bin/bash

#
# This file is part of Grafite <https://github.com/marcocosta97/grafite>.
# Copyright (C) 2023 Marco Costa.
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

if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters, usage: generate_datasets.sh <grafite_build_path> <real_datasets_path>"
fi

GRAFITE_BUILD_PATH=$(realpath $1)
if [ ! -d "$GRAFITE_BUILD_PATH" ]; then
  echo "Grafite build path does not exist"
  exit 1
fi
REAL_DATASETS_PATH=$(realpath $2)
if [ ! -d "$REAL_DATASETS_PATH" ]; then
  echo "Real datasets path does not exist"
  exit 1
fi

WORKLOAD_GEN_PATH=$(realpath $GRAFITE_BUILD_PATH/bench/workload_gen)
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
    $WORKLOAD_GEN_PATH --mixed --kdist kuniform --qdist qcorrelated --corr-degree ${x}
    mv kuniform/ kuniform_${i}/
    x=$(echo $x + 0.1 | bc)
    i=$(($i + 1))
  done
}

generate_constr_time_test() {
  i=5
  x=100000
  while [ $i -le 8 ]
  do
    $WORKLOAD_GEN_PATH --kdist kuniform --qdist quniform --range-size 5 -n ${x} -q $(echo "($x * 0.1)/1" | bc)
    mv kuniform/ kuniform_${i}/
    x=$(echo "$x * 10" | bc)
    i=$(($i + 1))
  done
}

generate_expansion_test() {
  expansion_count=6

  $WORKLOAD_GEN_PATH --mixed --kdist kuniform --qdist quniform --expansion-count ${expansion_count}
  mv kuniform/ kuniform_0/
  $WORKLOAD_GEN_PATH --mixed --kdist kuniform --qdist qcorrelated --corr-degree 0.2 --expansion-count ${expansion_count}
  mv kuniform/ kuniform_2/
}

generate_b_tree_test() {
  expansion_count=3
  true_frac_count=10

  #$WORKLOAD_GEN_PATH --mixed --kdist kuniform -n 100000000 --qdist quniform -q 5000000 --range-size 0 5 --expansion-count ${expansion_count} --true-frac-count ${true_frac_count}
  #$WORKLOAD_GEN_PATH --mixed --kdist knormal -n 100000000 --qdist qnormal -q 5000000 --range-size 0 5 --expansion-count ${expansion_count} --true-frac-count ${true_frac_count}
  $WORKLOAD_GEN_PATH --mixed --binary-keys $REAL_DATASETS_PATH/books_200M_uint64 -q 10000000 --range-size 0 5 --expansion-count ${expansion_count} --true-frac-count ${true_frac_count}
}

mkdir -p $OUT_PATH/expansion_test && cd $OUT_PATH/expansion_test || exit 1
if ! generate_expansion_test ; then
  echo "[!!] expansion_test generation failed"
  exit 1
fi
echo "[!!] expansion_test dataset generated"

mkdir -p $OUT_PATH/b_tree_test && cd $OUT_PATH/b_tree_test || exit 1
if ! generate_b_tree_test ; then
  echo "[!!] b_tree_test generation failed"
  exit 1
fi
echo "[!!] b_tree_test dataset generated"

: '
mkdir -p ../corr_test && cd ../corr_test || exit 1
if ! generate_corr_test ; then
  echo "[!!] corr_test generation failed"
  exit 1
fi
echo "[!!] corr_test (figure 1, 5) dataset generated"
mkdir -p ../lemma_test && cd ../lemma_test || exit 1
if ! $WORKLOAD_GEN_PATH -n 10000000 -q 1000000 --qdist quniform --range-size $(seq 0 16) ; then
  echo "[!!] lemma_test generation failed"
  exit 1
fi
echo "[!!] lemma_test (figure 2) dataset generated"
mkdir -p ../fpr_test && cd ../fpr_test || exit 1
if ! $WORKLOAD_GEN_PATH --mixed ; then
  echo "[!!] fpr_test generation failed"
  exit 1
fi
echo "[!!] fpr_test (figure 3) dataset generated"
mkdir -p ../fpr_real_test && cd ../fpr_real_test || exit 1
if ! $WORKLOAD_GEN_PATH --mixed --binary-keys $REAL_DATASETS_PATH/books_200M_uint64 $REAL_DATASETS_PATH/fb_200M_uint64 $REAL_DATASETS_PATH/osm_cellids_200M_uint64 ; then
  echo "[!!] fpr_real_test generation failed"
  exit 1
fi
echo "[!!] fpr_real_test (figure 4) dataset generated"
mkdir -p ../true_test && cd ../true_test || exit 1
if ! $WORKLOAD_GEN_PATH --mixed --qdist qtrue ; then
  echo "[!!] true_test generation failed"
  exit 1
fi
echo "[!!] true_test (figure 6) dataset generated"
mkdir -p ../constr_time_test && cd ../constr_time_test || exit 1
if ! generate_constr_time_test ; then
  echo "[!!] constr_time_test generation failed"
  exit 1
fi
echo "[!!] constr_time_test (figure 7) dataset generated"
if ! generate_workload_shift_test ; then
  echo "[!!] workload_shift_test generation failed"
  exit 1
fi
echo "[!!] workload_shift_test dataset generated"
'
echo "[!!] success, all datasets generated"
