#!/bin/bash

if [[ "$#" -ge 2 ]] && [ "$1" != "small" ]; then
    echo "$#"
    echo "$1"
    echo "Invalid parameters, usage: bash evaluate.sh [small]"
    exit 1
fi

SMALL=""
if [[ "$1" -eq "small" ]]; then
    SMALL="small"
fi

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

cd ../.. && mkdir -p paper_results && cd paper_results
bash ${project_root}/bench/scripts/download_datasets.sh
bash ${project_root}/bench/scripts/generate_datasets.sh ${project_root}/build real_datasets ${SMALL}
#bash ${project_root}/bench/scripts/execute_tests.sh ${project_root}/build workloads

: '
cd ../Memento_Filter/
git checkout expandable
rm -rf build/*
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
cd ..

cd ../paper_results
bash ${project_root}/bench/scripts/generate_datasets.sh ${project_root}/build real_datasets ${SMALL}
bash ${project_root}/bench/scripts/execute_tests.sh ${project_root}/build workloads ${SMALL}

cd ../Memento_Filter/
git checkout master
cd ../paper_results/
python3 ${project_root}/bench/scripts/plot.py
'

