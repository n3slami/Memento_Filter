#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <stdio.h>
#include <string>

const uint32_t bigint_rng_seed = 1;
static std::mt19937 bigint_rng(bigint_rng_seed);

static union {
    uint32_t i;
    char c[4];
} bint = {0x01020304};
static const bool is_little_endian = (bint.c[0] != 1);

struct SimpleBigInt {
    uint32_t len;
    uint8_t *num;

    SimpleBigInt(uint32_t length) 
    { 
        len = length;
        num = new uint8_t[len];
        memset(num, 0, len);
        /*
        for (uint32_t i = 0; i < len; i += sizeof(uint32_t)) {
            uint32_t random_part = bigint_rng();
            memcpy(num + i, &random_part, sizeof(random_part));
        }
        */
    }

    ~SimpleBigInt()
    {
        delete num;
    }

    SimpleBigInt &operator=(uint64_t v) 
    {
        memcpy(num + len - sizeof(v), &v, sizeof(v));
        if (is_little_endian)
            std::reverse(num + len - sizeof(v), num + len);
        return *this;
    }

    SimpleBigInt &operator=(const SimpleBigInt &other) 
    {
        memcpy(num, other.num, len);
        return *this;
    }

    SimpleBigInt &operator+=(const uint64_t other) 
    {
        uint64_t other_copy = other;
        for (uint32_t i = 0, carry = 0; i < len && (i < sizeof(other) || carry); i++) {
            uint32_t new_byte = num[len - i - 1] + carry 
                                + (i < sizeof(other) ? (other_copy & 255) : 0);
            other_copy >>= 8;
            carry = new_byte >= 256;
            num[len - i - 1] = new_byte - carry * 256;
        }
        return *this;
    }

    SimpleBigInt &operator+=(const SimpleBigInt &other) 
    {
        for (uint32_t i = 0, carry = 0; i < len && (i < other.len || carry); i++) {
            uint32_t new_byte = num[len - i - 1] + carry 
                                + (i < other.len ? other.num[other.len - i - 1] : 0);
            carry = new_byte >= 256;
            num[len - i - 1] = new_byte - carry * 256;
        }
        return *this;
    }

    friend SimpleBigInt operator+(SimpleBigInt a, const uint64_t b) 
    {
        a += b;
        return a;
    }

    friend SimpleBigInt operator+(SimpleBigInt a, const SimpleBigInt &b) 
    {
        a += b;
        return a;
    }

    SimpleBigInt &randomize()
    {
        for (uint32_t i = 0; i < len; i += sizeof(uint32_t)) {
            uint32_t random_part = bigint_rng();
            memcpy(num + i, &random_part, sizeof(random_part));
        }
        return *this;
    }
};

