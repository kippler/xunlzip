
#pragma once

#include <SDKDDKVer.h>
#include <stdio.h>
#include <tchar.h>
#include <stdint.h>
#include <atlbase.h>
#include <atlstr.h>
#include <locale.h>


// 수정이 곤란한 warning
#pragma warning(disable: 26432)		// If you define or delete any default operation in the type 'class CWebMDLL', define or delete them all (c.21).
#pragma warning(disable: 26485)		// Expression 'L"\x44992\x52896"': No array to pointer decay (bounds.3).
#pragma warning(disable: 26408)		// Avoid malloc() and free(), prefer the nothrow version of new with delete (r.10).
#pragma warning(disable: 26489)		// Don't dereference a pointer that may be invalid: '*nSerialNumberSize'. 'nSerialNumberSize' may have been invalidated at line 750 (lifetime.1).
#pragma warning(disable: 26482)		// Only index into arrays using constant expressions (bounds.2).
#pragma warning(disable: 26481)		// Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable: 26446)		// Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
//#pragma warning(disable: 26438)		// Avoid 'goto' (es.76).
//#pragma warning(disable: 26429)		// Symbol 'cur' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable: 26409)		// Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11).
#pragma warning(disable: 26401)		// Do not delete a raw pointer that is not an owner<T> (i.11).
//#pragma warning(disable: 26400)		// Do not assign the result of an allocation or a function call with an owner<T> return value to a raw pointer, use owner<T> instead (i.11).
//#pragma warning(disable: 26486)		// Don't pass a pointer that may be invalid to a function. Parameter 1 'ps.hdc' in call to 'ATL::CWindow::BeginPaint' may be invalid 
#pragma warning(disable: 26447)		// The function is declared 'noexcept' but calls function 'FreeDLL()' which may throw exceptions (f.6).
#pragma warning(disable: 26440)		// Function 'CZoomWnd::OnDestroy' can be declared 'noexcept' (f.6).
//#pragma warning(disable: 26496)		// The variable 'dw' is assigned only once, mark it as const (con.4).
#pragma warning(disable: 26493)		// Don't use C-style casts (type.4).
#pragma warning(disable: 26439)		// This kind of function may not throw. Declare it 'noexcept' (f.6).


#pragma warning(disable: 26455)




