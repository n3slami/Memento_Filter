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

FIGURES="fpr,construction,true,correlated,vary_memento_size,expandability,btree"

function print_help_message_exit() {
    echo "Usage: execute_tests.sh <memento_path> <datasets_path> [-f|--figures fpr,construction,true,correlated,vary_memento_size,expandability,btree]"
    echo "The figures parameter is a list of comma-separated names describing what experiments to run:"
    echo "      - fpr:               measures FPR and query speed                                             (Fig.  9 in the paper)"
    echo "      - construction:      measures construction time                                               (Fig. 11 in the paper)"
    echo "      - true:              measures query speed of non-empty range queries                          (Fig. 10 in the paper)"
    echo "      - correlated:        measures FPR and query speed correlated worklaods                        (Fig.  8 in the paper)"
    echo "      - vary_memento_size: measures FPR and speed of Memento filter given different memento sizes   (Fig. 12 in the paper)"
    echo "By default, all figures are generated"
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

if [[ "$FIGURES" == *"correlated"* ]]; then 
    if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test corr $WORKLOADS_PATH/corr_test $MEMENTO_BUILD_PATH ; then
        echo "[!!] corr_test test failed"
        exit 1
    fi
    echo "[!!] corr_test (Fig. 8) test executed successfully"
fi

if [[ "$FIGURES" == *"fpr"* ]]; then 
    if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test fpr $WORKLOADS_PATH/fpr_test $MEMENTO_BUILD_PATH ; then
        echo "[!!] fpr_test test failed"
        exit 1
    fi
    echo "[!!] fpr_test (Fig. 9) test executed successfully"
    if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test fpr_real $WORKLOADS_PATH/fpr_real_test $MEMENTO_BUILD_PATH ; then
        echo "[!!] fpr_real_test test failed"
        exit 1
    fi
    echo "[!!] fpr_real_test (Fig. 9) test executed successfully"
fi 

if [[ "$FIGURES" == *"vary_memento_size"* ]]; then 
    if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test vary_memento $WORKLOADS_PATH/vary_memento_test $MEMENTO_BUILD_PATH ; then
        echo "[!!] vary_memento_test test failed"
        exit 1
    fi
    echo "[!!] vary_memento_test (Fig. 12) test executed successfully"
fi

if [[ "$FIGURES" == *"true"* ]]; then 
    if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test true $WORKLOADS_PATH/true_test $MEMENTO_BUILD_PATH ; then
        echo "[!!] true_test test failed"
        exit 1
    fi
    echo "[!!] true_test (Fig. 10) test executed successfully"
fi 

if [[ "$FIGURES" == *"construction"* ]]; then 
    if ! python3 $SCRIPT_DIR_PATH/test.py $ARGS --test constr_time $WORKLOADS_PATH/constr_time_test $MEMENTO_BUILD_PATH ; then
        echo "[!!] constr_time_test test failed"
        exit 1
    fi
    echo "[!!] constr_time_test (Fig. 11) test executed successfully"
fi

echo "[!!] success, all tests executed"
