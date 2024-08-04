#include <iostream> // כלול קלט/פלט סטנדרטי
#include <fstream>  // כלול קבצי קלט/פלט
#include <string>   // כלול עבודה עם מחרוזות
#include <fcntl.h>  // כלול פונקציות לפתיחת קבצים/צינורות
#include <sys/stat.h> // כלול פונקציות לעבודה עם קבצים וצינורות
#include <unistd.h> // כלול פונקציות של מערכת Unix
#include "blockChain.h" // כלול את קובץ הכותרת של מחלקת blockChain
#include <unordered_map>

#define SERVER_PIPE "/mnt/mta/server_pipe"
#define SERVER_PIPE_FOR_ID "/mnt/mta/server_pipe_for_id"
#define MINER_PIPE_PREFIX "/mnt/mta/miner_pipe_"
#define CONFIG_FILE "/mnt/mta/config.txt"
#define LOG_PATH "/var/log/mtacoin.log"
#define MAX_PATH_LEN 256

std::unordered_map<int,int> miners_subscribed;

void writeLogMessageToFile(std::ofstream& logFile, std::string messageToSend)
{
    logFile << messageToSend << std::endl;
}

int readDifficultyFromFile(std::ofstream& logFile) {
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

TLV creatingNewTLV(int difficulty)
{
    TLV genesis;
    genesis.block.height = 0;
    genesis.block.timeStamp = time(nullptr);
    genesis.block.prev_hash = 0;
    genesis.block.difficulty = difficulty;
    genesis.block.nonce = 0;
    genesis.block.relayed_by = -1;
    genesis.block.hash = calculateHash(genesis.block);

    return genesis;
}

void createPipe(const std::string& pipeName) {
    mkfifo(pipeName.c_str(), 0666);
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

void addMiner(int serverPipeForIdFd, std::vector<int>& miners_pipes, std::vector<BLOCK_T> blocks_chain, std::ofstream& logFile, int num_miners, char miner_pipe[]) {
    // הגדל את מספר הכורים
    num_miners++;

    // צור את שם הצינור עבור הכורה החדש
    sprintf(miner_pipe, "%s%d", MINER_PIPE, num_miners); // יצירת /mnt/mta/miner_pipe_id
    createPipe(miner_pipe);

    // פתח את הצינור לכתיבה והוסף אותו לרשימת הצינורות
    int minerPipeFd = open(miner_pipe, O_WRONLY);
    if (minerPipeFd == -1) {
        writeLogMessageToFile(logFile, "Error opening miner pipe " + std::string(miner_pipe));
        return;
    }

    miners_subscribed[num_miners] = minerPipeFd;
    miners_pipes.push_back(minerPipeFd);

    // כתוב את מזהה הכורה לצינור של השרת
    write(serverPipeForIdFd, &num_miners, sizeof(int));

    // שלח את הבלוק הנוכחי לכורה החדש
    TLV tlv_to_send;
    tlv_to_send.block = blocks_chain.back();
    write(minerPipeFd, &tlv_to_send, sizeof(tlv_to_send));

    // כתוב לוג של הפעולה
    writeLogMessageToFile(logFile, "Miner " + std::to_string(num_miners) + " connected with pipe " + std::string(miner_pipe));
}

int main() {
    TLV tlv_to_send;
    TLV first_tlv;  
    int num_miners=0;

    char server_pipe[MAX_PATH_LEN] = SERVER_PIPE;
    char server_pipe_for_id[MAX_PATH_LEN] = SERVER_PIPE_FOR_ID;
    char miner_pipe[MAX_PATH_LEN];

    std::ofstream logFile(LOG_PATH);
    if (!logFile) {
        std::cerr << "Error: Could not open log file at " << LOG_PATH << std::endl;
        return;
    }

    int difficulty = readDifficultyFromFile(logFile); // קריאת רמת הקושי מקובץ הקונפיגורציה

    std::vector<int> miners_pipes; // יצירת רשימה ריקה עבור המינימר
    std::vector<BLOCK_T> blocks_chain;
    first_tlv = creatingNewTLV(difficulty);
    blocks_chain.push_back(first_tlv.block);

    tlv_to_send.block.prev_hash = blocks_chain.back().hash;
    tlv_to_send.block.height = static_cast<int>(blocks_chain.size());
    tlv_to_send.block.difficulty = difficulty;

    createPipe(server_pipe);
    int serverPipeFd = open(server_pipe, O_RDONLY);
    writeLogMessageToFile(logFile, "Listeting on " + std::string(SERVER_PIPE));

    // if (mkfifo(server_pipe_for_id, 0666) == -1 && errno != EEXIST) {
    //     perror("mkfifo");
    //     return 1;
    // }

    // blockChain blockchain(difficulty); // יצירת אובייקט של הבלוקצ'יין עם רמת הקושי
    createPipe(server_pipe_for_id);
    int serverPipeForIdFd = open(server_pipe_for_id, O_WRONLY);

    while (true) {
        ssize_t bytesRead = read(serverPipeFd,&tlv_to_send, sizeof(TLV)); // קריאת שם הצינור של הכורה מהצינור של השרת
        if(tlv_to_send.subscription)
        {
            addMiner(serverPipeForIdFd, miners_pipes, blocks_chain, logFile, num_miners, miner_pipe);
        }
        else
        {
            if (isBlockValid(tlv_to_send.block, blocks_chain))
            {
                std::string serverMassegeNewBlockAdded = "Server: New block added by " + std::to_string(tlv_to_send.block.relayed_by) +
                                 ", attributes: height(" + std::to_string(tlv_to_send.block.height) + 
                                 "), timestamp (" + std::to_string(tlv_to_send.block.timeStamp) + 
                                 "), hash(0x" + std::to_string(tlv_to_send.block.hash) + 
                                 "), prev_hash(0x" + std::to_string(tlv_to_send.block.prev_hash) + 
                                 "), difficulty(" + std::to_string(tlv_to_send.block.difficulty) + 
                                 "), nonce(" + std::to_string(tlv_to_send.block.nonce) + ")";
                writeLogMessageToFile(logFile, serverMassegeNewBlockAdded);

                blocks_chain.push_back(tlv_to_send.block);

                tlv_to_send.block.prev_hash = blocks_chain.back().hash;
                tlv_to_send.block.height = static_cast<int>(blocks_chain.size());
                tlv_to_send.block.difficulty = difficulty;

                for (auto& minerData : miners_subscribed)
                {
                    write(minerData.second, &tlv_to_send, sizeof(TLV));
                }
            }
        }
    } 

    logFile.close(); 
    for (auto& minerData : miners_subscribed)
        {
            close(minerData.second);
        }
    close(serverPipeFd);
}
