/*
 * xSF - File structure
 * By Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]
 * Last modification on 2014-09-24
 *
 * Partially based on the vio*sf framework
 */

#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include <sstream>

class XSFFile
{
protected:
	uint8_t xSFType;
	bool hasFile;
	std::vector<uint8_t> rawData, reservedSection, programSection;
  std::map<std::string, std::string> tags;
	void ReadXSF(std::istream &xSF, uint32_t programSizeOffset, uint32_t programHeaderSize, bool readTagsOnly = false);
public:
	XSFFile();
	XSFFile(std::istream& xSF, uint32_t programSizeOffset = 0, uint32_t programHeaderSize = 0, bool readTagsOnly = false);
	bool IsValidType(uint8_t type) const;
	void Clear();
	bool HasFile() const;
	std::vector<uint8_t> &GetReservedSection();
	std::vector<uint8_t> GetReservedSection() const;
	std::vector<uint8_t> &GetProgramSection();
	std::vector<uint8_t> GetProgramSection() const;
	void SetTag(const std::string &name, const std::string &value);
	bool GetTagExists(const std::string &name) const;
	std::string GetTagValue(const std::string &name) const;
	template<typename T> T GetTagValue(const std::string &name, const T &defaultValue) const
	{
    T value = defaultValue;
    if (this->GetTagExists(name)) {
      std::istringstream ss(this->GetTagValue(name));
      ss >> value;
    }
    return value;
	}
	unsigned long GetLengthMS(unsigned long defaultLength) const;
	unsigned long GetFadeMS(unsigned long defaultFade) const;
	void SaveFile() const;
};

inline uint32_t Get32BitsLE(const uint8_t *input)
{
  return input[0] | (input[1] << 8) | (input[2] << 16) | (input[3] << 24);
}

inline uint32_t Get32BitsLE(std::istream &input)
{
  uint8_t bytes[4];
  input.read(reinterpret_cast<char *>(bytes), 4);
  return Get32BitsLE(bytes);
}
