/* ---------------------------------------------------------------------------
 * Originally from Robin Edwards. Modified for use on ESP-devices.
 * Thanks to Robin Edwards for this excellent basic interpreter!
 * 
 * Basic Interpreter
 * Robin Edwards 2014
 * ---------------------------------------------------------------------------
 * This BASIC is modelled on Sinclair BASIC for the ZX81 and ZX Spectrum. It
 * should be capable of running most of the examples in the manual for both
 * machines, with the exception of anything machine specific (i.e. graphics,
 * sound & system variables).
 *
 * Notes
 *  - All numbers (except line numbers) are floats internally
 *  - Multiple commands are allowed per line, seperated by :
 *  - LET is optional e.g. LET a = 6: b = 7
 *  - MOD provides the modulo operator which was missing from Sinclair BASIC.
 *     Both numbers are first rounded to ints e.g. 5 mod 2 = 1
 *  - CONT can be used to continue from a STOP. It does not continue from any
 *     other error condition.
 *  - Arrays can be any dimension. There is no single char limit to names.
 *  - Like Sinclair BASIC, DIM a(10) and LET a = 5 refer to different 'a's.
 *     One is a simple variable, the other is an array. There is no ambiguity
 *     since the one being referred to is always clear from the context.
 *  - String arrays differ from Sinclair BASIC. DIM a$(5,5) makes an array
 *     of 25 strings, which can be any length. e.g. LET a$(1,1)="long string"
 *  - functions like LEN, require brackets e.g. LEN(a$)
 *  - String manipulation functions are LEFT$, MID$, RIGHT$
 *  - RND is a nonary operator not a function i.e. RND not RND()
 *  - LIST takes an optional start and end e.g. LIST 1,100 or LIST 50
 * ---------------------------------------------------------------------------
 */

// Enable debugging:
// 		Search for: //serial.print(.*?);
//		Replace with: //Serial.print\1; //debug 

// Disable debugging:
//		Search for: serial.print(.*?)//debug
// 		Replace with: //Serial.print\1

#include "magnatic-esp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <limits.h>

#include "bcbasic.h"

#define basicX 80
#define basicY 60

//char basicScreen[basicX*basicY];
//char basicScreenColor[basicX*basicY];
char * basicScreen;
char * basicScreenColor;
char lineDirty[basicY];
int hostX=0;
int hostY=0;

int sysPROGEND;
int sysSTACKSTART, sysSTACKEND;
int sysVARSTART, sysVAREND;
int sysGOSUBSTART, sysGOSUBEND;
int MEMORY_SIZE;
char inputMode = 0;
char inkeyChar = 0;
bool flashCursor=true;
unsigned long nextFlashCursorEvent=0;
char fgColor=COLOR_WHITE;
char bgColor=COLOR_BLUE;
bool programRunning=false;

WiFiClient basicOutput;
int freeHeapAfterBasicRun=0;
unsigned long scriptRuntime=0;
bool outputEnabled=false;
String libraryTimeBasic=__TIMESTAMP__;
String scriptTrigger;
File basicFile;
String basicFilename;
int basicFileReadPosition=0;
int basicFileWritePosition=0;
bool basicFileOpen=false;
String basicHttpRecvParamName="value";
String basicHttpRecvParamValue="";
bool httpRecvAvailable=false;
bool httpKeyAvailable=false;
char httpKey;

char string_0[] = "OK";
char string_1[] = "Bad number";
char string_2[] = "Line too long";
char string_3[] = "Unexpected input";
char string_4[] = "Unterminated string";
char string_5[] = "Missing bracket";
char string_6[] = "Error in expr";
char string_7[] = "Numeric expr expected";
char string_8[] = "String expr expected";
char string_9[] = "Line number too big";
char string_10[] = "Out of memory";
char string_11[] = "Div by zero";
char string_12[] = "Variable not found";
char string_13[] = "Bad command";
char string_14[] = "Bad line number";
char string_15[] = "Break pressed";
char string_16[] = "NEXT without FOR";
char string_17[] = "STOP statement";
char string_18[] = "Missing THEN in IF";
char string_19[] = "RETURN without GOSUB";
char string_20[] = "Wrong array dims";
char string_21[] = "Bad array index";
char string_22[] = "Bad string index";
char string_23[] = "Error in VAL input";
char string_24[] = "Bad parameter";
char string_25[] = "File not open";
char string_26[] = "Structure larger than 65535 bytes";

char* errorTable[] = {
    string_0, string_1, string_2, string_3,
    string_4, string_5, string_6, string_7,
    string_8, string_9, string_10, string_11,
    string_12, string_13, string_14, string_15,
    string_16, string_17, string_18, string_19,
    string_20, string_21, string_22, string_23,
    string_24, string_25, string_26
};

// Host-functions
char bytesFreeStr[] = "bytes free";

// Token flags
// bits 1+2 number of arguments
#define TKN_ARGS_NUM_MASK	0x03
// bit 3 return type (set if string)
#define TKN_RET_TYPE_STR	0x04
// bits 4-6 argument type (set if string)
#define TKN_ARG1_TYPE_STR	0x08
#define TKN_ARG2_TYPE_STR	0x10
#define TKN_ARG3_TYPE_STR	0x20

#define TKN_ARG_MASK		0x38
#define TKN_ARG_SHIFT		3
// bits 7,8 formatting
#define TKN_FMT_POST		0x40
#define TKN_FMT_PRE		0x80

//unsigned char mem[MEMORY_SIZE];
unsigned char * mem;
unsigned char tokenBuf[TOKEN_BUF_SIZE];

const TokenTableEntry tokenTable[] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {"(", 0}, {")",0}, {"+",0}, {"-",0},
    {"*",0}, {"/",0}, {"=",0}, {">",0},
    {"<",0}, {"<>",0}, {">=",0}, {"<=",0},
    {":",TKN_FMT_POST}, {";",0}, {",",0}, {"AND",TKN_FMT_PRE|TKN_FMT_POST},
    {"OR",TKN_FMT_PRE|TKN_FMT_POST}, {"NOT",TKN_FMT_POST}, {"PRINT",TKN_FMT_POST}, {"LET",TKN_FMT_POST},
    {"LIST",TKN_FMT_POST}, {"RUN",TKN_FMT_POST}, {"GOTO",TKN_FMT_POST}, {"REM",TKN_FMT_POST},
    {"STOP",TKN_FMT_POST}, {"INPUT",TKN_FMT_POST},  {"CONTINUE",TKN_FMT_POST}, {"IF",TKN_FMT_POST},
    {"THEN",TKN_FMT_PRE|TKN_FMT_POST}, {"LEN",1|TKN_ARG1_TYPE_STR}, {"VAL",1|TKN_ARG1_TYPE_STR}, {"RND",0},
    {"INT",1}, {"STR$", 1|TKN_RET_TYPE_STR}, {"FOR",TKN_FMT_POST}, {"TO",TKN_FMT_PRE|TKN_FMT_POST},
    {"STEP",TKN_FMT_PRE|TKN_FMT_POST}, {"NEXT", TKN_FMT_POST}, {"MOD",TKN_FMT_PRE|TKN_FMT_POST}, {"NEW",TKN_FMT_POST},
    {"GOSUB",TKN_FMT_POST}, {"RETURN",TKN_FMT_POST}, {"DIM", TKN_FMT_POST}, {"LEFT$",2|TKN_ARG1_TYPE_STR|TKN_RET_TYPE_STR},
    {"RIGHT$",2|TKN_ARG1_TYPE_STR|TKN_RET_TYPE_STR}, {"MID$",3|TKN_ARG1_TYPE_STR|TKN_RET_TYPE_STR}, {"CLS",TKN_FMT_POST}, {"PAUSE",TKN_FMT_POST},
    {"POSITION", TKN_FMT_POST},  {"PIN",TKN_FMT_POST}, {"PINMODE", TKN_FMT_POST}, {"INKEY$", 0},
    {"SAVE", TKN_FMT_POST}, {"LOAD", TKN_FMT_POST}, {"PINREAD",1}, {"ANALOGRD",1},
    {"DIR", TKN_FMT_POST}, {"DELETE", TKN_FMT_POST}, {"MILLIS",0}, {"HTTPGET$",1|TKN_ARG1_TYPE_STR|TKN_RET_TYPE_STR},
    {"FREEMEM",0}, {"FREEHOST",0}, {"IPADDR$",TKN_RET_TYPE_STR}, {"SETSSID", TKN_FMT_POST}, {"SETSSIDPW", TKN_FMT_POST},
    {"GETSSID$",TKN_RET_TYPE_STR}, {"OPEN",TKN_FMT_POST}, {"CLOSE",0}, {"READLINE$",TKN_RET_TYPE_STR}, {"WRITE",TKN_FMT_POST}, {"ERASE", TKN_FMT_POST},
    {"EOF",0}, {"HTTPRECV$", TKN_RET_TYPE_STR}, {"REBOOT",0}, {"INDEXOF",2|TKN_ARG1_TYPE_STR|TKN_ARG2_TYPE_STR}, {"COUNTOF",2|TKN_ARG1_TYPE_STR|TKN_ARG2_TYPE_STR},
    {"FGCOLOR",TKN_FMT_POST}, {"BGCOLOR",TKN_FMT_POST}, {"SETMEMSIZE", TKN_FMT_POST}, {"SETFG", TKN_FMT_POST}, {"SETBG", TKN_FMT_POST}, {"HELP", 0}, {"HELP2", 0},
    {"HTTPRECV", 0}, {"DATADIR", 0}, {"RSEEK", TKN_FMT_POST}, {"READPOS",0}, {"CHR$", 1|TKN_RET_TYPE_STR}, {"WSEEK", TKN_FMT_POST}, {"READ$",1|TKN_RET_TYPE_STR},
    {"WRITEPOS",0}, {"HELP3", 0}
};


char host_getKey(){
	char c=inkeyChar;
	inkeyChar=0;
	if (c>=32 && c<=127) {
		return c;
	}
	return 0;
}

float readFloatFromBuffer(unsigned char *p){
	//Serial.println("\treadFloatFromBuffer called"); 
	// Undo the magic of writeFloatToBuffer
	float numVal;
	void* numValPointer=&numVal;
	*(unsigned char*)numValPointer=*p++;
	*(unsigned char*)(numValPointer+1)=*p++;
	*(unsigned char*)(numValPointer+2)=*p++;
	*(unsigned char*)(numValPointer+3)=*p++;
	return numVal;
}

long readLongFromBuffer(unsigned char *p){
	//Serial.println("\treadLongFromBuffer called"); 
	// Undo the magic of writeLongToBuffer
	long numVal;
	void* numValPointer=&numVal;
	*(unsigned char*)numValPointer=*p++;
	*(unsigned char*)(numValPointer+1)=*p++;
	*(unsigned char*)(numValPointer+2)=*p++;
	*(unsigned char*)(numValPointer+3)=*p++;
	return numVal;
}

int readLengthFromBuffer(unsigned char *p){
	//Serial.println("\treadLengthFromBuffer called"); 
	// Undo the magic of writeLengthToBuffer
	return *p*256+*(p+1);
}

void writeFloatToBuffer(float val, unsigned char *p){
	//Serial.println("\twriteFloatToBuffer called"); 
    // Some magic is needed. The ESP-CPU can only handle correctly aligned 32 bit variables like long and float.
	void* valPointer=&val;
	*p++=*(unsigned char*)valPointer;
	*p++=*(unsigned char*)(valPointer+1);
	*p++=*(unsigned char*)(valPointer+2);
	*p++=*(unsigned char*)(valPointer+3);
}

void writeLongToBuffer(long val, unsigned char *p){
	//Serial.println("\twriteLongToBuffer called"); 
    // Some magic is needed. The ESP-CPU can only handle correctly aligned 32 bit variables like long and float.
	void* valPointer=&val;
	*p++=*(unsigned char*)valPointer;
	*p++=*(unsigned char*)(valPointer+1);
	*p++=*(unsigned char*)(valPointer+2);
	*p++=*(unsigned char*)(valPointer+3);
}

void writeIntToBuffer(int val, int *p){
	//Serial.println("\twriteLongToBuffer called"); 
    // Some magic is needed. The ESP-CPU can only handle correctly aligned 32 bit variables like long and float.
	void* valPointer=&val;
	*p++=*(unsigned char*)valPointer;
	*p++=*(unsigned char*)(valPointer+1);
	*p++=*(unsigned char*)(valPointer+2);
	*p++=*(unsigned char*)(valPointer+3);
}

void writeLengthToBuffer(int val, unsigned char *p){
	//Serial.println("\twriteLengthToBuffer called"); 
	*p++=val/256;
	*p=val%256;
}

/* **************************************************************************
 * PROGRAM FUNCTIONS
 * **************************************************************************/
void printTokens(unsigned char *p) {
	//Serial.println("\tprintTokens called"); 
    int modeREM = 0;
    while (*p != TOKEN_EOL) {
        if (*p == TOKEN_IDENT) {
            p++;
            while (*p < 0x80)
                host_outputChar(*p++);
            host_outputChar((*p++)-0x80);
        }
        else if (*p == TOKEN_NUMBER) {
            p++;
            host_outputFloat(readFloatFromBuffer(p));
            p+=4;
        }
        else if (*p == TOKEN_INTEGER) {
            p++;
            host_outputInt(readLongFromBuffer(p));
            p+=4;
        }
        else if (*p == TOKEN_STRING) {
            p++;
            if (modeREM) {
                host_outputString((char*)p);
                p+=1 + strlen((char*)p);
            }
            else {
                host_outputChar('\"');
                while (*p) {
                    if (*p == '\"') host_outputChar('\"');
                    host_outputChar(*p++);
                }
                host_outputChar('\"');
                p++;
            }
        }
        else {
            uint8_t fmt = tokenTable[*p].format;
            if (fmt & TKN_FMT_PRE)
                host_outputChar(' ');
            host_outputString((char *)tokenTable[*p].token);
            if (fmt & TKN_FMT_POST)
                host_outputChar(' ');
            if (*p==TOKEN_REM)
                modeREM = 1;
            p++;
        }
    }
}

void listProg(uint16_t first, uint16_t last) {
	//Serial.println("\tlistProg called"); 
    unsigned char *p = &mem[0];
    while (p < &mem[sysPROGEND]) {
        uint16_t lineNum = readLengthFromBuffer(p+2);
        if ((!first || lineNum >= first) && (!last || lineNum <= last)) {
            host_outputInt(lineNum);
            host_outputChar(' ');
            printTokens(p+4);
            host_newLine();
        }
        p+=readLengthFromBuffer(p);
    }
}

unsigned char *findProgLine(uint16_t targetLineNumber) {
	//Serial.print ("\tfindProgLine called looking for "); 
	//Serial.println(targetLineNumber); 
    unsigned char *p = &mem[0];
    while (p < &mem[sysPROGEND]) {
        uint16_t lineNum = readLengthFromBuffer(p+2);
        //Serial.print("\tFound line "); 
        //Serial.println(lineNum); 
        if (lineNum >= targetLineNumber)
            break;
        p+= readLengthFromBuffer(p);
    }
    return p;
}

void deleteProgLine(unsigned char *p) {
	//Serial.println("\tdeleteProgLine called"); 
    uint16_t lineLen = readLengthFromBuffer(p);
    sysPROGEND -= lineLen;
    memmove(p, p+lineLen, &mem[sysPROGEND] - p);
}

int doProgLine(uint16_t lineNumber, unsigned char* tokenPtr, int tokensLength)
{
	//Serial.println("\tdoProgLine called"); 
    // find line of the at or immediately after the number
    unsigned char *p = findProgLine(lineNumber);
    uint16_t foundLine = 0;
    if (p < &mem[sysPROGEND])
        foundLine = readLengthFromBuffer(p+2);
    // if there's a line matching this one - delete it
    if (foundLine == lineNumber)
        deleteProgLine(p);
    // now check to see if this is an empty line, if so don't insert it
    if (*tokenPtr == TOKEN_EOL)
        return 1;
    // we now need to insert the new line at p
    int bytesNeeded = 4 + tokensLength;	// length, linenum + tokens
    if (sysPROGEND + bytesNeeded > sysVARSTART)
        return 0;
    // make room if this isn't the last line
    if (foundLine)
        memmove(p + bytesNeeded, p, &mem[sysPROGEND] - p);
    writeLengthToBuffer(bytesNeeded,p);
    p += 2;
    writeLengthToBuffer(lineNumber,p);
    p += 2;
    memcpy(p, tokenPtr, tokensLength);
    sysPROGEND += bytesNeeded;
    return 1;
}

/* **************************************************************************
 * CALCULATOR STACK FUNCTIONS
 * **************************************************************************/

// Calculator stack starts at the start of memory after the program
// and grows towards the end
// contains either floats or null-terminated strings with the length on the end

int stackPushNum(float val) {
	//Serial.println("\tstackPushNum called"); 
    if (sysSTACKEND + sizeof(float) > sysVARSTART)
        return 0;	// out of memory
    unsigned char *p = &mem[sysSTACKEND];
    writeFloatToBuffer(val,p);
    sysSTACKEND += sizeof(float);
    return 1;
}
float stackPopNum() {
	//Serial.println("\tstackPopNum called"); 
    sysSTACKEND -= sizeof(float);
    unsigned char *p = &mem[sysSTACKEND];
    return readFloatFromBuffer(p);
}
int stackPushStr(char *str) {
	//Serial.println("\tstackPushStr called"); 
    int len = 1 + strlen(str);
    if (sysSTACKEND + len + 2 > sysVARSTART)
        return 0;	// out of memory
    unsigned char *p = &mem[sysSTACKEND];
    strcpy((char*)p, str);
    p += len;
    writeLengthToBuffer(len,p);
    sysSTACKEND += len + 2;
    return 1;
}
char *stackGetStr() {
	//Serial.println("\tstackGetStr called"); 
    // returns string without popping it
    unsigned char *p = &mem[sysSTACKEND];
    int len=readLengthFromBuffer(p-2);
    return (char *)(p-len-2);
}
char *stackPopStr() {
	//Serial.println("\tstackPopStr called"); 
    unsigned char *p = &mem[sysSTACKEND];
    int len=readLengthFromBuffer(p-2);
    sysSTACKEND -= (len+2);
    return (char *)&mem[sysSTACKEND];
}

void stackAdd2Strs() {
	//Serial.println("\tstackAdd2Strs called"); 
    // equivalent to popping 2 strings, concatenating them and pushing the result
    unsigned char *p = &mem[sysSTACKEND];
    int str2len = readLengthFromBuffer(p-2);
    sysSTACKEND -= (str2len+2);
    char *str2 = (char*)&mem[sysSTACKEND];
    p = &mem[sysSTACKEND];
    int str1len = readLengthFromBuffer(p-2);
    sysSTACKEND -= (str1len+2);
    char *str1 = (char*)&mem[sysSTACKEND];
    p = &mem[sysSTACKEND];
    // shift the second string up (overwriting the null terminator of the first string)
    memmove(str1 + str1len - 1, str2, str2len);
    // write the length and update stackend
    int newLen = str1len + str2len - 1;
    p += newLen;
    writeLengthToBuffer(newLen,p);
    sysSTACKEND += newLen + 2;
}

// mode 0 = LEFT$, 1 = RIGHT$
void stackLeftOrRightStr(int len, int mode) {
	//Serial.println("\tstackLeftOrRightStr called"); 
    // equivalent to popping the current string, doing the operation then pushing it again
    unsigned char *p = &mem[sysSTACKEND];
    int strlen = readLengthFromBuffer(p-2);
    len++; // include trailing null
    if (len > strlen) len = strlen;
    if (len == strlen) return;	// nothing to do
    sysSTACKEND -= (strlen+2);
    p = &mem[sysSTACKEND];
    if (mode == 0) {
        // truncate the string on the stack
        *(p+len-1) = 0;
    }
    else {
        // copy the rightmost characters
        memmove(p, p + strlen - len, len);
    }
    // write the length and update stackend
    p += len;
    writeLengthToBuffer(len,p);
    sysSTACKEND += len + 2;
}

void stackMidStr(int start, int len) {
	//Serial.println("\tstackMidStr called"); 
    // equivalent to popping the current string, doing the operation then pushing it again
    unsigned char *p = &mem[sysSTACKEND];
    int strlen = readLengthFromBuffer(p-2);
    len++; // include trailing null
    if (start > strlen) start = strlen;
    start--;	// basic strings start at 1
    if (start + len > strlen) len = strlen - start;
    if (len == strlen) return;	// nothing to do
    sysSTACKEND -= (strlen+2);
    p = &mem[sysSTACKEND];
    // copy the characters
    memmove(p, p + start, len-1);
    *(p+len-1) = 0;
    // write the length and update stackend
    p += len;
    writeLengthToBuffer(len,p);
    sysSTACKEND += len + 2;
}

/* **************************************************************************
 * VARIABLE TABLE FUNCTIONS
 * **************************************************************************/

// Variable table starts at the end of memory and grows towards the start
// Simple variable
// table +--------+-------+-----------------+-----------------+ . . .
//  <--- | len    | type  | name            | value           |
// grows | 2bytes | 1byte | null terminated | float/string    | 
//       +--------+-------+-----------------+-----------------+ . . .
//
// Array
// +--------+-------+-----------------+----------+-------+ . . .+-------+-------------+. . 
// | len    | type  | name            | num dims | dim1  |      | dimN  | elem(1,..1) |
// | 2bytes | 1byte | null terminated | 2bytes   | 2bytes|      | 2bytes| float       |
// +--------+-------+-----------------+----------+-------+ . . .+-------+-------------+. . 

// variable type byte
#define VAR_TYPE_NUM		0x1
#define VAR_TYPE_FORNEXT	0x2
#define VAR_TYPE_NUM_ARRAY	0x4
#define VAR_TYPE_STRING		0x8
#define VAR_TYPE_STR_ARRAY	0x10

unsigned char *findVariable(char *searchName, unsigned char searchMask) {
	//Serial.println("\tfindVariable called"); 
    unsigned char *p = &mem[sysVARSTART];
    while (p < &mem[sysVAREND]) {
		//Serial.println("\t\tFirst line of while loop"); 
        unsigned char type = *(p+2);
        if (type & searchMask) {
            unsigned char *name = p+3;
            if (strcasecmp((char*)name, searchName) == 0){
				//Serial.println("\t\tReturn with p"); 
                return p;
			}
        }
        p+=readLengthFromBuffer(p);
    }
	//Serial.println("\t\tReturn with NULL"); 
    return NULL;
}

void deleteVariableAt(unsigned char *pos) {
	//Serial.println("\tdeleteVariableAt called"); 
    int len = readLengthFromBuffer(pos);
    if (pos == &mem[sysVARSTART]) {
        sysVARSTART += len;
        return;
    }
    memmove(&mem[sysVARSTART] + len, &mem[sysVARSTART], pos - &mem[sysVARSTART]);
    sysVARSTART += len;
}

// todo - consistently return errors rather than 1 or 0?

int storeNumVariable(char *name, float val) {
	//Serial.println("\tstoreNumVariable called"); 
    // these can be modified in place
    int nameLen = strlen(name);
    unsigned char *p = findVariable(name, VAR_TYPE_NUM|VAR_TYPE_FORNEXT);
    if (p != NULL)
    {	
		//Serial.println("\t\tReplace old value"); 
		// replace the old value
        // (could either be VAR_TYPE_NUM or VAR_TYPE_FORNEXT)
        p += 3;	// len + type;
        p += nameLen + 1;
        writeFloatToBuffer(val,p);
    }
    else
    {	
		//Serial.println("\t\tAllocate a new variable"); 
		// allocate a new variable
        int bytesNeeded = 3;	// len + flags
        bytesNeeded += nameLen + 1;	// name
        bytesNeeded += sizeof(float);	// val

        if (sysVARSTART - bytesNeeded < sysSTACKEND)
            return 0;	// out of memory
        sysVARSTART -= bytesNeeded;

        unsigned char *p = &mem[sysVARSTART];
        writeLengthToBuffer(bytesNeeded,p);
        p += 2;
        *p++ = VAR_TYPE_NUM;
        strcpy((char*)p, name); 
        p += nameLen + 1;
        writeFloatToBuffer(val,p);
    }
    return 1;
}

int storeForNextVariable(char *name, float start, float step, float end, uint16_t lineNum, uint16_t stmtNum) {
	//Serial.println("\tstoreForNextVariable called"); 
    int nameLen = strlen(name);
    int bytesNeeded = 3;	// len + flags
    bytesNeeded += nameLen + 1;	// name
    bytesNeeded += 3 * sizeof(float);	// vals
    bytesNeeded += 2 * sizeof(uint16_t);

    // unlike simple numeric variables, these are reallocated if they already exist
    // since the existing value might be a simple variable or a for/next variable
    unsigned char *p = findVariable(name, VAR_TYPE_NUM|VAR_TYPE_FORNEXT);
    if (p != NULL) {
        // check there will actually be room for the new value
        uint16_t oldVarLen = readLengthFromBuffer(p);
        if (sysVARSTART - (bytesNeeded - oldVarLen) < sysSTACKEND)
            return 0;	// not enough memory
        deleteVariableAt(p);
    }

    if (sysVARSTART - bytesNeeded < sysSTACKEND)
        return 0;	// out of memory
    sysVARSTART -= bytesNeeded;

    p = &mem[sysVARSTART];
    writeLengthToBuffer(bytesNeeded,p);
    p += 2;
    *p++ = VAR_TYPE_FORNEXT;
    strcpy((char*)p, name); 
    p += nameLen + 1;
    writeFloatToBuffer(start,p);
    p += sizeof(float);
    writeFloatToBuffer(step,p);
    p += sizeof(float);
    writeFloatToBuffer(end,p);
    p += sizeof(float);
    writeLengthToBuffer(lineNum,p);
    p += sizeof(uint16_t);
    writeLengthToBuffer(stmtNum,p);
    return 1;
}

int storeStrVariable(char *name, char *val) {
	//Serial.println("\tstoreStrVariable called"); 
    int nameLen = strlen(name);
    int valLen = strlen(val);
    int bytesNeeded = 3;	// len + type
    bytesNeeded += nameLen + 1;	// name
    bytesNeeded += valLen + 1;	// val

    // strings and arrays are re-allocated if they already exist
    unsigned char *p = findVariable(name, VAR_TYPE_STRING);
    if (p != NULL) {
        // check there will actually be room for the new value
        uint16_t oldVarLen = readLengthFromBuffer(p);
        if (sysVARSTART - (bytesNeeded - oldVarLen) < sysSTACKEND)
            return 0;	// not enough memory
        deleteVariableAt(p);
    }

    if (sysVARSTART - bytesNeeded < sysSTACKEND)
        return 0;	// out of memory
    sysVARSTART -= bytesNeeded;

    p = &mem[sysVARSTART];
    writeLengthToBuffer(bytesNeeded,p);
    p += 2;
    *p++ = VAR_TYPE_STRING;
    strcpy((char*)p, name); 
    p += nameLen + 1;
    strcpy((char*)p, val);
    return 1;
}

int createArray(char *name, int isString) {
	//Serial.println("\tcreateArray called"); 
    // dimensions and number of dimensions on the calculator stack
    int nameLen = strlen(name);
    int bytesNeeded = 3;	// len + flags
    bytesNeeded += nameLen + 1;	// name
    bytesNeeded += 2;		// num dims
    int numElements = 1;
    int i = 0;
    int numDims = (int)stackPopNum();
    // keep the current stack position, since we'll need to pop these values again
    int oldSTACKEND = sysSTACKEND;	
    for (int i=0; i<numDims; i++) {
        int dim = (int)stackPopNum();
        numElements *= dim;
    }
    bytesNeeded += 2 * numDims + (isString ? 1 : sizeof(float)) * numElements;
    if(bytesNeeded>65535){
		host_outputString("Structure larger than 65535 bytes. Requested size = ");
		host_outputInt(bytesNeeded);
		return 0;
	}
    // strings and arrays are re-allocated if they already exist
    unsigned char *p = findVariable(name, (isString ? VAR_TYPE_STR_ARRAY : VAR_TYPE_NUM_ARRAY));
    if (p != NULL) {
        // check there will actually be room for the new value
        int oldVarLen = readLengthFromBuffer(p);
        if (sysVARSTART - (bytesNeeded - oldVarLen) < sysSTACKEND)
            return 0;	// not enough memory
        deleteVariableAt(p);
    }

    if (sysVARSTART - bytesNeeded < sysSTACKEND)
        return 0;	// out of memory
    sysVARSTART -= bytesNeeded;

    p = &mem[sysVARSTART];
    writeLengthToBuffer(bytesNeeded,p);
    p += 2;
    *p++ = (isString ? VAR_TYPE_STR_ARRAY : VAR_TYPE_NUM_ARRAY);
    strcpy((char*)p, name); 
    p += nameLen + 1;
    writeLengthToBuffer(numDims,p);
    p += 2;
    sysSTACKEND = oldSTACKEND;
    for (int i=0; i<numDims; i++) {
        int dim = (int)stackPopNum();
        writeLengthToBuffer(dim,p);
        p += 2;
    }
    memset(p, 0, numElements * (isString ? 1 : sizeof(float)));
    return 1;
}

int _getArrayElemOffset(unsigned char **p, int *pOffset) {
	//Serial.println("\t_getArrayElemOffset called"); 
    // check for correct dimensionality
    int numArrayDims = readLengthFromBuffer(*p); 
    *p+=2;
    int numDimsGiven = (int)stackPopNum();
    if (numArrayDims != numDimsGiven)
        return ERROR_WRONG_ARRAY_DIMENSIONS;
    // now lookup the element
    int offset = 0;
    int base = 1;
    for (int i=0; i<numArrayDims; i++) {
        int index = (int)stackPopNum();
        int arrayDim = readLengthFromBuffer(*p); 
        *p+=2; // TBD: Pointer to a pointer dereferenced is a pointer + 2. This should be safe.
        if (index < 1 || index > arrayDim)
            return ERROR_ARRAY_SUBSCRIPT_OUT_RANGE;
        offset += base * (index-1);
        base *= arrayDim;
    }
    //Serial.println("Before *pOffset = offset;"); 
    *pOffset = offset;
    //Serial.println("After *pOffset = offset;"); 
    return 0;
}

int setNumArrayElem(char *name, float val) {
	//Serial.println("\tsetNumArrayElem called"); 
    // each index and number of dimensions on the calculator stack
    unsigned char *p = findVariable(name, VAR_TYPE_NUM_ARRAY);
    if (p == NULL)
        return ERROR_VARIABLE_NOT_FOUND;
    p += 3 + strlen(name) + 1;
    
    int offset;
    int ret = _getArrayElemOffset(&p, &offset);
    if (ret) return ret;
    
    p += sizeof(float)*offset;
    writeFloatToBuffer(val,p);
    return ERROR_NONE;
}

int setStrArrayElem(char *name) {
	//Serial.println("\tsetStrArrayElem called"); 
    // string is top of the stack
    // each index and number of dimensions on the calculator stack

    // keep the current stack position, since we can't overwrite the value string
    int oldSTACKEND = sysSTACKEND;
    // how long is the new value?
    char *newValPtr = stackPopStr();
    int newValLen = strlen(newValPtr);

    unsigned char *p = findVariable(name, VAR_TYPE_STR_ARRAY);
    unsigned char *p1 = p;	// so we can correct the length when done
    if (p == NULL)
        return ERROR_VARIABLE_NOT_FOUND;

    p += 3 + strlen(name) + 1;
    
    int offset;
    int ret = _getArrayElemOffset(&p, &offset);
    if (ret) return ret;
    
    // find the correct element by skipping over null terminators
    int i = 0;
    while (i < offset) {
        if (*p == 0) i++;
        p++;
    }
    int oldValLen = strlen((char*)p);
    int bytesNeeded = newValLen - oldValLen;
    // check if we've got enough room for the new value
    if (sysVARSTART - bytesNeeded < oldSTACKEND)
        return 0;	// out of memory
    // correct the length of the variable
    writeLengthToBuffer(readLengthFromBuffer(p1)+bytesNeeded,p1);
    //*(uint16_t*)p1 += bytesNeeded;
    memmove(&mem[sysVARSTART - bytesNeeded], &mem[sysVARSTART], p - &mem[sysVARSTART]);
    // copy in the new value
    strcpy((char*)(p - bytesNeeded), newValPtr);
    sysVARSTART -= bytesNeeded;
    return ERROR_NONE;
}

float lookupNumArrayElem(char *name, int *error) {
	//Serial.println("\tlookupNumArrayElem called"); 
    // each index and number of dimensions on the calculator stack
    unsigned char *p = findVariable(name, VAR_TYPE_NUM_ARRAY);
    if (p == NULL) {
        *error = ERROR_VARIABLE_NOT_FOUND;
        return 0.0f;
    }
    p += 3 + strlen(name) + 1;
    
    int offset;
    int ret = _getArrayElemOffset(&p, &offset);
    if (ret) {
        *error = ret;
        return 0.0f;
    }
    p += sizeof(float)*offset;
    return readFloatFromBuffer(p);
}

char *lookupStrArrayElem(char *name, int *error) {
	//Serial.println("\tlookupStrArrayElem called"); 
    // each index and number of dimensions on the calculator stack
    unsigned char *p = findVariable(name, VAR_TYPE_STR_ARRAY);
    if (p == NULL) {
        *error = ERROR_VARIABLE_NOT_FOUND;
        return NULL;
    }
    p += 3 + strlen(name) + 1;

    int offset;
    int ret = _getArrayElemOffset(&p, &offset);
    if (ret) {
        *error = ret;
        return NULL;
    }
    // find the correct element by skipping over null terminators
    int i = 0;
    while (i < offset) {
        if (*p == 0) i++;
        p++;
    }
    return (char *)p;
}

float lookupNumVariable(char *name) {
	//Serial.println("\tlookupNumVariable called"); 
    unsigned char *p = findVariable(name, VAR_TYPE_NUM|VAR_TYPE_FORNEXT);
    if (p == NULL) {
        return FLT_MAX;
    }
    p += 3 + strlen(name) + 1;
    return readFloatFromBuffer(p);
}

char *lookupStrVariable(char *name) {
	//Serial.println("\tlookupStrVariable called"); 
    unsigned char *p = findVariable(name, VAR_TYPE_STRING);
    if (p == NULL) {
        return NULL;
    }
    p += 3 + strlen(name) + 1;
    return (char *)p;
}

ForNextData lookupForNextVariable(char *name) {
	//Serial.println("\tlookupForNextVariable called"); 
    ForNextData ret;
    unsigned char *p = findVariable(name, VAR_TYPE_NUM|VAR_TYPE_FORNEXT);
    if (p == NULL)
        ret.val = FLT_MAX;
    else if (*(p+2) != VAR_TYPE_FORNEXT)
        ret.step = FLT_MAX;
    else {
        p += 3 + strlen(name) + 1;
        ret.val = readFloatFromBuffer(p); 
        p += sizeof(float);
        ret.step = readFloatFromBuffer(p); 
        p += sizeof(float);
        ret.end = readFloatFromBuffer(p); 
        p += sizeof(float);
        ret.lineNumber = readLengthFromBuffer(p); 
        p += sizeof(uint16_t);
        ret.stmtNumber = readLengthFromBuffer(p);
    }
    return ret;
}

/* **************************************************************************
 * GOSUB STACK
 * **************************************************************************/
// gosub stack (if used) is after the variables
int gosubStackPush(int lineNumber,int stmtNumber) {
	//Serial.println("\tgosubStackPush called"); 
    int bytesNeeded = 2 * sizeof(uint16_t);
    if (sysVARSTART - bytesNeeded < sysSTACKEND)
        return 0;	// out of memory
    // shift the variable table
    memmove(&mem[sysVARSTART]-bytesNeeded, &mem[sysVARSTART], sysVAREND-sysVARSTART);
    sysVARSTART -= bytesNeeded;
    sysVAREND -= bytesNeeded;
    // push the return address
    sysGOSUBSTART = sysVAREND;
    unsigned char *p = &mem[sysGOSUBSTART];
    writeLengthToBuffer(lineNumber,p);
    writeLengthToBuffer(stmtNumber,p+2);
    return 1;
}

int gosubStackPop(int *lineNumber, int *stmtNumber) {
	//Serial.println("\tgosubStackPop called"); 
    if (sysGOSUBSTART == sysGOSUBEND)
        return 0;
    unsigned char *p = &mem[sysGOSUBSTART];
    *lineNumber = readLengthFromBuffer(p);
    *stmtNumber = readLengthFromBuffer(p+2);
    int bytesFreed = 2 * sizeof(uint16_t);
    // shift the variable table
    memmove(&mem[sysVARSTART]+bytesFreed, &mem[sysVARSTART], sysVAREND-sysVARSTART);
    sysVARSTART += bytesFreed;
    sysVAREND += bytesFreed;
    sysGOSUBSTART = sysVAREND;
    return 1;
}

/* **************************************************************************
 * LEXER
 * **************************************************************************/

static unsigned char *tokenIn, *tokenOut;
static int tokenOutLeft;

// nextToken returns -1 for end of input, 0 for success, +ve number = error code
int nextToken()
{
    //Serial.println("\tnextToken called"); 
    // Skip any whitespace.
    while (isspace(*tokenIn))
        tokenIn++;
    // check for end of line
    if (*tokenIn == 0) {
        *tokenOut++ = TOKEN_EOL;
        tokenOutLeft--;
        return -1;
    }
    //Serial.println("\t\tChecking for number: [0-9.]+"); 
    // Number: [0-9.]+
    // TODO - handle 1e4 etc
    if (isdigit(*tokenIn) || *tokenIn == '.') {   // Number: [0-9.]+
        int gotDecimal = 0;
        char numStr[MAX_NUMBER_LEN+1];
        int numLen = 0;
        do {
            if (numLen == MAX_NUMBER_LEN) return ERROR_LEXER_BAD_NUM;
            if (*tokenIn == '.') {
                if (gotDecimal) return ERROR_LEXER_BAD_NUM;
                else gotDecimal = 1;
            }
            numStr[numLen++] = *tokenIn++;
        } 
        while (isdigit(*tokenIn) || *tokenIn == '.');

        numStr[numLen] = 0;
        //Serial.println("\t\t\tNumber stored in string"); 
        if (tokenOutLeft <= 5) return ERROR_LEXER_TOO_LONG;
        tokenOutLeft -= 5;
        if (!gotDecimal) {
            long val = strtol(numStr, 0, 10);
            if (val == LONG_MAX || val == LONG_MIN)
                gotDecimal = true;
            else {
                *tokenOut++ = TOKEN_INTEGER;
                writeLongToBuffer(val,tokenOut);
                tokenOut += sizeof(long);
            }
        }
        if (gotDecimal)
        {
            *tokenOut++ = TOKEN_NUMBER;
            writeFloatToBuffer((float)strtod(numStr, 0),tokenOut);
            tokenOut += sizeof(float);
        }
        return 0;
    }
    //Serial.println("\t\tChecking for identifier: [a-zA-Z][a-zA-Z0-9]*[$]"); 
    // identifier: [a-zA-Z][a-zA-Z0-9]*[$]
    if (isalpha(*tokenIn)) {
        char identStr[MAX_IDENT_LEN+1];
        int identLen = 0;
        identStr[identLen++] = *tokenIn++; // copy first char
        while (isalnum(*tokenIn) || *tokenIn=='$') {
            if (identLen < MAX_IDENT_LEN)
                identStr[identLen++] = *tokenIn;
            tokenIn++;
        }
        identStr[identLen] = 0;
        //Serial.println("\t\t\tCheck to see if this is a keyword"); 
        // check to see if this is a keyword
        for (int i = FIRST_IDENT_TOKEN; i <= LAST_IDENT_TOKEN; i++) {
            if (strcasecmp(identStr, (char *)tokenTable[i].token) == 0) {
                if (tokenOutLeft <= 1) return ERROR_LEXER_TOO_LONG;
                tokenOutLeft--;
                *tokenOut++ = i;
                // special case for REM
                if (i == TOKEN_REM) {
                    *tokenOut++ = TOKEN_STRING;
                    // skip whitespace
                    while (isspace(*tokenIn))
                        tokenIn++;
                    // copy the comment
                    while (*tokenIn) {
                        *tokenOut++ = *tokenIn++;
                    }
                    *tokenOut++ = 0;
                }
                return 0;
            }
        }
        //Serial.println("\t\tNo matching keyword - this must be an identifier"); 
        // no matching keyword - this must be an identifier
        // $ is only allowed at the end
        char *dollarPos = strchr(identStr, '$');
        if  (dollarPos && dollarPos!= &identStr[0] + identLen - 1) return ERROR_LEXER_UNEXPECTED_INPUT;
        if (tokenOutLeft <= 1+identLen) return ERROR_LEXER_TOO_LONG;
        tokenOutLeft -= 1+identLen;
        *tokenOut++ = TOKEN_IDENT;
        strcpy((char*)tokenOut, identStr);
        tokenOut[identLen-1] |= 0x80;
        tokenOut += identLen;
        return 0;
    }
    //Serial.println("\t\tChecking for String"); 
    // string
    if (*tokenIn=='\"') {
        *tokenOut++ = TOKEN_STRING;
        tokenOutLeft--;
        if (tokenOutLeft <= 1) return ERROR_LEXER_TOO_LONG;
        tokenIn++;
        while (*tokenIn) {
            if (*tokenIn == '\"' && *(tokenIn+1) != '\"')
                break;
            else if (*tokenIn == '\"')
                tokenIn++;
            *tokenOut++ = *tokenIn++;
            tokenOutLeft--;
            if (tokenOutLeft <= 1) return ERROR_LEXER_TOO_LONG;
        }
        if (!*tokenIn) return ERROR_LEXER_UNTERMINATED_STRING;
        tokenIn++;
        *tokenOut++ = 0;
        tokenOutLeft--;
        return 0;
    }
    //Serial.println("\t\tHandle non-alpha tokens e.g. ="); 
    // handle non-alpha tokens e.g. =
    for (int i=LAST_NON_ALPHA_TOKEN; i>=FIRST_NON_ALPHA_TOKEN; i--) {
        // do this "backwards" so we match >= correctly, not as > then =
        int len = strlen((char *)tokenTable[i].token);
        if (strncmp((char *)tokenTable[i].token, (char*)tokenIn, len) == 0) {
            if (tokenOutLeft <= 1) return ERROR_LEXER_TOO_LONG;
            *tokenOut++ = i;
            tokenOutLeft--;
            tokenIn += len;
            return 0;
        }
    }
    return ERROR_LEXER_UNEXPECTED_INPUT;
}

int tokenize(unsigned char *input, unsigned char *output, int outputSize)
{
	//Serial.println("\ttokenize called"); 
    tokenIn = input;
    tokenOut = output;
    tokenOutLeft = outputSize;
    int ret;
    while (1) {
        ret = nextToken();
        if (ret) break;
    }
    return (ret > 0) ? ret : 0;
}

/* **************************************************************************
 * PARSER / INTERPRETER
 * **************************************************************************/

static char executeMode;	// 0 = syntax check only, 1 = execute
uint16_t lineNumber, stmtNumber;
// stmt number is 0 for the first statement, then increases after each command seperator (:)
// Note that IF a=1 THEN PRINT "x": print "y" is considered to be only 2 statements
static uint16_t jumpLineNumber, jumpStmtNumber;
static uint16_t stopLineNumber, stopStmtNumber;
static char breakCurrentLine;

static unsigned char *tokenBuffer, *prevToken;
static int curToken;
static char identVal[MAX_IDENT_LEN+1];
static char isStrIdent;
static float numVal;
static char *strVal;
static long numIntVal;

int getNextToken(){
	//Serial.println("\tgetNextToken called"); 
    prevToken = tokenBuffer;
    curToken = *tokenBuffer++;
    if (curToken == TOKEN_IDENT) {
		//Serial.println("\t\tTOKEN_IDENT found"); 
        int i=0;
        while (*tokenBuffer < 0x80)
            identVal[i++] = *tokenBuffer++;
        identVal[i] = (*tokenBuffer++)-0x80;
        isStrIdent = (identVal[i++] == '$');
        identVal[i++] = '\0';
        //Serial.print("\t\t\t"); 
        //Serial.println(identVal); 
    }
    else if (curToken == TOKEN_NUMBER) {
		//Serial.println("\t\tTOKEN_NUMBER found"); 
		numVal=readFloatFromBuffer(tokenBuffer);
        //Serial.print("\t\t\t"); 
		//Serial.println(numVal); 
        tokenBuffer += sizeof(float);
    }
    else if (curToken == TOKEN_INTEGER) {
		//Serial.println("\t\tTOKEN_INTEGER found"); 
        // these are really just for line numbers
		long val=readLongFromBuffer(tokenBuffer);
		numVal=(long)val;
        //Serial.print("\t\t\t"); 
		//Serial.println(numVal); 
        tokenBuffer += sizeof(long);
    }
    else if (curToken == TOKEN_STRING) {
		//Serial.println("\t\tTOKEN_STRING found"); 
        strVal = (char*)tokenBuffer;
        tokenBuffer += 1 + strlen(strVal);
        //Serial.print("\t\t\t"); 
        //Serial.println(strVal); 
    }
    return curToken;
}

// value (int) returned from parseXXXXX
#define ERROR_MASK						0x0FFF
#define TYPE_MASK						0xF000
#define TYPE_NUMBER						0x0000
#define TYPE_STRING						0x1000

#define IS_TYPE_NUM(x) ((x & TYPE_MASK) == TYPE_NUMBER)
#define IS_TYPE_STR(x) ((x & TYPE_MASK) == TYPE_STRING)

// forward declarations
int parseExpression();
int parsePrimary();
int expectNumber();
String host_toString(char *str);

// parse a number
int parseNumberExpr()
{
	//Serial.println("\tparseNumberExpr called"); 
    if (executeMode && !stackPushNum(numVal))
        return ERROR_OUT_OF_MEMORY;
    getNextToken(); // consume the number
    return TYPE_NUMBER;
}

// parse (x1,....xn) e.g. DIM a(10,20)
int parseSubscriptExpr() {
	//Serial.println("\tparseSubscriptExpr called"); 
    // stacks x1, .. xn followed by n
    int numDims = 0;
    if (curToken != TOKEN_LBRACKET) return ERROR_EXPR_MISSING_BRACKET;
    getNextToken();
    while(1) {
        numDims++;
        int val = expectNumber();
        if (val) return val;	// error
        if (curToken == TOKEN_RBRACKET)
            break;
        else if (curToken == TOKEN_COMMA)
            getNextToken();
        else
            return ERROR_EXPR_MISSING_BRACKET;
    }
    getNextToken(); // eat )
    if (executeMode && !stackPushNum(numDims))
        return ERROR_OUT_OF_MEMORY;
    return 0;
}

// parse a function call e.g. LEN(a$)
int parseFnCallExpr() {
	//Serial.println("\tparseFnCallExpr called"); 
    int op = curToken;
    int fnSpec = tokenTable[curToken].format;
    getNextToken();
    // get the required arguments and types from the token table
    if (curToken != TOKEN_LBRACKET) return ERROR_EXPR_MISSING_BRACKET;
    getNextToken();

    int reqdArgs = fnSpec & TKN_ARGS_NUM_MASK;
    int argTypes = (fnSpec & TKN_ARG_MASK) >> TKN_ARG_SHIFT;
    int ret = (fnSpec & TKN_RET_TYPE_STR) ? TYPE_STRING : TYPE_NUMBER;
    for (int i=0; i<reqdArgs; i++) {
        int val = parseExpression();
        if (val & ERROR_MASK) return val;
        // check we've got the right type
        if (!(argTypes & 1) && !IS_TYPE_NUM(val))
            return ERROR_EXPR_EXPECTED_NUM;
        if ((argTypes & 1) && !IS_TYPE_STR(val))
            return ERROR_EXPR_EXPECTED_STR;
        argTypes >>= 1;
        // if this isn't the last argument, eat the ,
        if (i+1<reqdArgs) {
            if (curToken != TOKEN_COMMA)
                return ERROR_UNEXPECTED_TOKEN;
            getNextToken();
        }
    }
    // now all the arguments will be on the stack (last first)
    if (executeMode) {
        int tmp;
        switch (op) {
        case TOKEN_INT:
            stackPushNum((float)floor(stackPopNum()));
            break;
        case TOKEN_STR:
            {
                char buf[16];
                if (!stackPushStr(host_floatToStr(stackPopNum(), buf)))
                    return ERROR_OUT_OF_MEMORY;
            }
            break;
        case TOKEN_CHR:
            {
				String character="";
				character=character+(char)((int)(floor(stackPopNum()))%256);
                if (!stackPushStr((char*)character.c_str()))
                    return ERROR_OUT_OF_MEMORY;
            }
            break;
        case TOKEN_READ:
            {
				if(basicFileOpen){
					basicFile=SPIFFS.open(basicFilename,"r");
					basicFile.seek(basicFileReadPosition,SeekSet);
					int readSize=(int)stackPopNum();
					char line[readSize+1];
					basicFile.readBytes(line,readSize);
					line[readSize]=0;
					basicFileReadPosition=basicFile.position();
					basicFile.close();
					if (!stackPushStr((char*)line))
						return ERROR_OUT_OF_MEMORY;
				}
				else{
					return ERROR_FILE_NOT_OPEN;
				}
			}
            break;
        case TOKEN_LEN:
            tmp = strlen(stackPopStr());
            if (!stackPushNum(tmp)) return ERROR_OUT_OF_MEMORY;
            break;
        case TOKEN_VAL:
            {
                // tokenise str onto the stack
                int oldStackEnd = sysSTACKEND;
                unsigned char *oldTokenBuffer = prevToken;
                int val = tokenize((unsigned char*)stackGetStr(), &mem[sysSTACKEND], sysVARSTART - sysSTACKEND);
                if (val) {
                    if (val == ERROR_LEXER_TOO_LONG) return ERROR_OUT_OF_MEMORY;
                    else return ERROR_IN_VAL_INPUT;
                }
                // set tokenBuffer to point to the new set of tokens on the stack
                tokenBuffer = &mem[sysSTACKEND];
                // move stack end to the end of the new tokens
                sysSTACKEND = tokenOut - &mem[0];
                getNextToken();
                // then parseExpression
                val = parseExpression();
                if (val & ERROR_MASK) {
                    if (val == ERROR_OUT_OF_MEMORY) return val;
                    else return ERROR_IN_VAL_INPUT;
                }
                if (!IS_TYPE_NUM(val))
                    return ERROR_EXPR_EXPECTED_NUM;
                // read the result from the stack
                float f = stackPopNum();
                // pop the tokens from the stack
                sysSTACKEND = oldStackEnd;
                // and pop the original string
                stackPopStr();
                // finally, push the result and set the token buffer back
                stackPushNum(f);
                tokenBuffer = oldTokenBuffer;
                getNextToken();
            }
            break;
        case TOKEN_LEFT:
            tmp = (int)stackPopNum();
            if (tmp < 0) return ERROR_STR_SUBSCRIPT_OUT_RANGE;
            stackLeftOrRightStr(tmp, 0);
            break;
        case TOKEN_RIGHT:
            tmp = (int)stackPopNum();
            if (tmp < 0) return ERROR_STR_SUBSCRIPT_OUT_RANGE;
            stackLeftOrRightStr(tmp, 1);
            break;
        case TOKEN_MID:
            {
                tmp = (int)stackPopNum();
                int start = stackPopNum();
                if (tmp < 0 || start < 1) return ERROR_STR_SUBSCRIPT_OUT_RANGE;
                stackMidStr(start, tmp);
            }
            break;
        case TOKEN_HTTPGET:
			{
				String response=getPayloadFromHttpRequest(String(stackPopStr()));
				stackPushStr((char*)response.c_str());
			}
			break;
		case TOKEN_INDEXOF:
			{
				String big=String(stackPopStr());
				String small=String(stackPopStr());
				stackPushNum((float)(big.indexOf(small)+1));
			}
			break;
		case TOKEN_COUNTOF:
			{
				String big=String(stackPopStr());
				String small=String(stackPopStr());
				int count=0;
				int startSearch=0;
				while(big.indexOf(small,startSearch)>-1){
					count++;
					startSearch=big.indexOf(small,startSearch)+1;
				}
				stackPushNum((float)count);
			}
			break;
        default:
            return ERROR_UNEXPECTED_TOKEN;
        }
    }
    if (curToken != TOKEN_RBRACKET) return ERROR_EXPR_MISSING_BRACKET;
    getNextToken();	// eat )
    return ret;
}

// parse an identifer e.g. a$ or a(5,3)
int parseIdentifierExpr() {
	//Serial.println("\tparseIdentifierExpr called"); 
    char ident[MAX_IDENT_LEN+1];
    if (executeMode)
        strcpy(ident, identVal);
    int isStringIdentifier = isStrIdent;
    getNextToken();	// eat ident
    if (curToken == TOKEN_LBRACKET) {
        // array access
        int val = parseSubscriptExpr();
        if (val) return val;
        if (executeMode) {
            if (isStringIdentifier) {
                int error = 0;
                char *str = lookupStrArrayElem(ident, &error);
                if (error) return error;
                else if (!stackPushStr(str)) return ERROR_OUT_OF_MEMORY;
            }
            else {
                int error = 0;
                float f = lookupNumArrayElem(ident, &error);
                if (error) return error;
                else if (!stackPushNum(f)) return ERROR_OUT_OF_MEMORY;
            }
        }
    }
    else {
        // simple variable
        if (executeMode) {
            if (isStringIdentifier) {
                char *str = lookupStrVariable(ident);
                if (!str) return ERROR_VARIABLE_NOT_FOUND;
                else if (!stackPushStr(str)) return ERROR_OUT_OF_MEMORY;
            }
            else {
                float f = lookupNumVariable(ident);
                if (f == FLT_MAX) return ERROR_VARIABLE_NOT_FOUND;
                else if (!stackPushNum(f)) return ERROR_OUT_OF_MEMORY;
            }
        }
    }
    return isStringIdentifier ? TYPE_STRING : TYPE_NUMBER;
}

// parse a string e.g. "hello"
int parseStringExpr() {
	//Serial.println("\tparseStringExpr called"); 
    if (executeMode && !stackPushStr(strVal))
        return ERROR_OUT_OF_MEMORY;
    getNextToken(); // consume the string
    return TYPE_STRING;
}

// parse a bracketed expressed e.g. (5+3)
int parseParenExpr() {
	//Serial.println("\tparseParenExpr called"); 
    getNextToken();  // eat (
    int val = parseExpression();
    if (val & ERROR_MASK) return val;
    if (curToken != TOKEN_RBRACKET)
        return ERROR_EXPR_MISSING_BRACKET;
    getNextToken();  // eat )
    return val;
}

int parse_RND() {
	//Serial.println("\tparse_RND called"); 
    getNextToken();
    if (executeMode && !stackPushNum((float)rand()/(float)RAND_MAX))
        return ERROR_OUT_OF_MEMORY;
    return TYPE_NUMBER;	
}

int parse_READPOS() {
    getNextToken();
    if (executeMode && !stackPushNum(float(basicFileReadPosition)))
        return ERROR_OUT_OF_MEMORY;
    return TYPE_NUMBER;	
}

int parse_WRITEPOS() {
    getNextToken();
    if (executeMode && !stackPushNum(float(basicFileWritePosition)))
        return ERROR_OUT_OF_MEMORY;
    return TYPE_NUMBER;	
}

int parse_MILLIS() {
	//Serial.println("\tparse_MILLIS called"); 
    getNextToken();
    if (executeMode && !stackPushNum((float)millis())){
        return ERROR_OUT_OF_MEMORY;
    }
    return TYPE_NUMBER;	
}

int parse_FREEMEM() {
	//Serial.println("\tparse_FREEMEM called"); 
    getNextToken();
    if (executeMode && !stackPushNum((float)host_getFreeMem())){
        return ERROR_OUT_OF_MEMORY;
    }
    return TYPE_NUMBER;	
}

int host_getFreeHostMem();

int parse_FREEHOSTMEM() {
	//Serial.println("\tparse_FREEHOSTMEM called"); 
    getNextToken();
    if (executeMode && !stackPushNum((float)host_getFreeHostMem())){
        return ERROR_OUT_OF_MEMORY;
    }
    return TYPE_NUMBER;	
}

int parse_IPADDR() {
    getNextToken();
    if (executeMode && !stackPushStr((char*)WiFi.localIP().toString().c_str())){
        return ERROR_OUT_OF_MEMORY;
    }
    return TYPE_STRING;	
}

int parse_GETSSID() {
    getNextToken();
    if (executeMode && !stackPushStr((char*)readStringFromSettingFile("SSID").c_str())){
        return ERROR_OUT_OF_MEMORY;
    }
    return TYPE_STRING;	
}

int parse_HTTPRECV() {
    getNextToken();
    if (executeMode && !stackPushStr((char*)basicHttpRecvParamValue.c_str())){
        return ERROR_OUT_OF_MEMORY;
    }
    httpRecvAvailable=false;
    return TYPE_STRING;	
}

void host_sleep(long ms){
	unsigned long end=millis()+ms;
	while(millis()<end){
		yield();
	}
}

int parse_NEWHTTPRECV(){
	getNextToken();
    if (executeMode && !stackPushNum((float)(httpRecvAvailable?1:0))){
        return ERROR_OUT_OF_MEMORY;
    }
	return TYPE_NUMBER;
}

int parse_EOF() {
    getNextToken();
    if(executeMode){
		if(basicFileOpen){
			basicFile=SPIFFS.open(basicFilename,"r");
			basicFile.seek(basicFileReadPosition,SeekSet);
			float eof=basicFile.available() ? 0 : 1;
			if (executeMode && !stackPushNum(eof)){
				return ERROR_OUT_OF_MEMORY;
			}
			return TYPE_NUMBER;	
		}
		else{
			return ERROR_FILE_NOT_OPEN;
		}
	}
}

int parseUnaryNumExp(){
	//Serial.println("\tparseUnaryNumExp called"); 
    int op = curToken;
    getNextToken();
    int val = parsePrimary();
    if (val & ERROR_MASK) return val;
    if (!IS_TYPE_NUM(val))
        return ERROR_EXPR_EXPECTED_NUM;
    switch (op) {
    case TOKEN_MINUS:
        if (executeMode) stackPushNum(stackPopNum() * -1.0f);
        return TYPE_NUMBER;
    case TOKEN_NOT:
        if (executeMode) stackPushNum(stackPopNum() ? 0.0f : 1.0f);
        return TYPE_NUMBER;
    default:
        return ERROR_UNEXPECTED_TOKEN;
    }
}

int parse_INKEY() {
    getNextToken();
    if (executeMode) {
		server.handleClient(); // Check for and handle http-requests
        char str[2];
        str[0] = host_getKey();
        str[1] = 0;
        if (!stackPushStr(str))
            return ERROR_OUT_OF_MEMORY;
    }
    return TYPE_STRING;	
}

int parse_READLINE() {
    getNextToken();
    if (executeMode) {
		if(basicFileOpen){
			basicFile=SPIFFS.open(basicFilename,"r");
			basicFile.seek(basicFileReadPosition,SeekSet);
			String line=basicFile.readStringUntil('\r');
			basicFileReadPosition=basicFile.position();
			basicFile.close();
			if (!stackPushStr((char*)line.c_str()))
				return ERROR_OUT_OF_MEMORY;
		}
		else{
			return ERROR_FILE_NOT_OPEN;
		}
    }
    return TYPE_STRING;	
}

/// primary
int parsePrimary() {
	//Serial.println("\tparsePrimary called"); 
    switch (curToken) {
		case TOKEN_IDENT:	
			return parseIdentifierExpr();
		case TOKEN_NUMBER:
		case TOKEN_INTEGER:
			return parseNumberExpr();
		case TOKEN_STRING:	
			return parseStringExpr();
		case TOKEN_LBRACKET:
			return parseParenExpr();

			// "pseudo-identifiers"
		case TOKEN_RND:	
			return parse_RND();
		case TOKEN_READPOS:	
			return parse_READPOS();
		case TOKEN_WRITEPOS:	
			return parse_WRITEPOS();
		case TOKEN_INKEY:
			return parse_INKEY();
		case TOKEN_MILLIS:	
			return parse_MILLIS();
		case TOKEN_FREEMEM:	
			return parse_FREEMEM();
		case TOKEN_FREEHOSTMEM:	
			return parse_FREEHOSTMEM();
		case TOKEN_IPADDR:	
			return parse_IPADDR();
		case TOKEN_GETSSID:	
			return parse_GETSSID();
		case TOKEN_EOF:	
			return parse_EOF();
		case TOKEN_HTTPRECV:	
			return parse_HTTPRECV();
		case TOKEN_NEWHTTPRECV:	
			return parse_NEWHTTPRECV();
		case TOKEN_READLINE: 
			return parse_READLINE();

			// unary ops
		case TOKEN_MINUS:
		case TOKEN_NOT:
			return parseUnaryNumExp();

			// functions
		case TOKEN_INT: 
		case TOKEN_STR: 
		case TOKEN_LEN: 
		case TOKEN_VAL:
		case TOKEN_LEFT: 
		case TOKEN_RIGHT: 
		case TOKEN_MID:
		case TOKEN_HTTPGET:
		case TOKEN_INDEXOF:
		case TOKEN_COUNTOF:
		case TOKEN_CHR:
		case TOKEN_READ:
			return parseFnCallExpr();


		default:
			return ERROR_UNEXPECTED_TOKEN;
    }
}

int getTokPrecedence() {
	//Serial.println("\tgetTokPrecendence called"); 
    if (curToken == TOKEN_AND || curToken == TOKEN_OR) return 5;
    if (curToken == TOKEN_EQUALS || curToken == TOKEN_NOT_EQ) return 10;
    if (curToken == TOKEN_LT || curToken == TOKEN_GT || curToken == TOKEN_LT_EQ || curToken == TOKEN_GT_EQ) return 20;
    if (curToken == TOKEN_MINUS || curToken == TOKEN_PLUS) return 30;
    else if (curToken == TOKEN_MULT || curToken == TOKEN_DIV || curToken == TOKEN_MOD) return 40;
    else return -1;
}

// Operator-Precedence Parsing
int parseBinOpRHS(int ExprPrec, int lhsVal) {
	//Serial.println("\tparseBinOpRHS called"); 
    // If this is a binop, find its precedence.
    while (1) {
        int TokPrec = getTokPrecedence();

        // If this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done.
        if (TokPrec < ExprPrec)
            return lhsVal;

        // Okay, we know this is a binop.
        int BinOp = curToken;
        getNextToken();  // eat binop

        // Parse the primary expression after the binary operator.
        int rhsVal = parsePrimary();
        if (rhsVal & ERROR_MASK) return rhsVal;

        // If BinOp binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS as its LHS.
        int NextPrec = getTokPrecedence();
        if (TokPrec < NextPrec) {
            rhsVal = parseBinOpRHS(TokPrec+1, rhsVal);
            if (rhsVal & ERROR_MASK) return rhsVal;
        }

        if (IS_TYPE_NUM(lhsVal) && IS_TYPE_NUM(rhsVal))
        {	// Number operations
            float r, l;
            if (executeMode) {
                r = stackPopNum();
                l = stackPopNum();
            }
            if (BinOp == TOKEN_PLUS) {
                if (executeMode) stackPushNum(l+r);
            }
            else if (BinOp == TOKEN_MINUS) {
                if (executeMode) stackPushNum(l-r);
            }
            else if (BinOp == TOKEN_MULT) {
                if (executeMode) stackPushNum(l*r);
            }
            else if (BinOp == TOKEN_DIV) {
                if (executeMode) {
                    if (r) stackPushNum(l/r);
                    else return ERROR_EXPR_DIV_ZERO;
                }
            }
            else if (BinOp == TOKEN_MOD) {
                if (executeMode) {
                    if ((int)r) stackPushNum((float)((int)l % (int)r));
                    else return ERROR_EXPR_DIV_ZERO;
                }
            }
            else if (BinOp == TOKEN_LT) {
                if (executeMode) stackPushNum(l < r ? 1.0f : 0.0f);
            }
            else if (BinOp == TOKEN_GT) {
                if (executeMode) stackPushNum(l > r ? 1.0f : 0.0f);
            }
            else if (BinOp == TOKEN_EQUALS) {
                if (executeMode) stackPushNum(l == r ? 1.0f : 0.0f);
            }
            else if (BinOp == TOKEN_NOT_EQ) {
                if (executeMode) stackPushNum(l != r ? 1.0f : 0.0f);
            }
            else if (BinOp == TOKEN_LT_EQ) {
                if (executeMode) stackPushNum(l <= r ? 1.0f : 0.0f);
            }
            else if (BinOp == TOKEN_GT_EQ) {
                if (executeMode) stackPushNum(l >= r ? 1.0f : 0.0f);
            }
            else if (BinOp == TOKEN_AND) {
                if (executeMode) stackPushNum(r != 0.0f ? l : 0.0f);
            }
            else if (BinOp == TOKEN_OR) {
                if (executeMode) stackPushNum(r != 0.0f ? 1 : l);
            }
            else
                return ERROR_UNEXPECTED_TOKEN;
        }
        else if (IS_TYPE_STR(lhsVal) && IS_TYPE_STR(rhsVal))
        {	// String operations
            if (BinOp == TOKEN_PLUS) {
                if (executeMode)
                    stackAdd2Strs();
            }
            else if (BinOp >= TOKEN_EQUALS && BinOp <=TOKEN_LT_EQ) {
                if (executeMode) {
                    char *r = stackPopStr();
                    char *l = stackPopStr();
                    int ret = strcmp(l,r);
                    if (BinOp == TOKEN_EQUALS && ret == 0) stackPushNum(1.0f);
                    else if (BinOp == TOKEN_NOT_EQ && ret != 0) stackPushNum(1.0f);
                    else if (BinOp == TOKEN_GT && ret > 0) stackPushNum(1.0f);
                    else if (BinOp == TOKEN_LT && ret < 0) stackPushNum(1.0f);
                    else if (BinOp == TOKEN_GT_EQ && ret >= 0) stackPushNum(1.0f);
                    else if (BinOp == TOKEN_LT_EQ && ret <= 0) stackPushNum(1.0f);
                    else stackPushNum(0.0f);
                }
                lhsVal = TYPE_NUMBER;
            }
            else
                return ERROR_UNEXPECTED_TOKEN;
        }
        else
            return ERROR_UNEXPECTED_TOKEN;
    }
}

int parseExpression()
{
	//Serial.println("\tparseExpression called"); 
    int val = parsePrimary();
    if (val & ERROR_MASK) return val;
    return parseBinOpRHS(0, val);
}

int expectNumber() {
	//Serial.println("\texpectNumber called"); 
    int val = parseExpression();
    if (val & ERROR_MASK) return val;
    if (!IS_TYPE_NUM(val))
        return ERROR_EXPR_EXPECTED_NUM;
    return 0;
}

int parse_RUN() {
	//Serial.println("\tparse_RUN called"); 
    getNextToken();
    uint16_t startLine = 1;
    if (curToken != TOKEN_EOL) {
        int val = expectNumber();
        if (val) return val;	// error
        if (executeMode) {
            startLine = (uint16_t)stackPopNum();
            if (startLine <= 0)
                return ERROR_BAD_LINE_NUM;
        }
    }
    if (executeMode) {
        // clear variables
        sysVARSTART = sysVAREND = sysGOSUBSTART = sysGOSUBEND = MEMORY_SIZE;
        jumpLineNumber = startLine;
        stopLineNumber = stopStmtNumber = 0;
        programRunning=true;
    }
    return 0;
}

int parse_GOTO() {
	//Serial.println("\tparse_GOTO called"); 
    getNextToken();
    int val = expectNumber();
    if (val) return val;	// error
    if (executeMode) {
        uint16_t startLine = (uint16_t)stackPopNum();
        if (startLine <= 0)
            return ERROR_BAD_LINE_NUM;
        jumpLineNumber = startLine;
    }
    return 0;
}

int parse_RSEEK() {
	//Serial.println("\tparse_GOTO called"); 
    getNextToken();
    int val = expectNumber();
    if (val) return val;	// error
    if (executeMode) {
		basicFileReadPosition=(int)stackPopNum();
    }
    return 0;
}

int parse_WSEEK() {
	//Serial.println("\tparse_GOTO called"); 
    getNextToken();
    int val = expectNumber();
    if (val) return val;	// error
    if (executeMode) {
		basicFileWritePosition=(int)stackPopNum();
    }
    return 0;
}

int parse_SETMEMSIZE() {
    getNextToken();
    int val = expectNumber();
    if (val) return val;	// error
    if (executeMode) {
        int memsize = (int)stackPopNum();
        writeStringToSettingFile("basicMemory",String(memsize));
    }
    return 0;
}

int parse_PAUSE() {
	//Serial.println("\tparse_PAUSE called"); 
    getNextToken();
    int val = expectNumber();
    if (val) return val;	// error
    if (executeMode) {
        unsigned long ms = (unsigned long)stackPopNum();
        if (ms < 0)
            return ERROR_BAD_PARAMETER;
        host_sleep(ms);
    }
    return 0;
}

int parse_LIST() {
	//Serial.println("\tparse_LIST called"); 
    getNextToken();
    uint16_t first = 0, last = 0;
    if (curToken != TOKEN_EOL && curToken != TOKEN_CMD_SEP) {
        int val = expectNumber();
        if (val) return val;	// error
        if (executeMode)
            first = (uint16_t)stackPopNum();
    }
    if (curToken == TOKEN_COMMA) {
        getNextToken();
        int val = expectNumber();
        if (val) return val;	// error
        if (executeMode)
            last = (uint16_t)stackPopNum();
    }
    if (executeMode) {
        listProg(first,last);
    }
    return 0;
}

int parse_PRINT() {
	//Serial.println("\tparse_PRINT called"); 
    getNextToken();
    // zero + expressions seperated by semicolons
    int newLine = 1;
    while (curToken != TOKEN_EOL && curToken != TOKEN_CMD_SEP) {
        int val = parseExpression();
        if (val & ERROR_MASK) return val;
        if (executeMode) {
            if (IS_TYPE_NUM(val))
                host_outputFloat(stackPopNum());
            else
                host_outputString(stackPopStr());
            newLine = 1;
        }
        if (curToken == TOKEN_SEMICOLON) {
            newLine = 0;
            getNextToken();
        }
    }
    if (executeMode) {
        if (newLine)
            host_newLine();
    }
    return 0;
}

int parse_WRITE() {
    getNextToken();
    // zero + expressions seperated by semicolons
    int newLine = 1;
    while (curToken != TOKEN_EOL && curToken != TOKEN_CMD_SEP) {
        int val = parseExpression();
        if (val & ERROR_MASK) return val;
        if (executeMode) {
			if(!basicFileOpen){
				return ERROR_FILE_NOT_OPEN;
			}
			basicFile=SPIFFS.open(basicFilename,"r+");
			basicFile.seek(basicFileWritePosition,SeekSet);
            if (IS_TYPE_NUM(val))
                basicFile.print(String(stackPopNum()));
            else
                basicFile.print(String(stackPopStr()));
            newLine = 1;
        }
        if (curToken == TOKEN_SEMICOLON) {
            newLine = 0;
            getNextToken();
        }
    }
    if (executeMode && newLine){
		if(!basicFileOpen){
			return ERROR_FILE_NOT_OPEN;
		}
        basicFile.write('\r');
    }
    if(executeMode){
		basicFileWritePosition=basicFile.position();
		basicFile.close();
	}
    return 0;
}

void host_moveCursor(int x, int y) {
    if (x<0) x = 0;
    if (x>=basicX) x = basicX-1;
    if (y<0) y = 0;
    if (y>=basicY) y = basicY-1;
    hostX = x;
    hostY = y; 
}

// parse a stmt that takes two int parameters 
// e.g. POSITION 3,2
int parseTwoIntCmd() {
	//Serial.println("\tparseTwoIntCmd called"); 
    int op = curToken;
    getNextToken();
    int val = expectNumber();
    if (val) return val;	// error
    if (curToken != TOKEN_COMMA)
        return ERROR_UNEXPECTED_TOKEN;
    getNextToken();
    val = expectNumber();
    if (val) return val;	// error
    if (executeMode) {
        int second = (int)stackPopNum();
        int first = (int)stackPopNum();
        switch(op) {
        case TOKEN_POSITION: 
            host_moveCursor(first,second); 
            break;
		/*
        case TOKEN_PIN: 
            host_digitalWrite(first,second); 
            break;
        case TOKEN_PINMODE: 
            host_pinMode(first,second); 
            break;
        */
        }
    }
    return 0;
}

// this handles both LET a$="hello" and INPUT a$ type assignments
int parseAssignment(bool inputStmt) {
	//Serial.println("\tparseAssignment called"); 
    char ident[MAX_IDENT_LEN+1];
    int val;
    if (curToken != TOKEN_IDENT) return ERROR_UNEXPECTED_TOKEN;
    if (executeMode)
        strcpy(ident, identVal);
    int isStringIdentifier = isStrIdent;
    int isArray = 0;
    getNextToken();	// eat ident
    if (curToken == TOKEN_LBRACKET) {
        // array element being set
        val = parseSubscriptExpr();
        if (val) return val;
        isArray = 1;
    }
    if (inputStmt) {
        // from INPUT statement
        /*
        if (executeMode) {
            char *inputStr = host_readLine();
            if (isStringIdentifier) {
                if (!stackPushStr(inputStr)) return ERROR_OUT_OF_MEMORY;
            }
            else {
                float f = (float)strtod(inputStr, 0);
                if (!stackPushNum(f)) return ERROR_OUT_OF_MEMORY;
            }
            host_newLine();
        }
        val = isStringIdentifier ? TYPE_STRING : TYPE_NUMBER;
        */
    }
    else {
        // from LET statement
        if (curToken != TOKEN_EQUALS) return ERROR_UNEXPECTED_TOKEN;
        getNextToken(); // eat =
        val = parseExpression();
        if (val & ERROR_MASK) return val;
    }
    // type checking and actual assignment
    if (!isStringIdentifier)
    {	// numeric variable
        if (!IS_TYPE_NUM(val)) return ERROR_EXPR_EXPECTED_NUM;
        if (executeMode) {
            if (isArray) {
                val = setNumArrayElem(ident, stackPopNum());
                if (val) return val;
            }
            else {
                if (!storeNumVariable(ident, stackPopNum())) return ERROR_OUT_OF_MEMORY;
            }
        }
    }
    else
    {	// string variable
        if (!IS_TYPE_STR(val)) return ERROR_EXPR_EXPECTED_STR;
        if (executeMode) {
            if (isArray) {
                // annoyingly, we've got the string at the top of the stack
                // (from parseExpression) and the array index stuff (from
                // parseSubscriptExpr) underneath.
                val = setStrArrayElem(ident);
                if (val) return val;
            }
            else {
                if (!storeStrVariable(ident, stackGetStr())) return ERROR_OUT_OF_MEMORY;
                stackPopStr();
            }
        }
    }
    return 0;
}

int parse_IF() {
	//Serial.println("\tparse_IF called"); 
    getNextToken();	// eat if
    int val = expectNumber();
    if (val) return val;	// error
    if (curToken != TOKEN_THEN)
        return ERROR_MISSING_THEN;
    getNextToken();
    if (executeMode && stackPopNum() == 0.0f) {
        // condition not met
        breakCurrentLine = 1;
        return 0;
    }
    else return 0;
}

int parse_FOR() {
	//Serial.println("\tparse_FOR called"); 
    char ident[MAX_IDENT_LEN+1];
    float start, end, step = 1.0f;
    getNextToken();	// eat for
    if (curToken != TOKEN_IDENT || isStrIdent) return ERROR_UNEXPECTED_TOKEN;
    if (executeMode)
        strcpy(ident, identVal);
    getNextToken();	// eat ident
    if (curToken != TOKEN_EQUALS) return ERROR_UNEXPECTED_TOKEN;
    getNextToken(); // eat =
    // parse START
    int val = expectNumber();
    if (val) return val;	// error
    if (executeMode)
        start = stackPopNum();
    // parse TO
    if (curToken != TOKEN_TO) return ERROR_UNEXPECTED_TOKEN;
    getNextToken(); // eat TO
    // parse END
    val = expectNumber();
    if (val) return val;	// error
    if (executeMode)
        end = stackPopNum();
    // parse optional STEP
    if (curToken == TOKEN_STEP) {
        getNextToken(); // eat STEP
        val = expectNumber();
        if (val) return val;	// error
        if (executeMode)
            step = stackPopNum();
    }
    if (executeMode) {
        if (!storeForNextVariable(ident, start, step, end, lineNumber, stmtNumber)) return ERROR_OUT_OF_MEMORY;
    }
    return 0;
}

int parse_NEXT() {
	//Serial.println("\tparse_NEXT called"); 
    getNextToken();	// eat next
    if (curToken != TOKEN_IDENT || isStrIdent) return ERROR_UNEXPECTED_TOKEN;
    if (executeMode) {
        ForNextData data = lookupForNextVariable(identVal);
        if (data.val == FLT_MAX) return ERROR_VARIABLE_NOT_FOUND;
        else if (data.step == FLT_MAX) return ERROR_NEXT_WITHOUT_FOR;
        // update and store the count variable
        data.val += data.step;
        storeNumVariable(identVal, data.val);
        // loop?
        if ((data.step >= 0 && data.val <= data.end) || (data.step < 0 && data.val >= data.end)) {
            jumpLineNumber = data.lineNumber;
            jumpStmtNumber = data.stmtNumber+1;
        }
    }
    getNextToken();	// eat ident
    return 0;
}

int parse_GOSUB() {
	//Serial.println("\tparse_GOSUB called"); 
    getNextToken();	// eat gosub
    int val = expectNumber();
    if (val) return val;	// error
    if (executeMode) {
        uint16_t startLine = (uint16_t)stackPopNum();
        if (startLine <= 0)
            return ERROR_BAD_LINE_NUM;
        jumpLineNumber = startLine;
        if (!gosubStackPush(lineNumber,stmtNumber))
            return ERROR_OUT_OF_MEMORY;
    }
    return 0;
}

void host_repaintScreen();

int parse_COLOR(){
    int op = curToken;
    getNextToken();
    int val;
    if(curToken==TOKEN_EOL){
		return ERROR_EXPR_EXPECTED_NUM;
	}
	if (curToken != TOKEN_EOL && curToken != TOKEN_CMD_SEP) {
        val = parseExpression();
        if (val & ERROR_MASK){
			return val;
		}
    }
    if(executeMode){
		char color=255;
		if(IS_TYPE_STR(val)){
			String colorStr=String(stackPopStr());
			colorStr.toUpperCase();
			if(colorStr=="BLACK"){
				color=COLOR_BLACK;
			}
			if(colorStr=="BLUE"){
				color=COLOR_BLUE;
			}
			if(colorStr=="GREEN"){
				color=COLOR_GREEN;
			}
			if(colorStr=="CYAN"){
				color=COLOR_CYAN;
			}
			if(colorStr=="RED"){
				color=COLOR_RED;
			}
			if(colorStr=="PURPLE"){
				color=COLOR_PURPLE;
			}
			if(colorStr=="YELLOW"){
				color=COLOR_YELLOW;
			}
			if(colorStr=="WHITE"){
				color=COLOR_WHITE;
			}
		}
		if(IS_TYPE_NUM(val)){
			float fcolor=stackPopNum();
			color=(char)fcolor;
		}
		if(color<8){
			if(op==TOKEN_FGCOLOR || op==TOKEN_SETFG){
				fgColor=color;
			}
			if(op==TOKEN_BGCOLOR || op==TOKEN_SETBG){
				bgColor=color;
			}
			if(op==TOKEN_SETBG || op==TOKEN_SETFG){
				host_repaintScreen();
			}
		}
	}
	return 0;
}

int parse_OPEN_ERASE_SSID(){
    int op = curToken;
    getNextToken();
    if(curToken==TOKEN_EOL){
		return ERROR_EXPR_EXPECTED_STR;
	}
	if (curToken != TOKEN_EOL && curToken != TOKEN_CMD_SEP) {
        int val = parseExpression();
        if (val & ERROR_MASK){
			return val;
		}
        if (!IS_TYPE_STR(val)){
            return ERROR_EXPR_EXPECTED_STR;
        }
    }
    if(executeMode){
		char paramStr[MAX_IDENT_LEN+1];
		if (strlen(stackGetStr()) > MAX_IDENT_LEN)
			return ERROR_BAD_PARAMETER;
		strcpy(paramStr, stackPopStr());
		if(op==TOKEN_OPEN){
			String filename="/"+String(paramStr)+".bdat";
			basicFilename=filename;
			if(!SPIFFS.exists(basicFilename)){
				basicFile=SPIFFS.open(basicFilename,"w");
				basicFile.close();
			}
			basicFileReadPosition=0;
			basicFileWritePosition=basicFile.size();
			basicFileOpen=true;
		}
		else if(op==TOKEN_ERASE){
			String filename="/"+String(paramStr)+".bdat";
			SPIFFS.remove(filename);
		}
		else if(op==TOKEN_SETSSID){
			writeStringToSettingFile("SSID",String(paramStr));
		}
		else if(op==TOKEN_SETSSIDPW){
			writeStringToSettingFile("SSIDPassword",String(paramStr));
		}
	}
	return 0;
}

void host_removeProgram(String filename);

// LOAD or LOAD "x"
// SAVE, SAVE+ or SAVE "x"
// DELETE "x"
int parseLoadSaveCmd() {
	//Serial.println("\tparseLoadSaveCmd called"); 
    int op = curToken;
    char autoexec = 0, gotFileName = 0;
    getNextToken();
	if (curToken != TOKEN_EOL && curToken != TOKEN_CMD_SEP) {
        int val = parseExpression();
        if (val & ERROR_MASK){
			return val;
		}
        if (!IS_TYPE_STR(val)){
            return ERROR_EXPR_EXPECTED_STR;
        }
        gotFileName = 1;
    }
	
	if(gotFileName==0){
		return ERROR_EXPR_EXPECTED_STR;
	}
	
    if (executeMode) {
        if (gotFileName) {
            char fileName[MAX_IDENT_LEN+1];
            if (strlen(stackGetStr()) > MAX_IDENT_LEN)
                return ERROR_BAD_PARAMETER;
            strcpy(fileName, stackPopStr());
            if (op == TOKEN_SAVE) {
                host_saveProgram(String(fileName));
            }
            else if (op == TOKEN_LOAD) {
                reset();
                host_loadProgram(String(fileName));
            }
            else if (op == TOKEN_DELETE) {
                host_removeProgram(String(fileName));
            }
        }
    }
    return 0;
}

void host_clearscreen(bool force);

#ifdef ESP8266
void host_directory(String ext){
	Dir dir=SPIFFS.openDir("");
	while(dir.next()){
		if(dir.fileName().endsWith(ext)){
			host_outputString("load \"");
			host_outputString((char*)dir.fileName().substring(1,dir.fileName().length()-ext.length()).c_str());
			host_outputChar('"');
			host_outputChar(0);
			host_outputInt(dir.fileSize());
			host_newLine();
		}
	}
}
#endif
#ifdef ESP32
void host_directory(String ext){
	File dir=SPIFFS.open("/");
	File file=dir.openNextFile();
	while(file){
		if(String(file.name()).endsWith(ext)){
			host_outputString("load \"");
			host_outputString((char*)String(file.name()).substring(1,String(file.name()).length()-ext.length()).c_str());
			host_outputChar('"');
			host_outputChar(0);
			host_outputInt(file.size());
			host_newLine();
		}
		file=dir.openNextFile();
	}
}
#endif

void host_welcome(bool force);

void showHelp(int page){
	File helpFile;
	host_clearscreen(false);
	if(page==1){
		helpFile=SPIFFS.open("/help.dat","r");
	}
	else if(page==2){
		helpFile=SPIFFS.open("/help2.dat","r");
	}
	else{
		helpFile=SPIFFS.open("/help3.dat","r");
	}
	while(helpFile.available()){
		host_outputChar(helpFile.read());
	}
	helpFile.close();
}

int parseSimpleCmd() {
	//Serial.println("\tparseSimpleCmd called"); 
    int op = curToken;
    getNextToken();	// eat op
    if (executeMode) {
        switch (op) {
            case TOKEN_NEW:
                reset();
                basicFileReadPosition=0;
                basicFileOpen=false;
                host_welcome(true);
                breakCurrentLine = 1;
                break;
            case TOKEN_STOP:
                stopLineNumber = lineNumber;
                stopStmtNumber = stmtNumber;
                programRunning=0;
                return ERROR_STOP_STATEMENT;
            case TOKEN_CONT:
                if (stopLineNumber) {
                    jumpLineNumber = stopLineNumber;
                    jumpStmtNumber = stopStmtNumber+1;
                }
                break;
            case TOKEN_RETURN:
            {
                int returnLineNumber, returnStmtNumber;
                if (!gosubStackPop(&returnLineNumber, &returnStmtNumber))
                    return ERROR_RETURN_WITHOUT_GOSUB;
                jumpLineNumber = returnLineNumber;
                jumpStmtNumber = returnStmtNumber+1;
                break;
            }
            case TOKEN_CLS:
                host_clearscreen(false);
                host_repaintScreen();
                break;
            case TOKEN_DIR:
                host_directory(".bas");
                break;
            case TOKEN_DATADIR:
                host_directory(".bdat");
                break;
            case TOKEN_CLOSE:
				basicFileOpen=false;
				basicFileReadPosition=0;
				basicFileWritePosition=0;
				break;
            case TOKEN_REBOOT:
				basicFile.close();
				host_outputString("Please wait while rebooting");
				delay(50);
				ESP.restart();
				break;
			case TOKEN_HELP:
				showHelp(1);
				break;
			case TOKEN_HELPTWO:
				showHelp(2);
				break;
			case TOKEN_HELPTHREE:
				showHelp(3);
				break;
        }
    }
    return 0;
}

int parse_DIM() {
	//Serial.println("\tparse_DIM called"); 
    char ident[MAX_IDENT_LEN+1];
    getNextToken();	// eat DIM
    if (curToken != TOKEN_IDENT) return ERROR_UNEXPECTED_TOKEN;
    if (executeMode)
        strcpy(ident, identVal);
    int isStringIdentifier = isStrIdent;
    getNextToken();	// eat ident
    int val = parseSubscriptExpr();
    if (val) return val;
    if (executeMode && !createArray(ident, isStringIdentifier ? 1 : 0))
        return ERROR_OUT_OF_MEMORY;
    return 0;
}

static int targetStmtNumber;

int parseStmts()
{
	//Serial.println("\tparseStmts called"); 
    int ret = 0;
    breakCurrentLine = 0;
    jumpLineNumber = 0;
    jumpStmtNumber = 0;

    while (ret == 0) {
        if (curToken == TOKEN_EOL)
            break;
        if (executeMode)
            sysSTACKEND = sysSTACKSTART = sysPROGEND;	// clear calculator stack
        int needCmdSep = 1;
        switch (curToken) {
			case TOKEN_PRINT: ret = parse_PRINT(); break;
			case TOKEN_LET: getNextToken(); ret = parseAssignment(false); break;
			case TOKEN_IDENT: ret = parseAssignment(false); break;
			case TOKEN_LIST: ret = parse_LIST(); break;
			case TOKEN_RUN: ret = parse_RUN(); break;
			case TOKEN_GOTO: ret = parse_GOTO(); break;
			case TOKEN_REM: getNextToken(); getNextToken(); break;
			case TOKEN_IF: ret = parse_IF(); needCmdSep = 0; break;
			case TOKEN_FOR: ret = parse_FOR(); break;
			case TOKEN_NEXT: ret = parse_NEXT(); break;
			case TOKEN_GOSUB: ret = parse_GOSUB(); break;
			case TOKEN_RSEEK: ret = parse_RSEEK(); break;
			case TOKEN_WSEEK: ret = parse_WSEEK(); break;
			case TOKEN_DIM: ret = parse_DIM(); break;
			case TOKEN_PAUSE: ret = parse_PAUSE(); break;
			
			case TOKEN_LOAD:
			case TOKEN_SAVE:
			case TOKEN_DELETE:
				ret = parseLoadSaveCmd();
				break;

			case TOKEN_POSITION:
				ret = parseTwoIntCmd(); 
				break;
				
			case TOKEN_SETMEMSIZE:
				ret = parse_SETMEMSIZE();
				break;
				
			case TOKEN_OPEN:
			case TOKEN_ERASE:
			case TOKEN_SETSSID:
			case TOKEN_SETSSIDPW:
				ret = parse_OPEN_ERASE_SSID();
				break;
			
			case TOKEN_FGCOLOR:
			case TOKEN_BGCOLOR:
			case TOKEN_SETFG:
			case TOKEN_SETBG:
				ret = parse_COLOR();
				break;
			
			case TOKEN_WRITE: 
				ret=parse_WRITE();
				break;

			case TOKEN_NEW:
			case TOKEN_STOP:
			case TOKEN_CONT:
			case TOKEN_CLS:
			case TOKEN_RETURN:
			case TOKEN_DIR:
			case TOKEN_DATADIR:
			case TOKEN_CLOSE:
			case TOKEN_REBOOT:
			case TOKEN_HELP:
			case TOKEN_HELPTWO:
			case TOKEN_HELPTHREE:
				ret = parseSimpleCmd();
				break;
							
			default: 
				ret = ERROR_UNEXPECTED_CMD;
        }
        // if error, or the execution line has been changed, exit here
        if (ret || breakCurrentLine || jumpLineNumber || jumpStmtNumber)
            break;
        // it should either be the end of the line now, and (generally) a command seperator
        // before the next command
        if (curToken != TOKEN_EOL) {
            if (needCmdSep) {
                if (curToken != TOKEN_CMD_SEP) ret = ERROR_UNEXPECTED_CMD;
                else {
                    getNextToken();
                    // don't allow a trailing :
                    if (curToken == TOKEN_EOL) ret = ERROR_UNEXPECTED_CMD;
                }
            }
        }
        // increase the statement number
        stmtNumber++;
        // if we're just scanning to find a target statement, check
        if (stmtNumber == targetStmtNumber)
            break;
    }
    return ret;
}

int processInput(unsigned char *tokenBuf) {
	//Serial.println("\tprocessInput called"); 
    // first token can be TOKEN_INTEGER for line number - stored as a float in numVal
    // store as WORD line number (max 65535)
    tokenBuffer = tokenBuf;
    getNextToken();
    // check for a line number at the start
    uint16_t gotLineNumber = 0;
    unsigned char *lineStartPtr = 0;
    if (curToken == TOKEN_INTEGER) {
		//Serial.print("\t\tLinenumber found "); 
        long val = (long)numVal;
		//Serial.println(val); 
        if (val <=65535) {
            gotLineNumber = (uint16_t)val;
            lineStartPtr = tokenBuffer;
            getNextToken();
        }
        else
            return ERROR_LINE_NUM_TOO_BIG;
    }

    executeMode = 0;
    targetStmtNumber = 0;
    int ret = parseStmts();	// syntax check
    if (ret != ERROR_NONE)
        return ret;

    if (gotLineNumber) {
        if (!doProgLine(gotLineNumber, lineStartPtr, tokenBuffer - lineStartPtr))
            ret = ERROR_OUT_OF_MEMORY;
    }
    else {
        // we start off executing from the input buffer
        tokenBuffer = tokenBuf;
        executeMode = 1;
        lineNumber = 0;	// buffer
        unsigned char *p;
        
        unsigned long nextYieldTime=millis()+100;

        while (1) {
            getNextToken();
            if(millis()>nextYieldTime){
				nextYieldTime=millis()+100;
				//Serial.println(F("Yielding"));
				yield(); // Give ESP time for Wifi-handling
			}

            stmtNumber = 0;
            // skip any statements? (e.g. for/next)
            if (targetStmtNumber) {
                executeMode = 0; 
                parseStmts(); 
                executeMode = 1;
                targetStmtNumber = 0;
            }
            // now execute
            ret = parseStmts();
            if (ret != ERROR_NONE)
                break;

            // are we processing the input buffer?
            if (!lineNumber && !jumpLineNumber && !jumpStmtNumber)
                break;	// if no control flow jumps, then just exit

            // are we RETURNing to the input buffer?
            if (lineNumber && !jumpLineNumber && jumpStmtNumber)
                lineNumber = 0;

            if (!lineNumber && !jumpLineNumber && jumpStmtNumber) {
                // we're executing the buffer, and need to jump stmt (e.g. for/next)
                tokenBuffer = tokenBuf;
            }
            else {
                // we're executing the program
                if (jumpLineNumber || jumpStmtNumber) {
                    // line/statement number was changed e.g. goto
                    p = findProgLine(jumpLineNumber);
                }
                else {
                    // line number didn't change, so just move one to the next one
                    p+= readLengthFromBuffer(p);
                }
                // end of program?
                if (p == &mem[sysPROGEND]){
					//host_outputString(errorTable[ERROR_NONE]);
					//host_newLine();
					host_newLine();
					programRunning=false;
                    break;	// end of program
                }

                lineNumber = readLengthFromBuffer(p+2);
                tokenBuffer = p+4;
                // if the target for a jump is missing (e.g. line deleted) and we're on the next line
                // reset the stmt number to 0
                if (jumpLineNumber && jumpStmtNumber && lineNumber > jumpLineNumber)
                    jumpStmtNumber = 0;
            }
            if (jumpStmtNumber){
                targetStmtNumber = jumpStmtNumber;
            }
           	server.handleClient(); // Check for and handle http-requests

#ifdef ESP8266
            if(Serial.available()){
				inkeyChar=Serial.read();
#endif
#ifdef ESP32
            if(Serial2.available()){
				inkeyChar=Serial2.read();
#endif
			}
			if(httpKeyAvailable){
				inkeyChar=httpKey;
				httpKeyAvailable=false;
			}
			if(inkeyChar==27){
                ret = ERROR_BREAK_PRESSED; 
                programRunning=false;
                inkeyChar=0;
                break;
			}
        }
    }
    return ret;
}

void reset() {
	//Serial.println("\treset called"); 
    // program at the start of memory
    sysPROGEND = 0;
    // stack is at the end of the program area
    sysSTACKSTART = sysSTACKEND = sysPROGEND;
    // variables/gosub stack at the end of memory
    sysVARSTART = sysVAREND = sysGOSUBSTART = sysGOSUBEND = MEMORY_SIZE;
    memset(&mem[0], 0, MEMORY_SIZE);

    stopLineNumber = 0;
    stopStmtNumber = 0;
    lineNumber = 0;
}

void host_init(int buzzerPin) {
}

void host_serialOutToVideocard(int x, int y, char c, char color){
#ifdef ESP8266
	Serial.write(255);
	Serial.write(x);
	Serial.write(y);
	Serial.write(c);
	Serial.write(color);
#endif
#ifdef ESP32
	Serial2.write(255);
	Serial2.write(x);
	Serial2.write(y);
	Serial2.write(c);
	Serial2.write(color);
#endif
}

void host_repaintScreen(){
	for(int y=0;y<basicY;y++){
		for(int x=0;x<basicX;x++){
			if(basicScreenColor[x+y*basicX]!=fgColor|(bgColor<<3)){
				basicScreenColor[x+y*basicX]=fgColor|(bgColor<<3);
				host_serialOutToVideocard(x,y,basicScreen[x+y*basicX],basicScreenColor[x+y*basicX]);
			}
		}
	}
}

void host_scrollBasicScreen(){
	hostY--;
	for(int y=0;y<basicY-1;y++){
		for(int x=0;x<basicX;x++){
			if(basicScreen[x+(y+1)*basicX]!=basicScreen[x+y*basicX] || basicScreenColor[x+(y+1)*basicX]!=basicScreenColor[x+y*basicX]){
				basicScreen[x+y*basicX]=basicScreen[x+(y+1)*basicX];
				basicScreenColor[x+y*basicX]=basicScreenColor[x+(y+1)*basicX];
				host_serialOutToVideocard(x,y,basicScreen[x+y*basicX],basicScreenColor[x+y*basicX]);
			}
		}
	}	
	for(int i=0;i<basicX;i++){
		if(basicScreen[i+(basicY-1)*basicX]!=0){
			basicScreen[i+(basicY-1)*basicX]=0;
			basicScreenColor[i+(basicY-1)*basicX]=fgColor|(bgColor<<3);
			host_serialOutToVideocard(i,basicY-1,basicScreen[i+(basicY-1)*basicX],basicScreenColor[i+(basicY-1)*basicX]);
		}
	}
	/*Serial.write(253);
	delay(20);*/
}

void host_outputString(char *str) {
	while (*str) {
		//Serial.print(*str);
		if(outputEnabled){
			basicOutput.print(*str);
		}
		host_outputChar(*str);
		str++;
	}
}

String host_toString(char *str){
	String temp="";
	while(*str){
		if(*str>=32 && *str<=126){ // Visibile ASCII-characters only
			temp+=*str;
		}
		str++;
	}
	return temp;
}

void host_outputProgMemString(const char *p) {
	if(outputEnabled){
		basicOutput.print(*p);
	}
	while (*p) {
		char c=*p;
		host_outputChar(c);
		p++;
	}
}

void host_outputChar(char c) {
    //Serial.print(c);
	if(outputEnabled){
		basicOutput.print(c);
	}
	basicScreen[hostX+hostY*basicX]=c;
	basicScreenColor[hostX+hostY*basicX]=fgColor|(bgColor<<3);
	if(c>=32 && c<=127){
		host_serialOutToVideocard(hostX,hostY,basicScreen[hostX+hostY*basicX],basicScreenColor[hostX+hostY*basicX]);
	}
	if(c==10){ // If carriage return
		hostX=basicX; // Goto EOL
	}
	else{
		hostX++;
	}
	if(hostX==basicX){
		hostX=0;
		hostY++;
		if(hostY==basicY){
			host_scrollBasicScreen();
		}
	}
}

int host_outputInt(long num) {
	//Serial.print(num);
	if(outputEnabled){
		basicOutput.print(num);
	}
    // returns len
    long i = num, xx = 1;
    int c = 0;
    do {
        c++;
        xx *= 10;
        i /= 10;
    } 
    while (i);
    for (int i=0; i<c; i++) {
        xx /= 10;
        char digit = ((num/xx) % 10) + '0';
        host_outputChar(digit);
    }
    return c;
}

String removeTrailingZeros(String f){
	if(f.indexOf(".")>-1){
		while(f.endsWith("0")){
			f.remove(f.length()-1);
		}
		if(f.endsWith(".")){
			f.remove(f.length()-1);
		}
	}
	return f;
}

char *host_floatToStr(float f, char *buf) {
	String temp=removeTrailingZeros(String(f,5));
	temp.toCharArray(buf,16);
    return buf;
}

void host_outputFloat(float f) {
	//Serial.print(f);
	if(outputEnabled){
		basicOutput.print(f);
	}
	if(ceil(f)==f){
		host_outputString((char *)String((long)ceil(f)).c_str());
	}
	else{
		host_outputString((char *)removeTrailingZeros(String(f,5)).c_str());
	}
}

void host_newLine() {
	//Serial.println();
	if(outputEnabled){
		basicOutput.println();
	}
	hostX=0;
	hostY++;
	if(hostY==basicY){
		host_scrollBasicScreen();
	}
}

void host_outputFreeMem(unsigned int val){
	host_newLine();
	host_outputInt(val);
	host_outputChar(' ');
	host_outputString(bytesFreeStr);      
}

void host_removeProgram(String filename){
	filename="/"+filename+".bas";
	SPIFFS.remove(filename);
}
	
void host_saveProgram(String filename) {
	filename="/"+filename+".bas";
	//Serial.println("\tsaveProgram called"); 
	SPIFFS.remove(filename);
	File nf=SPIFFS.open(filename,"w");
    unsigned char *p = &mem[0];
    while (p < &mem[sysPROGEND]) {
        uint16_t lineNum = readLengthFromBuffer(p+2);
		nf.print((long)lineNum);
		nf.print(' ');
		unsigned char * ppt=p+4;
		int modeREM = 0;
		while (*ppt != TOKEN_EOL) {
			if (*ppt == TOKEN_IDENT) {
				ppt++;
				while (*ppt < 0x80)
					nf.write((char)*ppt++);
				nf.write((char)(*ppt++)-0x80);
			}
			else if (*ppt == TOKEN_NUMBER) {
				ppt++;
				nf.print(readFloatFromBuffer(ppt));
				ppt+=4;
			}
			else if (*ppt == TOKEN_INTEGER) {
				ppt++;
				nf.print(readLongFromBuffer(ppt));
				ppt+=4;
			}
			else if (*ppt == TOKEN_STRING) {
				ppt++;
				if (modeREM) {
					while(*ppt!=0x00){
						nf.print((char)*ppt++);
					}
					ppt++;
				}
				else {
					nf.print('\"');
					while (*ppt) {
						if (*ppt == '\"'){
							nf.print('\"');
						}
						nf.print((char)*ppt++);
					}
					nf.print('\"');
					ppt++;
				}
			}
			else {
				uint8_t fmt = tokenTable[*ppt].format;
				if (fmt & TKN_FMT_PRE)
					nf.print(' ');
				nf.print((char *)tokenTable[*ppt].token);
				if (fmt & TKN_FMT_POST)
					nf.print(' ');
				if (*ppt==TOKEN_REM)
					modeREM = 1;
				ppt++;
			}
		}
		nf.println();
		p+=readLengthFromBuffer(p);
    }
	nf.close();
}

int host_addLineToProgram(String line){
	//Serial.println("\n\tAdding line "+line); 
	for(int i=0;i<TOKEN_BUF_SIZE;i++){
		tokenBuf[i]=0;
	}
	int ret=ERROR_NONE;
	ret = tokenize((unsigned char *)line.c_str(), tokenBuf, TOKEN_BUF_SIZE);
	if(ret!=ERROR_NONE){
		//Serial.println(line);
		//Serial.println(errorTable[ret]);
		basicOutput.println(line);
		basicOutput.println(errorTable[ret]);
		return ret;
	}
	else{
		//Serial.println("\tTokenize completed"); 
		/*for(int i=0;i<TOKEN_BUF_SIZE;i++){
			Serial.print(tokenBuf[i]);
			Serial.print('_');
		}
		Serial.println();*/
		ret=processInput(tokenBuf);
		//Serial.println("\tprocessInput completed"); 
		if (ret != ERROR_NONE) {
			//Serial.println(line);
			//Serial.println(errorTable[ret]);
			basicOutput.println(line);
			basicOutput.println(errorTable[ret]);
			return ret;
		}
	}
	return 0;
}

int host_loadProgram(String filename) {
	filename="/"+filename+".bas";
	// TBD: Optimize reading by reading until eol. Just make sure that during writing script.bas always only CR (\10) is used as eol.
	// 		Currently loading the script takes 5 to 10 times longer than running the script.
	reset();
    File f=SPIFFS.open(filename,"r");
    String line;
    while(f.available()){
		char c=f.read();
		if(c==13 || c==10){
			if(f.available()){
				c=f.read(); // In case of newline also read possible carriage return
			}
			else{
				c=10;
			}
			int err=host_addLineToProgram(line);
			if(err>0){
				return err;
			}
			if(c==10){ // Ignore carriage return
				line="";
			}
			else{
				line=c;
			}
			yield(); // Give ESP time for Wifi-handling
		}
		else{
			line=line+c;
		}
	}
    f.close();
    return 0;
}

void host_clearscreen(bool force){
	for(int y=0;y<basicY;y++){
		for(int x=0;x<basicX;x++){
			if(force){
				basicScreenColor[x+y*basicX]=fgColor|(bgColor<<3);
			}
			if(force || basicScreen[x+y*basicX]!=0){
				basicScreen[x+y*basicX]=0;
				host_serialOutToVideocard(x,y,0,basicScreenColor[x+y*basicX]);
			}
		}
	}
	/*Serial.write(254);
	delay(10);*/
	hostX=0;
	hostY=0;
}

void host_runProgram(String trigger){
	scriptTrigger=trigger;
	//Serial.println("\nRunning basic program");
	tokenBuf[0] = TOKEN_RUN;
	tokenBuf[1] = 0;
	host_clearscreen(true);
	processInput(tokenBuf);
	//Serial.println("\nEnd of basic program");
}

int host_getFreeMem(){
	return sysVARSTART - sysPROGEND;
}

int host_getFreeHostMem(){
	return ESP.getFreeHeap();
}

char *host_readLine() {
    inputMode = 1;

    if (hostX == 0){
//		for(int x=0;x<basicX;x++){
//			host_serialOutToVideocard(x,hostY,0,basicScreenColor[x+hostY*basicX]);
//		}
//		memset(basicScreen + basicX*(hostY), 0, basicX);
	}
    else{
		host_newLine();
	}
    int startPos = hostY*basicX+hostX;
    int pos = startPos;

    bool done = false;
    bool keyHandled=false;
    char c;
    while (!done) {
#ifdef ESP8266
        if (Serial.available()) {
#endif
#ifdef ESP32
        if (Serial2.available()) {
#endif
            // read the next key
            lineDirty[pos / basicX] = 1;
#ifdef ESP8266
            c = Serial.read();
#endif
#ifdef ESP32
            c = Serial2.read();
#endif
			keyHandled=true;
		}
		if(httpKeyAvailable && !keyHandled){
			c = httpKey;
			httpKeyAvailable=false;
			keyHandled=true;
		}
		if(keyHandled){
			keyHandled=false;
            if(!flashCursor && ((c>=32 && c<=126) || c==8 || c==13 || (c>=17 && c<=20) || (c>=1 && c<=4) || c==24)){ // Restore the actual character at the cursorlocation
				host_serialOutToVideocard(hostX,hostY,basicScreen[hostX+hostY*basicX],basicScreenColor[hostX+hostY*basicX]);
				flashCursor=true;
				nextFlashCursorEvent=0;
			}
            if (c>=32 && c<=126){
                basicScreen[pos++] = c;
                host_serialOutToVideocard(hostX,hostY,c,basicScreenColor[hostX+hostY*basicX]);
			}
            else if (c==8 && pos > startPos){ // BACKSPACE
                basicScreen[--pos] = 0;
                host_serialOutToVideocard(hostX-1,hostY,0,basicScreenColor[hostX+hostY*basicX]);
			}
			else if (c==17){ // Cursor down
				if(pos+basicX<basicX*basicY){
					pos=pos+basicX;
					startPos=((int)(pos/basicX))*basicX;
				}
			}
			else if (c==18){ // Cursor up
				if(pos-basicX>=0){
					pos=pos-basicX;
					startPos=((int)(pos/basicX))*basicX;
				}
			}
			else if (c==19){ // Cursor left
				if(pos-1>=0){
					pos=pos-1;
				}
			}
			else if (c==20){ // Cursor right
				if(pos+1<basicX*basicY){
					pos=pos+1;
				}
			}
			else if (c==2){ // Home
				pos=((int)(pos/basicX))*basicX;
			}
			else if (c==3){ // End
				while(pos<basicX*basicY && basicScreen[pos]>0){
					pos++;
				}
			}
            else if (c==13){ // ENTER
				while(pos<basicX*basicY && basicScreen[pos]>0){
					pos++;
				}
                done = true;
            }
            else if (c==24){ // DELETE = clear line
				int y=(pos/basicX);
				for(int x=0;x<basicX;x++){
					if(basicScreen[x+basicX*y]>0){
						basicScreen[x+basicX*y]=0;
						host_serialOutToVideocard(x,y,0,basicScreenColor[x+basicX*y]);
					}
				}
				pos=((int)(pos/basicX))*basicX;
			}
			else if (c==1){ // PageUp = Search empty line up
				pos=((int)(pos/basicX))*basicX;
				if(basicScreen[pos]==0 && pos>=basicX){
					pos-=basicX;
				}
				while(pos>=basicX && basicScreen[pos]>0){
					pos-=basicX;
				}
			}
			else if (c==4){ // PageDown = Search empty line down
				pos=((int)(pos/basicX))*basicX;
				if(basicScreen[pos]==0 && pos<basicX*(basicY-1)){
					pos+=basicX;
				}
				while(pos<basicX*(basicY-1) && basicScreen[pos]>0){
					pos+=basicX;
				}
			}
            hostX = pos % basicX;
            hostY = pos / basicX;
            // scroll if we need to
            if (hostY == basicY) {
                if (startPos >= basicX) {
                    startPos -= basicX;
                    pos -= basicX;
                    host_scrollBasicScreen();
                }
                else
                {
                    basicScreen[--pos] = 0;
					host_serialOutToVideocard(hostX-1,hostY,0,basicScreenColor[hostX+hostY*basicX]);
                    hostX = pos % basicX;
                    hostY = pos / basicX;
                }
            }
            yield();
        }
        else{
			if(millis()>nextFlashCursorEvent){
				if(flashCursor){
					host_serialOutToVideocard(hostX,hostY,'_',basicScreenColor[hostX+hostY*basicX]);
				}
				else{
					host_serialOutToVideocard(hostX,hostY,basicScreen[hostX+hostY*basicX],basicScreenColor[hostX+hostY*basicX]);
				}
				flashCursor=!flashCursor;
				nextFlashCursorEvent=millis()+500;
			}
		}
		espLoop();
    }
    basicScreen[pos] = 0;
    inputMode = 0;
    // remove the cursor
    lineDirty[hostY] = 1;
    return &basicScreen[startPos];
}

void host_welcome(bool force){
	host_clearscreen(force);
	host_outputString("Magnatic Basic Computer (C)2020");
	host_outputFreeMem(sysVARSTART - sysPROGEND);
	host_newLine();
	host_outputString("Go to http://");
	host_outputString((char*)wifiIPAddress.c_str());
	host_outputString("/keyboard.html for a browser based keyboard-interface");
	if(wifiIPAddress=="192.168.4.1"){
		host_newLine();
		byte mac[6];
		WiFi.macAddress(mac);
		String ssidString=readStringFromSettingFile("DeviceName")+"-"+String(mac[5],HEX)+String(mac[4],HEX)+String(mac[3],HEX)+String(mac[2],HEX)+String(mac[1],HEX)+String(mac[0],HEX);
		host_outputString("Connect to SSID \"");
		host_outputString((char*)ssidString.c_str());
		host_outputString("\" with password \"");
		host_outputString((char*)readStringFromSettingFile("APPassword").c_str());
		host_outputString("\"");
	}
	host_newLine();
	host_newLine();
}

void handleBasicRecv(){
	basicHttpRecvParamValue=serverArgument(basicHttpRecvParamName);
	httpRecvAvailable=true;
	server.send(200,"text/plain","OK");
}

void handleHttpKey(){
	httpKeyAvailable=true;
	httpKey=(char)server.arg(0)[0];
	server.send(200,"text/plain","OK");
}

void basicSetup(){
#ifdef ESP8266
	Serial.begin(1843200); // From now on the serial output is used to communicate to the videocard + keyboard-controller
	addConfigParameter(F("basicMemory"),F("Size of basic memory in bytes"),F("16384"),MainConfigPage,true,3);
#endif
#ifdef ESP32
	Serial2.begin(1843200); // The ESP32 is unable to start with connections to serial0, so serial2 is used to communicate with videocard + keyboard-controller
	addConfigParameter(F("basicMemory"),F("Size of basic memory in bytes"),F("113792"),MainConfigPage,true,3); // 113792 bytes is the largest block available on a ESP32
#endif
	basicScreen = (char*) malloc (basicX*basicY);
	basicScreenColor = (char*) malloc (basicX*basicY);
	bool memAllocationFailed=false;
	MEMORY_SIZE=readIntFromSettingFile("basicMemory");
	mem=(unsigned char*) malloc(MEMORY_SIZE);
	if(mem==NULL){ // Asked for to much memory. Reverting to absolute safe value.
		MEMORY_SIZE=8192;
		mem=(unsigned char*) malloc(MEMORY_SIZE);
		memAllocationFailed=true;
	}
	reset();
	server.on("/sourceinfo/basic",[](){server.send(200,"text/plain",libraryTimeBasic);});
	server.on("/basicrecv",handleBasicRecv);
	server.on("/httpkey",handleHttpKey);
	digitalWrite(ActivePin,HIGH);
	host_welcome(true);
	if(memAllocationFailed){
		bgColor=COLOR_RED;
		fgColor=COLOR_WHITE;
		host_outputString("ATTENTION! Configured amount of memory could not be allocated.");
		bgColor=COLOR_BLUE;
		fgColor=COLOR_WHITE;
		host_newLine();
		host_newLine();
	}
}

int x=0;
int y=0;

void basicLoop(){
    int ret = ERROR_NONE;

	// get a line from the user
	char *input = host_readLine();
	// otherwise tokenize
	ret = tokenize((unsigned char*)input, tokenBuf, TOKEN_BUF_SIZE);

    // execute the token buffer
    if (ret == ERROR_NONE) {
        host_newLine();
        ret = processInput(tokenBuf);
    }
    if (ret != ERROR_NONE) {
        host_newLine();
        if (lineNumber !=0) {
            host_outputInt(lineNumber);
            host_outputChar('-');
        }
        host_outputString(errorTable[ret]);
    }
	espLoop();
}
