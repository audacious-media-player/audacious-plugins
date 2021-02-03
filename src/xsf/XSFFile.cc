/*
 * xSF - File structure
 * By Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]
 * Last modification on 2014-09-24
 *
 * Partially based on the vio*sf framework
 */

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <zlib.h>
#include "XSFFile.h"

static inline void Set32BitsLE(uint32_t input, uint8_t *output)
{
	output[0] = input & 0xFF;
	output[1] = (input >> 8) & 0xFF;
	output[2] = (input >> 16) & 0xFF;
	output[3] = (input >> 24) & 0xFF;
}

// The whitespace trimming was modified from the following answer on Stack Overflow:
// http://stackoverflow.com/a/217605

struct IsWhitespace : std::unary_function<char, bool>
{
	bool operator()(const char &x) const
	{
		return x >= 0x01 && x <= 0x20;
	}
};

static inline std::string LeftTrimWhitespace(const std::string &orig)
{
	auto first_non_space = std::find_if(orig.begin(), orig.end(), std::not1(IsWhitespace()));
	return std::string(first_non_space, orig.end());
}

static inline std::string RightTrimWhitespace(const std::string &orig)
{
	auto last_non_space = std::find_if(orig.rbegin(), orig.rend(), std::not1(IsWhitespace())).base();
	return std::string(orig.begin(), last_non_space);
}

static inline std::string TrimWhitespace(const std::string &orig)
{
	return LeftTrimWhitespace(RightTrimWhitespace(orig));
}

XSFFile::XSFFile() : xSFType(0), hasFile(false), rawData(), reservedSection(), programSection(), tags()
{
}

XSFFile::XSFFile(std::istream &inFile, uint32_t programSizeOffset, uint32_t programHeaderSize, bool readTagsOnly) : xSFType(0), hasFile(true), rawData(), reservedSection(), programSection(), tags()
{
	this->ReadXSF(inFile, programSizeOffset, programHeaderSize, readTagsOnly);
}

void XSFFile::ReadXSF(std::istream &xSF, uint32_t programSizeOffset, uint32_t programHeaderSize, bool readTagsOnly)
{
	xSF.seekg(0, std::istream::end);
	auto filesize = xSF.tellg();
	xSF.seekg(0, std::istream::beg);

	if (filesize < 4)
		throw std::runtime_error("File is too small.");

	char PSFHeader[4];
	xSF.read(PSFHeader, 4);

	if (PSFHeader[0] != 'P' || PSFHeader[1] != 'S' || PSFHeader[2] != 'F')
		throw std::runtime_error("Not a PSF file.");

	this->xSFType = PSFHeader[3];

	this->rawData.resize(4);
  std::memcpy(&this->rawData[0], PSFHeader, 4);

	if (filesize < 16)
		throw std::runtime_error("File is too small.");

	uint32_t reservedSize = Get32BitsLE(xSF), programCompressedSize = Get32BitsLE(xSF);
	this->rawData.resize(reservedSize + programCompressedSize + 16);
	Set32BitsLE(reservedSize, &this->rawData[4]);
	Set32BitsLE(programCompressedSize, &this->rawData[8]);
	xSF.read(reinterpret_cast<char *>(&this->rawData[12]), 4);

	if (reservedSize)
	{
		if (filesize < reservedSize + 16)
			throw std::runtime_error("File is too small.");

		if (readTagsOnly)
			xSF.read(reinterpret_cast<char *>(&this->rawData[16]), reservedSize);
		else
		{
			this->reservedSection.resize(reservedSize);
			xSF.read(reinterpret_cast<char *>(&this->reservedSection[0]), reservedSize);
      std::memcpy(&this->rawData[16], &this->reservedSection[0], reservedSize);
		}
	}

	if (programCompressedSize)
	{
		if (filesize < reservedSize + programCompressedSize + 16)
			throw std::runtime_error("File is too small.");

		if (readTagsOnly)
			xSF.read(reinterpret_cast<char *>(&this->rawData[reservedSize + 16]), programCompressedSize);
		else
		{
			auto programSectionCompressed = std::vector<uint8_t>(programCompressedSize);
			xSF.read(reinterpret_cast<char *>(&programSectionCompressed[0]), programCompressedSize);
      std::memcpy(&this->rawData[reservedSize + 16], &programSectionCompressed[0], programCompressedSize);

			auto programSectionUncompressed = std::vector<uint8_t>(programHeaderSize);
			unsigned long programUncompressedSize = programHeaderSize;
			uncompress(&programSectionUncompressed[0], &programUncompressedSize, &programSectionCompressed[0], programCompressedSize);
      if (programUncompressedSize) {
        programUncompressedSize = Get32BitsLE(&programSectionUncompressed[programSizeOffset]) + programHeaderSize;
      }
			this->programSection.resize(programUncompressedSize);
			uncompress(&this->programSection[0], &programUncompressedSize, &programSectionCompressed[0], programCompressedSize);
		}
	}

	if (xSF.tellg() != filesize && filesize >= reservedSize + programCompressedSize + 21)
	{
		char tagheader[6] = "";
		xSF.read(tagheader, 5);
		if (std::string(tagheader) == "[TAG]")
		{
			auto startOfTags = xSF.tellg();
			unsigned lengthOfTags = static_cast<unsigned>(filesize - startOfTags);
			if (lengthOfTags)
			{
				auto rawtags = std::vector<char>(lengthOfTags);
				xSF.read(&rawtags[0], lengthOfTags);
				std::string name, value;
				bool onName = true;
				for (unsigned x = 0; x < lengthOfTags; ++x)
				{
					char curr = rawtags[x];
					if (curr == 0x0A)
					{
						if (!name.empty() && !value.empty())
						{
							name = TrimWhitespace(name);
							value = TrimWhitespace(value);
							if (this->tags.find(name) != this->tags.end())
								this->tags[name] += "\n" + value;
							else
								this->tags[name] = value;
						}
						name = value = "";
						onName = true;
						continue;
					}
					if (curr == '=')
					{
						onName = false;
						continue;
					}
					if (onName)
						name += curr;
					else
						value += curr;
				}
			}
		}
	}

	this->hasFile = true;
}

bool XSFFile::IsValidType(uint8_t type) const
{
	return this->xSFType == type;
}

void XSFFile::Clear()
{
	this->xSFType = 0;
	this->hasFile = false;
	this->reservedSection.clear();
	this->programSection.clear();
	this->tags.clear();
}

bool XSFFile::HasFile() const
{
	return this->hasFile;
}

std::vector<uint8_t> &XSFFile::GetReservedSection()
{
	return this->reservedSection;
}

std::vector<uint8_t> XSFFile::GetReservedSection() const
{
	return this->reservedSection;
}

std::vector<uint8_t> &XSFFile::GetProgramSection()
{
	return this->programSection;
}

std::vector<uint8_t> XSFFile::GetProgramSection() const
{
	return this->programSection;
}

void XSFFile::SetTag(const std::string &name, const std::string &value)
{
	this->tags[name] = value;
}

bool XSFFile::GetTagExists(const std::string &name) const
{
	return this->tags.find(name) != this->tags.end();
}

std::string XSFFile::GetTagValue(const std::string &name) const
{
	return this->GetTagExists(name) ? this->tags.at(name) : "";
}

static unsigned long StringToMS(const std::string& value, unsigned long defaultLength)
{
	double length = 0;
	if (!value.empty()) {
    std::istringstream ss(value);
    double part = 0;
    do {
      ss >> part;
      length = length * 60 + part;
    } while (ss.get() == ':' && ss);
  }
	if (!length)
		return defaultLength;
	return length * 1000;
}

unsigned long XSFFile::GetLengthMS(unsigned long defaultLength) const
{
	std::string value = this->GetTagValue("length");
  return StringToMS(value, defaultLength);
}

unsigned long XSFFile::GetFadeMS(unsigned long defaultFade) const
{
	std::string value = this->GetTagValue("fade");
  return StringToMS(value, defaultFade);
}

void XSFFile::SaveFile() const
{
  /*
	std::ofstream xSF;
	xSF.exceptions(std::ofstream::failbit);
#ifdef _WIN32
	xSF.open(ConvertFuncs::StringToWString(this->fileName).c_str(), std::ofstream::out | std::ofstream::binary);
#else
	xSF.open(this->fileName.c_str(), std::ofstream::out | std::ofstream::binary);
#endif

	xSF.write(reinterpret_cast<const char *>(&this->rawData[0]), this->rawData.size());

	auto allTags = this->tags.GetTags();
	if (!allTags.empty())
	{
		xSF.write("[TAG]", 5);
		std::for_each(allTags.begin(), allTags.end(), [&](const std::string &tag)
		{
			xSF.write(tag.c_str(), tag.length());
			xSF.write("\n", 1);
		});
	}
  */
}
