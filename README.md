# MIT 6.1810 (xv6-riscv 2026) — Lab 2: System calls

本分支（`syscall`）记录 Lab 2 的实现和踩坑记录，主要是 gdb 调试、系统调用沙盒 `interpose` 和 `attack` 三个部分。

## 代码改动索引

| 模块 | 修改/新增文件 | 说明 |
| :--- | :--- | :--- |
| **Using gdb** | [`answers-syscall.txt`](./answers-syscall.txt) | gdb 调试 `syscall()` 和空指针崩溃的记录 |
| **Sandbox** | [`kernel/proc.h`](./kernel/proc.h), [`kernel/proc.c`](./kernel/proc.c), [`kernel/syscall.h`](./kernel/syscall.h), [`kernel/syscall.c`](./kernel/syscall.c), [`kernel/sysfile.c`](./kernel/sysfile.c), [`user/user.h`](./user/user.h), [`user/usys.pl`](./user/usys.pl), [`Makefile`](./Makefile) | 添加 `interpose(mask, path)` 系统调用，限制进程的系统调用 |
| **Attack** | [`user/attack.c`](./user/attack.c) | 通过 `sbrk()` 扫描残留物理页，读出 `secret` 写进去的字符串 |

---

## 实现记录

### 1. Using gdb

- 文件：[`answers-syscall.txt`](./answers-syscall.txt)
- 跟着实验步骤在 `syscall()` 打断点看调用栈和寄存器：
  - 是 `usertrap()` 调用的 `syscall()`，系统调用号存在 `p->trapframe->a7`（开机第一次是 `0xf`，也就是 `SYS_open`）。
  - 故意把取系统调用号改成读地址 `0` 之后内核直接 panic：`syscall()` 跑在内核态、用的是内核页表，内核页表里并没有映射 `0` 地址，所以触发了 `scause = 0xd`（Load page fault），此时进程是 `init`（`pid = 1`）。

---

### 2. Sandbox（`interpose`）

- 主要涉及文件：[`kernel/proc.h`](./kernel/proc.h)、[`kernel/proc.c`](./kernel/proc.c)、[`kernel/syscall.c`](./kernel/syscall.c)、[`kernel/sysfile.c`](./kernel/sysfile.c)
- 先按流程接好新系统调用（`user.h` 声明、`usys.pl` 加入口、`syscall.h` 加编号 `SYS_interpose`、`syscall.c` 注册进 `syscalls[]`）。另外 `Makefile` 里要补上 `$U/_sandbox` 和 `$U/_sbtest`，不然跑 `sbtest` 相关测试会报 `exec sbtest failed`。
- 具体逻辑：
  - 在 `struct proc` 里加了 `mask` 和 `matchPath[MAXPATH]` 两个字段。
  - `kfork()` 时把父进程的 `mask` 和 `matchPath` 拷贝给子进程，保证 `sandbox_fork` 这种派生子进程的场景也能生效；`freeproc()` 里记得清回 `0` 和 `"-"`。
  - `sys_interpose()` 里用 `argint` 和 `argstr` 拿参数，加了 `(p->mask & mask) != p->mask` 的判断，防止已经进沙盒的进程再调 `interpose(0, ...)` 把限制清掉（对应 `sandbox_escape` 测试）。
  - 在 `syscall()` 分发前检查 `p->mask & (1 << num)`：如果被屏蔽了，只有 `SYS_open` / `SYS_exec` 且参数路径刚好等于 `p->matchPath` 才放行，否则不执行原函数，直接把 `p->trapframe->a0` 设成 `-1` 返回。

---

### 3. Attack xv6

- 文件：[`user/attack.c`](./user/attack.c)
- 看 `kernel/kalloc.c` 和 `kernel/vm.c` 可以发现，开了 `LAB_SYSCALL` 宏之后，`kfree`、`kalloc` 和 `uvmalloc` 里的 `memset` 都被 `#ifndef LAB_SYSCALL` 跳过了。而且空闲页链表 `freelist` 是头插头取的，所以 `secret` 退出释放的物理页很快会被下一个进程重新拿到。
- `attack.c` 里直接 `sbrk(28 * 4096)` 申请一块足够大的内存，然后从头往后找 `"Here it is: "`：
  - 注意不要匹配到 `secret` 自己常量段残留的 `"Here it is: \0"`，所以加了 `data[i + msg_len] != '\0'` 过滤。


## 运行与测试

```bash
# 编译启动
make qemu

# 跑全部评分测试
make grade

# gdb 调试
make qemu-gdb
gdb-multiarch
```

手动测试几个用例：

```text
$ sandbox 32768 - cat README
$ sandbox 32768 README grep xv6 README
$ sandbox 32768 - sh < exec.sh
$ sandbox 32768 - sbtest escape
$ secret abcdefgh
$ attack
```
