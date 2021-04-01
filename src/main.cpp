#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "global.hpp"
#include "cache.hpp"
#include "instr.hpp"
#include "utils.hpp"

using namespace std;

// #define LOG_CACHE_STATS

void readFile(string inFile, vector<Instr>& res) {
    ifstream fin(inFile);
    string strAddr;
    u64 addr;
    char cmd;
    if (fin.is_open()) {
        while (true) {
            fin >> strAddr >> cmd;
            if (fin.eof()) break;
            addr = toBin(strAddr);
            Instr instr(cmd == 'r', addr);
            res.push_back(instr);
            // cout << res.size() << endl;
        }
    }
}

void writeFile(string statsFile, vector<vector<float> >& stats) {
    cout << "opening file: " << statsFile << endl;
    ofstream fout(statsFile);
    if (fout.is_open()) {
        string header = "trace id\tcache space\treplace space\taccess count\tmiss rate\twrite mem count\tread mem count\tread miss\n";
        fout.write(header.c_str(), header.size());
        for (auto& line : stats) {
            char buf[512];
            snprintf(buf, sizeof(buf), "%.0f\t%.0f\t%.0f\t%.0f\t%.1f\t%.0f\t%.0f\t%.0f\n", 
                     line[0], line[1], line[2], line[3], 100.0 * line[4], line[5], line[6], line[7]);
            // string row = join<double>(line, '\t') + '\n';
            string row = buf;
            fout.write(row.c_str(), row.size());
        }
    } else {
        printf("Error opening output file\n");
        assert(false);
    }
    cout << "Saved result to " << statsFile << endl;
}

ReplacementPolicy sToReplace(const char* s) {
    string str(s);
    if (str == "binTree") return binTree;
    if (str == "LRU") return LRU;
    if (str == "PLRU") return PLRU;

    return replaceNull;
}

WritePolicy sToWrite(const char* s) {
    string str(s);
    if (str == "back_alloc") return back_alloc;
    if (str == "back_noAlloc") return back_noAlloc;
    if (str == "through_alloc") return through_alloc;
    if (str == "through_noAlloc") return through_noAlloc;
    return writeNull;
}

void testLayout(vector<Instr>* instrs) {
    // different block sizes and associativity
    int blockSizes[] = { 8, 32, 64 };
    int wayCnt[] = { 1, 0, 4, 8 };
    for (int blockSize : blockSizes) {
        for (int nWays : wayCnt) {

        }
    }
}

template<class T>
string join(vector<T> v, char c) {
    string ret = to_string(v[0]);
    for (int i = 1; i < (int) v.size(); ++i) {
        ret += c;
        ret += to_string(v[i]);
    }
    return ret;
}

void cntIndex(vector<Instr>& instrs, Cache& cache) {
    unordered_map<u64, u64> cnt;

    for (Instr& instr : instrs) {
        u64 index = cache.getIndex(instr.addr);
        cnt[index] ++;
    }

    for (auto& it : cnt) {
        u64 index = it.first;
        u64 c = it.second;
        if (c > 5) {
            cout << index << ": " << c << endl;
        }
    }
}

void cnt(vector<Instr>& instrs, Cache& cache) {
    unordered_map<u64, unordered_set<u64>> cnt;

    for (Instr& instr : instrs) {
        u64 index = cache.getIndex(instr.addr);
        u64 tag = cache.getTag(instr.addr);
        if (cnt.find(index) == cnt.end()) {
            cnt[index] = unordered_set<u64>();
        }
        cnt[index].insert(tag);
    }

    for (auto& it : cnt) {
        u64 index = it.first;
        auto s = it.second;
        if (s.size() > 2) {
            cout << index << ": ";
            cout << s.size() << " ";
            for (u64 a : s) {
                cout << a << " ";
            }
            cout << endl;
            // cout << index << ": " << s.size() << endl;
        }
    }
}

int blockSize = 8;
int numWays = 4;
ReplacementPolicy replacementPolicy = binTree;
WritePolicy writePolicy = back_alloc;

int parse_args(int argc, char** argv) {
    if (argc != 5) {
       cout << "ERROR: Must pass in 4 arguments: \n";
       cout << "1. block size\n";
       cout << "2. number of ways\n";
       cout << "3. replacement policy\n";
       cout << "4. write policy\n";
       cout << "NOTE: Order matters\n";
       return -1;
    }
    blockSize = atoi(argv[1]);
    numWays = atoi(argv[2]);
    replacementPolicy = sToReplace(argv[3]);
    writePolicy = sToWrite(argv[4]);
    if (replacementPolicy == replaceNull) {
       cout << "Invalid argument: " << argv[3] << endl;
       return -1;
    }
    if (writePolicy == writeNull) {
       cout << "Invalid argument: " << argv[4] << endl;
       return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    #ifdef ARG
    if (parse_args(argc, argv) == -1) {
        return 0;
    }
    #endif

    cout << "\n--- Init cache ---\n";
    cout << "Block size:            " << blockSize << endl;
    cout << "Number of ways:        " << numWays << endl;
    
    #ifdef ARG
    cout << "Replacement policy:    " << argv[3] << endl;
    cout << "Write policy:          " << argv[4] << "\n\n";
    #endif

    vector<vector<float> > stats;
    // loop files
    for (int i = 1; i <= 4; ++i) {
        Cache cache(blockSize, numWays, replacementPolicy, writePolicy);

        string inFile = "../input/" + to_string(i) + ".trace";
        string outFile = "../output/" + to_string(i) + ".log";
        cout << "reading file: " << inFile << endl;
        vector<Instr> instrs;
        readFile(inFile, instrs);

        cache.processInstrs(instrs);
        cache.outputLog(outFile);
        // for (auto& it : cache.rm->accCnt) {
        //     cout << it.first << ": " << it.second << endl;
        // }
        // cout << "indices: " << cache.idxCnt.size() << endl;
        // for (auto& it : cache.idxCnt) {
        //     cout << it.first << ": " << it.second << endl;
        // }
        // printBin(cache.rm->data, 2048);
        // exit(0);

        vector<float> t{
            (float) i,
            (float) cache.nBytes,
            (float) cache.rm->getNBytes(),
            (float) cache.getAccessCnt(),
            (float) cache.getMissRate(),
            (float) cache.getWriteMemCnt(),
            (float) cache.getReadMemCnt(),
            (float) cache.nReadMiss
        };
        stats.push_back(t);

        #ifdef LOG_CACHE_STATS
        cache.printStats();
        printf("Cache space = %lluB\n", cache.nBytes);
        printf("Replacement space = %lluB\n", cache.rm->nBytes);
        #endif
    }
    #ifdef ARG
    string argsJoined(argv[1]);
    for (int i = 2; i < argc; ++i) {
       argsJoined += "_" + string(argv[i]);
    }
    #else
    string argsJoined = "test";
    #endif
    string statsFile = "../output/stats/stats_" + argsJoined + ".tsv";

    writeFile(statsFile, stats);
    return 0;
}