#pragma once

// #define DEBUG
#define ARG

using u8 = unsigned char;
using u32 = unsigned int;
using u64 = unsigned long long;
using i64 = long long;

const u64 CACHE_SIZE    = 128 * 1024; // 128KB
const u64 ADDR_LEN      = 64;
const u8 LOG_HIT        = 0b0000'0001;
const u8 LOG_READ_MEM   = 0b0000'0010;
const u8 LOG_WRITE_MEM  = 0b0000'0100;
const u8 LOG_REPLACE    = 0b0000'1000;


enum ReplacementPolicy { binTree, LRU, PLRU, replaceNull };
enum WritePolicy { through_alloc, through_noAlloc, back_alloc, back_noAlloc, writeNull };
