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

#include "luamem.h"
#include "../vm/luado.h"

/**
 * 重新分配 Lua 的内存。
 *
 * @param L Lua 状态。
 * @param ptr 指向要重新分配的内存块的指针。
 * @param osize 内存块的旧大小。
 * @param nsize 内存块的新大小。
 * @return 指向重新分配的内存块的指针。
 * @throws LUA_ERRMEM 如果重新分配失败。
 */
void* luaM_realloc(struct lua_State* L, void* ptr, size_t osize, size_t nsize) {
    // 获取Lua状态的全局状态。
    struct global_State* g = G(L);

    // 计算内存块的旧大小。
    int oldsize = ptr ? osize : 0;

    // 使用全局状态的frealloc函数重新分配内存。
    void* ret = (*g->frealloc)(g->ud, ptr, oldsize, nsize);

    // 如果重新分配失败，则抛出错误。
    if (ret == NULL) {
        luaD_throw(L, LUA_ERRMEM);
    }

    // 返回指向重新分配的内存块的指针。
    return ret;
}
