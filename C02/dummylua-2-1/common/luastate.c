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
    L->next = L->previous = NULL;
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

struct lua_State* lua_newstate(lua_Alloc alloc, void* ud) {
    struct global_State* g;
    struct lua_State* L;
    
    struct LG* lg = (struct LG*)(*alloc)(ud, NULL, LUA_TTHREAD, sizeof(struct LG));
    if (!lg) {
        return NULL;
    }
    g = &lg->g;
    g->ud = ud;
    g->frealloc = alloc;
    g->panic = NULL;
    
    L = &lg->l.l;
    G(L) = g;
    g->mainthread = L;

    stack_init(L);

    return L;
}

#define fromstate(L) (cast(LX*, cast(lu_byte*, (L)) - offsetof(LX, l)))

static void free_stack(struct lua_State* L) {
    global_State* g = G(L);
    (*g->frealloc)(g->ud, L->stack, sizeof(TValue), 0);
    L->stack = L->stack_last = L->top = NULL;
    L->stack_size = 0;
}

void lua_close(struct lua_State* L) {
    struct global_State* g = G(L);
    struct lua_State* L1 = g->mainthread; // only mainthread can be close
    
    // because I have not implement gc, so we should free ci manual 
    struct CallInfo* ci = &L1->base_ci;
    while(ci->next) {
        struct CallInfo* next = ci->next->next;
        struct CallInfo* free_ci = ci->next;

        (*g->frealloc)(g->ud, free_ci, sizeof(struct CallInfo), 0);
        ci = next;
    }

    free_stack(L1);    
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


//在Lua虚拟机中，栈是一个连续的内存区域，每个栈元素（即一条指令或一个值）都被分配了一个索引（index），从1开始递增。
//在函数调用时，栈上的前若干个元素被用来存储函数的参数，而剩余的元素则被用来存储函数的局部变量和中间结果。这些元素的索引从1开始递增，直到栈顶（top）。
//因此，如果指定的栈索引位置idx大于等于0，表示该索引指向的值是当前函数调用的局部变量之一，其对应的TValue类型的指针可以通过该函数计算得到。
//而如果索引位置idx小于0，则表示该索引指向的值是当前函数调用栈之外的值，其对应的TValue类型的指针也可以通过该函数计算得到。
static TValue* index2addr(struct lua_State* L, int idx) {
    if (idx >= 0) {//如果指定的栈索引位置idx大于等于0，表示该栈索引位置在当前函数调用的局部变量中。
        assert(L->ci->func + idx < L->ci->top);//则根据当前的调用信息（ci）和指定的栈索引位置idx，计算出该局部变量的TValue类型的指针，并返回。
        return L->ci->func + idx;
    }
    else {
        assert(L->top + idx > L->ci->func);//指定的栈索引位置idx在当前函数调用的调用栈之外。
        return L->top + idx;//则根据当前的栈顶（top）和指定的栈索引位置idx，计算出该位置的TValue类型的指针，并返回。
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

//返回当前栈顶的索引，即栈中元素的数量。
int lua_gettop(struct lua_State* L) {
    return cast(int, L->top - (L->ci->func + 1));
}

/**
设置栈顶的位置
@param L Lua 状态机指针
@param idx 栈顶的目标位置 
**/
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

//弹出栈顶的一个元素，等价于 lua_settop(L, -1)。
void lua_pop(struct lua_State* L) {
    lua_settop(L, -1);
}
