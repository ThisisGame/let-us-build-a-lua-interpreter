# let-us-build-a-lua-interpreter
《Lua解释器构建：从虚拟机到编译器》随书源码

目录
Lua解释器构建：从虚拟机到编译器
前言
第1章 Lua解释器概述
1.1 Lua解释器
1.1.1 Lua解释器的整体架构
1.1.2 Lua解释器的运行机制
1.2 Lua虚拟机
1.2.1虚拟机简介
1.2.2虚拟机指令集
1.2.3虚拟机指令的编码格式
1.3 Lua编译器
1.3.1 Lua的词法分析器
1.3.2 Lua的语法分析器
1.4 从0开发一个Lua解释器:Dummylua项目
1.4.1 项目简介
1.4.2 项目架构说明
1.4.3 下载地址
第2章 Lua虚拟机
2.1 Lua虚拟机基础知识
2.1.1 基本类型定义
2.1.2 虚拟机全局状态--Global_State
2.1.3 虚拟机的线程结构--Lua_State
2.1.4 虚拟机中执行函数的基础--CallInfo结构
2.1.5 C函数在虚拟机线程中的调用流程
2.1.6 Dummylua项目的虚拟机基础实现
2.2 为虚拟机添加垃圾回收机制
2.2.1触发机制
2.2.2重启阶段
2.2.3传播扫描阶段
2.2.4原子处理阶段
2.2.5清除阶段
2.2.6暂停阶段
2.2.7 Dummylua项目的垃圾回收机制实现
2.3 Lua虚拟机的字符串
2.3.1 Lua字符串功能概述
2.3.2 Lua字符串结构
2.3.3 字符串的哈希运算
2.3.4 短字符串与内部化
2.3.5 长字符串与惰性哈希
2.3.6 Lua的Hash Dos攻击
2.3.7 Dummylua的字符串实现
2.4 Lua虚拟机的表
2.4.1 Lua表功能概述
2.4.2 Lua表的基本数据结构
2.4.3 表的初始化
2.4.4 键值的哈希运算
2.4.5 查找元素
2.4.6 值的更新与插入
2.4.7 调整表的大小
2.4.8 表遍历
2.4.9 Dummylua的表实现
第3章 Lua脚本的编译与虚拟机指令运行流程
3.1 第一个编译并运行脚本的例子：让Lua说“Hello World”
3.2 反编译工具--protodump
3.2.1 protodump工具简介
3.2.2使用protodump反编译Lua的字节码
3.2.3反编译结果分析
3.3 虚拟机如何运行编译后的指令
3.4 要实现的最基础的虚拟机指令
3.5 Lua内置编译器
3.5.1编译器简介
3.5.2 Lua编译器的构成
3.5.3 EBNF文法简介
3.5.4涉及的EBNF文法
3.6 标准库加载流程
3.7 将“Hello World”脚本加载编译并运行
3.8 让Dummylua能够编译并运行“Hello World”脚本
第4章 Lua编译器
4.1 Lua词法分析器
4.1.1 词法分析器简介
4.1.2 词法分析器基本数据结构
4.1.3 词法分析器的接口设计
4.1.4 词法分析器的初始化流程
4.1.5 Token识别流程
4.1.6 一个测试用例
4.1.7 Dummylua的词法分析器实现
4.2 Lua语法分析器基础--expr语句编译流程
4.2.1 语法分析器的主要工作
4.2.2实现的语法
4.2.3新增的虚拟机指令说明
4.2.4 语法分析器基本数据结构
4.2.5 expr语句的编译流程
4.2.6 为Dummylua添加编译Exprstat的功能
4.3 完整的Lua语法分析器
4.3.1 Lua的代码块
4.3.2 Local语句编译流程
4.3.3 Do-end语句编译流程
4.3.4 If语句编译流程
4.3.5 While语句编译流程
4.3.6 Repeat语句编译流程
4.3.7 For语句编译流程
4.3.8 Break语句编译流程
4.3.9 Function语句编译流程
4.3.10 Return语句编译流程
4.3.11 Dummylua的完整语法分析器实现
第5章 Lua的语言特性
5.1 元表
5.1.1元表简介
5.1.2元表的__index和__newindex域
5.1.3双目运算事件
5.1.4 Dummylua的元表实现
5.2 Userdata
5.2.1 Userdata的数据结构
5.2.2 Userdata的接口
5.2.3 Userdata的垃圾回收处理
5.2.4 Userdata的user domain域内部的堆内存清理
5.2.5 Userdata的使用例子
5.2.6 Dummylua的Userdata实现
5.3 Upvalue
5.3.1 Upvalue的定义
5.3.2 Lua函数探索
5.3.3 Upvalue的生成
5.3.4 Open Upvalue和Closed Upvalue
5.3.5 Dummylua的Upvalue实现
5.4 弱表
5.4.1弱表的定义
5.4.2 弱表的用途
5.4.3 弱键
5.4.4 弱值
5.4.5 完全弱引用
5.4.6 关于Ephemeron机制的深度思考
5.4.7 Dummylua的弱表实现
5.5 Require机制
5.5.1 Require函数简介
5.5.2 Require一个脚本的流程
5.5.3 Require一个动态链接库的流程
5.5.4 Dummylua的Require机制实现
5.6 Lua协程
5.6.1 Lua协程简介
5.6.2 Lua协程的应用情景
5.5.3 Lua协程的基本数据结构设计
5.5.4 Lua协程的运作流程
5.5.5 Dummylua的协程实现
第6章 dummylua开发案例
6.1 跑酷游戏案例：Chrome小恐龙跑酷
6.1.1 项目简介
6.1.2 OpenGL简介
6.1.3 为绘制API导出Lua接口
6.1.4 在Lua脚本中使用绘图接口
6.1.5 项目架构与运作流程
6.1.6 绘制层设计与实现
6.1.7 逻辑层设计与实现
6.2 打地鼠案例
6.2.1 项目简介
6.2.2 Cocos2dx引擎简介
6.2.3 导出Cocos2dx的主要接口到Lua层
6.2.4 在Lua脚本中使用Cocos2dx
6.2.5 实现完整的游戏案例
附录
附录A Lua虚拟机指令集
附录B Lua的EBNF语法
