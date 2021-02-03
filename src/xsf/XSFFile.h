/*
 * xSF - File structure
 * Copyright (c) 2014-2020 Naram Qashat <cyberbotx@cyberbotx.com>
 * Copyright (c) 2020-2021 Adam Higerd <chighland@gmail.com>
 *
 * Partially derived from Audio Overload SDK
 * Copyright (c) 2007-2008 R. Belmont and Richard Bannister.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the names of R. Belmont and Richard Bannister nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
