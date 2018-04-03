#pragma once

// We want to use the asiosocketmanager for communications, but wrap it in a class derived from a "general" comms interface class.This way can add serial later if necessary.
// Also this will allow us to more easily mock the comms layer for unit testing.

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>
#include <sstream>

#include "MD3.h"
#include "CRC.h"

#define EOMBIT 0x40
#define FOMBIT 0x80
#define APLBIT 0x0080
#define RSFBIT 0x0040
#define HCPBIT 0x0020
#define DCPBIT 0x0010
#define DIRECTIONBIT 0x80000000

const int MD3BlockArraySize = 6;
typedef  std::array<uint8_t, MD3BlockArraySize> MD3BlockArray;

uint8_t MD3CRC(const uint32_t data);
bool MD3CRCCompare(const uint8_t crc1, const uint8_t crc2);


// Every block in MD3 is 5 bytes, plus one zero byte. If we dont have 5 bytes, we dont have a block. The last block is marked as such.
// We will create a class to load the 6 byte array into, then we can just ask it to return the information in the variety of ways we require,
// depending on the block content.
// There are two ways this class is used - one for decoding, one for encoding.
// Create child classes for each of the functions where the data block has specific layout and meaning.
class MD3Block
{
public:
	// We have received 6 bytes (a block on a stream now we need to decode it)
	MD3Block(const MD3BlockArray _data)
	{
		data = _data[0] << 24 | _data[1] << 16 | _data[2] << 8 | _data[3];
		endbyte = _data[4];
		assert(_data[5] == 0x00);	// Sixth byte should always be zero.
	}

	// Create a formatted block including checksum
	// Note if the station address is set to 0x7F (MD3_EXTENDED_ADDRESS_MARKER), then the next data block contains the address.
	MD3Block(uint8_t stationaddress, bool mastertostation, MD3_FUNCTION_CODE functioncode, uint8_t moduleaddress, uint8_t channels, bool lastblock = false,
		bool APL = false, bool RSF = false, bool HCP = false, bool DCP = false)
	{
		// Channels -> 0 on the wire == 1 channel, 15 == 16
		channels--;

		uint32_t direction = mastertostation ? 0x0000 : DIRECTIONBIT;

		assert((stationaddress & 0x7F) == stationaddress);	// Max of 7 bits;
		assert((channels & 0x0F) == channels);	// Max 4 bits;

		uint32_t flags = 0;
		flags |= APL ? APLBIT : 0x0000;
		flags |= RSF ? RSFBIT : 0x0000;
		flags |= HCP ? HCPBIT : 0x0000;
		flags |= DCP ? DCPBIT : 0x0000;

		data = direction | (uint32_t)stationaddress << 24 | (uint32_t)functioncode << 16 | (uint32_t)moduleaddress << 8 | flags | channels;

		endbyte = MD3CRC(data);	// Max 6 bits returned

		// endbyte |= FOMBIT;	// This is a formatted block must be zero
		endbyte |= lastblock ? EOMBIT : 0x00;
	}

	MD3Block(uint16_t firstword, uint16_t secondword, bool lastblock = false)
	{
		data = (uint32_t)firstword << 16 | (uint32_t)secondword;

		endbyte = MD3CRC(data);	// Max 6 bits returned

		endbyte |= FOMBIT;	// NOT a formatted block, must be 1
		endbyte |= lastblock ? EOMBIT : 0x00;
	}
	MD3Block(char b1, char b2, char b3, char b4, bool lastblock = false)
	{
		data = (((uint32_t)b1 &0x0FF) << 24) | (((uint32_t)b2 & 0x0FF) << 16) | (((uint32_t)b3 & 0x0FF) << 8) | ((uint32_t)b4 & 0x0FF);

		endbyte = MD3CRC(data);	// Max 6 bits returned

		endbyte |= FOMBIT;	// NOT a formatted block, must be 1
		endbyte |= lastblock ? EOMBIT : 0x00;
	}

	MD3Block(uint32_t _data, bool lastblock = false)
	{
		data = _data;

		endbyte = MD3CRC(data);	// Max 6 bits returned

		endbyte |= FOMBIT;	// NOT a formatted block must be 1
		endbyte |= lastblock ? EOMBIT : 0x00;
	}

	bool IsEndOfMessageBlock()
	{
		return ((endbyte & EOMBIT) == EOMBIT);
	}
	void MarkAsEndOfMessageBlock()
	{
		endbyte |= EOMBIT;	// Does not change CRC
	}
	bool IsFormattedBlock()
	{
		return !((endbyte & FOMBIT) == FOMBIT);	// 0 is Formatted, 1 is Unformatted
	}

	// Apply to formatted blocks only!
	bool IsMasterToStationMessage()
	{
		return !((data & DIRECTIONBIT) == DIRECTIONBIT);
	}
	uint8_t GetStationAddress()
	{
		// TODO: SJE Extended Addressing - Address 0x7F indicates an extended address.
		return (data >> 24) & 0x7F;
	}
	uint8_t GetModuleAddress()
	{
		return (data >> 8) & 0xFF;
	}
	uint8_t GetFunctionCode()
	{
		// How do we check it is a valid function code first?
		// There are a few complex ways to check the values of an enum - mostly by creating a new class.
		// As we are likely to have some kind of function dispatcher class anyway - dont check here.
		return (data >> 16) & 0xFF;
	}
	uint8_t GetChannels()
	{
		// 0 on the wire is 1 channel
		return (data & 0x0F)+1;
	}
	bool GetAPL()
	{
		return ((data & APLBIT) == APLBIT);
	}
	bool GetRSF()
	{
		return ((data & RSFBIT) == RSFBIT);
	}
	bool GetHCP()
	{
		return ((data & HCPBIT) == HCPBIT);
	}
	bool GetDCP()
	{
		return ((data & DCPBIT) == DCPBIT);
	}

	bool CheckSumPasses()
	{
		uint8_t calc = MD3CRC(data);
		return MD3CRCCompare(calc, endbyte);
	}

	uint16_t GetFirstWord()
	{
		return (data >> 16);
	}
	uint16_t GetSecondWord()
	{
		return (data & 0xFFFF);
	}
	// The extended address would be retrived from the correct block with this call.
	uint32_t GetBlockData()
	{
		return data;
	}
	std::string ToBinaryString()
	{
		std::ostringstream oss;

		oss.put(data >> 24);
		oss.put((data >> 16) & 0x0FF);
		oss.put((data >> 8) & 0x0FF);
		oss.put(data & 0x0FF);
		oss.put(endbyte);
		oss.put(0x00);

		return oss.str();
	}
	std::string ToString()
	{
		std::ostringstream oss;

		oss << (data >> 24);
		oss << ',';
		oss << ((data >> 16) & 0x0FF);
		oss << ',';
		oss << ((data >> 8) & 0x0FF);
		oss << ',';
		oss << (data & 0x0FF);
		oss << ',';
		oss << (endbyte);
		oss << ',';
		oss << (0x00);
		return oss.str();
	}

private:
	uint32_t data = 0;
	uint8_t endbyte = 0;
};

