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
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <miner_id>" << std::endl; // הודעת שגיאה אם לא סופק מזהה הכורה
        return 1;
    }

    int minerId = std::stoi(argv[1]); // המרה של מזהה הכורה ממחרוזת למספר שלם
    std::string minerPipeName = getMinerPipeName(minerId); // יצירת שם הצינור של הכורה

    createPipe(minerPipeName); // יצירת צינור עבור הכורה

    int serverPipeFd = open(SERVER_PIPE, O_WRONLY); // פתיחת צינור השרת לכתיבה
    if (serverPipeFd == -1) {
        std::cerr << "Error: Could not open server pipe for writing." << std::endl; // הודעת שגיאה אם לא ניתן לפתוח את הצינור
        return 1;
    }

    write(serverPipeFd, minerPipeName.c_str(), minerPipeName.size() + 1); // כתיבת שם הצינור של הכורה לצינור של השרת
    close(serverPipeFd); // סגירת צינור השרת

    int minerPipeFd = open(minerPipeName.c_str(), O_RDONLY); // פתיחת צינור הכורה לקריאה
    if (minerPipeFd == -1) {
        std::cerr << "Error: Could not open miner pipe for reading." << std::endl; // הודעת שגיאה אם לא ניתן לפתוח את הצינור
        return 1;
    }

    BLOCK_T currentBlock;
    read(minerPipeFd, &currentBlock, sizeof(BLOCK_T)); // קריאת הבלוק הנוכחי מהצינור של הכורה

    blockChain blockchain(currentBlock.difficulty); // יצירת אובייקט של הבלוקצ'יין עם רמת הקושי של הבלוק הנוכחי
    blockchain.setBlock(currentBlock); // הגדרת הבלוק הנוכחי בבלוקצ'יין
    blockchain.startMining(minerId, minerPipeName); // התחלת כרייה

    close(minerPipeFd); // סגירת צינור הכורה
    unlink(minerPipeName.c_str()); // מחיקת הצינור של הכורה

    return 0;
}
