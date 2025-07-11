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
    echo "Invalid number of parameters, usage: execute_tests.sh <memento_path> <datasets_path> [tiny|small|medium|large]"
    exit 1
fi
OPTIONS=("tiny" "small" "medium" "large")
if [[ "$#" -eq 3 ]] && ! printf "%s\n" "${OPTIONS[@]}" | grep -Fxq "$3"; then
    echo "Invalid parameters, usage: execute_tests.sh <memento_path> <datasets_path> [tiny|small|medium|large]"
    exit 1
fi

OPTION="large"
if [[ "$#" -eq 3 ]] && printf "%s\n" "${OPTIONS[@]}" | grep -Fxq "$3"; then
    OPTION="$3"
fi

MEMENTO_BUILD_PATH=$(realpath $1)
WORKLOADS_PATH=$(realpath $2)

# ARGS="--numa --membind 0 --physcpubind 16 -i" # uncomment to run with numa
ARGS="" # comment to run without numa

SCRIPT_DIR_PATH=$(dirname -- "$( readlink -f -- "$0"; )")

OUT_PATH=./results

mkdir -p $OUT_PATH && cd $OUT_PATH || exit 1
: '
if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test expansion $WORKLOADS_PATH/expansion_test $MEMENTO_BUILD_PATH ; then
  echo "[!!] expansion_test test failed"
  exit 1
fi
echo "[!!] expansion_test test executed successfully"
'

if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test b_tree $WORKLOADS_PATH/b_tree_test $MEMENTO_BUILD_PATH --benchmark_size ${OPTION} ; then
  echo "[!!] b_tree_test test failed"
  exit 1
fi
echo "[!!] b_tree_test test executed successfully"


echo "[!!] success, all tests executed"
