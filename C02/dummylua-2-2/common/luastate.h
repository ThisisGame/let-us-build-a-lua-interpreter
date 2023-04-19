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

#ifndef luastate_h
#define luastate_h 

#include "luaobject.h"

#define LUA_EXTRASPACE sizeof(void*) 
#define G(L) ((L)->l_G)

//每次完整的垃圾回收之后，下一次垃圾回收器遍历内存块大小的增量。
//该值为 200，因此每次垃圾回收之后，下一次遍历内存块总大小会增加 200 字节。
#define STEPMULADJ 200
//表示垃圾回收器的速度倍率。每当Lua运行了这么多步骤，垃圾回收器会被触发。
//具体来说，这个值代表在启动新一轮垃圾回收之前，Lua执行的步骤数目要乘以这个值。
//该值也是 200，所以Lua需要执行200*GCSTEPSIZE个步骤才会启动新一轮的垃圾回收。
#define GCSTEPMUL 200 
//垃圾回收器使用的内存块大小，即Lua堆中每个内存块占用的大小（字节数）。
//该值为 1024，即 1KB。因此，每当Lua使用的内存大小达到 1KB，垃圾回收器就会被触发。
#define GCSTEPSIZE 1024  //1kb
//当Lua的堆大小超过可接受的范围时垃圾回收器的停顿时间（以毫秒为单位）。
//如果垃圾回收时间过长，会影响Lua的性能，所以需要在尽可能短的时间内完成垃圾回收。
//该值为 100 毫秒，意味着垃圾回收器在执行时最多只能让程序暂停100毫秒，超过这个时间就必须返回控制权。
#define GCPAUSE 100

typedef TValue* StkId;

struct CallInfo {
    StkId func;
    StkId top;
    int nresult;
    int callstatus;
    struct CallInfo* next;
    struct CallInfo* previous;
};

typedef struct lua_State {
    CommonHeader;       // gc header, all gcobject should have the commonheader
    StkId stack;
    StkId stack_last;
    StkId top;
    int stack_size;
    struct lua_longjmp* errorjmp;
    int status;
    struct lua_State* previous;
    struct CallInfo base_ci;
    struct CallInfo* ci;
    int nci;
    struct global_State* l_G;
    ptrdiff_t errorfunc;
    int ncalls;
    struct GCObject* gclist;
} lua_State;

typedef struct global_State {
    struct lua_State* mainthread;
    lua_Alloc frealloc;
    void* ud; 
    lua_CFunction panic;

    //gc fields
    lu_byte gcstate;
    lu_byte currentwhite;
    struct GCObject* allgc;         // gc root set
    struct GCObject** sweepgc;
    struct GCObject* gray;
    struct GCObject* grayagain;
    lu_mem totalbytes;

    //当前 Lua 堆栈中的所有对象所占据的内存大小之和，也就是待回收的内存量。
    l_mem GCdebt;

    //GCmemtrav 变量的作用是记录垃圾回收器当前正在遍历的内存块的总大小。
    //在 Lua 的垃圾回收过程中，需要对堆中的所有内存块进行遍历，以找出哪些内存块可以被释放。
    //由于堆中的内存块数量可能非常大，因此 Lua 采用了增量式垃圾回收算法，
    //在遍历内存块时将其分成多个小步骤进行处理，从而避免一次性遍历过多的内存块导致程序卡顿甚至崩溃。
    //而 GCmemtrav 变量就是用来记录当前垃圾回收步骤已经遍历的内存块总大小。
    //这个值通常会与 GCdebt 变量一起使用，帮助 Lua 动态调整垃圾回收器的行为。
    lu_mem GCmemtrav;
    //GCestimate 变量的作用是记录垃圾回收器执行完一次完整的垃圾回收后，估算出下次垃圾回收需要遍历的内存块总大小。
    //这个值可以帮助 Lua 优化垃圾回收器的性能，
    //因为如果知道下次垃圾回收需要遍历的内存块总大小，就可以根据这个值来分配合适的垃圾回收步骤，
    //从而避免遍历过多的内存块导致程序卡顿甚至崩溃。
    //因此，GCestimate 变量在动态调整垃圾回收器行为方面起着重要的作用。
    lu_mem GCestimate;
    int GCstepmul;
} global_State;

// GCUnion
union GCUnion {
    struct GCObject gc;
    lua_State th;
};

struct lua_State* lua_newstate(lua_Alloc alloc, void* ud);
void lua_close(struct lua_State* L);

void setivalue(StkId target, int integer);
void setfvalue(StkId target, lua_CFunction f);
void setfltvalue(StkId target, float number);
void setbvalue(StkId target, bool b);
void setnilvalue(StkId target);
void setpvalue(StkId target, void* p);

void setobj(StkId target, StkId value);

void increase_top(struct lua_State* L);
void lua_pushcfunction(struct lua_State* L, lua_CFunction f);
void lua_pushinteger(struct lua_State* L, int integer);
void lua_pushnumber(struct lua_State* L, float number);
void lua_pushboolean(struct lua_State* L, bool b);
void lua_pushnil(struct lua_State* L);
void lua_pushlightuserdata(struct lua_State* L, void* p);

lua_Integer lua_tointegerx(struct lua_State* L, int idx, int* isnum);
lua_Number lua_tonumberx(struct lua_State* L, int idx, int* isnum);
bool lua_toboolean(struct lua_State* L, int idx);
int lua_isnil(struct lua_State* L, int idx);

void lua_settop(struct lua_State* L, int idx);
int lua_gettop(struct lua_State* L);
void lua_pop(struct lua_State* L);

/**
 * @brief 获取Lua栈上指定位置的值，并返回该值在内存中的地址
 *
 * @param L 指向lua_State结构体的指针，用于表示当前Lua状态机的上下文
 * @param idx 代表在Lua栈上查找值时的位置，可以是正数或负数
 * @return TValue* 返回一个指向TValue类型的指针，该类型定义在Lua源码中，
 *                代表任何一种Lua值（如整数，字符串，函数等）在内存中的结构。
 *                通过调用index2addr函数并传入相应的参数，就可以获得某个特定值在内存中的地址。
 */ 
TValue* index2addr(struct lua_State* L, int idx);

#endif 
