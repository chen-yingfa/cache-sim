#pragma once

class Instr {
public:
    bool isread;
    u64 addr;
    Instr() : isread(true), addr(0) {}
    Instr(bool isread, u64 addr) : isread(isread), addr(addr) {}
    Instr(const Instr& other) {
        isread = other.isread;
        addr = other.addr;
    }

    void print() {
        cout << hex << addr << " " << (isread ? 'r' : 'w') << endl;
    }
};