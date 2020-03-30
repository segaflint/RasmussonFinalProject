/*
 * Code by Seth M Rasmusson
 * 02/12/2020
 * References for stdio https://www.tutorialspoint.com/c_standard_library/stdio_h.htm
*/

#include "IOMngr.h"

#define TRUE 1
#define FALSE 0

int hasListingFile;
int currentLineNum;
FILE *sourceFile = NULL;
FILE *listingFile = NULL;
char buffer[MAXLINE] = {'\0'};
int currentColumnNum = 0;
int currentLinePrinted = FALSE;
FILE * outputFile = NULL;
int callsToGetNextSourceChar = 0;
char * pointer;

int openFiles(char * sourceName, char * listingName){
    int openSuccessful = FALSE;
    //Open the source file whose name is given in sourceName
    //You can assume sourceName is assigned a legal
    //char* (i.e. a string)
    sourceFile = fopen(sourceName, "r");
    if(sourceFile == NULL) openSuccessful = FALSE;
    else openSuccessful = TRUE;

    // • If listingName is not NULL open the listing file whose name is given in listingName
    // – If listingName is NULL, the output goes to stdout
    if(listingName == NULL){
        hasListingFile = FALSE;
        outputFile = stdout;
    }
    else {
        listingFile = fopen(listingName, "w");
        if(listingFile != NULL) {
            outputFile = listingFile;
        } else {
            openSuccessful = FALSE;
        }
        hasListingFile = TRUE;
    }

    //Return 1 if the file open(s) were successful, otherwise return 0
    return openSuccessful;
}

void closeFiles(){
    //Close the source file and the listing file if one was created
    fclose(sourceFile);
    if(hasListingFile) fclose(listingFile);
}

char getNextSourceChar(){
    callsToGetNextSourceChar++;

    currentColumnNum++;
    if(buffer[currentColumnNum] == '\0') { // initial state or end of line
        pointer = fgets(buffer, MAXLINE, sourceFile);
        currentColumnNum = 0;// reset column number
        currentLineNum++;// next line number
        currentLinePrinted = FALSE; // reset
    }

    //print the line if not already printed
    if(!currentLinePrinted && pointer != NULL) {
        fprintf(outputFile, "%d: %s", currentLineNum, buffer);
        currentLinePrinted = TRUE;
    }

    // Return the next source char
    // This function is also responsible for echoing the lines in the source file to the listing file (if one exists)
    // The lines in the listing file should be numbered
    // Return EOF when the end of the source file is reached
    if( pointer == NULL ) {
        return EOF;
    }


    return buffer[currentColumnNum];
}

void writeIndicator(int column){
    char * indicator = "^\n";
    // Write a line containing a single ‘^’ character in the indicated column
    // • If there is no listing file then the current line should be echoed to stdout
    //  the first time (for that line) that writeIndicator or writeMessage is called.
    fprintf(outputFile, "   ");
    while(column != 0) {
        fprintf(outputFile, " ");
        column--;
    }
    fprintf(outputFile, "%s", indicator);

}

void writeMessage(char * message){
    // Write the message on a separate line
    fprintf(outputFile, "%s\n", message);
}

int getCurrentLineNum(){
    // – Return the current line number
    return currentLineNum;
}

int getCurrentColumnNum(){
    // – Return the current column number in the
    // current line.
    return currentColumnNum;
}
