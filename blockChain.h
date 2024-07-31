#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <vector>
#include <string>
#define MAIN_PATH "/mnt/mta/"
#define SERVER_PATH "server_pipe" // הגדר שם צינור עבור השרת
#define DIFFICULTY_FILE "difficulty.conf" // הגדר מיקום קובץ הקונפיגורציה של רמת הקושי
#define MINER_PIPE "miner_pipe_" // הגדר שם צינור עבור המינימר
#define MAX_PATH_LEN 512
struct BLOCK_T {
    int index;
    int timestamp;
    std::string data;
    std::string previousHash;
    std::string hash;
    int difficulty;
};
class TLV {
public:
    BLOCK_T block;
    bool subscription = true;
}
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
