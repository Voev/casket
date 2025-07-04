#include <casket/utils/hexlify.hpp>
#include <casket/utils/exception.hpp>

namespace casket {

inline uint8_t char2digit(const char ch) {
    switch (ch) {
        case '0':
            return 0;
        case '1':
            return 1;
        case '2':
            return 2;
        case '3':
            return 3;
        case '4':
            return 4;
        case '5':
            return 5;
        case '6':
            return 6;
        case '7':
            return 7;
        case '8':
            return 8;
        case '9':
            return 9;
        case 'a':
        case 'A':
            return 0x0A;
        case 'b':
        case 'B':
            return 0x0B;
        case 'c':
        case 'C':
            return 0x0C;
        case 'd':
        case 'D':
            return 0x0D;
        case 'e':
        case 'E':
            return 0x0E;
        case 'f':
        case 'F':
            return 0x0F;
        default:
            throw RuntimeError("invalid hexadecimal symbol");
    }
}

std::string hexlify(const std::uint8_t* bytes, std::size_t size) {
    static const uint8_t kHexMap[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    std::string out;
    out.resize(size * 2);

    for (size_t i = 0, j = 0; i < size && j < out.size(); ++i) {
        out[j++] = kHexMap[(bytes[i] >> 4)];
        if (j < out.size()) {
            out[j++] = kHexMap[bytes[i] & 0xF];
        }
    }

    return out;
}

std::vector<uint8_t> unhexlify(nonstd::string_view in) {

    ThrowIfFalse(in.size() % 2 == 0, "even string length required");

    std::vector<uint8_t> out;
    out.resize(in.size() / 2);

    for (size_t i = 0, j = 0; i < in.size() && j < out.size(); i += 2) {
        out[j++] = char2digit(in[i]) << 4 | char2digit(in[i + 1]);
    }

    return out;
}

} // namespace casket
