/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

//open()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "arch_rar.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>

#ifndef HAVE_STRNLEN
size_t
strnlen(const char *s, size_t maxlen)
{
	size_t i = 0;

	while (i < maxlen && (*s++))
		i++;

	return i;
}
#endif
	
arch_Rar::arch_Rar(const string& aFileName)
{
	//check if file exists
	int lFileDesc = open(aFileName.c_str(), O_RDONLY);
	char lBuffer[350];
	uint32 lLength;
	uint32 lCount;
	uint32 lPos = 0;
	bool lFound = false;
	string lName, lGoodName;

	if(lFileDesc == -1)
	{
		mSize = 0;
		return;
	}
	
	close(lFileDesc);

	string lCommand = "unrar l \"" + aFileName + '\"';   //get info
	FILE *f = popen(lCommand.c_str(), "r");
	
	if(f <= 0)
	{
		mSize = 0;
		return;
	}

	int num = 7;
	while (num--) // ignore 7 lines.
	{
		if(!fgets(lBuffer, 90, f))
		{
			mSize = 0;
			return;
		}
        }
	
	bool eof = false;
	while(!eof)
	{
		if(fgets(lBuffer, 350, f) <= 0 || f <= 0)
			break;
		if (strlen(lBuffer) > 1)
			lBuffer[strlen(lBuffer)-1] = 0;
		
		lLength = strlen(lBuffer);
		lCount = 0;
		for(uint32 i = lLength - 1; i > 0; i--)
		{
			if(lBuffer[i] == ' ')
			{
				lBuffer[i] = 0;
				if(lBuffer[i - 1] != ' ')
				{
					lCount++;
					if(lCount == 9)
					{
						lPos = i;
						break;
					}
				}
			}
		}
		
		while(lBuffer[lPos] == '\0')
			lPos++;
		lName = &lBuffer[1]; // get rid of the space.

		mSize = strtol(lBuffer + lPos, NULL, 10);
		
		if(IsOurFile(lName))
		{
			lFound = true;
			break;
		}		
	}
	
	if(!lFound)
	{
		mSize = 0;
		return;
	}

	pclose(f);
	
	mMap = new char[mSize];
	if(mMap == NULL)
	{
		mSize = 0;
		return;
	}
	
	lCommand = "unrar p -inul \"" + aFileName + "\" \"" + lName + '\"';  
        //decompress to stdout
	f = popen(lCommand.c_str(), "r");
	
	if (f <= 0)
	{
		mSize = 0;
		return;
	}

	if(fread((char *)mMap, sizeof(char), mSize, f) != mSize)
		mSize = 0;
	
	pclose(f);
}

arch_Rar::~arch_Rar()
{
	if(mSize != 0)
		delete [] (char*)mMap;
}

bool arch_Rar::ContainsMod(const string& aFileName)
{
	//check if file exists
	const uint32 LBUFFER_SIZE = 350;
	string lName;
	int lFileDesc = open(aFileName.c_str(), O_RDONLY);
	char lBuffer[LBUFFER_SIZE];

	if(lFileDesc == -1)
		return false;
	
	close(lFileDesc);

	string lCommand = "unrar l \"" + aFileName + '\"';   //get info
	FILE *f = popen(lCommand.c_str(), "r");
	
	if(f <= 0)
		return false;

	int num = 7;
	while (num--)
	{
		
		if(!fgets(lBuffer, 90, f)) //ignore a line.
		{
			pclose(f);
			return false;
		}

	}

	bool eof = false;
	while(!eof)
	{
		if(fgets(lBuffer, LBUFFER_SIZE, f) == NULL)
			break; // End of file or read error
		uint32 lLength = strnlen(lBuffer, LBUFFER_SIZE);
		if (lLength > 1)
			lBuffer[lLength-1] = 0;
		
		uint32 lCount = 0;
		// Remove non-filename related data from the end of the each
		// filename line (9 fields separated by a space)
		for(uint32 i = lLength - 1; i > 0; i--)
		{
			if(lBuffer[i] == ' ')
			{
				lBuffer[i] = 0;
				if(lBuffer[i - 1] != ' ')
				{
					lCount++;
					if(lCount == 9)
						break;
				}
			}
		}
		
		lName = lBuffer;
		
		if(IsOurFile(lName)) {
			pclose(f);
			return true;
		}
	}

	pclose(f);
	return false;
}
