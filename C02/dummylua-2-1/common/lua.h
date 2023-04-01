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

#ifndef lua_h
#define lua_h 

#include <stdarg.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>


// 定义整型和浮点型数据类型
#if defined(LLONG_MAX)
#define LUA_INTEGER long
#define LUA_NUMBER double
#else
#define LUA_INTEGER int 
#define LUA_NUMBER float 
#endif

// 错误码

#define LUA_OK 0
#define LUA_ERRERR 1
#define LUA_ERRMEM 2
#define LUA_ERRRUN 3 

// 宏定义

// 强制类型转换宏
#define cast(t, exp) ((t)(exp))
// 保存栈位置的宏 对象指针减去栈底指针的值。
#define savestack(L, o) ((o) - (L)->stack)
// 恢复栈位置的宏
#define restorestack(L, o) ((L)->stack + (o)) 

// basic object type
#define LUA_TNUMBER 1
#define LUA_TLIGHTUSERDATA 2
#define LUA_TBOOLEAN 3
#define LUA_TSTRING 4
#define LUA_TNIL 5
#define LUA_TTABLE 6
#define LUA_TFUNCTION 7
#define LUA_TTHREAD 8
#define LUA_TNONE 9

// stack define

#define LUA_MINSTACK 20 //虚拟机初始化时栈的最小容量
#define LUA_STACKSIZE (2 * LUA_MINSTACK) //即虚拟机初始化时栈的默认容量
#define LUA_EXTRASTACK 5 // Lua 虚拟机的额外堆栈大小，即虚拟机在需要扩展栈时额外分配的元素数量。当栈的容量不足时，Lua 虚拟机会自动扩展栈的容量，每次扩展的大小为 LUA_EXTRASTACK，也就是 5 个元素。
#define LUA_MAXSTACK 15000 //Lua 虚拟机的最大堆栈大小，即栈的容量上限。
#define LUA_ERRORSTACK 200 // Lua 错误处理栈的大小，即在处理错误时，Lua 虚拟机所使用的栈的大小。当 Lua 虚拟机运行出错时，会将错误信息保存在错误处理栈中
#define LUA_MULRET -1 //Lua 函数的多返回值标记，用于指示一个函数返回多个值。当一个函数返回多个值时，可以使用 LUA_MULTRET 标记来指示返回值的数量，其值为 -1。
#define LUA_MAXCALLS 200//Lua 调用栈的最大深度，即函数调用嵌套的最大层数。

// error tips
#define LUA_ERROR(L, s) printf("LUA ERROR:%s", s);

#endif 
