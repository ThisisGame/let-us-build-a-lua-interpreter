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


/**
 * @brief 这个函数用于创建一个新的字符串对象。
 *
 * @param L lua_State 状态机指针
 * @param str const char* 字符串起始地址
 * @param tag int 字符串类型（LUA_SHRSTR或LUA_LNGSTR）
 * @param l unsigned int 字符串长度
 * @param hash unsigned int 字符串hash值
 * 
 * @return struct TString* 返回一个新的TString对象
 */
static struct TString* createstrobj(struct lua_State* L, const char* str, int tag, unsigned int l, unsigned int hash) {
    size_t total_size = sizelstring(l);

    // 创建新的对象并填充内存
    struct GCObject* o = luaC_newobj(L, tag, total_size);
    struct TString* ts = gco2ts(o);
    memcpy(getstr(ts), str, l * sizeof(char));
    getstr(ts)[l] = '\0';
    ts->extra = 0;

    // 填充不同类型的字段
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
/**
 * @brief 这个函数用于创建一个内部字符串。
 *
 * @param L lua_State 状态机指针
 * @param str const char* 字符串起始地址
 * @param l unsigned int 字符串长度
 * 
 * @return struct TString* 返回一个新的TString对象
 */
static struct TString* internalstr(struct lua_State* L, const char* str, unsigned int l) {
    // 获取全局状态和字符串表
    struct global_State* g = G(L);
    struct stringtable* tb = &g->strt;
    // 计算字符串hash值，从字符串表中查找该字符串是否已经存在
    unsigned int h = luaS_hash(L, str, l, g->seed); 
    struct TString** list = &tb->hash[lmod(h, tb->size)];

    for (struct TString* ts = *list; ts; ts = ts->u.hnext) {
        if (ts->shrlen == l && (memcmp(getstr(ts), str, l * sizeof(char)) == 0)) {
            // 如果该字符串已经存在，并且已经被回收，则重新标记为白色
            if (isdead(g, ts)) {
                changewhite(ts);
            }
            return ts;
        }
    }

    // 如果该字符串不存在，则创建一个新的TString对象，并插入字符串表中
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


/**
 * @brief 创建一个新的字符串，并返回TString对象指针。
 *
 * @param L lua_State 状态机指针
 * @param str const char* 字符串起始地址
 * @param l unsigned int 字符串长度
 * 
 * @return struct TString* 返回一个新的TString对象
 */
struct TString* luaS_newlstr(struct lua_State* L, const char* str, unsigned int l) {
    // 如果字符串长度小于等于MAXSHORTSTR，则调用internalstr函数创建短字符串
    if (l <= MAXSHORTSTR) {
        return internalstr(L, str, l); 
    }
    else {
        // 否则调用luaS_createlongstr函数创建长字符串
        return luaS_createlongstr(L, str, l); 
    }
}


/**
 * @brief 创建一个新的字符串，并返回TString对象指针。如果该字符串已存在，则直接返回缓存中的TString对象指针。
 *
 * @param L lua_State 状态机指针
 * @param str const char* 字符串起始地址
 * @param l unsigned int 字符串长度
 * 
 * @return struct TString* 返回一个新的TString对象，或者缓存中已有的TString对象。
 */
struct TString* luaS_new(struct lua_State* L, const char* str, unsigned int l) {
    // 计算字符串的哈希值
    unsigned int hash = point2uint(str); 
    // 根据哈希值定位在缓存数组中的位置
    int i = hash % STRCACHE_M;
    // 遍历指定位置的缓存链表
    for (int j = 0; j < STRCACHE_N; j ++) {
        // 获取当前遍历到的TString对象指针
        struct TString* ts = G(L)->strcache[i][j];
        // 判断该TString对象指针是否与要创建的字符串相同
        if (strcmp(getstr(ts), str) == 0) {
            // 如果相同，直接返回该TString对象指针
            return ts;
        }
    }

    // 将缓存数组中除了第一个元素以外的元素往后移动一位
    for (int j = STRCACHE_N - 1; j > 0; j--) {
        G(L)->strcache[i][j] = G(L)->strcache[i][j - 1];
    }

    // 创建一个新的TString对象，并存储在缓存数组的第一个位置
    G(L)->strcache[i][0] = luaS_newlstr(L, str, l);
    // 返回该TString对象指针
    return G(L)->strcache[i][0];
}



/**
 * @brief 从全局字符串表中移除一个指定的TString对象指针。
 *
 * @param L lua_State 状态机指针
 * @param ts struct TString* 要移除的TString对象指针
 */
void luaS_remove(struct lua_State* L, struct TString* ts) {
    // 获取全局状态变量指针
    struct global_State* g = G(L);
    // 计算要移除的TString对象应该所在的哈希桶位置，并获取该桶位置的链表指针
    struct TString** list = &g->strt.hash[lmod(ts->hash, g->strt.size)];

    // 遍历该链表
    for (struct TString* o = *list; o; o = o->u.hnext) {
        if (o == ts) { // 如果找到了要移除的TString对象
            // 将当前链表元素指针指向下一个元素的指针，相当于删除当前链表元素
            *list = o->u.hnext;
            break;
        }
        // 更新当前元素的指针，使其指向下一个元素的指针
        list = &(*list)->u.hnext;
    }
}


/**
 * @brief 清空Lua字符串缓存中的white字符串。
 *
 * @param L lua_State 状态机指针
 */
void luaS_clearcache(struct lua_State* L) {
    // 获取全局状态变量指针
    struct global_State* g = G(L);

    // 遍历Lua字符串缓存，将white字符串替换为错误消息字符串
    for (int i = 0; i < STRCACHE_M; i ++) {
        for (int j = 0; j < STRCACHE_N; j ++) {
            if (iswhite(g->strcache[i][j])) {
                g->strcache[i][j] = g->memerrmsg;
            }
        }
    }
}


/**
 * @brief 判断两个短字符串是否相等。
 *
 * 如果 a 和 b 指向同一TString对象，则认为它们相等；
 * 如果 a 和 b 两个TString对象的长度和内容都相同，则认为它们相等。
 *
 * @param L lua_State 状态机指针
 * @param a 待比较的第一个 TStirng 对象指针
 * @param b 待比较的第二个 TStirng 对象指针
 * @return int 相等返回 1，不等返回 0
 */
int luaS_eqshrstr(struct lua_State* L, struct TString* a, struct TString* b) {
    // 如果 a 和 b 指向同一TString对象，则认为它们相等
    return (a == b) ||
        // 如果 a 和 b 两个TString对象的长度和内容都相同，则认为它们相等
        (a->shrlen == b->shrlen && strcmp(getstr(a), getstr(b)) == 0);
}


/**
 * @brief 判断两个长字符串是否相等。
 *
 * 如果 a 和 b 指向同一TString对象，则认为它们相等；
 * 如果 a 和 b 两个TString对象的长度和内容都相同，则认为它们相等。
 *
 * @param L lua_State 状态机指针
 * @param a 待比较的第一个 TStirng 对象指针
 * @param b 待比较的第二个 TStirng 对象指针
 * @return int 相等返回 1，不等返回 0
 */
int luaS_eqlngstr(struct lua_State* L, struct TString* a, struct TString* b) {
    // 如果 a 和 b 指向同一TString对象，则认为它们相等
    return (a == b) ||
        // 如果 a 和 b 两个TString对象的长度和内容都相同，则认为它们相等
        (a->u.lnglen == b->u.lnglen && strcmp(getstr(a), getstr(b)) == 0);
}


/**
 * @brief 对传入的字符串进行哈希计算。
 *
 * 此函数采用 BKDR 算法。具体实现为对传入的 str 字符串中的每个第 step 个字符取出，
 * 进行哈希计算，按位运算得到最终结果并返回。
 *
 * @param L lua_State 状态机指针
 * @param str 待哈希的字符串指针
 * @param l 字符串长度
 * @param h 哈希值的初值（如果调用方未提供，则可以赋值为随机数，比如 5381）
 * @return unsigned int 计算得到的哈希值
 */
unsigned int luaS_hash(struct lua_State* L, const char* str, unsigned int l, unsigned int h) {
    // 按位异或上一次的哈希值和当前的字符串长度
    h = h ^ l;
    // 设定步长
    unsigned int step = (l >> 5) + 1;
    // 循环处理每一个步长的字符
    for (int i = 0; i < l; i = i + step) {
        // 采用 BKDR 算法对字符进行哈希计算，并将结果与哈希值 h 做异或操作
        h ^= (h << 5) + (h >> 2) + cast(lu_byte, str[i]);
    }
    // 返回计算得到的哈希值
    return h;
}


/**
 * @brief 对传入的长字符串进行哈希计算。
 *
 * 此函数对于同一个字符串只会进行一次哈希计算，然后将结果缓存在 TString 结构
 * 的 hash 字段中以供后续使用。如果调用方需要强制重新计算哈希值，则可以先将
 * TString 结构的 extra 字段置为 0。
 *
 * @param L lua_State 状态机指针
 * @param ts 待哈希的长字符串 TString 结构指针
 * @return unsigned int 返回已缓存或新计算得到的哈希值
 */
unsigned int luaS_hashlongstr(struct lua_State* L, struct TString* ts) {
    // 如果该 TString 结构尚未计算过哈希值，则进行计算
    if (ts->extra == 0) {
        // 采用 luaS_hash 函数计算哈希值，并缓存该值和 extra 字段
        ts->hash = luaS_hash(L, getstr(ts), ts->u.lnglen, G(L)->seed);
        ts->extra = 1;
    }
    // 返回 TString 结构保存的哈希值
    return ts->hash;
}


/**
 * @brief 创建一个长字符串对象。
 *
 * 该函数接受一个 char* 类型的字符串和一个 size_t 类型的长度作为参数，并创建
 * 一个 TString 类型的 Lua 对象。所谓长字符串，是指长度大于等于 LUAI_MAXSHORTLEN
 * （即 40）的字符串。
 *
 * @param L lua_State 状态机指针
 * @param str 待存储的字符串指针
 * @param l 字符串的长度
 * @return struct TString* 返回新创建的长字符串对象指针
 */
struct TString* luaS_createlongstr(struct lua_State* L, const char* str, size_t l) {
    // 使用 createstrobj 函数创建一个 TString 类型的对象，并设置对象类型为 LUA_LNGSTR
    // 并将传入的字符串与长度信息写入对象的 u.lng 字段中
    return createstrobj(L, str, LUA_LNGSTR, l, G(L)->seed);
}

