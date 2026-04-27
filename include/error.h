#include <stdlib.h>
#include <stdbool.h>

#ifndef ERROR_H
#define ERROR_H

/*
 * error.h - First class errors for C
 * Have you ever wanted to have a Rust-like error experience in C?
 * Look no further than this library! Using a couple simple macros,
 * we can emulate their complicated enum system, and I'd argue that
 * we do it better. Besides, it's in a better programming language.
 *
 * Enjoy!
 *
 * Licenced to you under the MIT license - see below.
*/

/*
 * Example usage:
 *
 * #include "error.h"
 * #include <stdio.h>
 * 
 * // You can't write char*, you have to define it with a typedef
 * typedef char* charptr;
 * 
 * Result(int, charptr) myFn(int x) {
 *    if (x > 5) {
 *        return Error(int, charptr, "Your number is too big");
 *    }
 *    return Success(int, charptr, x);
 * }
 * 
 * int main() {
 *    ResultType(int, charptr) res = myFn(10);
 *    if (res.error) {
 *        printf("Uh oh, error is: %s\n", res.as.error);
 *    } else {
 *        printf("Got a result! It is %d\n", res.as.success);
 *    }
 * }
 *
 */

/*
 * Copyright 2026 Maxwell Jeffress
 * 
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the “Software”), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
*/

// Creates a new struct with the a (success) and b (error) types. 
// If Result(a, b) has already been called with the same paramaters, please
// use ResultType(a, b) instead.
#define Result(a, b) struct __ResultType_##a##_##b { bool error; union {a success; b error;} as; }

// Uses an existing Result(a, b) struct.
#define ResultType(a, b) struct __ResultType_##a##_##b


// Creates a __ResultType_a_b struct, with .error as false and .as.success as res.
#define Success(a, b, res) (ResultType(a, b)) { .error = false, .as.success = res }

// Creates a __ResultType_a_b struct, with .error as true and .as.error as res.
#define Error(a, b, res) (ResultType(a, b)) { .error = true, .as.error = res }

#endif