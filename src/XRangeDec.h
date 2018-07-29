////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Markov_chain_algorithm#Range_coding_of_bits
/// 
/// @author   park
/// @date     Wed Jul 25 14:48:04 2018
/// 
/// Copyright(C) 2018 Bandisoft, All rights reserved.
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////


#pragma once

#include "XStream.h"


// 비트가 0 일 확률 데이타
// 2^11 이 최대값이며 0.5 에 해당하는 중간값은 2^10(1024)
struct XProb
{
	XProb()
	{
		prob = (1 << 10);
	}
	void Init() {prob = (1 << 10);}
	int& operator()() { return prob; }
	int prob;
};



/*

The following scheme is used for the match length encoding:

Binary encoding    Binary Tree structure    Zero-based match length
sequence                                    (binary + decimal):

0 xxx              LowCoder[posState]       xxx
1 0 yyy            MidCoder[posState]       yyy + 8
1 1 zzzzzzzz       HighCoder                zzzzzzzz + 16

*/


// 길이 관련 확률 데이타
struct XLenProb
{
	// prop.pb 에 따라서 동적으로 처리해야 하지만, 일단 귀찮으니까 정적으로 처리하자.
	enum {PB =2};
	enum { LOW_BITS = 3 };
	enum { MID_BITS = 3 };
	enum { HIGH_BITS = 8 };
	XProb	choice;
	XProb	choice2;
	XProb	low[1 << PB][1 << LOW_BITS];	// 0 xxx 형태
	XProb	mid[1 << PB][1 << MID_BITS];	// 1 0 yyy 형태
	XProb	high[1 << HIGH_BITS];			// 1 1 zzzzzzzz 형태 . 1차원 배열이다.

	void Init()
	{
		choice.Init();
		choice2.Init();
		for (int i = 0; i < (1 << PB); i++)
		{
			for (int j = 0; j < (1 << LOW_BITS); j++) low[i][j].Init();
			for (int j = 0; j < (1 << MID_BITS); j++) mid[i][j].Init();
		}
		for (int j = 0; j < (1 << HIGH_BITS); j++) high[j].Init();
	}
};


class XRangeDec
{
public :
	XRangeDec();
	~XRangeDec();
	bool		StreamInit(XReadStream* in);
	bool		DecodeBit(XProb& prob, uint32_t& bit);
	bool		DecodeTree(XProb* prob, uint32_t& symbol, int bitsLen);
	bool		DecodeTreeRev(XProb* prob, uint32_t& symbol, int bitsLen);
	bool		DecodeDirect(uint32_t& symbol, int bitsLen);
	bool		DecodeMatched(XProb* prob, uint32_t& symbol, const unsigned matchByte);
	bool		DecodeLen(XLenProb& prob, int pos_state, uint32_t& len);

private:
	bool		Normalize();
	bool		GetByte(uint8_t& b);

private :
	uint32_t		code;		// (representing the encoded point within the range)
	uint32_t		range;		// (representing the range size)
	XReadStream*	m_in;
};

