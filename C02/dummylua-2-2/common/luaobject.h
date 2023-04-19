/* Copyright (c) 2018 Manistein,https://manistein.github.io/blog/  

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.*/

#ifndef luaobject_h
#define luaobject_h 

#include "lua.h"

typedef struct lua_State lua_State;

typedef LUA_INTEGER lua_Integer;
typedef LUA_NUMBER lua_Number;
typedef unsigned char lu_byte;
typedef int (*lua_CFunction)(lua_State* L);
typedef void* (*lua_Alloc)(void* ud, void* ptr, size_t osize, size_t nsize);

// lua number type 
#define LUA_NUMINT (LUA_TNUMBER | (0 << 4))
#define LUA_NUMFLT (LUA_TNUMBER | (1 << 4))

// lua function type 
#define LUA_TLCL (LUA_TFUNCTION | (0 << 4))
#define LUA_TLCF (LUA_TFUNCTION | (1 << 4))
#define LUA_TCCL (LUA_TFUNCTION | (2 << 4))

// string type 
#define LUA_LNGSTR (LUA_TSTRING | (0 << 4))
#define LUA_SHRSTR (LUA_TSTRING | (1 << 4))

// GCObject 
// next 是一个指针类型，指向链表中下一个对象
// tt_ 表示 Lua 数据类型的标记位 
// marked 则表示该对象是否已经被垃圾回收机制标记为可回收。
#define CommonHeader struct GCObject* next; lu_byte tt_; lu_byte marked

// 表示在执行完全部一轮垃圾回收后，再进行下一轮垃圾回收所需要的“步数”。
// LUA_GCSTEPMUL 的值越大，表示 Lua 在进行 GC 时可以容忍更多未释放内存存在，从而减少 GC 的开销。
#define LUA_GCSTEPMUL 200

struct GCObject {
    CommonHeader;
};

/// @brief 定义了一个联合体 lua_Value，用于在 Lua 中表示不同的数据类型。
typedef union lua_Value {
    struct GCObject* gc;//指向 Lua 中的垃圾回收对象结构体 GCObject
    void* p;//指针类型，指向任意类型的指针数据。
    int b;//一个整数类型，用于表示布尔类型数据。
    lua_Integer i;//长整型类型
    lua_Number n;//浮点型类型
    lua_CFunction f;//函数指针类型，用于表示 C 函数类型数据。
} Value;

typedef struct lua_TValue {
    Value value_;
    int tt_;
} TValue;

typedef struct TString {
    CommonHeader;
} TString;

#endif 
