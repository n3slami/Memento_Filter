#!/bin/bash

if [ "$#" -ne 1 || "$1" -ne "small" ]; then
    echo "Invalid parameters, usage: evaluate.sh [small]"
fi

SMALL=""
if [ "$1" -eq "small" ]; then
    SMALL="small"
fi

project_root=$(pwd)
pushd

git branch -r \
  | grep -v '\->' \
  | sed "s,\x1B\[[0-9;]*[a-zA-Z],,g" \
  | while read remote; do \
      git branch --track "${remote#origin/}" "$remote"; \
    done
git fetch --all
git pull --all

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8

cd .. && mkdir paper_results && cd paper_results
bash ${project_root}/bench/scripts/download_datasets.sh
bash ${project_root}/bench/scripts/generate_datasets.sh ${project_root}/build real_datasets ${SMALL}
python3 ${project_root}/bench/scripts/run_benchmarks.py ${project_root}/build workloads

popd
pushd
git checkout expandable
rm -rf build/*
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
cd ..

cd ../paper_results
bash ${project_root}/bench/scripts/generate_datasets.sh ${project_root}/build real_datasets ${SMALL}
python3 ${project_root}/bench/scripts/run_benchmarks.py ${project_root}/build workloads

popd
git checkout master
python3 ${project_root}/bench/scripts/plot.py

