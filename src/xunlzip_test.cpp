#include "stdafx.h"
#include "xunlzip_test.h"
#include "XStream.h"
#include "Global.h"
#include "XUnlzip.h"



bool ExtractLzip(LPCWSTR compFile, LPCWSTR extFile)
{
	XUnlzip lunzip;

	if (lunzip.Open(compFile) == false)
	{
		wprintf(L"can't open lzip file:%s\n", compFile);
		return false;
	}


	wprintf(L"Extracting %ls -> %ls\n", compFile, extFile);

	if (lunzip.Extract(extFile)== false)
	{
		wprintf(L"can't extract to:%ls\n", extFile);
		return false;
	}

	wprintf(L"done\n");

	return true;
}



bool TestLzip(LPCWSTR compFile)
{
	XUnlzip lunzip;
	const ULONGLONG tick = ::GetTickCount64();

	if (lunzip.Open(compFile) == false)
	{
		wprintf(L"can't open lzip file:%ls\n", compFile);
		return false;
	}

	XDummyWriteStream dummy;

	wprintf(L"Testing %ls\n", compFile);


	if (lunzip.Extract(dummy) == false)
	{
		wprintf(L"error at extract file%ls", compFile);
		return false;
	}

	wprintf(L"done(%I64dms)\n", (INT64)(GetTickCount64()-tick));

	return true;
}



