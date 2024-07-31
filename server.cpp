#include <iostream> // כלול קלט/פלט סטנדרטי
#include <fstream>  // כלול קבצי קלט/פלט
#include <string>   // כלול עבודה עם מחרוזות
#include <fcntl.h>  // כלול פונקציות לפתיחת קבצים/צינורות
#include <sys/stat.h> // כלול פונקציות לעבודה עם קבצים וצינורות
#include <unistd.h> // כלול פונקציות של מערכת Unix
#include "blockChain.h" // כלול את קובץ הכותרת של מחלקת blockChain

#define PIPE_NAME "/mnt/mta/server_pipe" // הגדר שם צינור עבור השרת
#define DIFFICULTY_FILE "/mnt/mta/difficulty.conf" // הגדר מיקום קובץ הקונפיגורציה של רמת הקושי

void createPipe(const std::string& pipeName) {
    mkfifo(pipeName.c_str(), 0666); // יצירת צינור חדש עם הרשאות קריאה וכתיבה
}

int readDifficultyFromFile() {
    std::ifstream file(DIFFICULTY_FILE); // פתיחת קובץ הקונפיגורציה לקריאה
    if (!file.is_open()) {
        std::cerr << "Error: Could not open difficulty configuration file." << std::endl; // הודעת שגיאה אם לא ניתן לפתוח את הקובץ
        exit(EXIT_FAILURE); // יציאה עם קוד שגיאה
    }

    int difficulty;
    file >> difficulty; // קריאת רמת הקושי מהקובץ
    file.close(); // סגירת הקובץ

    return difficulty; // החזרת רמת הקושי
}

void handleNewMiner(blockChain& blockchain, const std::string& minerPipeName) {
    int minerPipeFd = open(minerPipeName.c_str(), O_WRONLY); // פתיחת צינור של הכורה לכתיבה
    if (minerPipeFd == -1) {
        std::cerr << "Error: Could not open miner pipe for writing." << std::endl; // הודעת שגיאה אם לא ניתן לפתוח את הצינור
        return;
    }

    BLOCK_T currentBlock = blockchain.getBlock(); // קבלת הבלוק הנוכחי מהבלוקצ'יין
    write(minerPipeFd, &currentBlock, sizeof(BLOCK_T)); // כתיבת הבלוק לצינור של הכורה
    close(minerPipeFd); // סגירת הצינור של הכורה
}

int main() {
    createPipe(PIPE_NAME); // יצירת צינור עבור השרת
    int difficulty = readDifficultyFromFile(); // קריאת רמת הקושי מקובץ הקונפיגורציה
    blockChain blockchain(difficulty); // יצירת אובייקט של הבלוקצ'יין עם רמת הקושי

    int serverPipeFd = open(PIPE_NAME, O_RDONLY); // פתיחת צינור השרת לקריאה
    if (serverPipeFd == -1) {
        std::cerr << "Error: Could not open server pipe for reading." << std::endl; // הודעת שגיאה אם לא ניתן לפתוח את הצינור
        return 1;
    }

    char minerPipeName[256];
    while (true) {
        ssize_t bytesRead = read(serverPipeFd, minerPipeName, sizeof(minerPipeName)); // קריאת שם הצינור של הכורה מהצינור של השרת
        if (bytesRead > 0) {
            handleNewMiner(blockchain, std::string(minerPipeName)); // טיפול בכורה החדש
        }
    }

    close(serverPipeFd); // סגירת צינור השרת
    unlink(PIPE_NAME); // מחיקת הצינור של השרת
    return 0;
}
