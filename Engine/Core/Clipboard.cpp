#include "Clipboard.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Clipboard
{
    std::string getText()
    {
        std::string result;
        if (!OpenClipboard(nullptr))
            return result;
        // Texte Unicode de préférence (copié depuis un navigateur, Discord...), converti en UTF-8
        if (HANDLE h = GetClipboardData(CF_UNICODETEXT))
        {
            if (const wchar_t *w = (const wchar_t *)GlobalLock(h))
            {
                int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
                if (len > 1)
                {
                    result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, w, -1, &result[0], len, nullptr, nullptr);
                }
                GlobalUnlock(h);
            }
        }
        else if (HANDLE h = GetClipboardData(CF_TEXT))
        {
            if (const char *s = (const char *)GlobalLock(h))
            {
                result = s;
                GlobalUnlock(h);
            }
        }
        CloseClipboard();
        return result;
    }

    bool setText(const std::string &text)
    {
        int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (len <= 0 || !OpenClipboard(nullptr))
            return false;
        EmptyClipboard();
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
        bool ok = false;
        if (mem)
        {
            if (wchar_t *w = (wchar_t *)GlobalLock(mem))
            {
                MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, w, len);
                GlobalUnlock(mem);
                ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
            }
            if (!ok)
                GlobalFree(mem);
        }
        CloseClipboard();
        return ok;
    }
}

#else

namespace Clipboard
{
    std::string getText() { return ""; }
    bool setText(const std::string &) { return false; }
}

#endif
