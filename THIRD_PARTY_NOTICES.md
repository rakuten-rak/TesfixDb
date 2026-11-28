# Third-party notices

Tesfix includes the following software. This is a good-faith summary, not legal advice.

## SQLite

The database engine. SQLite is in the public domain and needs no attribution.
https://www.sqlite.org/copyright.html

## nlohmann/json

JSON parsing and serialization (compiled into the WebAssembly file).
Licensed under the MIT licence, which requires this notice to stay with the software.

SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann https://nlohmann.me

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Emscripten runtime and C/C++ libraries

`dist/tesfix_wasm.mjs` and `dist/tesfix_wasm.wasm` were produced with Emscripten
(https://emscripten.org). They contain Emscripten's runtime glue code and parts of the C and C++
standard libraries it links in. At the time of writing these are licensed as follows; check the
licences of the Emscripten version you built with.

- Emscripten: MIT licence and University of Illinois/NCSA licence.
- musl libc: MIT licence.
- LLVM libc++ / libc++abi: Apache License 2.0 with LLVM Exceptions.