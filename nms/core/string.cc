#include <nms/core.h>
#include <nms/test.h>

namespace nms
{

NMS_API u32 strlen(const char* s) {
    if (s == nullptr) {
        return 0u;
    }
    auto len = u32(::strlen(s));
    return len;
}


NMS_API List<StrView> split(StrView str, StrView delimiters) {
    List<StrView> list;

    while (true) {
        const auto n = str.count();
        if (n == 0) {
            break;
        }

        auto b = 0u;
        while (b < n && delimiters.contains(str[b]) == true) {
            ++b;
        }

        auto e = b + 1;
        while (e < n && delimiters.contains(str[e]) == false) {
            ++e;
        }

        const auto s = str.slice( b, e - 1);
        list.append(s);

        if (e + 1 >= n) {
            break;
        }
        str = str.slice(e + 1, n - 1);
    }

    return list;
}

}
