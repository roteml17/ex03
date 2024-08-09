#include <iostream>
#include <fstream>  // כלול קבצי קלט/פלט
#include <string>   // כלול עבודה עם מחרוזות
#include <fcntl.h>  // כלול פונקציות לפתיחת קבצים/צינורות
#include <sys/stat.h> // כלול פונקציות לעבודה עם קבצים וצינורות
#include <unistd.h> // כלול פונקציות של מערכת Unix
#include <unordered_map>
#include <zlib.h>
#include <vector>
#include <errno.h>
#include <cerrno>  // Include errno for error handling
#include <cstring> // Include strerror for error message

#define SERVER_PIPE "/mnt/mta/server_pipe"
#define SERVER_PIPE_FOR_ID "/mnt/mta/serverPipeForId"
#define MINER_PIPE_PREFIX "/mnt/mta/miner_pipe_"
#define CONFIG_FILE "/mnt/mta/config.txt"
#define LOG_PATH "/var/log/mtacoin.log"
#define MAX_PATH_LEN 256
#define BLOCK_HEADER "newBlock:"
#define SUBSCRIPTION_HEADER "subscription:"

struct BLOCK_T {
    int height;
    int timeStamp;
    unsigned int hash;
    unsigned int prev_hash;
    int difficulty;
    int nonce;
    int relayed_by;
};

int difficulty = 0;
BLOCK_T blockToMine = {};
std::unordered_map<int,int> miners_subscribed;

void writeLogMessageToFile(FILE* logFile, std::string messageToSend)
{
    std::cout << messageToSend << std::endl;
}

int readDifficultyFromFile(FILE* logFile) {
    int difficulty;
    std::ifstream configFile(CONFIG_FILE);
    if (!configFile.is_open())
    {
        writeLogMessageToFile(logFile, "ERROR: No Configuration File");
        difficulty = 0;
    }
    else
    {
        std::string line;
        bool foundDifficulty = false;
        while (std::getline(configFile, line))
        {
            if (line.find("DIFFICULTY=") != std::string::npos)
            {
                std::string difficultySTR = line.substr(line.find('=')+1);
                int tempDiff = std::stoi(difficultySTR);
                if (tempDiff >=0 && tempDiff<=31)
                {
                    difficulty = tempDiff;
                    writeLogMessageToFile(logFile, "Difficulty set to " + std::to_string(difficulty));
                    foundDifficulty = true;
                    break;
                }
                else
                {
                    writeLogMessageToFile(logFile, "ERROR: Difficulty must be between 0 and 31");
                }
            }
        }
    }
    return difficulty;
}

int calculateHash(const BLOCK_T& block) {
    std::string input = std::to_string(block.height) +
                   std::to_string(block.timeStamp) +
                   std::to_string(block.prev_hash) +
                   std::to_string(block.nonce) +
                   std::to_string(block.relayed_by);
    return crc32(0,reinterpret_cast<const unsigned char*>(input.c_str()),input.length());
}

BLOCK_T creatingfirstBlock()
{
    BLOCK_T genesis;
    genesis.height = 0;
    genesis.timeStamp = time(nullptr);
    genesis.prev_hash = 0;
    genesis.difficulty = difficulty;
    genesis.nonce = 0;
    genesis.relayed_by = -1;
    genesis.hash = calculateHash(genesis);

    return genesis;
}

void createPipe(char* pipeName) {
    int result = mkfifo(pipeName, 0766);
    if (result == -1) {
        std::string errorMessage = "Error: Could not create pipe " + std::string(pipeName) + ": " + std::strerror(errno);
    }
}

bool validationProofOfWork(int hash, int difficulty)
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

//block validation
bool isBlockValid(const BLOCK_T& block, std::vector<BLOCK_T> &blocks_chain)
{
    // Check if the block's prev_hash matches the hash of the previous block
    if (block.height > 0 && block.prev_hash != blocks_chain.back().hash)
    {
        return false;
    }

    // Check if the block's hash satisfies the required difficulty level
    return validationProofOfWork(block.hash, block.difficulty);
}

int extractMinerID(const std::string& line) {
    size_t startPos = line.find_first_of("0123456789"); // Find the first digit
    if (startPos != std::string::npos) {
        size_t endPos = line.find_first_not_of("0123456789", startPos); // Find the end of the number
        return std::stoi(line.substr(startPos, endPos - startPos)); // Extract the miner ID
    }
    return -1; // Return an invalid ID if no digit is found
}

// Function to create the pipe name based on the miner ID
std::string createPipeName(int minerID) {
    return "/mnt/mta/miner_pipe_" + std::to_string(minerID);
}

bool hashValidation(int calcultedHash)
{
    return (calcultedHash == blockToMine.hash);
}

void subscriptionOrNewBlockRequest(char buffer[], std::vector<int>& miners_pipes, FILE* logFile, std::vector<BLOCK_T> &blocks_chain) {

    std::string bufferStr(buffer);
    int subHeader_length = strlen(SUBSCRIPTION_HEADER);
    int blockHeader_length = strlen(BLOCK_HEADER);

    if (bufferStr.compare(0, subHeader_length, SUBSCRIPTION_HEADER) == 0)
        {
            int minerID = extractMinerID(buffer);

            if (minerID == -1) {
                writeLogMessageToFile(logFile, "Invalid miner ID in connection request");
                return;
                }

            std::string currentPipeName = createPipeName(minerID);
            int minerPipeFD = open(currentPipeName.c_str(), O_WRONLY);

            if (minerPipeFD == -1) {
                writeLogMessageToFile(logFile, "Error opening miner #" + std::to_string(minerID) + " pipe");
                return;
                }

            // Add miner pipe FD to list of miners' FDs
            miners_subscribed.emplace(minerID, minerPipeFD);
            miners_pipes.push_back(minerID);

            writeLogMessageToFile(logFile, "Received connection request from miner #" + std::to_string(minerID) + 
                                                                ", pipe name: " + currentPipeName);
            write(minerPipeFD, &blockToMine, sizeof(BLOCK_T));

        }
    else if (bufferStr.compare(0, blockHeader_length, BLOCK_HEADER) == 0)
        {
            // Extract the block from the buffer
            BLOCK_T minedBlock;
            std::memcpy(&minedBlock, buffer + blockHeader_length, sizeof(BLOCK_T));

            // Calculate checksum
            unsigned int checksum = calculateHash(minedBlock);

            // Validate the block
            if (isBlockValid(minedBlock ,blocks_chain) && hashValidation(checksum)) {
                // Log the successful validation and block addition
                std::string logMessage = "Server: New block added by " + std::to_string(minedBlock.relayed_by) + 
                                 ", attributes: height(" + std::to_string(minedBlock.height) + 
                                 "), timestamp (" + std::to_string(minedBlock.timeStamp) + 
                                 "), hash(0x" + std::to_string(minedBlock.hash) + 
                                 "), prev_hash(0x" + std::to_string(minedBlock.prev_hash) + 
                                 "), difficulty(" + std::to_string(minedBlock.difficulty) + 
                                 "), nonce(" + std::to_string(minedBlock.nonce) + ")";
                writeLogMessageToFile(logFile, logMessage);

                // Add mined block to the blockchain
                blocks_chain.insert(blocks_chain.begin(), minedBlock);

                // Create a new block based on the mined block
                BLOCK_T newBlock = {};
                newBlock.prev_hash = minedBlock.hash;
                newBlock.height = (int)blocks_chain.size();
                newBlock.difficulty = difficulty;                

                // Send the new block to all subscribed miners
                for (auto& miner : miners_subscribed) {
                    write(miner.second, &newBlock, sizeof(BLOCK_T));
                }
            }
        }
}

int main() {
    char server_pipe[MAX_PATH_LEN] = SERVER_PIPE;
    char server_pipe_for_id[MAX_PATH_LEN] = SERVER_PIPE_FOR_ID;
    char miner_pipe[MAX_PATH_LEN];
    std::vector<int> miners_pipes; // יצירת רשימה ריקה עבור המינימר
    std::vector<BLOCK_T> blocks_chain;

    FILE* logFile = fopen(LOG_PATH, "r");
    if (logFile == NULL) {
        std::cout << "Error: Could not open log file at " << LOG_PATH << std::endl;
        return 1;
    }

    difficulty = 15;
    //readDifficultyFromFile(); // קריאת רמת הקושי מקובץ הקונפיגורציה

    createPipe(server_pipe);
    int serverPipeFd = open(server_pipe, O_RDWR);
    if (serverPipeFd == -1) {
    std::cout << "Error opening server pipe " << server_pipe << ". Error message: " << strerror(errno) << std::endl;
    return 1;
    }

    writeLogMessageToFile(logFile, "Listeting on " + std::string(SERVER_PIPE));

    char buffer[256];

    BLOCK_T first_block = creatingfirstBlock();
    blocks_chain.push_back(first_block);

    blockToMine.prev_hash = blocks_chain.front().hash;
    blockToMine.height = (int)blocks_chain.size();
    blockToMine.difficulty = difficulty;   

    while (true) {
        ssize_t bytesRead = read(serverPipeFd, buffer, 256); //read if sub or blk
        if(bytesRead > 0) 
        {
            subscriptionOrNewBlockRequest(buffer, miners_pipes, logFile, blocks_chain);        
        }
    } 

    //logFile.close(); 
    for (auto& minerData : miners_subscribed)
        {
            close(minerData.second);
        }
    close(serverPipeFd);
}
