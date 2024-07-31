#include "blockChain.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

blockChain::blockChain(int difficulty) : difficulty(difficulty) {
    BLOCK_T genesis;
    genesis.index = 0;
    genesis.timestamp = time(nullptr);    
    genesis.data = "Genesis Block";
    genesis.previousHash = "";
    genesis.hash = calculateHash(genesis);

    chain.push_back(genesis); // הוספת הבלוק הראשון (הגנזיס) לבלוקצ'יין
}

std::string blockChain::calculateHash(const BLOCK_T& block) const {
    std::string toHash = std::to_string(block.index) + block.previousHash + std::to_string(block.timestamp) + block.data;
    return std::to_string(std::hash<std::string>{}(toHash));
}

void blockChain::addBlock(const BLOCK_T& newBlock) {
    chain.push_back(newBlock); // הוספת הבלוק החדש לבלוקצ'יין
}

BLOCK_T blockChain::getBlock() const {
    return chain.back(); // החזרת הבלוק האחרון בשרשרת
}

void blockChain::setBlock(const BLOCK_T& block) {
    chain.push_back(block); // הוספת הבלוק המתקבל לבלוקצ'יין
}

void blockChain::startMining(int minerId, const std::string& minerPipeName) {
    BLOCK_T newBlock;
    newBlock.index = chain.size();
    newBlock.previousHash = chain.back().hash;
    newBlock.timestamp = std::time(nullptr);
    newBlock.data = "Block " + std::to_string(newBlock.index);

    while (true) {
        newBlock.hash = calculateHash(newBlock);
        if (newBlock.hash.substr(0, difficulty) == std::string(difficulty, '0')) {
            std::cout << "Miner " << minerId << " found a block: " << newBlock.hash << std::endl;
            break;
        }
        newBlock.timestamp = std::time(nullptr);
    }

    int minerPipeFd = open(minerPipeName.c_str(), O_WRONLY);
    if (minerPipeFd == -1) {
        std::cerr << "Error: Could not open miner pipe for writing." << std::endl;
        return;
    }

    write(minerPipeFd, &newBlock, sizeof(BLOCK_T));
    close(minerPipeFd);
}
