# MIT 6.1810 (xv6-riscv 2026) — Lab 1: Xv6 and Unix Utilities

本分支（`util`）记录 Lab 1 的实现与笔记。实验内容主要是熟悉 xv6 用户态程序开发与常用 Unix 系统调用（`pause`、`read`、`open`、`fstat`、`fork`、`exec`、`wait` 等），并在此基础上完成了几项 Shell 和内核控制台扩展。



## 功能与代码索引

| 模块 | 类型 | 修改/新增文件 | 简要说明 |
| :--- | :---: | :--- | :--- |
| **sleep** | 必做 | [`user/sleep.c`](./user/sleep.c) | 调用 `pause()` 系统调用，按指定时钟中断次数（ticks）挂起进程 |
| **sixfive** | 必做 | [`user/sixfive.c`](./user/sixfive.c) | 用状态机解析输入文件或标准输入，输出 5 或 6 的倍数 |
| **memdump** | 必做 | [`user/memdump.c`](./user/memdump.c) | 按格式字符串解析原始内存字节（`i/p/h/c/s/S`） |
| **find & `-exec`** | 必做 | [`user/find.c`](./user/find.c) | 递归查找文件，支持正则匹配及 `-exec` 执行命令 |
| **match** | 扩展 | [`user/match.c`](./user/match.c) | 独立的基础正则匹配命令行工具（`^ $ . *`） |
| **uptime** | 扩展 | [`user/uptime.c`](./user/uptime.c) | 调用 `uptime()` 输出系统启动以来的时钟中断计数 |
| **Shell 后台任务与 `wait`** | 扩展 | [`user/sh.c`](./user/sh.c) | 实现 `wait` 内置命令，处理 `cmd &` 后台运行与前台进程回收 |
| **Tab 自动补全** | 扩展 | [`kernel/console.c`](./kernel/console.c), [`user/sh.c`](./user/sh.c) | 修改控制台输入中断为 Raw Mode，在用户态 Shell 实现回显、退格与路径 Tab 补全 |

---

## 实现说明与理解

### 1. `sleep` 与 `uptime`

- **代码位置**：[`user/sleep.c`](./user/sleep.c)、[`user/uptime.c`](./user/uptime.c)、[`kernel/sysproc.c`](./kernel/sysproc.c)
- **实验内容**：实现 `sleep <ticks>` 让进程暂停指定个时钟滴答；扩展实现 `uptime` 打印系统启动以来的时钟滴答数。
- **实现思路**：
  - `sleep` 检查参数数量后用 `atoi()` 解析参数并调用 `pause(ticks)`（输入 `0` 或非数字时 `atoi()` 返回 `0`，直接返回不阻塞）。
  - `uptime` 调用 `uptime()` 读取内核维护的全局计数 `ticks`。

---

### 2. `sixfive`

- **代码位置**：[`user/sixfive.c`](./user/sixfive.c)
- **实验内容**：读取一个或多个文件（无参数时读标准输入），提取被分隔符 `"-\r\t\n./, "` 隔开的纯数字，打印其中能被 5 或 6 整除的数（例如 `xv6`、`6th` 这类包含非数字字符的词不算数字）。
- **实现思路**：
  - 使用三态状态机（`Seq` / `Number` / `Invalid`）逐字节 `read(fd, &ch, 1)` 处理输入：
    - 遇到分隔符时，若当前是 `Number` 则检查并输出，然后重置为 `Seq`；
    - 遇到数字时在 `Seq` / `Number` 态下累加数值，在 `Invalid` 态下忽略；
    - 遇到其他非数字字符则切到 `Invalid`，直到下一个分隔符。
  - 文件结束（EOF）时若仍处于 `Number` 态，再补做一次判断输出，处理末尾无换行符的情况。

---

### 3. `memdump`

- **代码位置**：[`user/memdump.c`](./user/memdump.c)
- **实验内容**：实现 `memdump(char *fmt, char *data, int len)`，根据格式串将一段原始内存按不同类型解析输出，并处理输入长度不足的边界情况。
- **实现思路**：
  - 遍历格式串 `fmt`，按对应宽度移动偏移量 `i`：
    - `i`：4 字节有符号十进制整数
    - `p`：8 字节十六进制数
    - `h`：2 字节无符号短整型
    - `c`：1 字节字符
    - `s`：取 8 字节作为指针并解引用，打印其指向的 C 字符串
    - `S`：将当前位置起的剩余字节直接作为 C 字符串打印，并把游标移至 `len`
  - 每次读取前检查 `len - i < width`，剩余字节不足时报错退出，防止越界读取。

---

### 4. `find`、`-exec` 与正则匹配

- **代码位置**：[`user/find.c`](./user/find.c)、[`user/match.c`](./user/match.c)
- **实验内容**：在指定目录树下递归查找目标文件或目录；支持 `-exec <cmd> [args...]` 对每个匹配路径执行指定命令；扩展支持用正则表达式匹配文件名，并提供独立的 `match` 工具。
- **实现思路**：
  - 用 `open()` + `fstat()` 判断当前路径类型：文件/设备直接匹配 basename；目录先判断自身名称，再循环 `read(fd, &de, sizeof(de))` 遍历目录项，跳过空闲项（`de.inum == 0`）以及 `.` 和 `..`，拼接路径后递归查找。
  - `-exec` 先缓存待执行的命令参数，命中目标时将当前路径追加到参数末尾，再通过 `fork()` → 子进程 `exec()` → 父进程 `wait(0)` 执行。
  - 复用 `match` / `matchhere` / `matchstar` 实现基础正则匹配（支持 `^`、`$`、`.`、`*`）。

---

### 5. 控制台 Raw Mode 与 Shell Tab 自动补全

- **代码位置**：[`kernel/console.c`](./kernel/console.c)、[`user/sh.c`](./user/sh.c)
- **实验内容**：为 xv6 Shell 增加按 `Tab` 键自动补全当前目录或子路径文件名的能力。

#### 为什么原版 xv6 无法直接在 Shell 做 Tab 补全？
原版控制台驱动 [`consoleintr()`](./kernel/console.c#L150-L216) 工作在 **行缓冲模式（Canonical Mode）**：
- 内核环形缓冲区 `cons.buf[128]` 用三个下标划分：`[cons.r, cons.w)` 是已提交可读数据，`[cons.w, cons.e)` 是正在编辑的行。
- 默认模式下由内核负责回显和退格，**直到收到 `\n`、`Ctrl-D` 或缓冲区满时**才提交 `cons.w = cons.e` 并 `wakeup(&cons.r)`。
- 因此按下 `Tab` 时，用户态 Shell 的 `read(0, ...)` 仍处于阻塞状态，收不到按键事件。

#### 修改方案

- **内核（[`kernel/console.c`](./kernel/console.c#L149-L166)）**：
  - 增加 `console_raw_mode` 开关：保留 `Ctrl-P` 触发 `procdump()`，其余字符不经过内核回显和行缓冲，直接写入 `cons.buf`、推进 `cons.w = cons.e` 并唤醒 `consoleread()`，把行编辑与回显交给用户态处理（类似 POSIX `termios` 关闭 `ICANON` 和 `ECHO`）。
- **用户态 Shell（[`user/sh.c`](./user/sh.c#L184-L292)、[`user/sh.c`](./user/sh.c#L308-L310)）**：
  - 通过 `fstat(0, &st)` 判断 `fd 0` 是否为终端设备（`T_DEVICE`）：交互终端走 `getcmd_interactive()`，文件/管道重定向（如 `sh < findtest.sh`）仍走 `getcmd()`，避免把脚本内容回显到屏幕。
  - `getcmd_interactive()` 逐字节 `read(0, &c, 1)`，自行处理回显、退格擦除（`\b \b`）、回车提交和 `\t` 触发补全。
  - `do_tab_completion()` 从当前输入末尾向左找到最后一个 token，按 `/` 拆分目录（默认 `.`）和文件名前缀，打开目录扫描 `struct dirent` 做前缀匹配（`my_strncmp`）；唯一匹配时补全剩余后缀并回显。
---

### 6. Shell `wait` 内置命令与后台任务

- **代码位置**：[`user/sh.c`](./user/sh.c#L317-L345)
- **实验内容**：扩展 Shell，支持 `wait` 内置命令等待后台任务结束。
- **实现思路**：
  - **为什么 `cd` 和 `wait` 必须是内置命令**：`chdir()` 只影响当前进程的 `cwd`，`wait()` 也只能回收当前进程的直接子进程，如果放在子进程里执行，无法改变父 Shell 的状态或回收父 Shell 启动的后台任务。因此 `wait` 必须在父进程中循环执行 `while (wait(0) != -1);`。
  - **前后台进程回收**：在 `main()` 中解析语法树后，若为 `BACK` 类型直接 `fork()` 执行不阻塞；若为前台命令，则记录其 `pid` 并循环 `while ((wpid = wait(0)) != pid && wpid != -1);`，避免被早先退出的后台进程打断前台等待，同时顺带回收已结束的后台子进程。

---

## 运行与测试

### 常用命令

```bash
# 编译并启动 xv6
make qemu

# 运行 Lab 1 官方评分测试
make grade

# GDB 调试（打开两个终端分别运行）
make qemu-gdb
gdb-multiarch
```

### 在 Shell 中手动体验扩展功能

```text
$ uptime                  # 查看系统启动以来的时钟 ticks
$ match a.*c abbbc        # 测试正则匹配工具
$ find . ^f.*             # 用正则递归搜索以 f 开头的文件
$ sleep 10 &              # 后台运行任务
$ wait                    # 等待并回收所有后台子进程
$ fin<Tab>                # 按 Tab 键自动补全
```
