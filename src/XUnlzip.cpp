#include "stdafx.h"
#include "XUnlzip.h"
#include "XLzmaDec.h"
#include "Global.h"


XUnlzip::XUnlzip()
{
}


XUnlzip::~XUnlzip()
{
}


bool XUnlzip::Open(LPCWSTR filePathName)
{
	Close();

	if (m_in.Open(filePathName) == FALSE)
	{
		ASSERT(0);
		return false;
	}

	return true;
}

void XUnlzip::Close()
{
	m_in.Close();
}


bool XUnlzip::Extract(LPCWSTR outFilePathName)
{
	// 파일 열기
	XFileWriteStream out;
	if (out.Open(outFilePathName) == FALSE)
	{
		// can't open output file
		ASSERT(0);
		return false;
	}

	return Extract(out);
}

bool XUnlzip::Extract(XWriteStream& outStream)
{
	bool ret = _Extract(outStream);
	if (ret == false)
		return false;

	// 연속 스트림?
	while(  (INT64)(m_in.GetPos() + sizeof(SLzipHeader) + sizeof(SLzipTail)) < m_in.GetSize())
	{
//#ifdef _DEBUG
//		wprintf(L"next stream\n");
//#endif
		ret = _Extract(outStream);
		if (ret == false)
			return false;
	}

	return true;
}



bool XUnlzip::_Extract(XWriteStream& outStream)
{
	// 시작 위치
	const int64_t		offsetStart = m_in.GetPos();
	const int64_t		outSizeCur = outStream.GetSize();

	SLzipHeader	header;
	if (m_in.Read_((LPVOID)&header, sizeof(header)) == FALSE)
	{
		ASSERT(0);
		return false;
	}

	if (memcmp(header.signature, "LZIP", 4) != 0)
	{
		// invalid signature
		ASSERT(0);
		return false;
	}

	if(header.version !=0x01)
	{
		// invalid version
		ASSERT(0);
		return false;
	}

	enum
	{
		min_dictionary_size = 1 << 12,
		max_dictionary_size = 1 << 29,
	};

	// 사전 크기 구하기
	// The dictionary size is calculated by taking a power of 2 (the base size) and substracting from it a fraction between 0/16 and 7/16 of the base size.
	// Bits 4-0 contain the base 2 logarithm of the base size (12 to 29).
	// Bits 7-5 contain the numerator of the fraction (0 to 7) to substract from the base size to obtain the dictionary size.
	uint32_t dicSize = 1 << (header.dicSize & 0x1F);
	dicSize -= (dicSize / 16) * ((header.dicSize >> 5) & 7);
	if (dicSize < min_dictionary_size || dicSize > max_dictionary_size)
	{
		// invalid dic size
		ASSERT(0);
		return false;
	}

	uint32_t crc32 = 0;
	const bool ret = DecodeLzma(outStream, dicSize, crc32);
	if (ret == false)
		return false;

	// 꼬리 읽기
	SLzipTail tail;
	if (m_in.Read_(&tail, sizeof(tail)) == FALSE)
	{
		// can't read tail
		ASSERT(0);
		return false;
	}

	if (tail.crc32 != crc32)
	{
		// invalid crc
		ASSERT(0);
		return false;
	}

	// 끝 위치
	const int64_t		offsetEnd = m_in.GetPos();
	const int64_t		streamSize = offsetEnd - offsetStart;

	if ((int64_t)tail.memberSize != streamSize)
	{
		// invalid size
		ASSERT(0);
		return false;
	}

	if (tail.dataSize != (uint64_t)outStream.GetSize()- outSizeCur)
	{
		// invalid uncompressed size
		ASSERT(0);
		return false;
	}

	return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         압축 해제
/// @param  
/// @return 
/// @date   Wed Jul 25 18:58:19 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
bool XUnlzip::DecodeLzma(XWriteStream& out, uint32_t dicSize, uint32_t& crc32)
{
	XLzmaDec lzmaDec;
	const XLzmaProp prop;

	if (lzmaDec.Init(&m_in, &out, dicSize, prop) == false)
	{
		// stream init failed
		ASSERT(0);
		return false;
	}

	const bool ret = lzmaDec.Decode();

	if(ret)
		crc32 = lzmaDec.CRC32();

	return ret;
}


