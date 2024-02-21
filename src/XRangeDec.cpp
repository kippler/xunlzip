#include "stdafx.h"
#include "XRangeDec.h"
#include "_private.h"

#pragma warning(disable: 26429)


XRangeDec::XRangeDec()
{
	range = 0xffffffff;
	code = 0;
	m_in = NULL;
}

bool XRangeDec::StreamInit(XReadStream* in)
{
	m_in = in;

	uint8_t zero;
	if (GetByte(zero) == false)
	{
		ASSERT(0);
		return false;
	}
	// lzma-specification: If initial byte is not equal to ZERO, the LZMA Decoder must stop decoding and report error.
	if (zero != 0)
	{
		ASSERT(0);
		return false;
	}


	for (int i = 0; i < 4; i++)
	{
		if (Normalize() == false)
		{
			ASSERT(0);
			return false;
		}
	}
	range = 0xffffffff;

	return true;
}


XRangeDec::~XRangeDec()
{
}



// Normalization proceeds in this way:
bool XRangeDec::Normalize()
{
	// Shift both range and code left by 8 bits
	range = range << 8;
	code = code << 8;

	// Read a byte from the compressed stream
	// Set the least significant 8 bits of code to the byte value read
	uint8_t b;
	if (GetByte(b) == false)
		return false;

	code = code + b;
	return true;
}


bool XRangeDec::GetByte(uint8_t& b)
{
	if (m_in->Read_(&b, 1) == FALSE)
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///         한번에 여러 비트 디코드
/// @param  
/// @return 
/// @date   Thu Jul 26 15:01:24 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
위키 백과
	The range decoder also provides the bit-tree, reverse bit-tree and fixed probability integer decoding facilities, 
	which are used to decode integers, and generalize the single-bit decoding described above. 
	To decode unsigned integers less than limit, an array of (limit − 1) 11-bit probability variables is provided, 
	which are conceptually arranged as the internal nodes of a complete binary tree with limit leaves.

	Non-reverse bit-tree decoding works by keeping a pointer to the tree of variables, which starts at the root. 
	As long as the pointer doesn't point to a leaf, a bit is decoded using the variable indicated by the pointer, 
	and the pointer is moved to either the left or right children depending on whether the bit is 0 or 1; 
	when the pointer points to a leaf, the number associated with the leaf is returned.

 LZMA SPEC

	LZMA uses a tree of Bit model variables to decode symbol that needs
	several bits for storing. There are two versions of such trees in LZMA:
	1) the tree that decodes bits from high bit to low bit (the normal scheme).
	2) the tree that decodes bits from low bit to high bit (the reverse scheme).

	Each binary tree structure supports different size of decoded symbol
	(the size of binary sequence that contains value of symbol).
	If that size of decoded symbol is "NumBits" bits, the tree structure
	uses the array of (2 << NumBits) counters of CProb type.
	But only ((2 << NumBits) - 1) items are used by encoder and decoder.
	The first item (the item with index equal to 0) in array is unused.
	That scheme with unused array's item allows to simplify the code.

	리터럴은 아무래도 비슷한게 나오다 보니, MSB가 같으면, 하위 비트도 비슷할 확률이 높으므로
	MSB 부터 시작을 해서 만들어진 심볼을 기준으로 다음 비트에 접근하면 동일한 확률이 높아져서
	이렇게 설계한듯.

	반대로 integer 는 LSB 에 1 이 많고, MSB 에는 주로 0 이기 때문인지, REVERSE 하게 접근한다.

	아래 DecodeTree() 는 리터럴을 가져올때만 호출되고, 그래서 LZMA SPEC 문서의 DecodeLiteral() 의 일부와 중복된다.

*/

bool XRangeDec::DecodeTree(XProb* prob, uint32_t& symbol, int bitsLen)
{
	// 그냥 0 으로 해도 효율에는 차이 없을거 같은데 , SDK 에 그냥 1로 되어 있다. 따라하자.
	symbol = 1;

	uint32_t bit = 0;

	// 지정된 비트만큼
	for (int i = 0; i < bitsLen; i++)
	{ 
		// 변경된 심볼값에 따라 prob 의 선택도 바뀐다.
		if (DecodeBit(prob[symbol], bit) == false)
			ERROR_RETURN;

		// 비트를 순서대로 가져온다. - 하위에 비트 추가
		symbol = (symbol << 1) + bit;
	}

	// 맨 처음에 1로 세팅했팅하고 시프트된 값 다시 빼주기
	symbol = symbol - (1<< bitsLen);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///         트리 가져와서 뒤집기
/// @param  
/// @return 
/// @date   Fri Jul 27 13:44:21 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
bool XRangeDec::DecodeTreeRev(XProb* prob, uint32_t& symbol, int bitsLen)
{
	uint32_t revSymbol = 0;
	if (DecodeTree(prob, revSymbol, bitsLen) == false)
		return false;

	// 뒤집기
	symbol = 0;
	for (int i = 0; i < bitsLen; ++i)
	{
		symbol = (symbol << 1) | (revSymbol & 1);
		revSymbol >>= 1;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///         다이렉트 디코딩 - 고정확률 디코딩
///			서로 연관성이 없어서 압축이 불가능한 데이터는 이걸로 처리하는듯하다.
/// @param  
/// @return 
/// @date   Fri Jul 27 14:24:08 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
bool XRangeDec::DecodeDirect(uint32_t& symbol, int bitsLen)
{							
	symbol = 0;
	for (int i = bitsLen; i > 0; --i)
	{
		range >>= 1;
		symbol <<= 1;
		if (code >= range) { code -= range; symbol |= 1; }

		if (range <= 0x00FFFFFFU)			// normalize
			Normalize();
	}
	return true;
}


/*

스펙 문서에는 이렇게 설명되어 있음

	If (State > 7), the Literal Decoder also uses "matchByte" that represents
	the byte in OutputStream at position the is the DISTANCE bytes before
	current position, where the DISTANCE is the distance in DISTANCE-LENGTH pair
	of latest decoded match.

	if (state >= 7)
	{
		unsigned matchByte = OutWindow.GetByte(rep0 + 1);
		do
		{
			unsigned matchBit = (matchByte >> 7) & 1;
			matchByte <<= 1;
			unsigned bit = RangeDec.DecodeBit(&probs[((1 + matchBit) << 8) + symbol]);
			symbol = (symbol << 1) | bit;
			if (matchBit != bit)
			break;
		}
		while (symbol < 0x100);
	}
	while (symbol < 0x100)
		symbol = (symbol << 1) | RangeDec.DecodeBit(&probs[symbol]);

LZIP 문서에는

	The context array 'bm_literal' is special. 
	In principle it acts as a normal bit tree context, the one selected by 'literal_state'. 

	But if the previous decoded byte was not a literal, two other bit tree contexts are used 
	depending on the value of each bit in 'match_byte' (the byte at the latest used distance), 
	until a bit is decoded that is different from its corresponding bit in 'match_byte'. 
	After the first difference is found, the rest of the byte is decoded using the normal bit tree context. 
	(See 'decode_matched' in the source).

	<해석하자면>
	리터럴을 디코딩 할때, 이전 상태가 리터럴이 아닌 경우, (==이전 상태가 리핏)
	'match_byte'(최근 "출력"한 바이트가 아니라, 최근 repeat 을 "시작"하면서 출력한 바이트)
	의 각 비트 값에 따라 두 개의 다른 비트 트리 컨텍스트가 사용되며 해당 비트와는 
	다른 비트가 디코딩 될 때까지 사용됩니다
	첫 번째 차이가 발견 된 후 나머지 바이트는 일반 비트 트리 컨텍스트를 사용하여 디코딩됩니다.

	<즉>
	최근 출력한 바이트 보다는, 좀전에 리핏할때 처음 쓴 바이트가 조금 더 연관성이 높다고 생각한듯?

*/

bool XRangeDec::DecodeMatched(XProb* prob, uint32_t& symbol, const unsigned matchByte)
{
	// 심볼 초기화
	symbol = 1;
	unsigned bit = 0;

	for (int i = 7; i >= 0; i--)
	{
		// matchByte 에서 비트 하나씩 뽑고
		const unsigned matchBit = (matchByte >> i) & 1;

		// <이 부분이 중요>
		// 
		// "심볼과 matchBit 를 조합한" 컨텍스트를 사용해서 비트 하나씩 디코딩해서
		if (DecodeBit(prob[((1 + matchBit) << 8) + symbol], bit) == false)
			ERROR_RETURN;

		// 심볼에 저장하고
		symbol = (symbol << 1) | bit;

		// match 바이트와 첫번째 차이가 발견된 경우
		if (matchBit != bit)
		{
			// 나머지는 일반 컨텍스트로 디코딩
			while (symbol < 0x100)
			{
				if (DecodeBit(prob[symbol], bit) == false)
					ERROR_RETURN;

				symbol = (symbol << 1) | bit;
			}
			break;
		}
	}


	// 맨 처음에 1로 세팅했팅하고 시프트된 값 다시 빼주기
	symbol = symbol & 0xff;

	return true;
}



// Context-based range decoding of a bit using the prob probability variable proceeds in this way:
bool XRangeDec::DecodeBit(XProb& prob, uint32_t& bit)
{

	// Set bound to floor(range / 2 ^ 11) * prob
	const uint32_t bound = (range / (1 << 11)) * prob.prob;

	// If code is less than bound:
	if (code < bound)
	{
		// Set range to bound
		range = bound;

		// Set prob to prob + floor((2^11 - prob) / 2^5)
		prob.prob = prob.prob + ((1 << 11) - prob.prob) / (1 << 5);

		// Return bit 0
		bit = 0;
	}
	// Otherwise (if code is greater than or equal to the bound):
	else
	{
		// Set range to range - bound
		range = range - bound;
		
		// Set code to code - bound
		code = code - bound;

		// Set prob to prob - floor(prob / 2^5)
		prob.prob = prob.prob - (prob.prob / (1 << 5));

		// Return bit 1
		bit = 1;
	}

	// If range is less than 2^24, perform normalization
	if (range < (1 << 24))
	{
		if (Normalize() == false)
			return false;
	}


	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
///         LEN 데이터를 디코딩한다.
/// @param  
/// @return 
/// @date   Thu Jul 26 17:39:53 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
/*

== SDK 스펙 曰 ==

The following scheme is used for the match length encoding:

Binary encoding    Binary Tree structure    Zero-based match length
sequence                                    (binary + decimal):

0 xxx              LowCoder[posState]       xxx
1 0 yyy            MidCoder[posState]       yyy + 8
1 1 zzzzzzzz       HighCoder                zzzzzzzz + 16

*/
bool XRangeDec::DecodeLen(XLenProb& prob, int pos_state, uint32_t& len)
{
	uint32_t bit = 0;

	if (DecodeBit(prob.choice, bit) == false) ERROR_RETURN;
	if (bit == 0)
	{
		if (DecodeTree(prob.low[pos_state], len, XLenProb::LOW_BITS) == false)
			ERROR_RETURN;
		return true;
	}

	if (DecodeBit(prob.choice2, bit) == false) ERROR_RETURN;
	if (bit == 0)
	{
		if (DecodeTree(prob.mid[pos_state], len, XLenProb::MID_BITS) == false)
			ERROR_RETURN;
		len += 8;
		return true;
	}

	if(DecodeTree(prob.high, len, XLenProb::HIGH_BITS)==false)
		ERROR_RETURN;

	len += 16;

	return true;
}

