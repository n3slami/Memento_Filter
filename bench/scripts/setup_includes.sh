#!/bin/bash

git submodule update --init ../include/grafite/
cd ../include/grafite/
git submodule update --init lib/sux/
git submodule update --init lib/sdsl-lite/
cd -

git submodule update --init ../include/SuRF/

git submodule update --init ../include/Proteus/

git submodule update --init ../include/REncoder/
if [ -z "$(ls -A ../include/REncoder/)" ]; then
    # The original REncoder implementation relies on undefined behavior. Rectifying it...
    sed -i "300s/.*/int64_t u = (p & 0x000000000000000FU) | 0xFFFFFFFFFFFFFFE0U;/" ../include/REncoder/src/REncoder.h
    sed -i "301s/.*/bt |= sl <= 1 && el >= 1 ? (1u << (-u)) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "302s/.*/bt |= sl <= 2 && el >= 2 ? (1u << (-(u >> 1))) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "303s/.*/bt |= sl <= 3 && el >= 3 ? (1u << (-(u >> 2))) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "304s/.*/bt |= sl <= 4 && el >= 4 ? (1u << (-(u >> 3))) : 0;/" ../include/REncoder/src/REncoder.h

    sed -i "320s/.*/int64_t u = (p & 0x000000000000000FU) | 0xFFFFFFFFFFFFFFE0U;/" ../include/REncoder/src/REncoder.h
    sed -i "323s/.*/bt |= level >= 1 ? (1u << (-u)) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "324s/.*/bt |= level >= 2 ? (1u << (-(u >> 1))) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "325s/.*/bt |= level >= 3 ? (1u << (-(u >> 2))) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "326s/.*/bt |= level >= 4 ? (1u << (-(u >> 3))) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "332s/.*/bt |= (1u << (-u));/" ../include/REncoder/src/REncoder.h
    sed -i "333s/.*/bt |= (1u << (-(u >> 1)));/" ../include/REncoder/src/REncoder.h
    sed -i "334s/.*/bt |= (1u << (-(u >> 2)));/" ../include/REncoder/src/REncoder.h
    sed -i "335s/.*/bt |= (1u << (-(u >> 3)));/" ../include/REncoder/src/REncoder.h

    sed -i "568s/.*/int64_t mask = ~((1ll << (level + 2)) - 1);/" ../include/REncoder/src/REncoder.h
    sed -i "571s/.*/uint32_t res = (rbf->querybt((p >> level + 1) + (plen - level - 1 >> 2) * 1000000007) >> (-(p & (~(1ll << level + 1)) | mask))) & 1;/" ../include/REncoder/src/REncoder.h

    sed -i "2i #include <cstdio>" filename
fi

git submodule update --init ../include/SNARF/

git submodule update --init ../include/Oasis-RangeFilter/
sed -i '/add_subdirectory(benchmark)/d' ../include/Oasis-RangeFilter/CMakeLists.txt

git submodule update --init ../include/cqf/

