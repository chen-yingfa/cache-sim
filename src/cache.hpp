#pragma once

#include <cstdio>
#include <memory>
#include <cassert>
#include <fstream>
#include <chrono>

#include "global.hpp"
#include "utils.hpp"
#include "instr.hpp"
#include "replacementManager.hpp"

#define LOG_PROGRESS

#ifdef LOG_PROGRESS
int log_step = 5000;
#endif

class Cache {
public:
    // Constants
    u8* data;

    // Parameters
    u64 blockSize;
    u64 nWays;
    WritePolicy writePolicy;
    ReplacementPolicy replacementPolicy;

    // these needs to be set on instantiation
    u64 nBlocks;
    u64 nBytes;
    u64 nSets;
    u64 bitsPerLine;
    u64 bytesPerLine;
    u64 bytesPerSet;
    u64 lenTag;
    u64 lenIndex;
    u64 lenOffset;

    // Might be unused, depending parameters
    ReplacementManager* rm;

    // stats, updated as time goes
    u64 nRead = 0;
    u64 nWrite = 0;
    u64 nReadMiss = 0;
    u64 nWriteMiss = 0;


    unordered_map<u64, int> hashTable;
    int lastInvalidWayIndex;
    vector<u8> log;
    
    #ifdef DEBUG
    int curInstr = 0;
    unordered_map<int, int> idxCnt {};
    #endif

public:
    Cache(
        u64 blockSize,
        u64 numWays,
        ReplacementPolicy replacementPolicy,
        WritePolicy writePolicy) 
    :   
        blockSize(blockSize),
        nWays(numWays),
        replacementPolicy(replacementPolicy),
        writePolicy(writePolicy)
    {
        // assert(isPowerOfTwo(CACHE_SIZE));
        //assert(isPowerOfTwo(nWays));
        //assert(isPowerOfTwo(blockSize));

        nBlocks = CACHE_SIZE / blockSize;
        if (nWays == 0ll) nWays = nBlocks;
        assert(nWays > 0ll);
        nSets = nBlocks / nWays;
        lenIndex = log2u(nSets);
        lenOffset = log2u(blockSize);
        lenTag = ADDR_LEN - lenIndex - lenOffset;
        assert(1ll << lenIndex == nSets);
        assert(1ll << lenOffset == blockSize);

        bitsPerLine = lenTag + 1; // 1 valid bit
        if (isWriteBack(writePolicy)) {
            bitsPerLine++; // 1 dirty bit
        }
        bytesPerLine = (bitsPerLine + 7) / 8;
        bytesPerSet = bytesPerLine * nWays;
        nBytes = bytesPerSet * nSets;

        data = new u8[(int) nBytes];
        for (int i = 0; i < (int) nBytes; ++i) {
            data[i] = 0u;
        }

        // Replacement data
        if (replacementPolicy == binTree) {
            rm = new RMBinTree(nWays, nSets);
        } else if (replacementPolicy == LRU) {
            rm = new RMLRU(nWays, nSets);
        } else if (replacementPolicy == PLRU) {
            rm = new RMPLRU(nWays, nSets);
        } else {
            // TODO: add another replacement policy
            printf("Not implemented\n");
            exit(0);
        }

        #ifdef DEBUG
        cout << "--- Cache ---\n";
        cout << "# blocks: " << nBlocks << endl;
        cout << "# ways:   " << nWays << endl;
        cout << "# sets:   " << nSets << endl;
        cout << "B/line:   " << bytesPerLine << endl;
        cout << "B/set:    " << bytesPerSet << endl;
        cout << "b/line:   " << bitsPerLine << endl;
        cout << "offset:   " << lenOffset << endl;
        cout << "index:    " << lenIndex << endl;
        cout << "tag:      " << lenTag << endl;
        cout << "-------------\n" << endl;
        #endif

        if (nWays == nBlocks) {
            hashTable = unordered_map<u64, int> ();
            lastInvalidWayIndex = 0;
        }
    };

    ~Cache() {
        delete [] data;
        delete rm;
    }

    /*
        Getters and setters
    */

    inline u8* at(u64 index, u64 wayIndex) const {
        return data + index * bytesPerSet + wayIndex * bytesPerLine;
    }

    u64 getOffset(u64 addr) {
        return getBits(addr, 0, lenOffset);
    }

    u64 getIndex(u64 addr) {
        return getBits(addr, lenOffset, lenIndex);
    }

    u64 getTag(u64 addr) {
        return getBits(addr, lenOffset + lenIndex, lenTag);
    }

    u64 getLineTag(u8* line) {
        return getBits(line, 1, lenTag); // 1 valid bit
    }

    u64 findLineIndex(u8* line) {
        u64 dist = line - data;
        return dist / bytesPerSet;
    }

    u64 getWayIndex(u8* line) {
        u64 dist = line - data;
        u64 byteOffset = dist % bytesPerSet;
        return byteOffset / bytesPerLine;
    }

    u8* getSet(u64 addr) {
        u64 index = getIndex(addr);
        return at(index, 0);
    }

    int findInvalidLine(u64 index) {
        for (int i = 0; i < nWays; ++i) {
            if (!isValid(index, i)) {
                return i;
            }
        }
        printSetValidity(index);
        // All lines are valid!
        return -1;
    }

    int findLine(u64 addr) {
        u64 tag = getTag(addr);
        if (nWays == nBlocks) {
            // Fully-mapped
            auto it = hashTable.find(tag);
            if (it == hashTable.end()) {
                return -1;
            } else {
                return it->second;
            }
        }

        u64 index = getIndex(addr);
        for (u64 i = 0; i < nWays; ++i) {
            // u8* line = set + i * bytesPerLine;
            u8* line = getLine(index, i);
            u64 lineTag = getLineTag(line);

            #ifdef DEBUG
            printf("line tag = %llx\n", lineTag);
            #endif
            if (isValid(line) && lineTag == tag) {
                // Hit
                #ifdef DEBUG
                printf("< HIT >        <------------  %llu\n", i);
                #endif
                return i;
            }
        }
        // Miss
        #ifdef DEBUG
        printf("< MISS >\n");
        #endif
        return -1;
    }

    u8* getLine(u64 index, int wayIndex) {
        assert(wayIndex == -1 || (u64) wayIndex < nWays);
        if (wayIndex == -1) {
            // When some blocks are invalid and replacement policy 
            // requires cache to find those blocks itself, it returns
            // -1 as way index to cache

            if (nWays == nBlocks) {
                return at(index, lastInvalidWayIndex++);
            }

            int i = 0;
            for (; i < nWays; ++i) {
                u8* line = getLine(index, i);
                if (!isValid(line)) {
                    break;
                }
            }
            assert(i < nWays);
            if (i == nWays - 1ll) {
                // Because we search invalid blocks from left to right,
                // when we return the last block in set to be replaced,
                // the set is filled.
                // cout << "on set filled " << index << endl;
                rm->onSetFilled(index);
            }
            // cout << "some blocks are invalid, return " << i << endl;
            // printSetValidity(index);
            return at(index, i);
        } else {
            return at(index, wayIndex);
        }
    }

    bool isValid(u8* line) {
        return getBit(line, 0) == 1;
    }

    bool isDirty(u8* line) {
        assert(isWriteBack(writePolicy));
        return getBit(line, bitsPerLine - 1) == 1;
    }

    bool isValid(u64 index, int wayIndex) {
        u8* line = at(index, wayIndex);
        return isValid(line);
    }

    void setTag(u8* line, u64 tag) {
        // NOTE: Pollutes all other bits
        // u64* p = (u64*) line;
        // *p = tag << 1;

        tag <<= 1;
        
        u8* src = (u8*) &tag;
        for (int i = 0; i < bytesPerLine; ++i) {
            line[i] = src[i];
        }
    }

    void setValid(u8* line, bool val) {
        setBit(line, 0, val);
    }

    void setDirty(u64 index, int wayIndex, bool val) {
        #ifdef DEBUG
        printf("set dirty %u\n", val);
        #endif
        assert(isWriteBack(writePolicy));
        assert(wayIndex >= 0);
        u8* line = getLine(index, wayIndex);
        setBit(line, bitsPerLine - 1, val);
    }

    // End of getters and setters

    void processInstrs(vector<Instr>& instrs) {
        #ifdef LOG_PROGRESS
        int i = 0;
        auto t_start = std::chrono::high_resolution_clock::now();
        auto t_last_log = t_start;
        #endif

        for (Instr& instr : instrs) {
            #ifdef DEBUG
            printf("--- INSTRUCTION ID = %lld --- %u\n", i++, instr.isread);
            curInstr++;
            #endif
            processInstr(instr);

            #ifdef LOG_PROGRESS
            auto t_cur = chrono::high_resolution_clock::now();
            double elapsed_time_ms = chrono::duration<double, std::milli>(t_cur - t_start).count();
            double diff_ms = chrono::duration<double, milli>(t_cur - t_last_log).count();
            if (diff_ms > 1000) {
                printf("[%d/%ld] time elapsed: %.1fs\n", i, instrs.size(), elapsed_time_ms / 1000.0);
                t_last_log = t_cur;
            }
            i++;
            #endif
        }
    }

    void printSet(int index) {
        u8* line = at(index, 0);
        bool valid = isValid(line);
        bool dirty = isDirty(line);
        u64 tag = getLineTag(line);
        printf("%d, %u %llx %u\n", index, dirty, tag, valid);
    }

    void processInstr(Instr& instr) {
        u8 accessInfo = 0;
        if (instr.isread) {
            read(instr.addr, accessInfo);
        } else {
            write(instr.addr, 0, accessInfo);
        }
        log.push_back(accessInfo);
    }

    void read(u64 addr, u8& accessInfo) {
        u64 index = getIndex(addr);

        u64 tag = getTag(addr);
        #ifdef DEBUG
        printf("addr  = %llx\n", addr);
        printf("index = %llu\n", index);
        printf("tag   = %llx\n", tag);
        #endif

        int wayIndex = findLine(addr);
        
        accessInfo = 0;
        if (wayIndex != -1) {
            // Hit
            #ifdef DEBUG
            printf("read hit\n");
            #endif
            accessInfo |= LOG_HIT;
            rm->onAccess(index, wayIndex);
        } else {
            // Miss
            accessInfo |= LOG_REPLACE;
            int replaceWayIndex = rm->getReplacement(index);
            #ifdef DEBUG
            idxCnt[replaceWayIndex]++;
            #endif
            replace(index, replaceWayIndex, addr, accessInfo);
            rm->onAccess(index, replaceWayIndex);

            nReadMiss++;
        }
        nRead++;
    }

    void write(u64 addr, u8 byte, u8& accessInfo) {
        u64 index = getIndex(addr);

        u64 tag = getTag(addr);
        #ifdef DEBUG
        printf("addr  = %llx\n", addr);
        printf("index = %llu\n", index);
        printf("tag   = %llx\n", tag);
        #endif

        int wayIndex = findLine(addr);

        accessInfo = 0;
        if (wayIndex != -1) {
            // Hit
            #ifdef DEBUG
            printf("write hit\n");
            #endif
            accessInfo |= LOG_HIT;
            rm->onAccess(index, wayIndex);
            if (isWriteBack(writePolicy)) { 
                // Write-back
                #ifdef DEBUG
                printf("write hit, goto setDirty\n");
                #endif
                setDirty(index, wayIndex, true);
            } else {
                accessInfo |= LOG_WRITE_MEM;
            }
        } else {
            // Miss
            if (isWriteAlloc(writePolicy)) {
                u64 replaceWayIndex = rm->getReplacement(index);
                #ifdef DEBUG
                idxCnt[replaceWayIndex]++;
                #endif
                if (replaceWayIndex == -1) {
                    replaceWayIndex = findInvalidLine(index);
                }
                replace(index, replaceWayIndex, addr, accessInfo);
                rm->onAccess(index, replaceWayIndex);

                accessInfo |= LOG_REPLACE;
                if (isWriteThrough(writePolicy)) {
                    // Write to memory after replacing on Write-through
                    accessInfo |= LOG_WRITE_MEM;
                } else {
                    // Writing to new block makes it dirty
                    setDirty(index, replaceWayIndex, true);
                }
            } else {
                accessInfo |= LOG_WRITE_MEM;
            }

            nWriteMiss++;
        }
        nWrite++;
    }

    void replace(u64 index, int wayIndex, u64 addr, u8& accessInfo) {
        #ifdef DEBUG
        printf("replace\n");
        #endif
        // wayIndex == -1 means we should replace the first invalid line in the set
        bool replacingInvalid = false;
        int oldWayIndex = wayIndex;
        if (wayIndex == -1) {
            replacingInvalid = true;
            wayIndex = findInvalidLine(index);
        } else {
            replacingInvalid = !isValid(index, wayIndex);
        }
        assert(wayIndex >= 0);

        u8* line = at(index, wayIndex);
        u64 tag = getTag(addr);
        u64 oldTag = getLineTag(line);

        if (isWriteBack(writePolicy) && isValid(line) && isDirty(line)) {
            // Write dirty block to memory
            accessInfo |= LOG_WRITE_MEM;
        }
        // printSet(2539);
        setTag(line, tag);
        // printSet(2539);
        setValid(line, true);
        if (isWriteBack(writePolicy)) {
            setDirty(index, wayIndex, false);
        }

        if (replacingInvalid && wayIndex == nWays - 1) {
            // Because cache will not be invalid after becoming valid,
            // so when the last line is being replaced, we know that
            // all lines in this set are valid
            rm->onSetFilled(index);
        }

        if (nWays == nBlocks) {
            auto it = hashTable.find(oldTag);
            if (it != hashTable.end()) {
                // cout << "ERASE\n";
                hashTable.erase(oldTag);
            }
            hashTable.insert({{tag, wayIndex}});
            if (hashTable.size() == nWays) {
                rm->onSetFilled(index);
            }
            assert(hashTable.size() <= nWays);
        }
    }

    /*
        Stats
    */

    double getHitRate() {
        return 1.0 - getMissRate();
    }

    double getMissRate() {
        double access = static_cast<double>(getAccessCnt());
        double miss = static_cast<double>(getMissCnt());
        return miss / access;
    }

    u64 getAccessCnt() {
        return nRead + nWrite;
    }
    
    u64 getMissCnt() {
        return nReadMiss + nWriteMiss;
    }

    u64 getWriteMemCnt() {
        u64 cnt = 0;
        for (u8 info : log) {
            if (info & LOG_WRITE_MEM) {
                cnt++;
            }
        }
        return cnt;
    }

    u64 getReadMemCnt() {
        u64 cnt = 0;
        for (u8 info : log) {
            if (info & LOG_REPLACE) {
                cnt++;
            }
        }
        return cnt;
    }

    /*
        Utils
    */

    void outputLog(string filename) {
        ofstream fout(filename);
        if (fout.is_open()) {
            for (u8 info : log) {
                if (info & LOG_HIT) {
                    fout.write("Hit\n", 4);
                } else {
                    fout.write("Miss\n", 5);
                }
            }
        }
    }

    void printSetValidity(int index) {
        cout << index << ": ";
        for (int i = 0; i < nWays; ++i) {
            u8* line = getLine(index, i);
            cout << isValid(line) << " ";
        }
        cout << endl;
    }

    bool isSetFilled(int index) {
        for (int i = 0; i < nWays; ++i) {
            u8* line = getLine(index, i);
            if (!isValid(line)) return false;
        }
        return true;
    }

    bool isSetEmpty(int index) {
        for (int i = 0; i < nWays; ++i) {
            u8* line = getLine(index, i);
            if (isValid(line)) return false;
        }
        return true;
    }

    void printStats() {
        printf("access:       %llu\n", getAccessCnt());
        printf("misses:       %llu\n", getMissCnt());
        printf("reads:        %llu\n", nRead);
        printf("read misses:  %llu\n", nReadMiss);
        printf("writes:       %llu\n", nWrite);
        printf("write misses: %llu\n", nWriteMiss);
        printf("miss rate:    %.1f%%\n", 100.0 * getMissRate());
    }

    void printValidCnt() {
        u8* line = data;
        int cnt = 0;
        for (int i = 0; i < nBlocks; ++i) {
            if (isValid(line)) ++cnt;
            line += bytesPerLine;
        }
        cout << "valid cnt: " << cnt << endl;
    }
};