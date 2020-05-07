# RasmussonFinalProject
A compiler project to compile c-like code into MIPS assembly instructions

Functional areas of the compiler language:

1. INTEGER EXPRESSIONS : *Fully functional*
2. INTEGER I/O: *Fully functional*
  * Syntax for I/O:
    * print(ExpressionList): prints a list of expressions
    * printLine : prints a newline
    * printSpaces(Expression): prints the number of spaces derived from the expression
    * read(VariableList): reads user input into local and global ints and array values
3. CONTROL STRUCTURES: *Fully functional*
4. ARRAYS: *Fully functional*
5. Functions: *Fully functional*
  * Includes support for:
    * Void and Int Functions
    * Value Parameters of type int and array
    * Local variables of int type and arrays (NOTE: locals should be declared before opening curly brackets)
    * Recursion
6. BOOLEAN EXPRESSIONS AND TYPE: *Unsupported*

####### TESTING #######
  All tests given by Dr. Gendreau passed (t1-t17) and were translated into this compiler's language. The translated
files can be found in the myTests folder included. One further test (testLocalArrays.txt) was performed to demonstrate the use of local arrays.
Output for each can be found in the testsOutput folder included. Format for each output file shows any user input and
the output in the form that was given using MARS. For instance, if the program reads a number into variable x then prints
variable x, output would look like the following, given user input of 5:
5
5
