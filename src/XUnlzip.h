////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// LZIP 포맷 래퍼 클래스
/// 
/// @author   park
/// @date     Wed Jul 25 17:57:45 2018
/// 
/// Copyright(C) 2018 Bandisoft, All rights reserved.
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////


#pragma once

#include "XStream.h"

/*
	LZIP 은 아래와 같은 스트림이 1번 이상 연속되어 저장됨
		+--+--+--+--+----+----+=============+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		| ID string | VN | DS | LZMA stream | CRC32 |   Data size   |  Member size  |
		+--+--+--+--+----+----+=============+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

#pragma pack(1)
struct SLzipHeader
{
	uint8_t	signature[4];
	uint8_t	version;
	uint8_t dicSize;
};
struct SLzipTail
{
	uint32_t crc32;
	uint64_t dataSize;		// size of the uncompressed data
	uint64_t memberSize;	// member size including header and trailer
};
#pragma pack()


class XUnlzip
{
public :
	XUnlzip();
	~XUnlzip();

	bool		Open(LPCWSTR filePathName);
	void		Close();
	bool		Extract(LPCWSTR outFilePathName);
	bool		Extract(XWriteStream& outStream);

private :
	bool		DecodeLzma(XWriteStream& out, uint32_t dicSize, uint32_t& crc32);
	bool		_Extract(XWriteStream& outStream);

private :
	XFileReadStream		m_in;

};

