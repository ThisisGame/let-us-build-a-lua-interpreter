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

#include "luado.h"
#include "../common/luamem.h"

#define LUA_TRY(L, c, a) if (_setjmp((c)->b) == 0) { a } 

#ifdef _WINDOWS_PLATFORM_ 
#define LUA_THROW(c) longjmp((c)->b, 1) 
#else 
#define LUA_THROW(c) _longjmp((c)->b, 1) 
#endif 

struct lua_longjmp {
    struct lua_longjmp* previous;
    jmp_buf b;
    int status;
};

void seterrobj(struct lua_State* L, int error) {
    lua_pushinteger(L, error);
}

void luaD_checkstack(struct lua_State* L, int need) {
    if (L->top + need > L->stack_last) {
        luaD_growstack(L, need);
    }
}


void luaD_growstack(struct lua_State* L, int size) {
    if (L->stack_size > LUA_MAXSTACK) {
        luaD_throw(L, LUA_ERRERR);
    }

    int stack_size = L->stack_size * 2;
    int need_size = ((int)(L->top - L->stack)) + size + LUA_EXTRASTACK;
    if (stack_size < need_size) {
        stack_size = need_size;
    }
    
    if (stack_size > LUA_MAXSTACK) {
        stack_size = LUA_MAXSTACK + LUA_ERRORSTACK;
        LUA_ERROR(L, "stack overflow");
    } 

    TValue* old_stack = L->stack;
    L->stack = luaM_realloc(L, L->stack, L->stack_size, stack_size * sizeof(TValue));
    L->stack_size = stack_size;
    L->stack_last = L->stack + stack_size - LUA_EXTRASTACK;
    int top_diff = cast(int, L->top - old_stack);
    L->top = restorestack(L, top_diff);

    struct CallInfo* ci;
    ci = &L->base_ci;
    while(ci) {
        int func_diff = cast(int, ci->func - old_stack);
        int top_diff = cast(int, ci->top - old_stack);
        ci->func = restorestack(L, func_diff);
        ci->top = restorestack(L, top_diff);

        ci = ci->next;
    }
}


/**
@brief 抛出 Lua 异常
@param L Lua 状态机指针
@param error 异常代码 
**/ 
void luaD_throw(struct lua_State* L, int error) {
    struct global_State* g = G(L);
    if (L->errorjmp) {
        L->errorjmp->status = error;
        LUA_THROW(L->errorjmp);
    }
    else {
        if (g->panic) {
            (*g->panic)(L);
        }
        abort();
    }
}

int luaD_rawrunprotected(struct lua_State* L, Pfunc f, void* ud) {
    int old_ncalls = L->ncalls;//保存当前调用栈的深度，以便将来恢复。
    struct lua_longjmp lj;//创建一个新的longjmp结构，用于保存错误处理信息。
    lj.previous = L->errorjmp;//
    lj.status = LUA_OK;
    L->errorjmp = &lj;

    LUA_TRY(
        L,
        L->errorjmp,
        (*f)(L, ud);
    )

    L->errorjmp = lj.previous;//如果函数f成功运行，则将longjmp结构从错误处理栈中移除，
    L->ncalls = old_ncalls;//恢复之前保存的调用栈深度
    return lj.status;
}

/**
@brief 获取下一个CallInfo结构体。
此函数用于获取给定lua_State、函数和结果数量的下一个CallInfo结构体。
在Lua中，每次函数调用都会创建一个新的CallInfo对象，并将其添加到调用栈中。这个函数用于创建新的CallInfo对象，并将其添加到调用栈中。
@param L 要获取下一个CallInfo结构体的lua_State。
@param func 要获取下一个CallInfo结构体的函数。
@param nresult 下一个CallInfo结构体的结果数量。
@return 返回指向下一个CallInfo结构体的指针。 
**/
static struct CallInfo* next_ci(struct lua_State* L, StkId func, int nresult) {
    struct global_State* g = G(L);
    struct CallInfo* ci;//创建一个新的CallInfo对象,CallInfo结构体是用于协调Lua函数调用过程的数据结构。
    ci = luaM_realloc(L, NULL, 0, sizeof(struct CallInfo));
    ci->next = NULL;//将其插入到当前lua_State的CallInfo链表中
    ci->previous = L->ci;
    L->ci->next = ci;
    ci->nresult = nresult;
    ci->callstatus = LUA_OK;
    ci->func = func;
    ci->top = L->top + LUA_MINSTACK;//将当前函数的栈顶指针 ci->top 设置为 L->top 向上增加 LUA_MINSTACK 的位置，以确保函数运行时有足够的栈空间。
    L->ci = ci;//将其设置为当前活动的CallInfo对象，并返回其指针。

    return ci;
}

/**

@brief 在 Lua 虚拟机中执行函数调用。
该函数首先根据函数类型进行判断，如果是 C 函数类型，则将函数指针取出，保存函数在栈中的位置，并为栈分配一个最小的 Lua 栈空间（LUA_MINSTACK）。
然后调用 next_ci 函数来设置新的 CallInfo，将函数指针和结果数量作为参数。
接着调用 C 函数执行，将返回值数量保存为 n。
最后，调用 luaD_poscall 函数以完成函数调用过程，并返回 1。
如果函数类型不是 C 函数，则直接返回 0。
@param L 要执行函数调用的 lua_State。
@param func 要调用的函数在栈中的位置。
@param nresult 调用函数后期望返回的结果数量。
@return 如果是 C 函数类型，返回 1；否则返回 0。 
**/
int luaD_precall(struct lua_State* L, StkId func, int nresult) {
    switch(func->tt_) {
        case LUA_TLCF: {//如果是 C 函数类型
            lua_CFunction f = func->value_.f;//将函数指针取出

            ptrdiff_t func_diff = savestack(L, func);//保存函数在栈中的位置,savestack 和 restorestack 函数用于在栈上保存和恢复值的位置.
            luaD_checkstack(L, LUA_MINSTACK);//并为栈分配一个最小的 Lua 栈空间（LUA_MINSTACK）
            func = restorestack(L, func_diff);

            next_ci(L, func, nresult);  //设置新的 CallInfo，将函数指针和结果数量作为参数。                      
            int n = (*f)(L);//调用 C 函数执行，将返回值数量保存为 n。
            assert(L->ci->func + n <= L->ci->top);
            luaD_poscall(L, L->top - n, n);//完成函数调用过程，并返回 1。
            return 1; 
        } break;
        default:break;
    }

    return 0;
}

/**

@brief 执行函数调用后的操作，处理 Lua 函数的返回结果和错误。
在 Lua 函数返回后，该函数会处理返回结果和可能发生的错误。
@param L Lua 状态。
@param first_result 函数调用的第一个返回结果（在栈顶）。
@param nresult 函数返回结果的数量。 
**/
int luaD_poscall(struct lua_State* L, StkId first_result, int nresult) {
    StkId func = L->ci->func;//首先获取当前函数在栈中的位置
    int nwant = L->ci->nresult;//获取当前函数期望的返回结果数量

    switch(nwant) {//根据 函数期望的返回结果数量 的不同情况进行分支判断：
        case 0: {
            L->top = L->ci->func;//将栈顶指针 L->top 指向当前函数的位置。
        } break;
        case 1: {
            if (nresult == 0) {
                first_result->value_.p = NULL;
                first_result->tt_ = LUA_TNIL;
            }
            //将第一个返回结果的值存储到当前函数的位置，并将栈顶指针指向当前函数的下一个位置。
            //setobj 函数用于将一个栈值复制到另一个栈值位置
            setobj(func, first_result);
            first_result->value_.p = NULL;
            first_result->tt_ = LUA_TNIL;

            L->top = func + nwant;
        } break;
        case LUA_MULRET: {
            int nres = cast(int, L->top - first_result);
            int i;
            for (i = 0; i < nres; i++) {
                StkId current = first_result + i;
                setobj(func + i, current);
                current->value_.p = NULL;
                current->tt_ = LUA_TNIL;
            }
            L->top = func + nres;
        } break;
        default: {
            if (nwant > nresult) {//期望的结果数量大于返回结果的数量,则将多余的返回结果设置为 nil 值
                int i;
                for (i = 0; i < nwant; i++) {
                    if (i < nresult) {
                        StkId current = first_result + i;
                        setobj(func + i, current);
                        current->value_.p = NULL;
                        current->tt_ = LUA_TNIL;
                    }
                    else {
                        StkId stack = func + i;
                        stack->tt_ = LUA_TNIL;
                    }
                }
                L->top = func + nwant;
            }
            else {//否则，将多余的期望结果设置为 nil 值，并将返回结果的值存储到当前函数的位置。
                int i;
                for (i = 0; i < nresult; i++) {
                    if (i < nwant) {
                        StkId current = first_result + i;
                        setobj(func + i, current);
                        current->value_.p = NULL;
                        current->tt_ = LUA_TNIL;
                    }
                    else {
                        StkId stack = func + i;
                        stack->value_.p = NULL;
                        stack->tt_ = LUA_TNIL; 
                    }
                }
                L->top = func + nresult;
            } 
        } break;
    }

    //最后，将当前 CallInfo 结构体弹出，并释放其内存。
    struct CallInfo* ci = L->ci;
    L->ci = ci->previous;
    L->ci->next = NULL;
    
    // because we have not implement gc, so we should free ci manually
    struct global_State* g = G(L);
    (*g->frealloc)(g->ud, ci, sizeof(struct CallInfo), 0); 

    return LUA_OK;
}


/**
调用一个 Lua 函数
@param L Lua 解释器的状态指针
@param func 函数栈指针，指向要调用的函数
@param nresult 期望的返回值数量
@return 返回值为 LUA_OK，表示调用成功
**/
int luaD_call(struct lua_State* L, StkId func, int nresult) {
    if (++L->ncalls > LUA_MAXCALLS) {//判断当前调用的层数是否已经超过了最大层数
        luaD_throw(L, 0);
    }

    if (!luaD_precall(L, func, nresult)) {
        // TODO luaV_execute(L);
    }
    
    L->ncalls--;//执行完毕后，更新当前调用层数，并返回 LUA_OK。
    return LUA_OK;
}

/**
重置尚未使用的栈空间
@param L Lua 解释器的状态指针
@param old_top 旧栈顶的偏移量 
**/
static void reset_unuse_stack(struct lua_State* L, ptrdiff_t old_top) {
    struct global_State* g = G(L);
    StkId top = restorestack(L, old_top);//调用 restorestack 函数，将栈顶位置恢复到 old_top。
    for (; top < L->top; top++) {//遍历从 old_top 位置开始到当前栈顶位置（L->top）的所有栈空间
        if (top->value_.p) {// 如果值为指针类型，调用全局状态的内存分配函数（g->frealloc）释放其内存空间，并将其指针值设为 NULL。
            (*g->frealloc)(g->ud, top->value_.p, sizeof(top->value_.p), 0);
            top->value_.p = NULL; 
        }
        top->tt_ = LUA_TNIL;//将该栈空间的类型设置为 LUA_TNIL。
    }
}

int luaD_pcall(struct lua_State* L, Pfunc f, void* ud, ptrdiff_t oldtop, ptrdiff_t ef) {
    int status;
    struct CallInfo* old_ci = L->ci;//保存当前的调用信息（ci）和错误处理函数（errorfunc）。
    ptrdiff_t old_errorfunc = L->errorfunc;    
    
    status = luaD_rawrunprotected(L, f, ud);//调用luaD_rawrunprotected函数执行待执行的函数 f，并返回执行状态。
    if (status != LUA_OK) {//如果执行状态不是LUA_OK，说明执行过程中出现了错误，需要进行错误处理
        // because we have not implement gc, so we should free ci manually
        struct global_State* g = G(L);
        struct CallInfo* free_ci = L->ci;
        while(free_ci) {
            if (free_ci == old_ci) {//如果当前的调用信息是调用luaD_pcall函数之前的调用信息（old_ci），则跳过该调用信息，继续遍历下一个调用信息。
                free_ci = free_ci->next;
                continue;
            }

            struct CallInfo* previous = free_ci->previous;//否则，将当前调用信息的前一个调用信息（previous）的next指针置为NULL，以便将当前调用信息从链表中删除。
            previous->next = NULL;
            
            struct CallInfo* next = free_ci->next;//记录下一个调用信息（next），用于下一次循环遍历。
            (*g->frealloc)(g->ud, free_ci, sizeof(struct CallInfo), 0);//调用free函数释放当前调用信息占用的内存空间。
            free_ci = next;
        }
        
        reset_unuse_stack(L, oldtop);//重置未使用的栈空间
        L->ci = old_ci;//恢复调用信息（ci）
        L->top = restorestack(L, oldtop);
        seterrobj(L, status);//设置错误信息
    }
    
    L->errorfunc = old_errorfunc; 
    return status;
}
