////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// LZIP 디코더 구현 테스트
/// 
/// @author   parkkh
/// @date     Wed Jul 25 18:02:40 2018
/// 
/// Copyright(C) 2018 Bandisoft, All rights reserved.
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "XStream.h"
#include "Global.h"
#include "xunlzip_test.h"

#pragma warning(disable: 26429)



int wmain(const int argc, const wchar_t * const argv[])
{
	_wsetlocale(LC_ALL, L"korean");
				
	
	if (argc != 3 && argc !=2)
	{
		printf("xunlzip.exe [infile] [outfile] \n");
		printf("	- extract lzip to outfile\n");
		printf("xunlzip.exe [infile] \n");
		printf("	- test only\n");
		return 1;
	}


	if (argc == 3)
	{
		CString compFile = argv[1];
		CString extFile = argv[2];

		ExtractLzip(compFile, extFile);
	}
	else if (argc == 2)
	{
		CString compFile = argv[1];

		TestLzip(compFile);
	}

	

    return 0;
}

