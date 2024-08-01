#include <iostream>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "blockChain.h"

#define SERVER_PIPE "/mnt/mta/server_pipe"

void createPipe(const std::string& pipeName) {
    mkfifo(pipeName.c_str(), 0666); // Create a named pipe with read/write permissions
}

std::string getMinerPipeName(int minerId) {
    return "/mnt/mta/miner_pipe_" + std::to_string(minerId); // Generate pipe name based on miner ID
}

int main() {
    // Step 1: Create an initial pipe with a placeholder name
    std::string minerPipeName = getMinerPipeName(0); // Initial placeholder name
    createPipe(minerPipeName);

    // Step 2: Notify the server of this miner's pipe name
    int serverPipeFd = open(SERVER_PIPE, O_WRONLY);
    if (serverPipeFd == -1) {
        std::cerr << "Error: Could not open server pipe for writing." << std::endl;
        return 1;
    }

    write(serverPipeFd, minerPipeName.c_str(), minerPipeName.size() + 1);
    close(serverPipeFd);

    // Step 3: Open the initial miner pipe to read the assigned miner ID
    int minerPipeFd = open(minerPipeName.c_str(), O_RDONLY);
    if (minerPipeFd == -1) {
        std::cerr << "Error: Could not open miner pipe for reading." << std::endl;
        return 1;
    }

    int minerId;
    read(minerPipeFd, &minerId, sizeof(minerId));
    close(minerPipeFd);

    // Step 4: Update the pipe name with the correct miner ID and create the pipe
    minerPipeName = getMinerPipeName(minerId);
    createPipe(minerPipeName);

    // Step 5: Open the updated miner pipe to read data and start mining
    minerPipeFd = open(minerPipeName.c_str(), O_RDONLY);
    if (minerPipeFd == -1) {
        std::cerr << "Error: Could not open updated miner pipe for reading." << std::endl;
        return 1;
    }

    BLOCK_T currentBlock;
    read(minerPipeFd, &currentBlock, sizeof(BLOCK_T));

    blockChain blockchain(currentBlock.difficulty);
    blockchain.setBlock(currentBlock);
    blockchain.startMining(minerId, minerPipeName);

    close(minerPipeFd);
    unlink(minerPipeName.c_str());

    return 0;
}
