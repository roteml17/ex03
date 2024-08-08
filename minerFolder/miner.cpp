#include <iostream> // כלול קלט/פלט סטנדרטי
#include <string>   // כלול עבודה עם מחרוזות
#include <fcntl.h>  // כלול פונקציות לפתיחת קבצים/צינורות
#include <sys/stat.h> // כלול פונקציות לעבודה עם קבצים וצינורות
#include <unistd.h> // כלול פונקציות של מערכת Unix
#include <fstream>
#include <zlib.h>
#include <cstring>

#define SERVER_PIPE "/mnt/mta/server_pipe" // הגדר את שם הצינור של השרת
#define LOG_PATH "/var/log/mtacoin.log"
#define SERVER_PIPE_FOR_ID "/mnt/mta/serverPipeForId"
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

struct TLV {
public:
    BLOCK_T block;
    bool subscription = true;
};



void createPipe(const std::string& pipeName) {
    mkfifo(pipeName.c_str(), 0766); // יצירת צינור חדש עם הרשאות קריאה וכתיבה
}

std::string getMinerPipeName(int minerId) {
    return "/mnt/mta/miner_pipe_" + std::to_string(minerId); // יצירת שם צינור עבור הכורה בהתבסס על מזהה הכורה
}

void writeLogMessageToFile(FILE* logFile, std::string messageToSend)
{
    std::cout << messageToSend << std::endl;
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

int main(int argc, char* argv[]) {
    TLV tlv_to_send = creatingNewTLV(0);
    tlv_to_send.subscription = true;

    FILE* logFile = fopen(LOG_PATH, "r");
    if (logFile == NULL) {
        std::cout << "Error: Could not open log file at " << LOG_PATH << std::endl;
        return 1;
    }

    char server_pipe[MAX_PATH_LEN] = SERVER_PIPE;
    char server_pipe_for_id[MAX_PATH_LEN] = SERVER_PIPE_FOR_ID;
    char miner_pipe[MAX_PATH_LEN];

    int server_pipe_fd = open(server_pipe,O_RDWR); 
    int server_pipe_for_id_fd = open(server_pipe_for_id, O_RDWR);


    ssize_t b = write(server_pipe_fd,&tlv_to_send ,sizeof(TLV));

    int id = 0;
    read(server_pipe_for_id_fd,&id,sizeof(int));
    std::cout << "Miner id: " << id << std::endl;
    sprintf(miner_pipe, "/mnt/mta/miner_pipe_%d", id); // create /mnt/mta/miner_pipe_id
    std::cout << "Miner pipe name: " << miner_pipe << std::endl;


    createPipe(miner_pipe);
    int miner_pipe_fd = open(miner_pipe,O_RDWR|O_NONBLOCK); //open the pipe of the miner***
    std::string messageFromMinerToServer = "Miner " + std::to_string(id) + " sent connect request on " + miner_pipe;
    ssize_t bytes_written = write(server_pipe_fd, messageFromMinerToServer.c_str(), messageFromMinerToServer.size());
    if (bytes_written == -1) {
        std::cerr << "Error: Could not write to server pipe."  << strerror(errno)<< std::endl;
        return 1;
    } else {
        std::cout << "Sent connect request to server: " << messageFromMinerToServer << std::endl;
    }
    writeLogMessageToFile(logFile, messageFromMinerToServer);
    sleep(1);

    read(miner_pipe_fd, &tlv_to_send, sizeof(TLV));
    std::string messageFromMinerAboutBlock = "Miner #" + std::to_string(id) + 
                          " received first block: relayed_by(" + std::to_string(tlv_to_send.block.relayed_by) + "), " + 
                          "height(" + std::to_string(tlv_to_send.block.height) + "), " + 
                          "timestamp(" + std::to_string(tlv_to_send.block.timeStamp) + "), " + 
                          "hash(0x" + std::to_string(tlv_to_send.block.hash) + "), " + 
                          "prev_hash(0x" + std::to_string(tlv_to_send.block.prev_hash) + "), " + 
                          "difficulty(" + std::to_string(tlv_to_send.block.difficulty) + "), " + 
                          "nonce(" + std::to_string(tlv_to_send.block.nonce) + ")"; 
    writeLogMessageToFile(logFile, messageFromMinerAboutBlock);
    tlv_to_send.subscription = false; ///////////


    while (true)
    {
        tlv_to_send.block.relayed_by = id;
        tlv_to_send.block.nonce = 0;

        while (true)
        {
            tlv_to_send.block.timeStamp = time(nullptr);
            tlv_to_send.block.hash = calculateHash(tlv_to_send.block);

            if (validationProofOfWork(tlv_to_send.block.hash, tlv_to_send.block.difficulty))
            {
                std::string messageFromMinerNewBlockMined = "Miner #" + std::to_string(id) + 
                                                            " mined a new block #" + std::to_string(tlv_to_send.block.height) + 
                                                            ", with the hash 0x" + std::to_string(tlv_to_send.block.hash) + 
                                                            ", difficulty " + std::to_string(tlv_to_send.block.difficulty); 
                writeLogMessageToFile(logFile, messageFromMinerNewBlockMined);
                write(server_pipe_fd, &tlv_to_send, sizeof(TLV));

                tlv_to_send.block.prev_hash = tlv_to_send.block.hash; //
                tlv_to_send.block.height++; //
                break; //
            }

            TLV newTLV;
            read(miner_pipe_fd, &newTLV, sizeof(TLV));

            if (newTLV.block.height != 0 && newTLV.block.height != tlv_to_send.block.height) 
            {
                tlv_to_send = newTLV;
                std::string messageFromMinerNewBlockReceived = "Miner #" + std::to_string(id) + 
                                      " received a new block: height(" + std::to_string(tlv_to_send.block.height) + 
                                      "), timestamp (" + std::to_string(tlv_to_send.block.timeStamp) + 
                                      "), hash(0x" + std::to_string(tlv_to_send.block.hash) + 
                                      "), prev_hash(0x" + std::to_string(tlv_to_send.block.prev_hash) + 
                                      "), difficulty(" + std::to_string(tlv_to_send.block.difficulty) + 
                                      "), nonce(" + std::to_string(tlv_to_send.block.nonce) + ")"; 
                writeLogMessageToFile(logFile, messageFromMinerNewBlockReceived);
                break;
            }
            tlv_to_send.block.nonce++;
        }
        //logFile.close();
        close(server_pipe_fd);
        close(miner_pipe_fd);
    }
}
