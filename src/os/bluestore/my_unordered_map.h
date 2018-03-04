
#define _UNORDERED_MAP_H
#define _HASHTABLE_H 1
#include <unordered_map>
#undef _HASHTABLE_H
#undef _UNORDERED_MAP_H
#if __GNUC__ == 7
#include "gcc-7/hashtable.h"
#include "gcc-7/unordered_map.h"
#elif __GNUC__ == 6
#error not handled gcc 6
#elif __GNUC__ == 5
#include "gcc-5/hashtable.h"
#include "gcc-5/unordered_map.h"
#elif __GNUC__ == 4
#include "gcc-4/hashtable.h"
#include "gcc-4/unordered_map.h"
#else
#error compiler not handled
#endif

#if 0
#ifndef _GLIBCXX_UNORDERED_MAP
#define _GLIBCXX_UNORDERED_MAP 1

#pragma GCC system_header

#if __cplusplus < 201103L
# include <bits/c++0x_warning.h>
#else

#include <utility>
#include <type_traits>
#include <initializer_list>
#include <tuple>
#include <bits/allocator.h>
#include <ext/alloc_traits.h>
#include <ext/aligned_buffer.h>
#include <bits/stl_function.h> // equal_to, _Identity, _Select1st
#include <bits/functional_hash.h>
#include "hashtable.h"
#include "unordered_map.h"
#include <bits/range_access.h>

#ifdef _GLIBCXX_DEBUG
# include <debug/unordered_map>
#endif

#ifdef _GLIBCXX_PROFILE
# include <profile/unordered_map>
#endif

#endif // C++11

#endif // _GLIBCXX_UNORDERED_MAP
#endif
