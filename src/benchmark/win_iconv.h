
#ifndef WIN_ICONV_H
#define WIN_ICONV_H

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#if defined(_MSC_VER)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // WIN32_LEAN_AND_MEAN

#include <memory.h>
#include <string.h>     // For memset()
#include <wchar.h>      // For MultiByteToWideChar(), WideCharToMultiByte()
#include <tchar.h>

//
// From: https://blog.csdn.net/chenlycly/article/details/123589349
//

static inline
int AnsiToUnicode(const char * pchSrc, WCHAR * pchDest, int nDestLen)
{
    if (pchSrc == NULL || pchDest == NULL) {
        return -1;
    }

    int output_len = -1;

    int nUnicodeLen = MultiByteToWideChar(CP_ACP, 0, pchSrc, -1, NULL, 0);
    WCHAR * pUnicode = new WCHAR[nUnicodeLen + 1];
    if (pUnicode != NULL) {
        memset(pUnicode, 0, (nUnicodeLen + 1) * sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, pchSrc, -1, pUnicode, nUnicodeLen + 1);

        UINT nDestCapacity = nDestLen / sizeof(WCHAR);
        UINT nLen = (UINT)wcslen(pUnicode);
        if ((nLen + 1) > nDestCapacity) {
            wcsncpy(pchDest, pUnicode, nDestCapacity - 1);
            pchDest[nDestCapacity - 1] = L'\0';
            // output_len is include '\0' null terminator
            output_len = nDestCapacity;
        } else {
            wcscpy(pchDest, pUnicode);
            // output_len is include '\0' null terminator
            output_len = nLen + 1;
        }

        delete[] pUnicode;
    }

    return output_len;
}

static inline
int Utf8ToUnicode(const char* pchSrc, WCHAR * pchDest, int nDestLen)
{
    if (pchSrc == NULL || pchDest == NULL) {
        return -1;
    }

    int output_len = -1;

    int nUnicodeLen = MultiByteToWideChar(CP_UTF8, 0, pchSrc, -1, NULL, 0);
    WCHAR * pUnicode = new WCHAR[nUnicodeLen + 1];
    if (pUnicode != NULL) {
        memset(pUnicode, 0, (nUnicodeLen + 1) * sizeof(WCHAR));
        MultiByteToWideChar(CP_UTF8, 0, pchSrc, -1, pUnicode, nUnicodeLen + 1);

        UINT nDestCapacity = nDestLen / sizeof(WCHAR);
        UINT nLen = (UINT)wcslen(pUnicode);
        if ((nLen + 1) > nDestCapacity) {
            wcsncpy(pchDest, pUnicode, nDestCapacity - 1);
            pchDest[nDestCapacity - 1] = L'\0';
            // output_len is include '\0' null terminator
            output_len = nDestCapacity;
        } else {
            wcscpy(pchDest, pUnicode);
            // output_len is include '\0' null terminator
            output_len = nLen + 1;
        }

        delete[] pUnicode;
    }

    return output_len;
}

static inline
int AnsiToUtf8(const char * pchSrc, char * pchDest, int nDestLen)
{
    if (pchSrc == NULL || pchDest == NULL) {
        return -1;
    }

    int output_len = -1;

    // ANSI ==> Unicode
    int nUnicodeLen = MultiByteToWideChar(CP_ACP, 0, pchSrc, -1, NULL, 0);
    WCHAR * pUnicode = new WCHAR[nUnicodeLen + 1];
    if (pUnicode != NULL) {
        memset(pUnicode, 0, (nUnicodeLen + 1) * sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, pchSrc, -1, pUnicode, nUnicodeLen + 1);

        // Unicode ==> utf-8
        int nUtf8Len = WideCharToMultiByte(CP_UTF8, 0, pUnicode, -1, NULL, 0, NULL, NULL);
        char * pUtf8 = new char[nUtf8Len + 1];
        if (pUtf8 != NULL) {
            memset(pUtf8, 0, nUtf8Len + 1);
            WideCharToMultiByte(CP_UTF8, 0, pUnicode, -1, pUtf8, nUtf8Len + 1, NULL, NULL);

            int nLen = (int)strlen(pUtf8);
            if ((nLen + 1) > nDestLen) {
                strncpy(pchDest, pUtf8, nDestLen - 1);
                pchDest[nDestLen - 1] = '\0';
                // output_len is include '\0' null terminator
                output_len = nDestLen;
            } else {
                strcpy(pchDest, pUtf8);
                // output_len is include '\0' null terminator
                output_len = nLen + 1;
            }

            delete[] pUtf8;
        }

        delete[] pUnicode;
    }

    return output_len;
}

static inline
int Utf8ToAnsi(const char * pchSrc, char * pchDest, int nDestLen)
{
    if (pchSrc == NULL || pchDest == NULL) {
        return -1;
    }

    int output_len = -1;

    // utf-8 ==> Unicode
    int nUnicodeLen = MultiByteToWideChar(CP_UTF8, 0, pchSrc, -1, NULL, 0);
    WCHAR * pUnicode = new WCHAR[nUnicodeLen + 1];
    if (pUnicode != NULL) {
        memset(pUnicode, 0, (nUnicodeLen + 1) * sizeof(WCHAR));
        MultiByteToWideChar(CP_UTF8, 0, pchSrc, -1, pUnicode, nUnicodeLen + 1);

        // Unicode ==> utf-8
        int nAnsiLen = WideCharToMultiByte(CP_ACP, 0, pUnicode, -1, NULL, 0, NULL, NULL);
        char * pAnsi = new char[nAnsiLen + 1];
        if (pAnsi != NULL) {
            memset(pAnsi, 0, nAnsiLen + 1);
            WideCharToMultiByte(CP_ACP, 0, pUnicode, -1, pAnsi, nAnsiLen + 1, NULL, NULL);

            int nLen = (int)strlen(pAnsi);
            if ((nLen + 1) > nDestLen) {
                strncpy(pchDest, pAnsi, nDestLen - 1);
                pchDest[nDestLen - 1] = '\0';
                // output_len is include '\0' null terminator
                output_len = nDestLen;
            } else {
                strcpy(pchDest, pAnsi);
                // output_len is include '\0' null terminator
                output_len = nLen + 1;
            }

            delete[] pAnsi;
        }

        delete[] pUnicode;
    }

    return output_len;
}

#endif // _MSC_VER

#endif // WIN_ICONV_H
