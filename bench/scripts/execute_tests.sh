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
    echo "Illegal number of parameters, usage: execute_tests.sh <grafite_path> <datasets_path>"
fi

MEMENTO_BUILD_PATH=$(realpath $1)
WORKLOADS_PATH=$(realpath $2)

# ARGS="--numa --membind 0 --physcpubind 16 -i" # uncomment to run with numa
ARGS="" # comment to run without numa

SCRIPT_DIR_PATH=$(dirname -- "$( readlink -f -- "$0"; )")

OUT_PATH=./results

mkdir -p $OUT_PATH && cd $OUT_PATH || exit 1
if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test b_tree $WORKLOADS_PATH/b_tree_test $MEMENTO_BUILD_PATH ; then
  echo "[!!] b_tree_test test failed"
  exit 1
fi
echo "[!!] b_tree_test test executed successfully"

exit 0

mkdir -p $OUT_PATH && cd $OUT_PATH || exit 1
if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test expansion $WORKLOADS_PATH/expansion_test $MEMENTO_BUILD_PATH ; then
  echo "[!!] expansion_test test failed"
  exit 1
fi
echo "[!!] expansion_test test executed successfully"

:'
if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test corr $WORKLOADS_PATH/corr_test $MEMENTO_BUILD_PATH ; then
  echo "[!!] corr_test test failed"
  exit 1
fi
echo "[!!] corr_test (figure 1,3) test executed successfully"
'

echo "[!!] success, all tests executed"
