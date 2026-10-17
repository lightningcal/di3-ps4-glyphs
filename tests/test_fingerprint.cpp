#include "fingerprint.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
int main()
{
    const std::array<unsigned char, 16> packed{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    const std::array<unsigned char, 24> padded{1,2,3,4,5,6,7,8,99,99,99,99,
                                            9,10,11,12,13,14,15,16,88,88,88,88};
    auto check = [](bool ok) { if (!ok) std::exit(1); };
    check(pixelFingerprint(packed.data(), 8, 2, 2) == pixelFingerprint(padded.data(), 12, 2, 2));
    check(pixelFingerprint(packed.data(), 8, 2, 2) == 0xe7ecfb4f6e006495ull);
    check(pixelFingerprint(nullptr, 8, 2, 2) == 0);
    check(pixelFingerprint(packed.data(), 7, 2, 2) == 0);
    check(pixelFingerprint(packed.data(), 8, 0, 2) == 0);
    auto changed = packed;
    changed[10] ^= 1;
    check(pixelFingerprint(changed.data(), 8, 2, 2) != pixelFingerprint(packed.data(), 8, 2, 2));
    std::cout << "Fingerprint checks passed.\n";
}
