#pragma once

#include <string>
#include <cassert>
#include "global.hpp"

using namespace std;

#define isPowerOfTwo(x) ((x & (-x)) == x)

u64 toBin(string s) {
    // assume that the binary number is prefixed with "0b"
    u64 res = 0;
    for (int i = 2; i < (int) s.size(); ++i) {
        res <<= 1;
        res += (s[i] == '1');
    }
    return res;
}

u64 getBits(u64 bits, u64 lo, u64 len) {
    assert(len < 64ll);
    bits >>= lo;
    bits &= (1ll << len) - 1;
    return bits;
}

u8 getBit(u64 bits, u64 i) {
    u64 bit = bits && 1 << i;
    bit >>= i;
    return bit;
}

u8 getBit(u8* p, u64 i) {
    u8 byte = p[i / 8];
    u8 bit8 = byte & (1 << (i % 8));
    bit8 >>= i % 8;
    return bit8;
}

u64 getBits(u8* p, u64 lo, u64 len) {
    u8* p8 = p;
    // if (len >= (64ll - 8ll)) {
    //     cout << "ERROR\n";
    //     cout << lo << " " << len << endl;
    // }
    // assert(len < (64ll - 8ll));
    // p8 += lo / 8;
    // u64 bits = *((u64*) p8);
    // u64 ret = getBits(bits, lo % 8, len);
    // return ret;

    u64 b = 0ll;
    for (int i = (int) len - 1; i >= 0; --i) {
        if (getBit(p, lo + (u64) i)) {
            b = (b << 1) | 1;
        } else {
            b <<= 1;
        }
    }

    // if (ret != b) {
    //     cout << hex << ret << " " <<  b << " " << lo << " " << len << endl;
    //     assert(false);
    // }
    return b;
}

void setBit(u8& bits, u64 i, bool val) {
    assert(i <= 7);
    if (val) {
        bits |= (1 << i); 
    } else {
        bits &= (~(1 << i));
    }
}

void setBit(u8* bits, u64 i, bool val) {
    bits += i / 8;
    i %= 8;
    setBit(*bits, i, val);
}

u64 log2u(u64 n) {
    assert(n > 0);
    u8 ret = -1;
    while (n) {
        ret++;
        n >>= 1;
    }
    return ret;
}

inline bool isWriteBack(WritePolicy policy) {
    return policy == back_alloc || policy == back_noAlloc;
}

inline bool isWriteThrough(WritePolicy policy) {
    return !isWriteBack(policy);
}

inline bool isWriteAlloc(WritePolicy policy) {
    return policy == through_alloc || policy == back_alloc;
}

void printBin(u64 n) {
    for (int i = 0; i < 64; ++i) {
        cout << (1 & n);
        if ((i + 1) % 4 == 0 && i < 63) cout << " ";
        n >>= 1;
    }
    cout << endl;
}

void printBin(u8* p) {
    u64* p64 = (u64*) p;
    u64 n = * p64;
    printBin(n);
}

void printBin(u8* p, int len) {
    cout << "printBin " << p << " " << len << endl;
    for (int i = 0; i < len; ++i) {
        if (getBit(p, i)) {
            cout << 1;
        } else {
            cout << 0;
        }
    }
    cout << endl;
}

void printBin(u8* p, int lo, int len) {
    for (int i = 0; i < len; ++i) {
        if (getBit(p, lo + i)) {
            cout << 1;
        } else {
            cout << 0;
        }
    }
    cout << endl;
}