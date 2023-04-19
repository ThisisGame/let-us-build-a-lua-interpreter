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

#include "luastate.h"
#include "luamem.h"
#include "../vm/luagc.h"

typedef struct LX {
    lu_byte extra_[LUA_EXTRASPACE];
    lua_State l;
} LX;

typedef struct LG {
    LX l;
    global_State g;
} LG;

static void stack_init(struct lua_State* L) {
    L->stack = (StkId)luaM_realloc(L, NULL, 0, LUA_STACKSIZE * sizeof(TValue));
    L->stack_size = LUA_STACKSIZE;
    L->stack_last = L->stack + LUA_STACKSIZE - LUA_EXTRASTACK;
    L->previous = NULL;
    L->status = LUA_OK;
    L->errorjmp = NULL;
    L->top = L->stack;
    L->errorfunc = 0;

    int i;
    for (i = 0; i < L->stack_size; i++) {
        setnilvalue(L->stack + i);
    }
    L->top++;

    L->ci = &L->base_ci;
    L->ci->func = L->stack;
    L->ci->top = L->stack + LUA_MINSTACK;
    L->ci->previous = L->ci->next = NULL;
}

/**
 * @brief 创建一个新的Lua状态机对象
 *
 * 该函数分配内存空间用于创建一个新的Lua状态机对象，并进行初始化。它接受
 * 两个参数：`alloc` 和 `ud`。其中，`alloc` 是一个函数指针，用来分配内存；
 * `ud` 则是一个指向分配器传递的用户数据指针。
 *
 * @param alloc 分配器函数指针
 * @param ud 用户自定义数据
 * @return struct lua_State* 返回一个新的Lua状态机指针
 **/
struct lua_State* lua_newstate(lua_Alloc alloc, void* ud) {
    struct global_State* g;
    struct lua_State* L;
    
    struct LG* lg = (struct LG*)(*alloc)(ud, NULL, 0, sizeof(struct LG));
    if (!lg) {
        return NULL;
    }
    g = &lg->g;
    g->ud = ud;
    g->frealloc = alloc;
    g->panic = NULL;
    
    L = &lg->l.l;
    L->nci = 0;
    G(L) = g;
    g->mainthread = L;

    // gc init
    g->gcstate = GCSpause;
    g->currentwhite = bitmask(WHITE0BIT);//将当前Lua状态机使用的“白色”状态设置为1
    g->totalbytes = sizeof(LG);
    g->allgc = NULL;
    g->sweepgc = NULL;
    g->gray = NULL;
    g->grayagain = NULL;
    
    //将当前Lua全局状态变量g中的字段GCdebt设置为0，以重置当前Lua状态机的内存使用情况。
    //垃圾回收器会定期检查这个值，并且根据系统的负载来动态调整垃圾回收的频率和力度。
    g->GCdebt = 0;

    //表示当前没有开始遍历内存。
    //在 Lua 的垃圾回收过程中，会通过标记-清除算法遍历程序中的所有对象，并对被标记的对象进行内存回收。
    //在这个过程中，GCmemtrav 记录着垃圾回收器已经遍历过的内存大小，以便在内存使用量达到一定阈值时触发垃圾回收过程。
    //因此，当程序需要重新进行一轮垃圾回收时，就需要将 GCmemtrav 重置为零.
    //以便在下一轮垃圾回收中重新扫描整个程序的内存区域，准确记录内存使用状况并进行垃圾回收。
    g->GCmemtrav = 0;

    g->GCestimate = 0;//表示当前没有计算内存使用量的估算值。
    g->GCstepmul = LUA_GCSTEPMUL;

    L->marked = luaC_white(g);
    L->gclist = NULL;
    L->tt_ = LUA_TTHREAD;

    stack_init(L);

    return L;
}

#define fromstate(L) (cast(LX*, cast(lu_byte*, (L)) - offsetof(LX, l)))

/**
 * @brief 释放Lua状态机对象中的栈空间
 *
 * 该函数用于释放一个Lua状态机对象中的栈空间。
 *
 * @param L 指向Lua状态机对象的指针
 **/
static void free_stack(struct lua_State* L) {
    // 获取全局状态机对象指针
    global_State* g = G(L);

    // 通过内存分配器来释放栈空间
    luaM_free(L, L->stack, sizeof(TValue));

    // 重置栈相关变量
    L->stack = NULL;
    L->stack_last = NULL;
    L->top = NULL;
    L->stack_size = 0;
}


/**
 * @brief 关闭Lua状态机对象
 *
 * 释放Lua状态机对象相关的所有内存和资源，包括所有线程、栈空间和对象。
 * 注意：只能关闭主线程（mainthread）对应的状态机对象。
 *
 * @param L 指向要关闭的Lua状态机对象的指针
 **/
void lua_close(struct lua_State* L) {
    // 获取全局状态机对象指针
    struct global_State* g = G(L);

    // 获取主线程（mainthread）对应的状态机对象
    struct lua_State* L1 = g->mainthread; // 只有主线程能够被关闭

    // 释放所有栈中的对象
    luaC_freeallobjects(L);

    // 释放所有非主协程数据结构
    struct CallInfo* base_ci = &L1->base_ci;
    struct CallInfo* ci = base_ci->next;

    while(ci) {
        struct CallInfo* next = ci->next;
        struct CallInfo* free_ci = ci;

        // 通过内存分配器释放协程数据结构
        luaM_free(L, free_ci, sizeof(struct CallInfo));
        ci = next;
    }

    // 释放主线程的栈空间
    free_stack(L1);
    
    // 通过全局内存分配器释放主线程对应的状态机对象及其占用的内存
    (*g->frealloc)(g->ud, fromstate(L1), sizeof(LG), 0);
}


void setivalue(StkId target, int integer) {
    target->value_.i = integer;
    target->tt_ = LUA_NUMINT;
}

void setfvalue(StkId target, lua_CFunction f) {
    target->value_.f = f;
    target->tt_ = LUA_TLCF;
}

void setfltvalue(StkId target, float number) {
    target->value_.n = number;
    target->tt_ = LUA_NUMFLT;
}

void setbvalue(StkId target, bool b) {
    target->value_.b = b ? 1 : 0;
    target->tt_ = LUA_TBOOLEAN;
}

void setnilvalue(StkId target) {
    target->tt_ = LUA_TNIL;
}

void setpvalue(StkId target, void* p) {
    target->value_.p = p;
    target->tt_ = LUA_TLIGHTUSERDATA;
}

void setobj(StkId target, StkId value) {
    target->value_ = value->value_;
    target->tt_ = value->tt_;
}

void increase_top(struct lua_State* L) {
    L->top++;
    assert(L->top <= L->stack_last);    
}

void lua_pushcfunction(struct lua_State* L, lua_CFunction f) {
    setfvalue(L->top, f);
    increase_top(L); 
}

void lua_pushinteger(struct lua_State* L, int integer) {
    setivalue(L->top, integer);
    increase_top(L);
}

void lua_pushnumber(struct lua_State* L, float number) {
    setfltvalue(L->top, number);
    increase_top(L);
}

void lua_pushboolean(struct lua_State* L, bool b) {
    setbvalue(L->top, b);
    increase_top(L);
}

void lua_pushnil(struct lua_State* L) {
    setnilvalue(L->top);
    increase_top(L);
}

void lua_pushlightuserdata(struct lua_State* L, void* p) {
    setpvalue(L->top, p);
    increase_top(L);
}

TValue* index2addr(struct lua_State* L, int idx) {
    if (idx >= 0) {
        assert(L->ci->func + idx < L->ci->top);
        return L->ci->func + idx;
    }
    else {
        assert(L->top + idx > L->ci->func);
        return L->top + idx;
    }
}

lua_Integer lua_tointegerx(struct lua_State* L, int idx, int* isnum) {
    lua_Integer ret = 0;
    TValue* addr = index2addr(L, idx); 
    if (addr->tt_ == LUA_NUMINT) {
        ret = addr->value_.i;
        *isnum = 1;
    }
    else {
        *isnum = 0;
        LUA_ERROR(L, "can not convert to integer!\n");
    }

    return ret;
}

lua_Number lua_tonumberx(struct lua_State* L, int idx, int* isnum) {
    lua_Number ret = 0.0f;
    TValue* addr = index2addr(L, idx);
    if (addr->tt_ == LUA_NUMFLT) {
        *isnum = 1;
        ret = addr->value_.n;
    }
    else {
        LUA_ERROR(L, "can not convert to number!");
        *isnum = 0;
    }
    return ret;
}

bool lua_toboolean(struct lua_State* L, int idx) {
    TValue* addr = index2addr(L, idx);
    return !(addr->tt_ == LUA_TNIL || addr->value_.b == 0);
}

int lua_isnil(struct lua_State* L, int idx) {
    TValue* addr = index2addr(L, idx);
    return addr->tt_ == LUA_TNIL;
}

int lua_gettop(struct lua_State* L) {
    return cast(int, L->top - (L->ci->func + 1));
}

void lua_settop(struct lua_State* L, int idx) {
    StkId func = L->ci->func;
    if (idx >=0) {
        assert(idx <= L->stack_last - (func + 1));
        while(L->top < (func + 1) + idx) {
            setnilvalue(L->top++);
        }
        L->top = func + 1 +idx;
    }
    else {
        assert(L->top + idx > func);
        L->top = L->top + idx;
    }
}

void lua_pop(struct lua_State* L) {
    lua_settop(L, -1);
}
