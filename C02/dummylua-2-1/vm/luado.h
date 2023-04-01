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

#ifndef luado_h
#define luado_h 

#include "../common/luastate.h"

//定义了一个函数指针类型Pfunc，用于指向Lua解释器中的C函数。
typedef int (*Pfunc)(struct lua_State* L, void* ud);
//将错误对象压入堆栈中，以便后续处理。
void seterrobj(struct lua_State* L, int error);
//检查堆栈是否足够容纳指定数量的值，如果不足则扩展堆栈。
void luaD_checkstack(struct lua_State* L, int need);
//扩展堆栈的大小。
void luaD_growstack(struct lua_State* L, int size);
//抛出一个错误，用于中断当前的函数调用和执行流程。
void luaD_throw(struct lua_State* L, int error);

//保护模式下的函数调用，用于调用指定的C函数并捕获其中可能出现的错误。
int luaD_rawrunprotected(struct lua_State* L, Pfunc f, void* ud);

/**
在保护模式下调用 Lua 函数，给定参数并指定期望的结果数量。
@param L 指向 Lua 状态的指针。
@param func 要调用的函数在栈中的索引。
@param nresult 期望的结果数量。
@return 如果成功调用函数，则返回0，如果发生错误，则返回错误代码。 */
int luaD_precall(struct lua_State* L, StkId func, int nresult);
//在函数调用完成后，做一些清理工作，例如将函数的返回值弹出堆栈、恢复调用信息等等。
int luaD_poscall(struct lua_State* L, StkId first_result, int nresult);
//调用一个Lua函数，并将其返回值压入堆栈中。
int luaD_call(struct lua_State* L, StkId func, int nresult);
//保护模式下的函数调用，用于调用一个Lua函数并捕获其中可能出现的错误，与luaD_rawrunprotected函数类似，但是它会在发生错误时返回一个错误码，而不是直接抛出异常。
int luaD_pcall(struct lua_State* L, Pfunc f, void* ud, ptrdiff_t oldtop, ptrdiff_t ef);

#endif 
