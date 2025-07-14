#!/bin/bash

FIGURE_OPTIONS=("fpr" "construction" "true" "correlated" "vary_memento_size" "expandability" "btree")
SIZE_OPTIONS=("tiny" "small" "medium" "large")

FIGURES="fpr,construction,true,correlated,vary_memento_size,expandability,btree"
SIZE="tiny"

function print_help_message_exit() {
    echo "Usage: evaluate.sh [-f|--figures fpr,construction,true,correlated,vary_memento_size,expandability,btree] [-s|--size tiny|small|medium|large]"
    echo "The figures parameter is a list of comma-separated names describing what experiments to run:"
    echo "      - fpr:               measures FPR and query speed                                             (Fig.  9 in the paper)"
    echo "      - construction:      measures construction time                                               (Fig. 11 in the paper)"
    echo "      - true:              measures query speed of non-empty range queries                          (Fig. 10 in the paper)"
    echo "      - correlated:        measures FPR and query speed correlated worklaods                        (Fig.  8 in the paper)"
    echo "      - vary_memento_size: measures FPR and speed of Memento filter given different memento sizes   (Fig. 12 in the paper)"
    echo "      - expandability:     measures FPR and insert speed under growing dataset                      (Fig. 13 in the paper)"
    echo "      - btree:             measures end-to-end B-tree performance when using Memento filter         (Fig. 14 in the paper)"
    echo "By default, all figures are generated"
    echo "Default size parameter is tiny"
    exit $1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--size)
            SIZE="$2"
            if ! printf "%s\n" "${SIZE_OPTIONS[@]}" | grep -Fxq "$SIZE"; then
                echo "Unknown size $SIZE"
                print_help_message_exit 1;
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

project_root=$(pwd)

git branch -r \
  | grep -v '\->' \
  | sed "s,\x1B\[[0-9;]*[a-zA-Z],,g" \
  | while read remote; do \
      git branch --track "${remote#origin/}" "$remote"; \
    done
git fetch --all
git pull --all

mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
if [[ $? -ne 0 ]]; then
    echo "Compilation of default suite failed"
    exit 1
fi

cd ../.. && mkdir -p paper_results && cd paper_results
bash ${project_root}/bench/scripts/download_datasets.sh
bash ${project_root}/bench/scripts/generate_datasets.sh ${project_root}/build real_datasets -f ${FIGURES} -s ${SIZE}
bash ${project_root}/bench/scripts/execute_tests.sh ${project_root}/build workloads -f ${FIGURES}

cd ${project_root} 
git checkout expandable
rm -rf build/*
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
if [[ $? -ne 0 ]]; then
    echo "Compilation of expandable suite failed"
    cd ${project_root} 
    git checkout master
    exit 1
fi
cd ..

cd ../paper_results
bash ${project_root}/bench/scripts/generate_datasets.sh ${project_root}/build real_datasets -f ${FIGURES} -s ${SIZE}
bash ${project_root}/bench/scripts/execute_tests.sh ${project_root}/build workloads -f ${FIGURES} -s ${SIZE}

cd ${project_root} 
git checkout master
cd ../paper_results/
python3 ${project_root}/bench/scripts/plot.py -f ${FIGURES}
