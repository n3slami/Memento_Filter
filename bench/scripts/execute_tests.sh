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
    echo "Usage: execute_tests.sh <memento_path> <datasets_path> [-f|--figures expandability,btree] [-s tiny|small|medium|large]"
    echo "The figures parameter is a list of comma-separated names describing what workloads to run:"
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
WORKLOADS_PATH=$(realpath $2)
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
                    print_help_message_exit 1;
                fi
            done
            shift # past argument
            shift # past value
            ;;
        -*|--*)
            echo "Unknown option $1"
            print_help_message_exit 1;
            ;;
        *)
            echo "Unknown argument $1"
            print_help_message_exit 1;
            ;;
    esac
done

# ARGS="--numa --membind 0 --physcpubind 16 -i" # uncomment to run with numa
ARGS="" # comment to run without numa

SCRIPT_DIR_PATH=$(dirname -- "$( readlink -f -- "$0"; )")

OUT_PATH=./results

mkdir -p $OUT_PATH && cd $OUT_PATH || exit 1

if [[ "$FIGURES" == *"expandability"* ]]; then 
    if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test expansion $WORKLOADS_PATH/expansion_test $MEMENTO_BUILD_PATH ; then
        echo "[!!] expansion_test test failed"
        exit 1
    fi
    echo "[!!] expansion_test test executed successfully"
fi

if [[ "$FIGURES" == *"btree"* ]]; then 
    if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test b_tree $WORKLOADS_PATH/b_tree_test $MEMENTO_BUILD_PATH --benchmark_size ${SIZE} ; then
        echo "[!!] b_tree_test test failed"
        exit 1
    fi
    echo "[!!] b_tree_test test executed successfully"
fi


echo "[!!] success, all tests executed"
