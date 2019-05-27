//========================================================
// CALLSIGN HASH (10, 12, 22) and STORE AND LOAD
// This source code from Wsjtx-2.1.0 RC5 and kgoba/ft8_lib
// Test widht Wsjtx-2.1.0 RC5
//
// Namespaces are not used for compatibility with C language projects.
// 
// By KD8CEC  kd8cec@gmail.com
//========================================================

#include <string.h>
#include <stdio.h>
#include "hashcall.h"

const char A0[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/";

//this function from kgoba/ft8_lib
static int char_index(const char *string, char c) 
{
    for (int i = 0; *string; ++i, ++string) {
        if (c == *string) {
            return i;
        }
    }
    return -1;  // Not found
}

//converted from Wsjt-x 2.1.0 RC5
uint32_t ihashcall(char *c0, int m)
{
    //character*38 c
    int64_t n8 = 0;
    for (int i = 0; i < 11; i++)
    {
        int j = char_index(A0, c0[i]);
        n8 = 38 * n8 + j;
    }

    uint64_t rst = (uint64_t(47055833459ll * n8) >> (64-m));
    return rst;
}

//===============================
//FOR HASH22
uint32_t  CALL22_Hash[CALL22_MAX];
char CALL22_Text[CALL22_MAX][13];
int  CALL22_Cnt = 0;
//--------------------------------
//FOR HASH12
uint32_t  CALL12_Hash[CALL12_MAX];
char CALL12_Text[CALL12_MAX][13];
int  CALL12_Cnt = 0;
//--------------------------------

//FOR HASH10
uint32_t  CALL10_Hash[CALL10_MAX];
char CALL10_Text[CALL10_MAX][13];
int  CALL10_Cnt = 0;
//--------------------------------

int FindHashIndex(uint32_t *iHashData, uint32_t callHash, int hashMaxCount)
{
    for (int i = 0; i < hashMaxCount; i++)
        if (iHashData[i] == callHash)
            return i;

    return -1;
}



char mycall13[13];

void save_hash_call(char *c13)
{
    char cw[13];

    if (c13[0] == ' ' || (
	c13[0] == '<' &&
	c13[1] == '.' &&
	c13[2] == '.' &&
	c13[3] == '.' &&
	c13[4] == '>'
	))
    {
        return;
    }

    int callLength = strlen(c13);

    if (callLength < 3) return;

    int writeIndex = 0;
    int findedSpCallChar = 0;


    for (int i = 0; i < 13; i++)
    {
        if (findedSpCallChar == 2)
           cw[writeIndex++] = ' ';
        else if (findedSpCallChar == 0 && c13[i] == '<') 
            findedSpCallChar = 1;
  	else if (findedSpCallChar == 1 && c13[i] == '>')
            findedSpCallChar = 2;
        else if (c13[i] == 0)
            break;
        else 
            cw[writeIndex++] = c13[i];
    }	//end of for

    while (writeIndex < 13)
        cw[writeIndex++] = ' ';

    uint32_t n10 = ihashcall(cw, 10);
    if(n10 >= 1 && n10 <= 1024 && (strcmp(cw,mycall13) != 0))
    {
    	//strcpy(calls10[n10], cw);
    	int foundIndex = FindHashIndex(CALL10_Hash, n10, CALL10_MAX);
	if (foundIndex > -1)
            memcpy(&CALL10_Text[foundIndex], &cw, 13);
        else
        {
            //printf("CALL10_MAX:%d \n", CALL10_MAX);
	    //Shift data
	    for (int i = CALL10_MAX -1; i > 0; i--)
	    {
		CALL10_Hash[i] = CALL10_Hash[i-1];
		memcpy(&CALL10_Text[i],&CALL10_Text[i -1], 13);
	    }

	    CALL10_Hash[0] = n10;
	    memcpy(&CALL10_Text[0], &cw, 13);
        }
	if (CALL10_Cnt < CALL10_MAX)
		CALL10_Cnt++;

        //printf("Saved 10; %d, %d \n", CALL10_Cnt, n10);
    }	// end of if

    uint32_t n12 = ihashcall(cw, 12);
    if(n12 >= 1 && n12 <= 4096 && (strcmp(cw,mycall13) != 0))
    {
    	int foundIndex = FindHashIndex(CALL12_Hash, n12, CALL12_MAX);
	if (foundIndex > -1)
            memcpy(&CALL12_Text[foundIndex], &cw, 13);
        else
        {
	    //Shift data
	    for (int i = CALL12_MAX -1; i > 0; i--)
	    {
		CALL12_Hash[i] = CALL12_Hash[i-1];
		memcpy(&CALL12_Text[i],&CALL12_Text[i -1], 13);
	    }

	    CALL12_Hash[0] = n12;
	    memcpy(&CALL12_Text[0], &cw, 13);
        }

	if (CALL12_Cnt < CALL12_MAX)
		CALL12_Cnt++;

        //printf("Saved 12; %d, %d \n", CALL12_Cnt, n12);
    }	// end of if


    uint32_t n22 = ihashcall(cw, 22);
    if(strcmp(cw,mycall13) != 0)
    {
    	int foundIndex = FindHashIndex(CALL22_Hash, n22, CALL22_MAX);
	if (foundIndex > -1)
            memcpy(&CALL22_Text[foundIndex], &cw, 13);
        else
        {
	    //Shift data
	    for (int i = CALL22_MAX -1; i > 0; i--)
	    {
		CALL22_Hash[i] = CALL22_Hash[i-1];
		memcpy(&CALL22_Text[i],&CALL22_Text[i -1], 13);
	    }

	    CALL22_Hash[0] = n22;
	    memcpy(&CALL22_Text[0], &cw, 13);
        }

	if (CALL22_Cnt < CALL22_MAX)
		CALL22_Cnt++;

        //printf("Saved 22; %d, %u \n", CALL22_Cnt, n22);
    }	// end of if
}


int StrTrim(char *line)
{
	#define MAX_LINE_SIZE 100
        int len = 0;
        char cpTrim[MAX_LINE_SIZE];
        int xMan = 0;
        int i;
 
        len = strlen(line);
        if (len >= MAX_LINE_SIZE)
        {
        	return -1;
        }
 
        strcpy(cpTrim, line);
 
        for (i = 0; i < len; i++)
        {
                if (cpTrim[i] == ' ' || cpTrim[i] == '\t')
                        xMan++;
                else
                        break;
        }
 
        for (i = len-2; i >= 0; i--)
        {
                if (cpTrim[i] == ' ' || cpTrim[i] == '\t' || cpTrim[i] == '\n')
                        cpTrim [i] = '\0';
                else
                        break;
        }
 
        strcpy (line, cpTrim+xMan);
 
        return strlen(line);
}

void hash12(uint32_t n12, char * c13)
{
    strcpy(c13, "<...>");
    //if (n12 < 1 || n12 > 4096) return;
    int foundIndex = FindHashIndex(CALL12_Hash, n12, CALL12_MAX);
    if (foundIndex > -1)
    {
        strcpy(c13, "<");
	char tmpCallsign[13];
	memcpy(tmpCallsign, CALL12_Text[foundIndex], 13);
        StrTrim(tmpCallsign);
	strcat(c13, tmpCallsign);
	strcat(c13, ">");
    } 
}


void hash22(uint32_t n22, char * c13)
{
    strcpy(c13, "<...>");
    int foundIndex = FindHashIndex(CALL22_Hash, n22, CALL22_MAX);
    if (foundIndex > -1)
    {
        strcpy(c13, "<");
	char tmpCallsign[13];
	memcpy(tmpCallsign, CALL22_Text[foundIndex], 13);
        StrTrim(tmpCallsign);
	strcat(c13, tmpCallsign);
	strcat(c13, ">");
    }
}

