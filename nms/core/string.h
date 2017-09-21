#pragma once

#include <nms/core/list.h>

namespace nms
{

template<u32 Icapicity=0 > using U8String   = List<char,     Icapicity>;
template<u32 Icapicity=0 > using U16String  = List<char16_t, Icapicity>;
template<u32 Icapicity=0 > using Tu32String = List<char32_t, Icapicity>;

NMS_API u32 strlen(const char* s);

inline StrView mkStrView(const char* s) {
    return {s, strlen(s)};
}

/* split a TString into pieces */
NMS_API List<StrView> split(StrView str, StrView delimiters);

}
