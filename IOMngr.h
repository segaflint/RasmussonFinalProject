#include <stdlib.h>
#include <stdio.h>

#define MAXLINE 1024

int openFiles(char * sourceName, char * listingName);
    /*
    Open the source file whose name is given in
    sourceName
    – You can assume sourceName is assigned a legal
    char* (i.e. a string)
    • If listingName is not NULL open the listing file whose name is given in listingName
    – If listingName is NULL, the output goes to stdout
    • Return 1 if the file open(s) were successful, otherwise return 0
    */

void closeFiles();
//Close the source file and the listing file if one was created

char getNextSourceChar();
// Return the next source char
// • This function is also responsible for echoing the lines in the source file to the listing file (if one exists)
// • The lines in the listing file should be numbered
// • Return EOF when the end of the source file is reached

void writeIndicator(int column);
// Write a line containing a single ‘^’ character in the indicated column
// • If there is no listing file then the current line should be echoed to stdout
//  the first time (for that line) that writeIndicator or writeMessage is called.

void writeMessage(char * message);
// Write the message on a separate line

int getCurrentLineNum();
// – Return the current line number

int getCurrentColumnNum();
// – Return the current column number in the
// current line.
