#include <iostream> // כלול קלט/פלט סטנדרטי
#include <fstream>  // כלול קבצי קלט/פלט
#include <string>   // כלול עבודה עם מחרוזות
#include <fcntl.h>  // כלול פונקציות לפתיחת קבצים/צינורות
#include <sys/stat.h> // כלול פונקציות לעבודה עם קבצים וצינורות
#include <unistd.h> // כלול פונקציות של מערכת Unix
#include "blockChain.h" // כלול את קובץ הכותרת של מחלקת blockChain





int main() {
    TLV tlv_to_send;
    tlv_to_send.subscription =false;
    //tlv_to_send.block intilzie
    TLV tlv_to_check;
    int num_miners=0;
     char* server_pipe  = (char*)malloc(MAX_PATH_LEN);
     char* server_pipe_for_id= (char*)malloc(MAX_PATH_LEN);
     char* miner_pipe  = (char*)malloc(MAX_PATH_LEN);

    std::vector<int> miners_pipes; // יצירת רשימה ריקה עבור המינימר

     
    if (mkfifo(server_pipe, 0666) == -1) { // server reads from 
        if (errno != EEXIST) {
            perror("mkfifo");
            return 1;
        }
    }
    int difficulty = readDifficultyFromFile(); // קריאת רמת הקושי מקובץ הקונפיגורציה

    blockChain blockchain(difficulty); // יצירת אובייקט של הבלוקצ'יין עם רמת הקושי

    int serverPipeFd = open(server_pipe, O_RDONLY);
    if (serverPipeFd == -1) {
        std::cerr << "Error: Could not open server pipe for reading." << std::endl; // הודעת שגיאה אם לא ניתן לפתוח את הצינור
        return 1;
    }

    char minerPipeName[256];
    while (true) {
        
        ssize_t bytesRead = read(serverPipeFd,&tlv_to_check, sizeof(minerPipeName)); // קריאת שם הצינור של הכורה מהצינור של השרת
        if(tlv_to_check.subscription)
        {
            num_miners++;
            sprintf(miner_pipe, "%s%d", MINER_PIPE, num_miners);// create /mnt/mta/miner_pipe_id
            // open pipe for mine with path /mnt/mta/miner_pipe_id
            if (mkfifo(miner_pipe, 0666) == -1) {
                if (errno != EEXIST) {
                    perror("mkfifo");
                    return 1;
                }
            }
            miners_pipes.push_back(open(miner_pipe, O_WRONLY));
            write(miners_pipes[num_miners-1], &tlv_to_send, sizeof(tlv_to_check));
        }
        else
        {
            //check if he block is valid or not
            //print the added block 
            // make tlv_to_send
            //send tlv to all miners go with iteration on fd vector
        }

    }

    close(serverPipeFd); // סגירת צינור השרת
    unlink(PIPE_NAME); // מחיקת הצינור של השרת
    return 0;
}
