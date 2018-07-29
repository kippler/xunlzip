////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// LZMA 디코더
///
/// 참고
///		https://www.nongnu.org/lzip/manual/lzip_manual.html#Stream-format
///		https://www.7-zip.org/a/lzma-specification.7z
///		https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Markov_chain_algorithm
/// 
/// @author   park
/// @date     Wed Jul 25 18:50:43 2018
/// 
/// Copyright(C) 2018 Bandisoft, All rights reserved.
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////


#pragma once

#include "XStream.h"
#include "XRangeDec.h"

#define LZMA_STATES				12		// LZMA 의 상태 갯수
#define LITERAL_TREE_SIZE		0x300	// 8비트 리터럴에 대한 트리 접근(0xff) + match 비트 하나 + 공백(?) 비트 하나 
#define LEN_TO_SLOT_STATE		4		// ==kNumLenToPosStates len 에서 slot 으로 접근할때 사용할 문맥 갯수
#define DIST_SLOT_BITS			6		// DIST SLOT 은 6비트

#define kMatchMinLen			2		// LEN 최소값
#define kEndPosModelIndex		14
#define kNumFullDistances		(1 << (kEndPosModelIndex >> 1))
#define kNumAlignBits			4
#define LEN_DIST_SPECIAL		(1 + kNumFullDistances - kEndPosModelIndex)		// == 115 . DIST 문서에서 XXXX 혹은 YYYY 부분.
#define LEN_DIST_ALIGN			(1<<kNumAlignBits)								// == 16


// LZMA 상태 클래스. LZMA 는 12가지 상태를 가진다.
struct XLzmaState
{
	XLzmaState()
	{
		state = 0;		// 초기상태는 0 이다.
	}
	void Init() { state = 0; }
	operator int() const { return state; }
	int state;
	void	LIT()		// 상태 바꾸기
	{
		static const int next[LZMA_STATES] = { 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 4, 5 };
		state = next[state];
	}
	void	SHORTREP() { state = (state < 7) ? 9 : 11; }
	void	LONGREP() { state = (state < 7) ? 8 : 11; }
	void	MATCH() { state = (state < 7) ? 7 : 10; }
	bool	is_char() const { return state < 7; }
};


struct XLzmaProp
{
	XLzmaProp()
	{
		// lc is the number of high bits of the previous byte to use as a context for literal encoding (the default value used by the LZMA SDK is 3)
		// lp is the number of low bits of the dictionary position to include in literal_pos_state(the default value used by the LZMA SDK is 0)
		// pb is the number of low bits of the dictionary position to include in pos_state (the default value used by the LZMA SDK is 2)
		lc = 3;		
		lp = 0;
		pb = 2;
	}
	int lc;			// LZIP 은 lc 를 3 으로 고정해서 쓰고 있다. == literal_context_bits
	int lp;
	int pb;			// LZIP 은 PB 를 2 로 고정
};


class XLzmaDec
{
public :
	XLzmaDec();
	~XLzmaDec();

	bool			Init(XReadStream* in, XWriteStream* out, uint32_t dicSize, const XLzmaProp& prop);
	bool			Decode();
	uint32_t		CRC32() { return m_crc32; }

private :
	void			Free();
	uint8_t			Peek(const unsigned int distance) const;
	bool			Write(uint8_t c);
	bool			Flush();
	bool			DecodeLiteral();
	bool			DecodeShortRepeat();
	bool			DecodeMatch(unsigned posState);
	bool			DecodeLongRepeat(unsigned posState);
	uint64_t		OutSize();

	enum { PB = 2 };							// pos_state 관련 비트수. 일단 상수로 처리하자.

private :
	XWriteStream*	m_out;						// 출력 스트림
	uint64_t		m_totOut;

private :
	XLzmaState		m_state;					// 현재 상태
	XRangeDec		m_rdec;						// 레인지 디코더. 모든 스트림은 레인지 디코더를 통과해서 LZ77에 전달된다.
	bool			m_eos;						// 스트림 끝인가?
	bool			m_isBroken;					// 손상된 스트림인가?

private:										// [확률 문맥]
	XProb*			m_probIsMatch;				// MATCH 여부 컨텍스트. 1: match 이다. 0: literal 이다.
	XProb*			m_probLiteral;				// 리터럴에 대한 컨텍스트 - 런타임에 크기가 결정되기 때문에 포인터

	XProb			m_probDistSlot[LEN_TO_SLOT_STATE][1 << DIST_SLOT_BITS];	// DIST SLOT 문맥 len 값으로 접근한다.
	XProb			m_probDistSpecial[LEN_DIST_SPECIAL];		// 스펙 문서의 DIST 부분에서 xxxx 혹은 YYYY 부분
	XProb			m_probDistAlign[LEN_DIST_ALIGN];			// 스펙 문서의 DIST 부분에서 ZZZZ 부분

	XProb			m_probRep[LZMA_STATES];		// 두번째 비트 - 0이면 match, 1 이면 rep
	XProb			m_probRep0[LZMA_STATES];	// 세번째 비트 - rep0 여부
	XProb			m_probRep1[LZMA_STATES];	// 네번째 비트
	XProb			m_probRep2[LZMA_STATES];	// 다섯번째 비트
	XLenProb		m_probRepLen;				// 길이 처리 prob
	XLenProb		m_probMatchLen;				// match 의 len 처리

	XProb			m_probRep0Long[LZMA_STATES][1<<PB];// after bit sequence 110 - LZIP 에서는 bm_len

private :										// [거리]
	uint32_t		m_rep0;
	uint32_t		m_rep1;
	uint32_t		m_rep2;
	uint32_t		m_rep3;

private :
	XLzmaProp		m_prop;						// LZMA 속성
	uint32_t		m_dicSize;					// 설정된 사전 크기
	uint8_t*		m_dic;						// 사전
	uint32_t		m_dicPos;					// m_dic 배열에 대한 현재 "쓸" 위치
	uint32_t		m_streamPos;				// m_dic 배열에서 아직 출력 버퍼로 쓰지 않은 첫번째 위치
	uint32_t		m_crc32;

#ifdef _DEBUG
	//XBuffer			m_compareBuf;				// 비교 버퍼
	//int64_t			m_comparePos;
#endif
};


