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

FIGURE_OPTIONS=("fpr" "construction" "true" "correlated" "vary_memento_size" "expandability" "btree")
SIZE_OPTIONS=("tiny" "small" "medium" "large")

FIGURES="expandability,btree"
SIZE="tiny"

function print_help_message_exit() {
    echo "Usage: generate_datasets.sh <memento_build_path> <real_datasets_path> [-f|--figures expandability,btree] [-s tiny|small|medium|large]"
    echo "The figures parameter is a list of comma-separated names describing what workloads to generate:"
    echo "      - expandability:     measures FPR and insert speed under growing dataset                      (Fig. 13 in the paper)"
    echo "      - btree:             measures end-to-end B-tree performance when using Memento filter         (Fig. 14 in the paper)"
    echo "By default, all figures are generated"
    echo "Default size parameter is tiny"
    exit $1
}

if [[ "$#" -lt 2 ]]; then
    echo "Too few parameters"
    print_help_message_exit 1
fi
MEMENTO_BUILD_PATH=$(realpath $1)
if [ ! -d "$MEMENTO_BUILD_PATH" ]; then
    echo "Memento build path does not exist"
    exit 1
fi
REAL_DATASETS_PATH=$(realpath $2)
if [ ! -d "$REAL_DATASETS_PATH" ]; then
    echo "Real datasets path does not exist"
    exit 1
fi
shift # past memento build path
shift # past real datasets path

while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--size)
            SIZE="$2"
            if ! printf "%s\n" "${SIZE_OPTIONS[@]}" | grep -Fxq "$SIZE"; then
                echo "Unknown size $SIZE"
                print_help_message_exit 1
            fi
            shift # past argument
            shift # past value
            ;;
        -f|--figures)
            FIGURES="$2"
            IFS="," read -ra FIGURES_ARRAY <<< "$FIGURES"
            for i in "${FIGURES_ARRAY[@]}"; do 
                if ! printf "%s\n" "${FIGURE_OPTIONS[@]}" | grep -Fxq "$i"; then
                    echo "Unknown figure $i"
                    print_help_message_exit 1
                fi
            done
            shift # past argument
            shift # past value
            ;;
        -*|--*)
            echo "Unknown option $1"
            print_help_message_exit 1
            ;;
        *)
            echo "Unknown argument $1"
            print_help_message_exit 1
            ;;
    esac
done

N_KEYS=200000000
N_QUERIES=10000000
if [ "$SIZE" == "${SIZE_OPTIONS[0]}" ]; then
    N_KEYS=200000
    N_QUERIES=10000
elif [ "$SIZE" == "${SIZE_OPTIONS[1]}" ]; then
    N_KEYS=2000000
    N_QUERIES=100000
elif [ "$SIZE" == "${SIZE_OPTIONS[2]}" ]; then
    N_KEYS=20000000
    N_QUERIES=1000000
fi

WORKLOAD_GEN_PATH=$(realpath $MEMENTO_BUILD_PATH/bench/workload_gen)
if [ ! -f "$WORKLOAD_GEN_PATH" ]; then
  echo "Workload generator does not exist"
  exit 1
fi

OUT_PATH=./workloads

generate_expansion_test() {
  expansion_count=6

  $WORKLOAD_GEN_PATH -n ${N_KEYS} -q ${N_QUERIES} --mixed --kdist kuniform --qdist qcorrelated --corr-degree 0.0 --expansion-count ${expansion_count}
  if [ -d "kuniform_0" ]; then
    rm -rf kuniform_0/
  fi
  mv kuniform/ kuniform_0/
  $WORKLOAD_GEN_PATH -n ${N_KEYS} -q ${N_QUERIES} --mixed --kdist kuniform --qdist qcorrelated --corr-degree 0.2 --expansion-count ${expansion_count}
  if [ -d "kuniform_2" ]; then
    rm -rf kuniform_2/
  fi
  mv kuniform/ kuniform_2/
}

generate_b_tree_test() {
  expansion_count=3
  true_frac_count=10

  n_keys=$(( ${N_KEYS} / 2 ))
  n_queries=$(( ${N_QUERIES} / 2 ))
  $WORKLOAD_GEN_PATH -n ${n_keys} -q ${n_queries} --mixed --kdist kuniform --qdist quniform --range-size 0 5 --expansion-count ${expansion_count} --true-frac-count ${true_frac_count}
  #$WORKLOAD_GEN_PATH -n ${n_keys} -q ${n_queries} --mixed --kdist knormal --qdist qnormal --range-size 0 5 --expansion-count ${expansion_count} --true-frac-count ${true_frac_count}
  #$WORKLOAD_GEN_PATH -n ${n_keys} -q ${n_queries} --mixed --binary-keys $REAL_DATASETS_PATH/books_200M_uint64 --range-size 0 5 --expansion-count ${expansion_count} --true-frac-count ${true_frac_count}
}

if [[ "$FIGURES" == *"expandability"* ]]; then 
    mkdir -p $OUT_PATH/expansion_test && cd $OUT_PATH/expansion_test || exit 1
    if ! generate_expansion_test ; then
      echo "[!!] expansion_test generation failed"
      exit 1
    fi
    echo "[!!] expansion_test (Fig. 13) dataset generated"
    cd -
fi

if [[ "$FIGURES" == *"btree"* ]]; then 
    mkdir -p $OUT_PATH/b_tree_test && cd $OUT_PATH/b_tree_test || exit 1
    if ! generate_b_tree_test ; then
        echo "[!!] b_tree_test generation failed"
        exit 1
    fi
    echo "[!!] b_tree_test (Fig. 14) dataset generated"
    cd -
fi

echo "[!!] success, all datasets generated"
