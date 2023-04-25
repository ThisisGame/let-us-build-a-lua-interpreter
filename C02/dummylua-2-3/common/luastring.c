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

#include "luastring.h"
#include "luamem.h"

#define MINSTRTABLESIZE 128
#define lmod(hash, size) ((hash) & (size - 1))

#define MEMERRMSG "not enough memory"

/**
 * @brief Initializes the string cache of a Lua state.
 * 
 * @param L Pointer to the Lua state
 */
void luaS_init(struct lua_State* L) {
    struct global_State* g = G(L);
    g->strt.nuse = 0; // 初始化全局状态机中保存字符串的数量
    g->strt.size = 0; // 字符串缓存区大小初始化为0，后续会动态增加
    g->strt.hash = NULL; // 哈希表指针初始化为NULL
    luaS_resize(L, MINSTRTABLESIZE); // 初始化并调整字符串缓存区空间
    // 创建内存错误提示信息和将其添加至垃圾收集器根节点
    g->memerrmsg = luaS_newlstr(L, MEMERRMSG, strlen(MEMERRMSG)); 
    luaC_fix(L, obj2gco(g->memerrmsg));

    // strcache table can not hold the string objects which will be sweep soon
    for (int i = 0; i < STRCACHE_M; i ++) { // 遍历每一个字符串缓存槽位
        for (int j = 0; j < STRCACHE_N; j ++) // 每个槽位中可能有多个对象，因此需要再次遍历
            g->strcache[i][j] = g->memerrmsg; // 将内存错误提示信息加入该槽位
    }
}


/**
 * @brief 这个函数是用来改变字符串表stringtable的大小，仅被短字符串使用。
 *        通常第二个参数必须是2的幂。
 *
 * @param L lua_State 状态机指针
 * @param nsize unsigned int 新的字符串表大小
 * 
 * @return unsigned int 返回新的字符串表大小
 */
int luaS_resize(struct lua_State* L, unsigned int nsize) {
    struct global_State* g = G(L);
    unsigned int osize = g->strt.size;
    if (nsize > osize) {
        // 重新分配hash表内存空间，并将新增的部分置为NULL
        luaM_reallocvector(L, g->strt.hash, osize, nsize, TString*);
        for (int i = osize; i < nsize; i ++) {
            g->strt.hash[i] = NULL;
        }
    }

    // 重新hash所有TString值到新的nsize上
    for (int i = 0; i < g->strt.size; i ++) {
       struct TString* ts = g->strt.hash[i];
       g->strt.hash[i] = NULL;

       while(ts) {
            struct TString* old_next = ts->u.hnext;
            unsigned int hash = lmod(ts->hash, nsize);
            ts->u.hnext = g->strt.hash[hash];
            g->strt.hash[hash] = ts;
            ts = old_next;
       }
    }

    // 如果新的字符串表大小小于原始大小，则缩小字符串哈希表。
    if (nsize < osize) {
        // 为了避免删除关键字符串，我们需要确保结束范围内的桶为空。
        lua_assert(g->strt.hash[nsize] == NULL && g->strt.hash[osize - 1] == NULL);
        luaM_reallocvector(L, g->strt.hash, osize, nsize, TString*);
    }
    g->strt.size = nsize;

    return g->strt.size;
}


static struct TString* createstrobj(struct lua_State* L, const char* str, int tag, unsigned int l, unsigned int hash) {
    size_t total_size = sizelstring(l);

    struct GCObject* o = luaC_newobj(L, tag, total_size);
    struct TString* ts = gco2ts(o);
    memcpy(getstr(ts), str, l * sizeof(char));
    getstr(ts)[l] = '\0';
    ts->extra = 0;

    if (tag == LUA_SHRSTR) {
        ts->shrlen = cast(lu_byte, l);
        ts->hash = hash;
        ts->u.hnext = NULL;
    }
    else if (tag == LUA_LNGSTR) {
        ts->hash = 0;
        ts->u.lnglen = l;
    }
    else {
        lua_assert(0);
    }
    
    return ts;
}

// only short strings can be interal
static struct TString* internalstr(struct lua_State* L, const char* str, unsigned int l) {
    struct global_State* g = G(L);
    struct stringtable* tb = &g->strt;
    unsigned int h = luaS_hash(L, str, l, g->seed); 
    struct TString** list = &tb->hash[lmod(h, tb->size)];

    for (struct TString* ts = *list; ts; ts = ts->u.hnext) {
        if (ts->shrlen == l && (memcmp(getstr(ts), str, l * sizeof(char)) == 0)) {
            if (isdead(g, ts)) {
                changewhite(ts);
            }
            return ts;
        }
    }

    if (tb->nuse >= tb->size && tb->size < INT_MAX / 2) {
        luaS_resize(L, tb->size * 2);
        list = &tb->hash[lmod(h, tb->size)];
    }

    struct TString* ts = createstrobj(L, str, LUA_SHRSTR, l, h);
    ts->u.hnext = *list;
    *list = ts;
    tb->nuse++;

    return ts;
}

struct TString* luaS_newlstr(struct lua_State* L, const char* str, unsigned int l) {
    if (l <= MAXSHORTSTR) {
        return internalstr(L, str, l); 
    }
    else {
        return luaS_createlongstr(L, str, l); 
    }
}

struct TString* luaS_new(struct lua_State* L, const char* str, unsigned int l) {
    unsigned int hash = point2uint(str); 
    int i = hash % STRCACHE_M;
    for (int j = 0; j < STRCACHE_N; j ++) {
        struct TString* ts = G(L)->strcache[i][j];
        if (strcmp(getstr(ts), str) == 0) {
            return ts;
        }
    }

    for (int j = STRCACHE_N - 1; j > 0; j--) {
        G(L)->strcache[i][j] = G(L)->strcache[i][j - 1];
    }

    G(L)->strcache[i][0] = luaS_newlstr(L, str, l);
    return G(L)->strcache[i][0];
}

// remove TString from stringtable, only for short string
void luaS_remove(struct lua_State* L, struct TString* ts) {
    struct global_State* g = G(L);
    struct TString** list = &g->strt.hash[lmod(ts->hash, g->strt.size)];

    for (struct TString* o = *list; o; o = o->u.hnext) {
        if (o == ts) {
            *list = o->u.hnext;
            break;
        }
        list = &(*list)->u.hnext;
    }
}

void luaS_clearcache(struct lua_State* L) {
    struct global_State* g = G(L);
    for (int i = 0; i < STRCACHE_M; i ++) {
        for (int j = 0; j < STRCACHE_N; j ++) {
            if (iswhite(g->strcache[i][j])) {
                g->strcache[i][j] = g->memerrmsg;
            }
        }
    }
}

int luaS_eqshrstr(struct lua_State* L, struct TString* a, struct TString* b) {
    return (a == b) || (a->shrlen == b->shrlen && strcmp(getstr(a), getstr(b)) == 0);
}

int luaS_eqlngstr(struct lua_State* L, struct TString* a, struct TString* b) {
    return (a == b) || (a->u.lnglen == b->u.lnglen && strcmp(getstr(a), getstr(b)) == 0);
}

unsigned int luaS_hash(struct lua_State* L, const char* str, unsigned int l, unsigned int h) {
    h = h ^ l;
    unsigned int step = (l >> 5) + 1;
    for (int i = 0; i < l; i = i + step) {
        h ^= (h << 5) + (h >> 2) + cast(lu_byte, str[i]);
    }
    return h;
}

unsigned int luaS_hashlongstr(struct lua_State* L, struct TString* ts) {
    if (ts->extra == 0) {
        ts->hash = luaS_hash(L, getstr(ts), ts->u.lnglen, G(L)->seed);
        ts->extra = 1;
    }
    return ts->hash;
}

struct TString* luaS_createlongstr(struct lua_State* L, const char* str, size_t l) {
    return createstrobj(L, str, LUA_LNGSTR, l, G(L)->seed);
}
