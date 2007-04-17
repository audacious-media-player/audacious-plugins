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

#include "arch_gzip.h"

	
arch_Gzip::arch_Gzip(const string& aFileName)
{
	//check if file exists
	int lFileDesc = open(aFileName.c_str(), O_RDONLY);
	
	if(lFileDesc == -1)
	{
		mSize = 0;
		return;
	}       
	close(lFileDesc);

	// file exists.       
	string lCommand = "gunzip -l \"" + aFileName + '\"';   //get info
	FILE *f = popen(lCommand.c_str(), "r");
	
	if (f <= 0)
	{
		mSize = 0;
		return;
	}
	
	char line[81];
	if(!fgets(line, 80, f)){   // ignore a line.
		mSize = 0;
		pclose(f);
		return;
	}
	if(fscanf(f, "%u", &mSize) != 1){  // ignore first number.
		mSize = 0;
		pclose(f);
		return;
	}

	if (fscanf(f, "%u", &mSize) != 1){; // keep second number.
		mSize = 0;
		pclose(f);
		return;
	}
	
	pclose(f);
	
	mMap = new char[mSize];
	if(mMap == NULL)
	{
		mSize = 0;
		return;
	}
	
	lCommand = "gunzip -c \"" + aFileName + '\"';  //decompress to stdout
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

arch_Gzip::~arch_Gzip()
{
	if(mSize != 0)
		delete [] (char*)mMap;
}

bool arch_Gzip::ContainsMod(const string& aFileName)
{
	string lName;
	int lFileDesc = open(aFileName.c_str(), O_RDONLY);
	uint32 num;
	float fnum;

	if(lFileDesc == -1)
		return false;
	
	close(lFileDesc);

	// file exists.       
	string lCommand = "gunzip -l \"" + aFileName + '\"';   //get info
	FILE *f = popen(lCommand.c_str(),"r");
	
	if (f <= 0) {
		pclose(f);
		return false;
	}
	
	char line[300];
	if(!fgets(line, 80, f)){   // ignore a line.
		pclose(f);
		return false;
	}	
	if(fscanf(f, "%i", &num) != 1){ // ignore first number
		pclose(f);
		return false;
	}
	if(fscanf(f, "%i", &num) != 1){ // ignore second number
		pclose(f);
		return false;
	}
	if(fscanf(f, "%f%%", &fnum) != 1){ // ignore ratio
		pclose(f);
		return false;
	}
	if(!fgets(line, 300, f)){   // read in correct line safely.
		pclose(f);
		return false;
	}
	if (strlen(line) > 1)
		line[strlen(line)-1] = 0;
	lName = line;
	
	pclose(f);
	
	return IsOurFile(lName);
}
