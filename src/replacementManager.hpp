#pragma once

#include "global.hpp"
#include "utils.hpp"

using namespace std;


class ReplacementManager {
public:
    u8* data;
    u64 nBytes;
    #ifdef DEBUG
    unordered_map<int, int> accCnt {};
    #endif

    virtual void onAccess(u64 index, u64 wayIndex) = 0;
    virtual void onReplace(u64 index, u64 wayIndex) = 0;
    virtual int getReplacement(u64 index) = 0;
    virtual void onSetFilled(u64 index) = 0;
    virtual int getNBytes() = 0;

    virtual ~ReplacementManager() {}
protected:
    ReplacementManager() : data(nullptr), nBytes(0) {}
};


class RMBinTree : public ReplacementManager {
public:
    u64 nWays;
    u64 nSets;
    u64 bytesPerSet;
    u64 nLines;
    RMBinTree(u64 nWays, u64 nSets)
    : 
        nWays(nWays),
        nSets(nSets),
        ReplacementManager()
    {
        if (nWays == 1) {
            // Direct-mapping
            nBytes = 0;
        } else {
            nLines = nWays * nSets;
            u64 bitsPerLine = nWays;
            nBytes = (nLines + 7ll) / 8ll;
            // bytesPerSet = (nWays + 7ll) / 8ll;
            // nBytes = (int)nSets * (int)bytesPerSet;
            data = new u8[nBytes];
            for (int i = 0; i < nBytes; ++i) {
                data[i] = 0;
            }
        }
    }
    ~RMBinTree() {
        if (nWays != 1) {
            delete [] data;
        }
    }

    int getNBytes() { return nBytes;}

    void onAccess(u64 index, u64 wayIndex) {
        if (nWays == 1) {
            return;
        }
        #ifdef DEBUG
        accCnt[wayIndex]++;
        #endif
        // u8* set = data + index * bytesPerSet;
        u64 baseIdx = index * nWays;
        u64 range = nWays;
        u64 idx = 1;
        while (range > 1ll) {
            u64 half = range / 2;
            bool accessLower = ((wayIndex % range) < half);
            range = half;

            u64 bitIdx = baseIdx + idx;
            setBit(data, bitIdx, accessLower);  // Set bit to point towards older half
            if (accessLower) {
                idx = 2 * idx;
            } else {
                idx = 2 * idx + 1;
            }
        }
    }

    void onReplace(u64 index, u64 wayIndex) {}

    int getReplacement(u64 index) {
        if (nWays == 1) {
            return 0;
        }
        // Return way index
        u64 baseIdx = index * nWays;

        if (getBit(data, baseIdx) == 0) {
            // some blocks are invalid
            return -1;
        } else { 
            // all blocks are valid (set is filled)
            u64 stride = nWays;
            u64 idx = 1;
            int ret = 0;
            while (stride > 0) {
                stride /= 2;
                bool lowerHalfIsNew = (getBit(data, baseIdx + idx) == 1);
                if (lowerHalfIsNew) {
                    // Lower half is newer
                    ret += (int) stride;
                    idx = 2 * idx + 1;
                } else {
                    // Upper half is newer
                    idx = 2 * idx;
                }
                assert(ret < 18000);
            }
            return ret;
        }
    }

    void onSetFilled(u64 index) {
        if (nWays == 1) return;
        // cout << "on set filled " << index << endl;
        // u8* set = data + index * bytesPerSet;
        // setBit(set, 0, true);
        int baseIdx = index * nWays;
        setBit(data, baseIdx, true);
    }
};


class RMLRU : public ReplacementManager {
public:
    u64 bytesPerSet;
    u64 bitsPerLine;
    u64 bitsPerSet;

    u64 nWays;
    u64 nSets;

    RMLRU(u64 nWays, u64 nSets)
    : 
        nWays(nWays),
        nSets(nSets),
        ReplacementManager()
    {
        bitsPerLine = log2u(nWays);
        bitsPerSet = nWays * bitsPerLine;
        bytesPerSet = (bitsPerSet + 7) / 8;
        nBytes = (int)nSets * (int)bytesPerSet;
        data = new u8[nBytes];
        for (u64 i = 0; i < nSets; ++i) {
            u8* set = data + i * bytesPerSet;
            for (u64 lineIdx = 0; lineIdx < nWays; ++lineIdx) {
                setLine(set, lineIdx, lineIdx);
            }
        }

        #ifdef DEBUG
        cout << "--- LRU cache ---\n";
        cout << "b/line: " << bitsPerLine << endl;
        cout << "b/set:  " << bitsPerSet << endl;
        cout << "B/set:  " << bytesPerSet << endl;
        cout << "-----------------\n";
        #endif
    }
    ~RMLRU() {
        delete[] data;
    }

    int getNBytes() { return nBytes;}
    void setLine(u8* set, u64 idx, u64 val) {
        for (u64 i = 0; i < bitsPerLine; ++i) {
            if (val & 1ll << i) {
                setBit(set, idx * bitsPerLine + i, true);
            } else {
                setBit(set, idx * bitsPerLine + i, false);
            }
        }
    }

    u64 at(u8* set, u64 idx) {
        return getBits(set, idx * bitsPerLine, bitsPerLine);
    }

    int find(u8* set, u64 val) {
        for (u64 idx = 0; idx < nWays; ++idx) {
            u64 elem = at(set, idx);
            if (elem == val) {
                return (int) idx;
            }
        }
        return -1; // Did not find
    }

    void moveBack(u8* set, u64 lineIdx) {
        u64 idx = lineIdx * bitsPerLine;
        u64 line = getBits(set, idx, bitsPerLine);
        for (u64 i = idx + bitsPerLine; i < bitsPerSet; ++i) {
            u8 bit = getBit(set, i);
            setBit(set, i - bitsPerLine, bit);
        }
        setLine(set, nWays - 1, line);
    }

    void onAccess(u64 index, u64 wayIndex) {
        if (nWays == 1) return;
        u8* set = data + bytesPerSet * index;
        int idx = find(set, wayIndex);
        moveBack(set, idx);
    }
    void onReplace(u64 index, u64 wayIndex) {}

    int getReplacement(u64 index) {
        // Returns signed int because other replacement methods might 
        // return -1 to delegate lookup of invalid lines to cache.
        if (nWays == 1) return 0;
        u8* set = data + bytesPerSet * index;
        return (int) at(set, 0);
    }

    void onSetFilled(u64 index) {
        // Pass
    }
};

class RMPLRU : public ReplacementManager {
public:
    u8* counters;
    u64 bytesPerSet;
    u64 bitsPerSet;
    u64 bitsPerLine;

    u64 nWays;
    u64 nSets;
    u64 nLines;

    int bitsPerCounter = 3;
    int protectCnt = 2;
    int bytesPerCounter;
    int totalCounterBytes;

    RMPLRU(u64 nWays, u64 nSets)
    : 
        nWays(nWays),
        nSets(nSets),
        ReplacementManager()
    {
        assert(nWays == 8ll);
        nLines = nWays * nSets;
        bitsPerLine = log2u(nWays);
        bitsPerSet = nWays * bitsPerLine;
        bytesPerSet = (bitsPerSet + 7) / 8;
        nBytes = (int)nSets * (int)bytesPerSet;
        data = new u8[nBytes];
        for (u64 i = 0; i < nSets; ++i) {
            u8* set = data + i * bytesPerSet;
            for (u64 lineIdx = 0; lineIdx < nWays; ++lineIdx) {
                setLine(set, lineIdx, lineIdx);
            }
        }

        // init counters
        protectCnt = nWays / 4;
        totalCounterBytes = (bitsPerCounter * nLines + 7) / 8;
        counters = new u8[totalCounterBytes];

        #ifdef DEBUG
        cout << "--- LRU cache ---\n";
        cout << "b/line: " << bitsPerLine << endl;
        cout << "b/set:  " << bitsPerSet << endl;
        cout << "B/set:  " << bytesPerSet << endl;
        cout << "-----------------\n";
        #endif
    }
    ~RMPLRU() {
        delete[] data;
    }

    int getNBytes() { return nBytes + totalCounterBytes;}
    void clearCounter(u64 bitIdx) {
        for (int i = 0; i < bitsPerCounter; ++i) {
            setBit(counters, bitIdx + i, false);
        }
    }

    u64 getCounterBitIdx(u64 index, u64 wayIndex) {
        u64 absIdx = index * nWays + wayIndex;
        u64 bitIdx = absIdx * bitsPerCounter;
        return bitIdx;
    }

    u64 getCounter(u64 index, u64 wayIndex) {
        u64 bitIdx = getCounterBitIdx(index, wayIndex);
        u64 cnt = getBits(counters, bitIdx, bitsPerCounter);
        return cnt;
    }

    void incrCounter(u64 bitIdx) {
        u64 val = getBits(counters, bitIdx, bitsPerCounter);
        val++;
        for (int i = 0; i < bitsPerCounter; ++i) {
            bool bit = val & (1 << i);
            setBit(counters, bitIdx + i, bit);
        }
    }

    void addCounter(u64 index, u64 wayIndex) {
        u64 bitIdx = getCounterBitIdx(index, wayIndex);
        incrCounter(bitIdx);
    }

    void setLine(u8* set, u64 idx, u64 val) {
        for (u64 i = 0; i < bitsPerLine; ++i) {
            if (val & 1ll << i) {
                setBit(set, idx * bitsPerLine + i, true);
            } else {
                setBit(set, idx * bitsPerLine + i, false);
            }
        }
    }

    u64 at(u8* set, u64 idx) {
        return getBits(set, idx * bitsPerLine, bitsPerLine);
    }

    int find(u8* set, u64 val) {
        for (u64 idx = 0; idx < nWays; ++idx) {
            u64 elem = at(set, idx);
            if (elem == val) {
                return (int) idx;
            }
        }
        return -1; // Did not find
    }

    void moveBack(u8* set, u64 lineIdx) {
        // cout << "move back " << lineIdx << endl;
        // printBin(set, bitsPerSet);
        u64 idx = lineIdx * bitsPerLine;
        u64 line = getBits(set, idx, bitsPerLine);
        for (u64 i = idx + bitsPerLine; i < bitsPerSet; ++i) {
            u8 bit = getBit(set, i);
            setBit(set, i - bitsPerLine, bit);
        }
        setLine(set, nWays - 1, line);
        // printBin(set, bitsPerSet);
        // cout << endl;
    }

    void onAccess(u64 index, u64 wayIndex) {
        if (nWays == 1) return;
        u8* set = data + bytesPerSet * index;
        int idx = find(set, wayIndex);
        moveBack(set, idx);
        addCounter(index, wayIndex);
    }

    void onReplace(u64 index, u64 wayIndex) {
        u64 bitIdx = getCounterBitIdx(index, wayIndex);
        clearCounter(bitIdx);
    }

    int getReplacement(u64 index) {
        // Returns signed int because other replacement methods might 
        // return -1 to delegate lookup of invalid lines to cache.
        if (nWays == 1) return 0;

        int* cnts = new int[nWays];
        /*
        Get all counters for this set, and sort ascending
        */
        for (int i = 0; i < nWays; ++i) {
            cnts[i] = getCounter(index, i);
        }
        for (int i = 0; i < nWays; ++i) {
            for (int j = i + 1; j < nWays; ++j) {
                if (cnts[i] > cnts[j]) {
                    int t = cnts[i];
                    cnts[i] = cnts[j];
                    cnts[j] = t;
                }
            }
        }

        // for (int i = 0; i < nWays; ++i) cout << cnts[i] << " ";
        // cout << endl;
        int maxCnt = cnts[nWays - 1 - protectCnt];
        u8* set = data + bytesPerSet * index;
        // printBin(set, bitsPerSet);
        for (int i = 0; i < nWays; ++i) {
            int idx = at(set, i);
            int cnt = getCounter(index, idx);
            // cout << "cnt: " << cnt << endl;
            if (cnt <= maxCnt) {
                delete [] cnts;
                return idx;
            }
        }
        assert(false);
        return -2;
    }

    void onSetFilled(u64 index) {}
};