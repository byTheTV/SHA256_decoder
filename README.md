# SHA256 decoder

!! Not Ended !!!

A C++ SHA256 implementation.

## Build

**Minimum C++11.**

Just run `make all`. There are no dependencies.

```
$ ./SHA256 "string"
$ ./SHA256 --bruteforce
```
in bin ofc

### As a library

```cpp
#include "SHA256.h"

//...

string s = "hello world";
SHA256 sha;
sha.update(s);
std::array<uint8_t, 32> digest = sha.digest();

std::cout << SHA256::toString(digest) << std::endl;
```
