#include <iostream> // כלול קלט/פלט סטנדרטי
#include <string>   // כלול עבודה עם מחרוזות
#include <fcntl.h>  // כלול פונקציות לפתיחת קבצים/צינורות
#include <sys/stat.h> // כלול פונקציות לעבודה עם קבצים וצינורות
#include <unistd.h> // כלול פונקציות של מערכת Unix
#include "blockChain.h" // כלול את קובץ הכותרת של מחלקת blockChain
#include <fstream>

#define SERVER_PIPE "/mnt/mta/server_pipe" // הגדר את שם הצינור של השרת
#define LOG_PATH "/var/log/mtacoin.log"
#define MAX_PATH_LEN 256

void createPipe(const std::string& pipeName) {
    mkfifo(pipeName.c_str(), 0666); // יצירת צינור חדש עם הרשאות קריאה וכתיבה
}

std::string getMinerPipeName(int minerId) {
    return "/mnt/mta/miner_pipe_" + std::to_string(minerId); // יצירת שם צינור עבור הכורה בהתבסס על מזהה הכורה
}

void writeLogMessageToFile(std::ofstream& logFile, std::string messageToSend)
{
    logFile << messageToSend << std::endl;
}

int calculateHash(const BLOCK_T& block) {
    std::string input = std::to_string(block.height) +
                   std::to_string(block.timeStamp) +
                   std::to_string(block.prev_hash) +
                   std::to_string(block.nonce) +
                   std::to_string(block.relayed_by);
    return crc32(0,reinterpret_cast<const unsigned char*>(input.c_str()),input.length());
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
    TLV tlv_to_send;
    tlv_to_send.subscription = true;

    std::ofstream logFile(LOG_PATH);
    if (!logFile) {
        std::cerr << "Error: Could not open log file at " << LOG_PATH << std::endl;
        return;
    }

    char server_pipe[MAX_PATH_LEN] = SERVER_PIPE;
    char server_pipe_for_id[MAX_PATH_LEN];
    strcpy(server_pipe_for_id, SERVER_PIPE);
    strcat(server_pipe_for_id, "_for_id");
    char miner_pipe[MAX_PATH_LEN];

    int server_pipe_for_id_fd = open(server_pipe_for_id, O_RDONLY);
    int server_pipe_fd = open(server_pipe,O_WRONLY);

    //write(server_pipe_fd,&tlv_to_send ,sizeof(TLV));
    int id;
    read(server_pipe_for_id_fd,&id,sizeof(int));
    sprintf(miner_pipe, "/mnt/mta/miner_pipe_%d", id); // create /mnt/mta/miner_pipe_id



    createPipe(miner_pipe);
    int miner_pipe_fd = open(miner_pipe,O_RDONLY|O_NONBLOCK); //open the pipe of the miner***
    sleep(1);
    std::string messageFromMinerToServer = "Miner " + std::to_string(id) + " sent connect request on " + miner_pipe;
    ssize_t bytes_written = write(server_pipe_fd, messageFromMinerToServer.c_str(), messageFromMinerToServer.size());
    if (bytes_written == -1) {
        std::cerr << "Error: Could not write to server pipe." << std::endl;
    } else {
        std::cout << "Sent connect request to server: " << messageFromMinerToServer << std::endl;
    }
    writeLogMessageToFile(logFile, messageFromMinerToServer);

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
        logFile.close();
        close(server_pipe_fd);
        close(miner_pipe_fd);
    }
}
