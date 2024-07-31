#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <vector>
#include <string>

struct BLOCK_T {
    int index;
    int timestamp;
    std::string data;
    std::string previousHash;
    std::string hash;
    int difficulty;
};

class blockChain {
public:
    blockChain(int difficulty);
    std::string calculateHash(const BLOCK_T& block) const;
    void addBlock(const BLOCK_T& newBlock);
    BLOCK_T getBlock() const;
    void setBlock(const BLOCK_T& block);
    void startMining(int minerId, const std::string& minerPipeName);

private:
    std::vector<BLOCK_T> chain;
    int difficulty;
};

#endif
