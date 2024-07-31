#include <iostream> // כלול קלט/פלט סטנדרטי
#include <string>   // כלול עבודה עם מחרוזות
#include <fcntl.h>  // כלול פונקציות לפתיחת קבצים/צינורות
#include <sys/stat.h> // כלול פונקציות לעבודה עם קבצים וצינורות
#include <unistd.h> // כלול פונקציות של מערכת Unix
#include "blockChain.h" // כלול את קובץ הכותרת של מחלקת blockChain

#define SERVER_PIPE "/mnt/mta/server_pipe" // הגדר את שם הצינור של השרת

void createPipe(const std::string& pipeName) {
    mkfifo(pipeName.c_str(), 0666); // יצירת צינור חדש עם הרשאות קריאה וכתיבה
}

std::string getMinerPipeName(int minerId) {
    return "/mnt/mta/miner_pipe_" + std::to_string(minerId); // יצירת שם צינור עבור הכורה בהתבסס על מזהה הכורה
}

int main(int argc, char* argv[]) {
    TLV tlv_to_send;
    char* server_pipe  = (char*)malloc(MAX_PATH_LEN);// s/mnt/mta/server_pipe
    char* server_pipe_for_id = (char*)malloc(MAX_PATH_LEN);// /mnt/mta/server_pipe_for_id
    char* miner_pipe  = (char*)malloc(MAX_PATH_LEN);// /mnt/mta/miner_pipe_
    int server_pipe_for_id_fd = open(server_pipe_for_id, O_RDONLY);
    int server_pipe_fd = open(server_pipe,O_WRONLY);
    int id;
    write(server_pipe_fd,&tlv_to_send ,sizeof(TLV));
    read(server_pipe_for_id_fd,&id,sizeof(int));
    sprintf(miner_pipe,"%s%s",miner_pipe,id); // create /mnt/mta/miner_pipe_id
    int miner_pipe_fd = open(miner_pipe,O_RDONLY|O_NONBLOCK);
    sleep(1);
    while (true)
    {
        read(miner_pipe_fd,&tlv_to_send ,sizeof(TLV));
        //change realyed_by to the miner id
        //mineblock the loop that check the zeros
    }
    
}
