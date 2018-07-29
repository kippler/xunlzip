#include "stdafx.h"
#include "XLzmaDec.h"
#include "_private.h"
#include "Global.h"
#include <algorithm>

#pragma warning(disable: 26495)
#undef min


XLzmaDec::XLzmaDec()
{
	m_dicSize = 0;
	m_dic = NULL;
	m_out = NULL;
	m_totOut = NULL;
	m_dicPos = 0;
	m_streamPos = 0;
	m_crc32 = 0;
	m_eos = false;
	m_isBroken = false;

	m_rep0 = m_rep1 = m_rep2 = m_rep3 = 0;

	m_probIsMatch = NULL;
	m_probLiteral = NULL;
}


XLzmaDec::~XLzmaDec()
{
	Free();
}

void XLzmaDec::Free()
{
	delete[] m_dic;
	m_dic = NULL;

	delete[] m_probIsMatch;
	m_probIsMatch = NULL;

	delete[] m_probLiteral;
	m_probLiteral = NULL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         LZMA 스트림 초기화
/// @param  
/// @return 
/// @date   Wed Jul 25 19:12:52 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
bool XLzmaDec::Init(XReadStream* in, XWriteStream* out, uint32_t dicSize, const XLzmaProp& prop)
{
	Free();

	if (m_rdec.StreamInit(in) == false)
	{
		ASSERT(0);
		return false;
	}

	// 변수 초기화
	m_out = out;
	m_totOut = 0;
	m_dicSize = dicSize;
	m_dicPos = 0;
	m_streamPos = 0;
	m_crc32 = 0;
	m_eos = false;
	m_isBroken = false;
	m_prop = prop;
	m_rep0 = m_rep1 = m_rep2 = m_rep3 = 0;

	// 사전 만들기
	m_dic = new uint8_t[dicSize];
	memset(m_dic, 0, dicSize);

	// 상태 초기화
	m_state.Init();

	// 검사...
	if (prop.pb != 2) { ASSERT(0); return FALSE; }			// 이 수치가 2가 아니면 상수로 박아서 에러 나는 부분 있음. m_probRep0Long 와 XLenProb

	// 각종 문맥 초기화
	for (int i = 0; i < LZMA_STATES; i++)
	{
		m_probRep[i].Init();
		m_probRep0[i].Init();
		m_probRep1[i].Init();
		m_probRep2[i].Init();

		for(int j = 0; j < (1 << prop.pb); j++)
			m_probRep0Long[i][j].Init();
	}
	m_probRepLen.Init();
	m_probMatchLen.Init();

	for (int i = 0; i < LEN_TO_SLOT_STATE; i++)
		for (int j = 0; j < (1 << DIST_SLOT_BITS); j++)
			m_probDistSlot[i][j].Init();

	for (int i = 0; i < LEN_DIST_SPECIAL; i++)
		m_probDistSpecial[i].Init();

	for (int i = 0; i < LEN_DIST_ALIGN; i++)
		m_probDistAlign[i].Init();



	// 포지션 문맥 만들기
	// 각 스테이트 뿐만 아니라, 디코딩된 위치도 문맥을 결정하는 요소이다. 왜 그랬을까?
	const int pos_states = (1 << prop.pb);
	m_probIsMatch = new XProb[LZMA_STATES * pos_states];


	// 리터럴 문맥 만들기 - LZIP 은 3 으로 고정이지만, LZMA 는 lc, lp 에 따라 바뀜
	//
	// LZMA 스펙 문서보면 이렇게 되어 있음. 
	// The LZMA Decoder uses(1 << (lc + lp)) tables with CProb values, where each table contains 0x300 CProb values :	
	//
	// lc 는 literal_context_bits 이고, lp 는 literal_pos_state 인데, 0x300 은 LITERAL_TREE_SIZE
	m_probLiteral = new XProb[(uint32_t)LITERAL_TREE_SIZE << (prop.lc + prop.lp)];


	return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         디코딩 
/// @param  
/// @return 
/// @date   Wed Jul 25 19:12:41 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
bool XLzmaDec::Decode()
{
	uint32_t bit0=0, bit1 = 0, bit2 = 0, bit3 = 0, bit4 = 0;

	//int loop = 0;

	for (;;)
	{
		if (m_eos)
		{
			if (Flush() == false)
				ERROR_RETURN;
			break;
		}

		/*
			0 + byte					literal		문자열
			1 + 0 + len + dis			match		거리 + 길이
			1 + 1 + 0 + 0 				shortrep	1바이트만 동일 + 최근 사용한 distance 사용
			1 + 1 + 0 + 1 + len 		longrep0	길이 + 최근 사용한 distance
			1 + 1 + 1 + 0 + len 		longrep1	길이 + 최근 2번째 사용한 distance
			1 + 1 + 1 + 1 + 0 + len		longrep2	길이 + 최근 3번째 사용한 distance
			1 + 1 + 1 + 1 + 1 + len		longrep3	길이 + 최근 4번째 사용한 distance
		*/

		// pos_state(posState) : 디코딩된 데이타의 위치값의 LSB pb(보통2) 비트의 값
		// 현재 state 뿐만 아니라, 디코딩 되는 위치를 문맥에 넣는건 이해가 가는데,
		// 굳이 MSB 가 아니라 LSB 인 이유는 뭘까...
		// 하위 2비트래 봤자 딱히 효율에는 큰 차이가 없을거 같은데 뭐 그렇게 만들 이유가 있겠지..
		unsigned posState = (unsigned)OutSize() & ((1 << m_prop.pb) - 1);	// 하위 N 비트 추출
		//long long _pos = OutSize();
		//loop++;
		//if (loop >= 2983) int x = 10;

		// 첫번째 비트 가져오기
		if (m_rdec.DecodeBit(m_probIsMatch[m_state + LZMA_STATES * posState], bit0) == false)
			ERROR_RETURN;

		// 0 + byte
		// 첫번째 비트가 0 이면 => 리터럴
		if (bit0 == 0)
		{
			if(DecodeLiteral()==false)
				ERROR_RETURN;
		}
		// 첫번째 비트가 1 이면 => match 혹은 repeat!
		else
		{
			// 두번째 비트 가져오기
			if (m_rdec.DecodeBit(m_probRep[m_state], bit1) == false)
				ERROR_RETURN;

			if (bit1)
			{
				// 1 + 1 + ??
				// 세번째 비트 가져오기
				if (m_rdec.DecodeBit(m_probRep0[m_state], bit2) == false)
					ERROR_RETURN;

				if (bit2 == 0)
				{
					// 네번째 비트 가져오고
					if (m_rdec.DecodeBit(m_probRep0Long[m_state][posState], bit3) == false)
						ERROR_RETURN;

					if (bit3 == 0)
					{
						// 1 + 1 + 0 + 0 			shortrep	1바이트만 동일 + 최근 사용한 distance 사용
						// 숏 리핏 처리하기
						if (DecodeShortRepeat() == false)
							ERROR_RETURN;
						continue;
					}
					else
					{
						// 1 + 1 + 0 + 1 + len 		longrep0	길이 + 최근 사용한 distance
						// 그냥 밑으로 간다.
					}
				}
				else
				{
					/*
						1 + 1 + 1 + 0 + len 		longrep1	길이 + 최근 2번째 사용한 distance
						1 + 1 + 1 + 1 + 0 + len		longrep2	길이 + 최근 3번째 사용한 distance
						1 + 1 + 1 + 1 + 1 + len		longrep3	길이 + 최근 4번째 사용한 distance
					*/

					// 네번째 비트 가져오고
					if (m_rdec.DecodeBit(m_probRep1[m_state], bit3) == false)
						ERROR_RETURN;

					uint32_t distance = 0;

					// 1 + 1 + 1 + 0 + len 		longrep1	길이 + 최근 2번째 사용한 distance
					if (bit3 == 0)
					{
						distance = m_rep1;
						m_rep1 = m_rep0;
						m_rep0 = distance;
					}
					else
					{
						// 다섯번째 비트 가져오고
						if (m_rdec.DecodeBit(m_probRep2[m_state], bit4) == false)
							ERROR_RETURN;

						// 1 + 1 + 1 + 1 + 0 + len		longrep2	길이 + 최근 3번째 사용한 distance
						if (bit4 == 0)
						{
							distance = m_rep2;
						}
						// 1 + 1 + 1 + 1 + 1 + len		longrep3	길이 + 최근 4번째 사용한 distance
						else
						{
							distance = m_rep3;
							m_rep3 = m_rep2;
						}

						m_rep2 = m_rep1;
						m_rep1 = m_rep0;
						m_rep0 = distance;
					}
				}

				// 롱 리핏 처리하기
				if (DecodeLongRepeat(posState) == false)
					ERROR_RETURN;
			}
			else
			{
				////////////////////////////////
				//
				// 1 + 0 + len + dis : match
				// 
				if(DecodeMatch(posState)==false)
					ERROR_RETURN;
			}
		}
	}

	return m_isBroken ? false : true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         사전에서 가져오기
/// @param  
/// @return 
/// @date   Thu Jul 26 14:22:31 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t XLzmaDec::Peek(const unsigned int distance) const
{
	// 현재 위치 뒤에서 가져오기
	if (m_dicPos > distance) 
		return m_dic[m_dicPos - distance - 1];

	// 앞쪽에서 가져오기
	return m_dic[m_dicSize + m_dicPos - distance - 1];
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         리터럴 디코드하기
/// @param  
/// @return 
/// @date   Thu Jul 26 14:10:08 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
bool XLzmaDec::DecodeLiteral()
{
	// 일단 사전에서 이전 글자 가져오기
	const uint8_t prevByte = Peek(0);


	// literal_pos_state 에 대한 위키백과 설명
	//
	// - The pos_state and literal_pos_state values consist of respectively the pb and lp (up to 4, from the LZMA header or LZMA2 properties packet) 
	//   least significant bits of the dictionary position (the number of bytes coded since the last dictionary reset modulo the dictionary size). 
	//
	// lzip 의 설명 literal_state(==literal_pos_state): 최근 디코딩된 바이트의 MSB 3비트의 값

	//
	// 이전 리터럴의 상위 (보통)3비트를 추출한값을 state 로 사용한다.
	// 나의 해석: 특수문자가, 대소문자등이 군집되어 있는 경우 레인지 코딩의 확률을 높이기 위해서 상위 3비트를 가지고 문맥으로 사용
	//
	const int literal_pos_state = prevByte >> (8 - (m_prop.lc + m_prop.lp));		


	uint32_t literal=0;
	XProb* prob = m_probLiteral + (literal_pos_state * (INT_PTR)LITERAL_TREE_SIZE);


	if (m_state.is_char())
	{
		// (이전 리터럴을 이용해서 선택된 트리 문맥을 가지고) 리터럴 가져오기
		if (m_rdec.DecodeTree(prob, literal, 8) == false)
			ERROR_RETURN;

		// 출력
		if (Write(uint8_t(literal)) == false)
			ERROR_RETURN;
	}
	else
	{
		/*
		SPEC 문서 
			If (State > 7), the Literal Decoder also uses "matchByte" that represents
			the byte in OutputStream at position the is the DISTANCE bytes before
			current position, where the DISTANCE is the distance in DISTANCE-LENGTH pair
			of latest decoded match.
		*/

		//
		// 여기로 오는 경우는 이전 상태가 캐릭터 출력 상태가 아니였음을 의미함
		// 이전 상태가 캐릭터가 아닌 경우, 캐릭터의 연속성이 깨졌을 가능성이 있기 때문에(아마도?)
		// 좀더 특수한(?) 디코딩 작업 수행
		//
		
		if (m_rdec.DecodeMatched(prob, literal, Peek(m_rep0)) == false)
			ERROR_RETURN;

		// 출력
		if (Write(uint8_t(literal)) == false)
			ERROR_RETURN;
	}

	// 상태 바꾸기
	m_state.LIT();

	return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         지금까지 출력한 크기
/// @param  
/// @return 
/// @date   Thu Jul 26 16:38:30 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t XLzmaDec::OutSize()
{
	return m_totOut;
}

// 출력
bool XLzmaDec::Write(uint8_t c)
{
#ifdef _DEBUG
	//if (m_compareBuf.dataSize == 0)
	//{
	//	XFileReadStream f;
	//	f.Open(L"c:\\temp\\fedora.iso");
	//	m_compareBuf.Alloc(1024 * 1024);
	//	f.Read_(m_compareBuf.data, 1024 * 1024);
	//	m_compareBuf.dataSize = 1024 * 1024;
	//	m_comparePos = 0;
	//}

	//if (m_compareBuf.data[m_comparePos] != c)
	//	ASSERT(0);
	//m_comparePos++;
#endif


	// 사전에 쓰고
	m_dic[m_dicPos] = c;
	m_dicPos++;

	if (m_dicPos >= m_dicSize)
	{
		if (Flush() == false)
			return false;
	}

	m_totOut++;
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///         사전에 남아 있는 데이타 flush 
/// @param  
/// @return 
/// @date   Thu Jul 26 18:46:16 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
bool XLzmaDec::Flush()
{
	// 쓸 데이타가 있는 경우인지 확인
	if (m_dicPos > m_streamPos)
	{
		const uint32_t size2write = m_dicPos - m_streamPos;
		m_crc32 = ::CRC32(m_crc32, m_dic + m_streamPos, size2write);

		// 버퍼에 쓰고
		if (m_out->Write(m_dic + m_streamPos, size2write) == FALSE)
			ERROR_RETURN;

		// 사전의 끝에 도달했나? - 맨 앞으로 보낸다.
		if (m_dicPos >= m_dicSize)
			m_dicPos = 0;

		m_streamPos = m_dicPos;

//#ifdef _DEBUG
		//wprintf(L"%I64d\r", m_totOut);
//#endif

	}

	return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         리핏 처리
/// @param  
/// @return 
/// @date   Thu Jul 26 17:09:24 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
#define MIN_MATCH_LEN		2
bool XLzmaDec::DecodeLongRepeat(unsigned posState)
{
	m_state.LONGREP();

	uint32_t len = 0;
	if (m_rdec.DecodeLen(m_probRepLen, posState, len) == false)
		ERROR_RETURN;

	len += MIN_MATCH_LEN;

	for (unsigned i = 0; i < len; i++)
		Write(Peek(m_rep0));

	return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         shortrep	1바이트만 동일 + 최근 사용한 distance 사용	
/// @param  
/// @return 
/// @date   Thu Jul 26 17:21:07 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
bool XLzmaDec::DecodeShortRepeat()
{
	m_state.SHORTREP();

	// 최근 사용한 dist 사용해서 1바이트 가져오기
	const uint8_t lit = Peek(m_rep0);
	if (Write(lit) == false)
		ERROR_RETURN;

	return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         1 + 0 + len + dis : match
/// @param  
/// @return 
/// @date   Thu Jul 26 19:07:14 2018
////////////////////////////////////////////////////////////////////////////////////////////////////
bool XLzmaDec::DecodeMatch(unsigned posState)
{
	// 최근 거리 하나씩 밀고
	m_rep3 = m_rep2; 
	m_rep2 = m_rep1; 
	m_rep1 = m_rep0;

	//if (m_rep2 == 93) ASSERT(0);

	////////////////////////////////////////////////////////////
	//
	// len 가져오기 
	//
	uint32_t len = 0;
	if (m_rdec.DecodeLen(m_probMatchLen, posState, len) == false)
		ERROR_RETURN;
	// len 은 2 부터 시작한다.
	len += kMatchMinLen;

	//
	// len 가져오기 
	//
	////////////////////////////////////////////////////////////



	////////////////////////////////////////////////////////////
	//
	// DIST 가져오기 
	//

	// DISTANCE 에 사용할 문맥은 len 값을 기반으로 만든다.
	// LZMA 스펙 왈: LZMA uses normalized match length(zero - based length) to calculate the context state "lenState" do decode the distance value : ......
	// len 에 2 더했으니까 2 빼주고, 최대값은 3이다.
	uint32_t lenState = std::min(len - kMatchMinLen, (uint32_t)(LEN_TO_SLOT_STATE-1));

	// dist_slot 가져오기
	uint32_t distSlot = 0;
	if (m_rdec.DecodeTree(m_probDistSlot[lenState], distSlot, DIST_SLOT_BITS) == false)
		ERROR_RETURN;

	/*
	lzip 문서
		Bit sequence						Description 
		slot								distances from 0 to 3 
		slot + direct_bits					distances from 4 to 127 
		slot + (direct_bits - 4) + 4 bits	distances from 128 to 2^32 - 1 
	*/

	if (distSlot <= 3)
	{
		// distSlot 이 3 이하면 distSlot 을 그냥 dist 에 대입해서 사용하면 된다.
		m_rep0 = distSlot;
	}
	else 
	{
		/*
		LZMA 스펙 문서		
			The encoding scheme for distance value is shown in the following table:

			posSlot		zero-based distance (binary)
			(==distSlot)
			0			0
			1			1
			2			10
			3			11

			4			10 x
			5			11 x
			6			10 xx
			7			11 xx
			8			10 xxx
			9			11 xxx
			10			10 xxxx
			11			11 xxxx
			12			10 xxxxx
			13			11 xxxxx

			14			10 yy zzzz
			15			11 yy zzzz
			16			10 yyy zzzz
			17			11 yyy zzzz
			...
			62			10 yyyyyyyyyyyyyyyyyyyyyyyyyy zzzz
			63			11 yyyyyyyyyyyyyyyyyyyyyyyyyy zzzz
		*/

		//
		// direct bits 의 길이 계산하기
		// 위 테이블에서 xxx 또는 (yy+zzzz) 의 길이가 directBits 다
		//
		const int numDirectBits = (int)((distSlot >> 1) - 1);

		// 일단 m_rep0 를 "10 xxxx" 형태에서 앞에 "10"을 먼저 만들고
		m_rep0 = (2 | (distSlot & 1)) << numDirectBits;

		// 4부터 13 사이면
		if (distSlot <= 13)
		{
			/*
				4			10 x
				5			11 x
				6			10 xx
				7			11 xx
				8			10 xxx
				9			11 xxx
				10			10 xxxx
				11			11 xxxx
				12			10 xxxxx
				13			11 xxxxx
			*/

			// numDirectBits 만큼 뒤집어서 가져오기
			// 뒤집어서 가져오는 이유는 일반 숫자는 리터럴과 성격이 다르기 때문인듯?
			uint32_t xxxx = 0;
			if (m_rdec.DecodeTreeRev(m_probDistSpecial + m_rep0 - distSlot, xxxx, numDirectBits) == false)
				ERROR_RETURN;

			// "10 xxxx" 형태 완성시키기
			m_rep0 += xxxx;

		}
		// 14 부터 63 까지
		else
		{
			/*
				14			10 yy zzzz
				15			11 yy zzzz
				16			10 yyy zzzz
				17			11 yyy zzzz
				...
				62			10 yyyyyyyyyyyyyyyyyyyyyyyyyy zzzz
				63			11 yyyyyyyyyyyyyyyyyyyyyyyyyy zzzz

				dist += RangeDec.DecodeDirectBits(numDirectBits - kNumAlignBits) << kNumAlignBits;
				dist += AlignDecoder.ReverseDecode(&RangeDec);

			*/

			//
			// YY 는 다이렉트로 디코딩(고정확률 디코딩) 한다. 아마도 yyy 는 확률적으로 연관성이 없어서 압축이 안되서 이렇게 처리하는듯
			// 
			uint32_t yyyy = 0;
			if (m_rdec.DecodeDirect(yyyy, numDirectBits - kNumAlignBits)==false)
				ERROR_RETURN;

			// ZZZ 가져오기
			uint32_t zzzz = 0;
			if (m_rdec.DecodeTreeRev(m_probDistAlign, zzzz, kNumAlignBits) == false)
				ERROR_RETURN;

			// "10 yyyy zzzz" 형태 완성시키기
			m_rep0 = m_rep0 + (yyyy<< kNumAlignBits) + zzzz;

			if (m_rep0 == 0xFFFFFFFFU)		// marker found
			{
				m_eos = true;
				if (len != kMatchMinLen)
					m_isBroken = true;
				return true;
			}

		}
	}

	// 상태 바꾸고
	m_state.MATCH();

	// 쓰기
	for (unsigned i = 0; i < len; i++)
		Write(Peek(m_rep0));

	return true;
}
