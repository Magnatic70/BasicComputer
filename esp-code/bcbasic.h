// v001:	* First version by Magnatic. Based on https://github.com/robinhedwards/ArduinoBASIC. Robin Edwards released this under MIT-license.
// 				* Adapted for ESP8266 (removed PROGMEM, fixed 4 byte alignment issues with buffers and basic-program-memory)
//				* Suitable for both ESP8266 and ESP32
//				* Added a number of new functions and commands to basic, removed all I/O-related functions and commands
//				* High-speed serial interface to FPGA. ESP-->FPGA =  display, ESP<--FPGA =  PS/2-keyboard
//				* Browser-based keyboard-interface

// v002:	* Moved from magnatic-esp8266 to magnatic-esp
//			* Fixed a bug where the ip address was incorrent when the deviceAP was used

#ifndef _BASIC_H
#define _BASIC_H

#include <stdint.h>

#define TOKEN_EOL		0
#define TOKEN_IDENT		1	// special case - identifier follows
#define TOKEN_INTEGER	        2	// special case - integer follows (line numbers only)
#define TOKEN_NUMBER	        3	// special case - number follows
#define TOKEN_STRING	        4	// special case - string follows

#define TOKEN_LBRACKET	        8
#define TOKEN_RBRACKET	        9
#define TOKEN_PLUS	    	10
#define TOKEN_MINUS		11
#define TOKEN_MULT		12
#define TOKEN_DIV		13
#define TOKEN_EQUALS	        14
#define TOKEN_GT		15
#define TOKEN_LT		16
#define TOKEN_NOT_EQ	        17
#define TOKEN_GT_EQ		18
#define TOKEN_LT_EQ		19
#define TOKEN_CMD_SEP	        20
#define TOKEN_SEMICOLON	        21
#define TOKEN_COMMA		22
#define TOKEN_AND		23	// FIRST_IDENT_TOKEN
#define TOKEN_OR		24
#define TOKEN_NOT		25
#define TOKEN_PRINT		26
#define TOKEN_LET		27
#define TOKEN_LIST		28
#define TOKEN_RUN		29
#define TOKEN_GOTO		30
#define TOKEN_REM		31
#define TOKEN_STOP		32
#define TOKEN_INPUT	        33
#define TOKEN_CONT		34
#define TOKEN_IF		35
#define TOKEN_THEN		36
#define TOKEN_LEN		37
#define TOKEN_VAL		38
#define TOKEN_RND		39
#define TOKEN_INT		40
#define TOKEN_STR		41
#define TOKEN_FOR		42
#define TOKEN_TO		43
#define TOKEN_STEP		44
#define TOKEN_NEXT		45
#define TOKEN_MOD		46
#define TOKEN_NEW		47
#define TOKEN_GOSUB		48
#define TOKEN_RETURN	        49
#define TOKEN_DIM		50
#define TOKEN_LEFT		51
#define TOKEN_RIGHT	        52
#define TOKEN_MID		53
#define TOKEN_CLS               54
#define TOKEN_PAUSE             55
#define TOKEN_POSITION          56
#define TOKEN_PIN               57
#define TOKEN_PINMODE           58
#define TOKEN_INKEY             59
#define TOKEN_SAVE              60
#define TOKEN_LOAD              61
#define TOKEN_PINREAD           62
#define TOKEN_ANALOGRD          63
#define TOKEN_DIR               64
#define TOKEN_DELETE            65
#define TOKEN_MILLIS			66
#define TOKEN_HTTPGET			67
#define TOKEN_FREEMEM			68
#define TOKEN_FREEHOSTMEM		69
#define TOKEN_IPADDR			70
#define TOKEN_SETSSID			71
#define TOKEN_SETSSIDPW			72
#define TOKEN_GETSSID			73
#define TOKEN_OPEN				74
#define TOKEN_CLOSE				75
#define TOKEN_READLINE			76
#define TOKEN_WRITE				77
#define TOKEN_ERASE				78
#define TOKEN_EOF				79
#define TOKEN_HTTPRECV			80
#define TOKEN_REBOOT			81
#define TOKEN_INDEXOF			82
#define TOKEN_COUNTOF			83
#define TOKEN_FGCOLOR			84
#define TOKEN_BGCOLOR			85
#define TOKEN_SETMEMSIZE		86
#define TOKEN_SETFG				87
#define TOKEN_SETBG				88
#define TOKEN_HELP				89
#define TOKEN_HELPTWO			90
#define TOKEN_NEWHTTPRECV		91
#define TOKEN_DATADIR			92
#define TOKEN_RSEEK				93
#define TOKEN_READPOS			94
#define TOKEN_CHR				95
#define TOKEN_WSEEK				96
#define TOKEN_READ				97
#define TOKEN_WRITEPOS			98
#define TOKEN_HELPTHREE			99

#define FIRST_IDENT_TOKEN 23
#define LAST_IDENT_TOKEN 99

#define FIRST_NON_ALPHA_TOKEN    8
#define LAST_NON_ALPHA_TOKEN    22

#define ERROR_NONE				0
// parse errors
#define ERROR_LEXER_BAD_NUM			1
#define ERROR_LEXER_TOO_LONG			2
#define ERROR_LEXER_UNEXPECTED_INPUT	        3
#define ERROR_LEXER_UNTERMINATED_STRING         4
#define ERROR_EXPR_MISSING_BRACKET		5
#define ERROR_UNEXPECTED_TOKEN			6
#define ERROR_EXPR_EXPECTED_NUM			7
#define ERROR_EXPR_EXPECTED_STR			8
#define ERROR_LINE_NUM_TOO_BIG			9
// runtime errors
#define ERROR_OUT_OF_MEMORY			10
#define ERROR_EXPR_DIV_ZERO			11
#define ERROR_VARIABLE_NOT_FOUND		12
#define ERROR_UNEXPECTED_CMD			13
#define ERROR_BAD_LINE_NUM			14
#define ERROR_BREAK_PRESSED			15
#define ERROR_NEXT_WITHOUT_FOR			16
#define ERROR_STOP_STATEMENT			17
#define ERROR_MISSING_THEN			18
#define ERROR_RETURN_WITHOUT_GOSUB		19
#define ERROR_WRONG_ARRAY_DIMENSIONS	        20
#define ERROR_ARRAY_SUBSCRIPT_OUT_RANGE	        21
#define ERROR_STR_SUBSCRIPT_OUT_RANGE	        22
#define ERROR_IN_VAL_INPUT			23
#define ERROR_BAD_PARAMETER                     24
#define ERROR_FILE_NOT_OPEN						25
#define ERROR_STRUCTURE_TO_BIG					26

#define MAX_IDENT_LEN	10
#define MAX_NUMBER_LEN	10

#ifdef ESP8266
//#define MEMORY_SIZE	16384
#define ActivePin 5
#endif
#ifdef ESP32
//#define MEMORY_SIZE 70000
#define ActivePin 21
#endif
#define TOKEN_BUF_SIZE    256

extern int MEMORY_SIZE;

#define COLOR_BLACK 0
#define COLOR_BLUE 1
#define COLOR_GREEN 2
#define COLOR_CYAN 3
#define COLOR_RED 4
#define COLOR_PURPLE 5
#define COLOR_YELLOW 6
#define COLOR_WHITE 7

//extern unsigned char mem[];
extern unsigned char * mem;
extern unsigned char tokenBuf[];
extern int sysPROGEND;
extern int sysSTACKSTART;
extern int sysSTACKEND;
extern int sysVARSTART;
extern int sysVAREND;
extern int sysGOSUBSTART;
extern int sysGOSUBEND;

extern WiFiClient basicOutput; // For output of basic program

extern uint16_t lineNumber;	// 0 = input buffer

typedef struct {
    float val;
    float step;
    float end;
    uint16_t lineNumber;
    uint16_t stmtNumber;
} 
ForNextData;

typedef struct {
    char *token;
    uint8_t format;
} 
TokenTableEntry;

//extern const char *errorTable[];
extern char* errorTable[];


void reset();
int tokenize(unsigned char *input, unsigned char *output, int outputSize);
int processInput(unsigned char *tokenBuf);

#endif

void host_init(int buzzerPin);
void host_sleep(long ms);
void host_outputString(char *str);
void host_outputProgMemString(const char *str);
void host_outputChar(char c);
void host_outputFloat(float f);
char *host_floatToStr(float f, char *buf);
int host_outputInt(long val);
void host_newLine();
void host_outputFreeMem(unsigned int val);
void host_saveProgram(String filename);
int host_loadProgram(String filename);
void host_runProgram(String trigger);
int host_getFreeMem();
int host_addLineToProgram(String line);
void basicSetup();
void basicLoop();
