#include <iostream> // כלול קלט/פלט סטנדרטי
#include <string>   // כלול עבודה עם מחרוזות
#include <fcntl.h>  // כלול פונקציות לפתיחת קבצים/צינורות
#include <sys/stat.h> // כלול פונקציות לעבודה עם קבצים וצינורות
#include <unistd.h> // כלול פונקציות של מערכת Unix
#include <fstream>
#include <zlib.h>
#include <cstring>
#include <vector>

#define SERVER_PIPE "/mnt/mta/server_pipe" // הגדר את שם הצינור של השרת
#define MINER_PIPE_PREFIX "/mnt/mta/miner_pipe_" 
#define LOG_PATH "/var/log/mtacoin.log"
#define SERVER_PIPE_FOR_ID "/mnt/mta/serverPipeForId"
#define BLOCK_HEADER "addANewBlock:"
#define SUBSCRIPTION_HEADER "subscription:"
#define MAX_PATH_LEN 256

struct BLOCK_T {
    int height;
    int timeStamp;
    unsigned int hash;
    unsigned int prev_hash;
    int difficulty;
    int nonce;
    int relayed_by;
};

BLOCK_T blockToMine = {};
int difficulty = 15;

void createPipe(const std::string& pipeName) {
    mkfifo(pipeName.c_str(), 0766); // יצירת צינור חדש עם הרשאות קריאה וכתיבה
}

std::string getMinerPipeName(int minerId) {
    return MINER_PIPE_PREFIX + std::to_string(minerId); // יצירת שם צינור עבור הכורה בהתבסס על מזהה הכורה
}

void writeLogMessageToFile(FILE* logFile, std::string messageToSend)
{
    std::cout << messageToSend << std::endl;
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

bool isBlockValid(const BLOCK_T& block, std::vector<BLOCK_T> blocks_chain)
{
    // Check if the block's prev_hash matches the hash of the previous block
    if (block.height > 0 && block.prev_hash != blocks_chain.back().hash)
    {
        return false;
    }

    // Check if the block's hash satisfies the required difficulty level
    return validationProofOfWork(block.hash, block.difficulty);
}

int calculateHash(const BLOCK_T& block) {
    std::string input = std::to_string(block.height) +
                   std::to_string(block.timeStamp) +
                   std::to_string(block.prev_hash) +
                   std::to_string(block.nonce) +
                   std::to_string(block.relayed_by);
    return crc32(0,reinterpret_cast<const unsigned char*>(input.c_str()),input.length());
}

int findNextAvailableMinerID() {

    for (int i = 1; i <= 4; i++) {
        std::string pipeNameToCheck = MINER_PIPE_PREFIX + std::to_string(i);

        //check if pipe doesn't exist
        if (access(pipeNameToCheck.c_str(), F_OK) == -1) {
            return i;
        }
    }

    return -1; //no available ID's
}

bool BuildTheSubBlockRequest(int minerID, const std::string& minerPipeName, int serverPipeFD, FILE* logFile) {
    std::string message = SUBSCRIPTION_HEADER " Miner #" + std::to_string(minerID) + " sent connect request on " + minerPipeName;

    if (write(serverPipeFD, message.c_str(), message.length()) == -1) {
        writeLogMessageToFile(logFile, "ERROR: Failed to send subscription request for miner #" + std::to_string(minerID));
        return false;
    }
    writeLogMessageToFile(logFile, message);

    // Open the miner's pipe to wait for the response from the server
    int minerPipeFD = open(minerPipeName.c_str(), O_RDONLY);
    if (minerPipeFD == -1) {
        writeLogMessageToFile(logFile, "ERROR: Failed to open miner #" + std::to_string(minerID) + " pipe");
        return false;
    }

    // Read the block from the server
    if (read(minerPipeFD, &blockToMine, sizeof(BLOCK_T)) == -1) {
        writeLogMessageToFile(logFile, "ERROR: Failed to read block for miner #" + std::to_string(minerID));
        close(minerPipeFD);
        return false;
    }

    // Log the received block details
    std::string logMessage = "Miner #" + std::to_string(minerID) +
                             " received first block: relayed_by(" + std::to_string(blockToMine.relayed_by) +
                             "), height(" + std::to_string(blockToMine.height) +
                             "), timestamp (" + std::to_string(blockToMine.timeStamp) +
                             "), hash(0x" + std::to_string(blockToMine.hash) +
                             "), prev_hash(0x" + std::to_string(blockToMine.prev_hash) +
                             "), difficulty(" + std::to_string(blockToMine.difficulty) +
                             "), nonce(" + std::to_string(blockToMine.nonce) + ")";

    writeLogMessageToFile(logFile, logMessage);
    close(minerPipeFD);

    return true;
}

int main(int argc, char* argv[]) {
    char server_pipe[MAX_PATH_LEN] = SERVER_PIPE;
    char server_pipe_for_id[MAX_PATH_LEN] = SERVER_PIPE_FOR_ID;
    char miner_pipe[MAX_PATH_LEN];

    int minerID = findNextAvailableMinerID();
    if (minerID == -1)
    {
        std::cerr << "ERROR: No available ID's" << std::endl;
        return 1;
    }

    FILE* logFile = fopen(LOG_PATH, "r");
    if (logFile == NULL) {
        std::cout << "Error: Could not open log file at " << LOG_PATH << std::endl;
        return 1;
    }

    std::string minerPipe = getMinerPipeName(minerID);
    createPipe(minerPipe.c_str());

    int server_pipe_fd = open(server_pipe, O_WRONLY);
    if (server_pipe_fd == -1){
        writeLogMessageToFile(logFile, std::string("ERROR: Failed to open server pipe"));
        fclose(logFile);
        return 1;
    }

    if (!BuildTheSubBlockRequest(minerID, minerPipe, server_pipe_fd, logFile)) {
        fclose(logFile);
        close(server_pipe_fd);
        return 1;
    } 

    
    int minerPipeFD = open(minerPipe.c_str(), O_RDONLY | O_NONBLOCK);
    if (minerPipeFD == -1){
        writeLogMessageToFile(logFile, std::string("ERROR: Failed to open miner #" + std::to_string(minerID) + " pipe"));
        fclose(logFile);
        close(server_pipe_fd);
        return 1;
    }

    char buffer[sizeof(BLOCK_T) + sizeof(BLOCK_HEADER)] = {0};  // Initialize the buffer with zeros
    // Copy the BLOCK_HEADER into the buffer
    std::memcpy(buffer, BLOCK_HEADER, sizeof(BLOCK_HEADER) - 1); // -1 to exclude the null terminator

    while (true)
    {
        blockToMine.relayed_by = minerID;
        blockToMine.nonce = 0;
        difficulty = blockToMine.difficulty;

        while (true)
        {
            blockToMine.nonce++;
            blockToMine.timeStamp = time(nullptr);
            blockToMine.prev_hash = blockToMine.hash;
            blockToMine.hash = calculateHash(blockToMine);
            
            if (validationProofOfWork(blockToMine.hash, blockToMine.difficulty))
            {
                std::string messageFromMinerNewBlockMined = "Miner #" + std::to_string(minerID) + 
                                                            " mined a new block #" + std::to_string(blockToMine.height) + 
                                                            ", with the hash 0x" + std::to_string(blockToMine.hash) + 
                                                            ", difficulty " + std::to_string(blockToMine.difficulty); 
                writeLogMessageToFile(logFile, messageFromMinerNewBlockMined);

                memcpy(buffer + 14, &blockToMine, sizeof(BLOCK_T));
                write(server_pipe_fd, buffer, sizeof(buffer));
            }

            BLOCK_T block = {};
            read(server_pipe_fd, &block, sizeof(BLOCK_T));
            //block.height = blockToMine.height + 1;

            if (block.height != 0 && block.height != blockToMine.height) 
            {
                blockToMine = block;
                std::string messageFromMinerNewBlockReceived = "Miner #" + std::to_string(minerID) + 
                                      " received a new block: height(" + std::to_string(blockToMine.height) + 
                                      "), timestamp (" + std::to_string(blockToMine.timeStamp) + 
                                      "), hash(0x" + std::to_string(blockToMine.hash) + 
                                      "), prev_hash(0x" + std::to_string(blockToMine.prev_hash) + 
                                      "), difficulty(" + std::to_string(blockToMine.difficulty) + 
                                      "), nonce(" + std::to_string(blockToMine.nonce) + ")"; 
                writeLogMessageToFile(logFile, messageFromMinerNewBlockReceived);
                break;
            }
        }
        //logFile.close();
        close(server_pipe_fd);
        close(minerPipeFD);
    }
}
