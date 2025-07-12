#!/bin/bash

git submodule update --init ../include/grafite/
cd ../include/grafite/
git submodule update --init lib/sux/
git submodule update --init lib/sdsl-lite/
cd -

git submodule update --init ../include/SuRF/

git submodule update --init ../include/Proteus/

FIX_RENCODER=0
if [ -z "$(ls -A ../include/REncoder/)" ]; then
    FIX_RENCODER=1
fi
git submodule update --init ../include/REncoder/
if [[ "${FIX_RENCODER}" -eq 1 ]]; then
    # The original REncoder implementation relies on undefined behavior. Rectifying it...
    sed -i "300s/.*/\\t\\t\\tint64_t u = (p \& 0x000000000000000FU) | 0xFFFFFFFFFFFFFFE0U;/" ../include/REncoder/src/REncoder.h
    sed -i "301s/.*/\\t\\t\\tbt |= sl <= 1 \&\& el >= 1 ? (1u << (-u)) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "302s/.*/\\t\\t\\tbt |= sl <= 2 \&\& el >= 2 ? (1u << (-(u >> 1))) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "303s/.*/\\t\\t\\tbt |= sl <= 3 \&\& el >= 3 ? (1u << (-(u >> 2))) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "304s/.*/\\t\\t\\tbt |= sl <= 4 \&\& el >= 4 ? (1u << (-(u >> 3))) : 0;/" ../include/REncoder/src/REncoder.h

    sed -i "320s/.*/\\t\\t\\tint64_t u = (p \& 0x000000000000000FU) | 0xFFFFFFFFFFFFFFE0U;/" ../include/REncoder/src/REncoder.h
    sed -i "323s/.*/\\t\\t\\t\\tbt |= level >= 1 ? (1u << (-u)) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "324s/.*/\\t\\t\\t\\tbt |= level >= 2 ? (1u << (-(u >> 1))) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "325s/.*/\\t\\t\\t\\tbt |= level >= 3 ? (1u << (-(u >> 2))) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "326s/.*/\\t\\t\\t\\tbt |= level >= 4 ? (1u << (-(u >> 3))) : 0;/" ../include/REncoder/src/REncoder.h
    sed -i "332s/.*/\\t\\t\\t\\tbt |= (1u << (-u));/" ../include/REncoder/src/REncoder.h
    sed -i "333s/.*/\\t\\t\\t\\tbt |= (1u << (-(u >> 1)));/" ../include/REncoder/src/REncoder.h
    sed -i "334s/.*/\\t\\t\\t\\tbt |= (1u << (-(u >> 2)));/" ../include/REncoder/src/REncoder.h
    sed -i "335s/.*/\\t\\t\\t\\tbt |= (1u << (-(u >> 3)));/" ../include/REncoder/src/REncoder.h

    sed -i "568s/.*/\\t\\tint64_t mask = ~((1ll << (level + 2)) - 1);/" ../include/REncoder/src/REncoder.h
    sed -i "571s/.*/\\t\\tuint32_t res = (rbf->querybt((p >> level + 1) + (plen - level - 1 >> 2) * 1000000007) >> (-(p \& (~(1ll << level + 1)) | mask))) \& 1;/" ../include/REncoder/src/REncoder.h

    sed -i "2i #include <cstdio>" ../include/REncoder/src/REncoder.h
fi

git submodule update --init ../include/SNARF/

FIX_OASIS=0
if [ -z "$(ls -A ../include/Oasis-RangeFilter/)" ]; then
    FIX_OASIS=1
fi
git submodule update --init ../include/Oasis-RangeFilter/
if [[ "${FIX_OASIS}" -eq 1 ]]; then
    sed -i "3i #include <cstddef>" ../include/Oasis-RangeFilter/src/include/learned_rf/bitset.h
    sed -i '/add_subdirectory(benchmark)/d' ../include/Oasis-RangeFilter/CMakeLists.txt
fi

FIX_RSQF=0
if [ -z "$(ls -A ../include/cqf/)" ]; then
    FIX_RSQF=1
fi
git submodule update --init ../include/cqf/
if [[ "${FIX_OASIS}" -eq 1 ]]; then
    # Seems like they forgot to zero out the memory they allocate, but assume
    # that it's zero... let's fix that
    sed -i "1650s/.*/memset(buffer, 0, total_num_bytes);/" ../include/cqf/src/gqf.c
fi

