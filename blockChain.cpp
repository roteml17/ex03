#include "blockChain.h"
#include <string.h>
blockChain::blockChain(int difficulty) : difficulty(difficulty) {
    BLOCK_T genesis;
    genesis.height = 0;
    genesis.timeStamp = time(nullptr);
    genesis.prev_hash = 0;
    genesis.difficulty = difficulty;
    genesis.nonce = 0;
    genesis.relayed_by = -1;
    genesis.hash = calculateHash(genesis);

    chain.push_back(genesis); // הוספת הבלוק הראשון (הגנזיס) לבלוקצ'יין
}

int blockChain::calculateHash(const BLOCK_T& block) const {
    std::string input = to_string(block.height) +
                   to_string(block.timeStamp) +
                   to_string(block.prev_hash) +
                   to_string(block.nonce) +
                   to_string(block.relayed_by);
    return crc32(0,reinterpret_cast<const unsigned char*>(input.c_str()),input.length());
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

bool blockChain::validationProofOfWork(int hash, int difficulty)
{
    for(int i = 0; i < difficulty; ++i)
    {
        if((hash & (1 << (31 - i))) != 0) 
        {
            return false;
        }
    }

    return true;    
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
