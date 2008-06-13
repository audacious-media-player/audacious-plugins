/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#include "open.h"
#include "arch_raw.h"
#include "arch_gzip.h"
#include "arch_zip.h"
#include "arch_rar.h"
#include "arch_bz2.h"

Archive* OpenArchive(const string& aFileName) //aFilename is url --yaz
{
	// TODO: archive support in Modplug plugin is broken - it uses popen() call and Unix commandline tools for that.
	// Plus Modplug allows only one audio file per archive which is wrong anyway.

	string lExt;
	uint32 lPos;
	// convert from uri to fs based filepath
	gchar *filename = g_filename_from_uri(aFileName.c_str(), NULL, NULL);
	if (filename == NULL)
		return new arch_Raw(aFileName);
	string lRealFileName(filename);
	g_free(filename);

	lPos = lRealFileName.find_last_of('.');
	if(lPos <= lRealFileName.length() && aFileName.find("file://") == 0)
	{
		lExt = lRealFileName.substr(lPos);
		for(uint32 i = 0; i < lExt.length(); i++)
			lExt[i] = tolower(lExt[i]);

		if (lExt == ".mdz")
			return new arch_Zip(lRealFileName);
		if (lExt == ".mdr")
			return new arch_Rar(lRealFileName);
		if (lExt == ".mdgz")
			return new arch_Gzip(lRealFileName);
		if (lExt == ".mdbz")
			return new arch_Bzip2(lRealFileName);
		if (lExt == ".s3z")
			return new arch_Zip(lRealFileName);
		if (lExt == ".s3r")
			return new arch_Rar(lRealFileName);
		if (lExt == ".s3gz")
			return new arch_Gzip(lRealFileName);
		if (lExt == ".xmz")
			return new arch_Zip(lRealFileName);
		if (lExt == ".xmr")
			return new arch_Rar(lRealFileName);
		if (lExt == ".xmgz")
			return new arch_Gzip(lRealFileName);
		if (lExt == ".itz")
			return new arch_Zip(lRealFileName);
		if (lExt == ".itr")
			return new arch_Rar(lRealFileName);
		if (lExt == ".itgz")
			return new arch_Gzip(lRealFileName);
		if (lExt == ".zip")
			return new arch_Zip(lRealFileName);
		if (lExt == ".rar")
			return new arch_Rar(lRealFileName);
		if (lExt == ".gz")
			return new arch_Gzip(lRealFileName);
		if (lExt == ".bz2")
			return new arch_Bzip2(lRealFileName);
	}

	return new arch_Raw(aFileName);
}

bool ContainsMod(const string& aFileName) //aFilename is url --yaz
{
	string lExt;
	uint32 lPos;

	// convert from uri to fs based filepath
	gchar *filename;
	filename = g_filename_from_uri(aFileName.c_str(), NULL, NULL);
	string lRealFileName(filename);
	g_free(filename);

	lPos = lRealFileName.find_last_of('.');
	if(lPos <= lRealFileName.length() && aFileName.find("file://") == 0)
	{
		lExt = lRealFileName.substr(lPos);
		for(uint32 i = 0; i < lExt.length(); i++)
			lExt[i] = tolower(lExt[i]);

		if (lExt == ".mdz")
			return arch_Zip::ContainsMod(lRealFileName);
		if (lExt == ".mdr")
			return arch_Rar::ContainsMod(lRealFileName);
		if (lExt == ".mdgz")
			return arch_Gzip::ContainsMod(lRealFileName);
		if (lExt == ".mdbz")
			return arch_Bzip2::ContainsMod(lRealFileName);
		if (lExt == ".s3z")
			return arch_Zip::ContainsMod(lRealFileName);
		if (lExt == ".s3r")
			return arch_Rar::ContainsMod(lRealFileName);
		if (lExt == ".s3gz")
			return arch_Gzip::ContainsMod(lRealFileName);
		if (lExt == ".xmz")
			return arch_Zip::ContainsMod(lRealFileName);
		if (lExt == ".xmr")
			return arch_Rar::ContainsMod(lRealFileName);
		if (lExt == ".xmgz")
			return arch_Gzip::ContainsMod(lRealFileName);
		if (lExt == ".itz")
			return arch_Zip::ContainsMod(lRealFileName);
		if (lExt == ".itr")
			return arch_Rar::ContainsMod(lRealFileName);
		if (lExt == ".itgz")
			return arch_Gzip::ContainsMod(lRealFileName);
		if (lExt == ".zip")
			return arch_Zip::ContainsMod(lRealFileName);
		if (lExt == ".rar")
			return arch_Rar::ContainsMod(lRealFileName);
		if (lExt == ".gz")
			return arch_Gzip::ContainsMod(lRealFileName);
		if (lExt == ".bz2")
			return arch_Bzip2::ContainsMod(lRealFileName);
	}
	
	return arch_Raw::ContainsMod(aFileName);
}
