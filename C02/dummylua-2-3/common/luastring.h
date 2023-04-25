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

#ifndef luastring_h
#define luastring_h

#include "luastate.h"
#include "../vm/luagc.h"

/**
 * @brief 计算长字符串对象大小。
 *
 * 该函数根据传入字符串长度计算一个长字符串对象所需的内存大小，用于后续对象分配
 * 时调用。
 *
 * @param l 字符串长度
 * @return size_t 返回长字符串对象所需的内存大小
 */
#define sizelstring(l) (sizeof(TString) + (l + 1) * sizeof(char))

/**
 * @brief 获取字符串数据。
 *
 * 该宏返回一个指针，指向传入 TString 对象（ts）中保存的字符串数据。具体实现中，
 * 该宏返回了传入对象的 data 成员变量地址。
 *
 * @param ts TString 类型对象指针
 * @return char* 返回字符串数据指针
 */
#define getstr(ts) (ts->data)

/**
 * @brief 初始化字符串模块。
 *
 * 该函数初始化字符串模块，并创建一个全局的字符串表用于存储 Lua 解释器中的所有字符
 * 串。同时，该函数会为两个预定义的字符串（"nil" 和 "true"）在该表中分配空间并存
 * 储。该函数应该在解释器初始化时被调用一次。
 *
 * @param L lua_State 状态机指针
 * @return void
 */
void luaS_init(struct lua_State* L);

/**
 * @brief 调整全局字符串表大小。
 *
 * 该函数扩展或收缩全局字符串表的大小。如果新的表尺寸大于当前表的大小，则会分
 * 配额外的空间以容纳更多的字符串。如果新表尺寸小于当前表的大小，则原有的字符
 * 串对象可能会被回收。
 *
 * @param L lua_State 状态机指针
 * @param nsize 新的表尺寸（单位为 TString 指针数）
 * @return int 执行结果状态码，0 表示成功
 */
int luaS_resize(struct lua_State* L, unsigned int nsize); // only for short string

/**
 * @brief 创建一个新的短字符串对象。
 * 
 * 该函数接受一个 char* 类型的字符串和一个 unsigned int 类型的长度作为参数，并创建
 * 一个 TString 类型的 Lua 对象。所谓短字符串，是指长度小于 LUAI_MAXSHORTLEN
 * （即 40）的字符串。
 *
 * @param L lua_State 状态机指针
 * @param str 待存储的字符串指针
 * @param l 字符串的长度
 * @return struct TString* 返回新创建的短字符串对象指针
 */
struct TString* luaS_newlstr(struct lua_State* L, const char* str, unsigned int l);

/**
 * @brief 创建一个新的字符串对象。
 *
 * 该函数根据传入字符串长度确定对象类型（短字符串或长字符串），并调用相应的函
 * 数创建新的字符串对象。如果传入长度大于 LUAI_MAXSHORTLEN，则会创建一个长字符串对
 * 象；否则将会创建一个短字符串对象。
 *
 * @param L lua_State 状态机指针
 * @param str 待存储的字符串指针
 * @param l 字符串的长度
 * @return struct TString* 返回新创建的字符串对象指针
 */
struct TString* luaS_new(struct lua_State* L, const char* str, unsigned int l);

/**
 * @brief 从字符串表中移除指定的短字符串对象。
 *
 * 该函数从全局字符串表中移除传入的短字符串对象。注意，该函数仅用于短字符串对
 * 象，且只能从全局字符串表中移除已存在的对象。
 *
 * @param L lua_State 状态机指针
 * @param ts 待移除的短字符串对象指针
 * @return void
 */
void luaS_remove(struct lua_State* L, struct TString* ts); // remove TString from stringtable, only for short string

/**
 * @brief 清空字符串缓存。
 *
 * 该函数会释放所有 Lua 解释器中保存的字符串对象，并重置全局字符串表大小为其初始
 * 值。调用该函数后，Lua 执行环境中的所有字符串对象都将被回收。
 *
 * @param L lua_State 状态机指针
 * @return void
 */
void luaS_clearcache(struct lua_State* L);

/**
 * @brief 判断两个短字符串是否相等。
 *
 * 该函数接受两个短字符串对象作为参数，并判断它们的值是否相等。
 *
 * @param L lua_State 状态机指针
 * @param a 第一个短字符串对象指针
 * @param b 第二个短字符串对象指针
 * @return int 执行结果状态码，0 表示成功且两字符串相等
 */
int luaS_eqshrstr(struct lua_State* L, struct TString* a, struct TString* b);

/**
 * @brief 判断两个长字符串是否相等。
 *
 * 该函数接受两个长字符串对象作为参数，并判断它们的值是否相等。
 *
 * @param L lua_State 状态机指针
 * @param a 第一个长字符串对象指针
 * @param b 第二个长字符串对象指针
 * @return int 执行结果状态码，0 表示成功且两字符串相等
 */
int luaS_eqlngstr(struct lua_State* L, struct TString* a, struct TString* b);

/**
 * @brief 计算字符串哈希值。
 *
 * 该函数计算传入字符串的哈希值，以便在全局字符串表中查找、插入或删除字符串。
 * 该函数根据 Lua 设计文档中描述的字符串哈希算法实现，可保证正确性和一致性。
 *
 * @param L lua_State 状态机指针
 * @param str 待计算哈希值的字符串指针
 * @param l 字符串的长度
 * @param h 上一个哈希值（用于解决冲突）
 * @return unsigned int 返回计算得到的哈希值
 */
unsigned int luaS_hash(struct lua_State* L, const char* str, unsigned int l, unsigned int h);

/**
 * @brief 计算长字符串对象的哈希值。
 *
 * 该函数计算传入长字符串的哈希值，以便在全局字符串表中查找、插入或删除长字
 * 符串。该函数采用加密哈希算法（MurmurHash2）实现，在平衡性和散列效率上均有不
 * 错的表现。注意，该函数仅用于长字符串对象。
 *
 * @param L lua_State 状态机指针
 * @param ts 待计算哈希值的长字符串对象指针
 * @return unsigned int 返回计算得到的哈希值
 */
unsigned int luaS_hashlongstr(struct lua_State* L, struct TString* ts);

/**
 * @brief 创建一个长字符串对象。
 *
 * 该函数接受一个 char* 类型的字符串和一个 size_t 类型的长度作为参数，并创建一个
 * TString 类型的 Lua 对象。所谓长字符串，是指长度大于等于 LUAI_MAXSHORTLEN
 * （即 40）的字符串。
 *
 * @param L lua_State 状态机指针
 * @param str 待存储的字符串指针
 * @param l 字符串的长度
 * @return struct TString* 返回新创建的长字符串对象指针
 */
struct TString* luaS_createlongstr(struct lua_State* L, const char* str, size_t l);


#endif 
