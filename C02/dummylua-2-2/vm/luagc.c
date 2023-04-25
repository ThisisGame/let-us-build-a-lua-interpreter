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

#include "luagc.h"
#include "../common/luamem.h"

#define GCMAXSWEEPGCO 25

#define gettotalbytes(g) (g->totalbytes + g->GCdebt)
#define white2gray(o) resetbits((o)->marked, WHITEBITS)
#define gray2black(o) l_setbit((o)->marked, BLACKBIT)
#define black2gray(o) resetbit((o)->marked, BLACKBIT)
#define sweepwholelist(L, list) sweeplist(L, list, MAX_LUMEM)

struct GCObject* luaC_newobj(struct lua_State* L, int tt_, size_t size) {
    struct global_State* g = G(L);
    struct GCObject* obj = (struct GCObject*)luaM_realloc(L, NULL, 0, size);
    obj->marked = luaC_white(g);
    obj->next = g->allgc;
    obj->tt_ = tt_;
    g->allgc = obj;

    return obj;
}

void reallymarkobject(struct lua_State* L, struct GCObject* gco) {
    struct global_State* g = G(L);
    white2gray(gco);

    switch(gco->tt_) {
        case LUA_TTHREAD:{
            linkgclist(gco2th(gco), g->gray);            
        } break;
        case LUA_TSTRING:{ // just for gc test now
            gray2black(gco);
            g->GCmemtrav += sizeof(struct TString);
        } break;
        default:break;
    }
}

static l_mem get_debt(struct lua_State* L) {
    struct global_State* g = G(L);
    int stepmul = g->GCstepmul; 
    l_mem debt = g->GCdebt;
    if (debt <= 0) {
        return 0;
    }

    debt = debt / STEPMULADJ + 1;
    debt = debt >= (MAX_LMEM / STEPMULADJ) ? MAX_LMEM : debt * g->GCstepmul;

    return debt; 
}

// mark root
static void restart_collection(struct lua_State* L) {
    struct global_State* g = G(L);
    g->gray = g->grayagain = NULL;
    markobject(L, g->mainthread); 
}

static lu_mem traversethread(struct lua_State* L, struct lua_State* th) {
    TValue* o = th->stack;
    for (; o < th->top; o ++) {
        markvalue(L, o);
    }

    return sizeof(struct lua_State) + sizeof(TValue) * th->stack_size + sizeof(struct CallInfo) * th->nci;
}

static void propagatemark(struct lua_State* L) {
    struct global_State* g = G(L);
    if (!g->gray) {
        return;
    }
    struct GCObject* gco = g->gray;
    gray2black(gco);
    lu_mem size = 0;

    switch(gco->tt_) {
        case LUA_TTHREAD:{
            black2gray(gco);
            struct lua_State* th = gco2th(gco);
            g->gray = th->gclist;
            linkgclist(th, g->grayagain);
            size = traversethread(L, th);
        } break;
        default:break;
    }

    g->GCmemtrav += size;
}

static void propagateall(struct lua_State* L) {
    struct global_State* g = G(L);
    while(g->gray) {
        propagatemark(L);
    }
}

static void atomic(struct lua_State* L) {
    struct global_State* g = G(L);
    g->gray = g->grayagain;
    g->grayagain = NULL;

    g->gcstate = GCSinsideatomic;
    propagateall(L);
    g->currentwhite = cast(lu_byte, otherwhite(g));
}

static lu_mem freeobj(struct lua_State* L, struct GCObject* gco) {
    switch(gco->tt_) {
        case LUA_TSTRING: {
            lu_mem sz = sizeof(TString);
            luaM_free(L, gco, sz); 
            return sz; 
        } break;
        default:{
            lua_assert(0);
        } break;
    }
    return 0;
}

/**
 * @brief 扫描并回收Lua中的垃圾对象
 *
 * 该函数扫描链表中的每个GCObject对象。如果对象已经"死亡"，则将其从链表中删除并释放其内存；
 * 如果未标记，则将其标记为白色以便下一次扫描使用；如果已经被标记，则仅将链表指针指向下一个节点。
 * 
 * @param L Lua状态机
 * @param p GCObject对象的列表
 * @param count 已扫描的GCObject计数器
 * @return struct GCObject** 返回链表当前指针
 */
static struct GCObject** sweeplist(struct lua_State* L, struct GCObject** p, size_t count) {
    // 获取全局状态
    struct global_State* g = G(L);
    // 获取另一个白色 (otherwhite) 的标记
    lu_byte ow = otherwhite(g);
    
    // 遍历GCObject对象
    while (*p != NULL && count > 0) {
        // 获取对象的标记
        lu_byte marked = (*p)->marked;
        
        // 判断对象是否是"死亡"状态
        if (isdeadm(ow, marked)) { 
            // 如果对象已经"死亡"，则将其从链表中删除并释放其内存
            struct GCObject* gco = *p;
            *p = (*p)->next;
            g->GCmemtrav += freeobj(L, gco); 
        } 
        else {
            // 将当前对象标记为白色，以备下一次扫描
            (*p)->marked &= cast(lu_byte, ~(bitmask(BLACKBIT) | WHITEBITS));
            (*p)->marked |= luaC_white(g);
            // 指向下一个节点
            p = &((*p)->next);
        }
        // 已经扫描的GCObject计数器减1
        // 该计数器的作用是避免扫描无限循环
        count --; 
    }
    // 如果链表全部扫描完成，则返回NULL，否则返回链表当前指针
    return (*p) == NULL ? NULL : p; 
}


/**
 * @brief 进入垃圾回收状态，将全局状态设置为"全部扫描"并开始扫描所有GCObject对象
 *
 * 该函数用于启动Lua中的垃圾回收操作。首先获取全局状态g，然后将其gcstate设置为GCSsweepallgc（即"全部扫描"）；
 * 最后，使用sweeplist函数对g->allgc链表进行扫描，并将sweepgc设置为链表当前指针。
 *
 * @param L Lua状态机
 */
static void entersweep(struct lua_State* L) {
    // 获取全局状态
    struct global_State* g = G(L);
    
    // 将gcstate设置为"全部扫描"
    g->gcstate = GCSsweepallgc; 
    
    // 对allgc链表进行扫描，并将sweepgc设置为链表当前指针
    g->sweepgc = sweeplist(L, &g->allgc, 1);
}


/**
 * @brief 垃圾回收的扫描工作函数，用于执行垃圾回收操作中的单个步骤
 *
 * 该函数用于依次执行垃圾回收操作中的每个步骤，并在完成所有步骤后返回。
 * 具体来说，函数首先获取全局状态g，然后判断sweepgc是否为空：
 * 若不为空，则使用sweeplist函数对sweepgc链表进行扫描，并将GCestimate设置为当前已经扫描的内存大小；
 * 若sweepgc为空，则将gcstate设置为GCSsweepend并清空sweepgc。
 *
 * @param L Lua状态机
 */
static void sweepstep(struct lua_State* L) {
    // 获取全局状态
    struct global_State* g = G(L);
    
    // 如果sweepgc不为空，则使用sweeplist函数对其进行扫描，并将GCestimate设置为当前已经扫描的内存大小
    if (g->sweepgc) {
        g->sweepgc = sweeplist(L, g->sweepgc, GCMAXSWEEPGCO);
        g->GCestimate = gettotalbytes(g);

        // 如果sweepgc仍不为空，则直接返回
        if (g->sweepgc) {
            return;
        }
    }
    // 设置gcstate为GCSsweepend，并清空sweepgc
    g->gcstate = GCSsweepend;
    g->sweepgc = NULL;
}


static lu_mem singlestep(struct lua_State* L) {
    struct global_State* g = G(L);
    switch(g->gcstate) {
        case GCSpause: {
            g->GCmemtrav = 0;
            restart_collection(L);
            g->gcstate = GCSpropagate;
            return g->GCmemtrav;
        } break;
        case GCSpropagate:{
            g->GCmemtrav = 0;
            propagatemark(L);
            if (g->gray == NULL) {
                g->gcstate = GCSatomic;
            }
            return g->GCmemtrav;
        } break;
        case GCSatomic:{
            g->GCmemtrav = 0;
            if (g->gray) {
                propagateall(L);
            }
            atomic(L);
            entersweep(L);
            g->GCestimate = gettotalbytes(g);
            return g->GCmemtrav;
        } break;
        case GCSsweepallgc: {
            g->GCmemtrav = 0;
            sweepstep(L);
            return g->GCmemtrav;
        } break;
        case GCSsweepend: {
            g->GCmemtrav = 0;
            g->gcstate = GCSpause;
            return 0;
        } break;
        default:break;
    }

    return g->GCmemtrav;
}

/**
 * @brief 设置垃圾回收器的内存限制
 * 
 * @param L 指针类型，指向当前Lua状态的结构体
 * @param debt l_mem类型整数，表示应该还给系统的内存数量
 */
static void setdebt(struct lua_State* L, l_mem debt) {
    // 获取全局变量g
    struct global_State* g = G(L);
    // 获取总内存字节数
    lu_mem totalbytes = gettotalbytes(g);

    // 设置总内存字节数为原值减去应该还给系统的内存数量
    g->totalbytes = totalbytes - debt;
    // 将应该还给系统的内存数量分配给GCdebt字段
    g->GCdebt = debt;
}

// when memory is twice bigger than current estimate, it will trigger gc
// again
static void setpause(struct lua_State* L) {
    struct global_State* g = G(L);
    l_mem estimate = g->GCestimate / GCPAUSE;
    estimate = (estimate * g->GCstepmul) >= MAX_LMEM ? MAX_LMEM : estimate * g->GCstepmul;
    
    l_mem debt = g->GCestimate - estimate;
    setdebt(L, debt);
}

/**
 * @brief 执行一轮垃圾回收
 * 
 * @param L Lua的状态机
 */
void luaC_step(struct lua_State*L) {
    // 获取Lua的全局状态
    struct global_State* g = G(L);
    // 获取当前状态机下的内存消耗
    l_mem debt = get_debt(L);
    do {
        // 单次执行垃圾回收
        l_mem work = singlestep(L);
        // 更新内存消耗
        debt -= work;
    } while (debt > -GCSTEPSIZE && G(L)->gcstate != GCSpause); // 判断是否需要继续执行

    // 判断当前的GC状态
    if (G(L)->gcstate == GCSpause) {
        // 如果是GC暂停，则调用setpause函数
        setpause(L);
    } else {
        // 否则重新计算debt的值
        debt = g->GCdebt / STEPMULADJ * g->GCstepmul;
        // 设置新的内存消耗值
        setdebt(L, debt);
    }
}


/**
 * @brief 释放所有无用对象
 * 
 * @param L Lua的状态机
 */
void luaC_freeallobjects(struct lua_State* L) {
    // 获取Lua的全局状态
    struct global_State* g = G(L);
    // 将currentwhite设置为WHITEBITS，表示需要回收所有GC对象
    g->currentwhite = WHITEBITS; // all gc objects must reclaim
    // 执行对allgc链表的遍历及清理操作
    sweepwholelist(L, &g->allgc);
}

