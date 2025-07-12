#!/bin/bash

OPTIONS=("tiny" "small" "medium" "large")
if [[ "$#" -ge 1 ]] && ! printf "%s\n" "${OPTIONS[@]}" | grep -Fxq "$1"; then
    echo "Invalid parameters, usage: bash evaluate.sh [tiny|small|medium|large]"
    echo "Default size parameter is tiny"
    exit 1
fi

OPTION="tiny"
if printf "%s\n" "${OPTIONS[@]}" | grep -Fxq "$1"; then
    OPTION="$1"
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
if [[ $? -ne 0 ]]; then
    echo "Compilation of default suite failed"
    exit 1
fi

cd ../.. && mkdir -p paper_results && cd paper_results
bash ${project_root}/bench/scripts/download_datasets.sh
bash ${project_root}/bench/scripts/generate_datasets.sh ${project_root}/build real_datasets ${OPTION}
bash ${project_root}/bench/scripts/execute_tests.sh ${project_root}/build workloads

cd ${project_root} 
git checkout expandable
rm -rf build/*
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
if [[ $? -ne 0 ]]; then
    echo "Compilation of expandable suite failed"
    exit 1
fi
cd ..

cd ../paper_results
bash ${project_root}/bench/scripts/generate_datasets.sh ${project_root}/build real_datasets ${OPTION}
bash ${project_root}/bench/scripts/execute_tests.sh ${project_root}/build workloads ${OPTION}

cd ${project_root} 
git checkout master
cd ../paper_results/
python3 ${project_root}/bench/scripts/plot.py
