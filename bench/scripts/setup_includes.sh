#!/bin/bash

git submodule update --init ../include/grafite/
cd ../include/grafite/
git submodule update --init lib/sux/
git submodule update --init lib/sdsl-lite/
cd -

git submodule update --init ../include/SuRF/

git submodule update --init ../include/Proteus/

git submodule update --init ../include/REncoder/

git submodule update --init ../include/SNARF/

git submodule update --init ../include/Oasis-RangeFilter/
sed -i '/add_subdirectory(benchmark)/d' ../include/Oasis-RangeFilter/CMakeLists.txt

git submodule update --init ../include/cqf/

