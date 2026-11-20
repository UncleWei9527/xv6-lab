// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include"kernel/stat.h"
#include"kernel/fs.h"
#include"kernel/fcntl.h"
// Parsed command representation
// cmd 里面的type 
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;//子命令
  char *file;//文件名开始地址
  char *efile;//文件名结束地址
  int mode;//文件打开模式
  int fd;//文件fd替换子命令的fd
};

struct pipecmd {
  int type;
  struct cmd *left;//左边命令
  struct cmd *right;//右边命令
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void); // Fork but panics on failure.
void panic(char *);
struct cmd *parsecmd(char *);
void runcmd(struct cmd *) __attribute__((noreturn));

// Execute cmd.  Never returns.
//
// 这段由儿子进程执行
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    exit(1);

  switch (cmd->type) {
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd *)cmd;
    if (ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    close(rcmd->fd);
    if (open(rcmd->file, rcmd->mode) < 0) {
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    if (fork1() == 0)
      runcmd(lcmd->left);
      //等待左边命令执行完毕 再继续进行右边命令执行
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
      //p[0]读 p[1]写 
    if (pipe(p) < 0)
      panic("pipe");
      //
    if (fork1() == 0) {
      close(1);
        //1->指向管道写端
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if (fork1() == 0) {
      close(0);
        //0 ->指向管道读段
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
      //关闭管道
    close(p[0]);
    close(p[1]);
      //等待子进程完成 
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    if (fork1() == 0)
      runcmd(bcmd->cmd);
      //不等待子进程 完成
    break;
  }
  exit(0);
}
//对于tab 键 +
int
getcmd(char *buf, int nbuf)
{
  struct stat st;
  if(fstat(0,&st)==0&&st.type==T_DEVICE)
    write(2, "$ ", 2);
  
  memset(buf, 0, nbuf);
  gets(buf, nbuf);

  if (buf[0] == 0) // EOF
    return -1;
  return 0;
}
int
my_strncmp(const char *p, const char *q, uint n)
{
  while (n > 0 && *p && *p == *q)
    n--, p++, q++;
  if (n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}   
char *
my_safestrcpy(char *s, const char *t, int n)
{
  char *os;

  os = s;
  if (n <= 0)
    return os;
  while (--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}
// user/sh.c
 int do_tab_completion(char *buf, int cur_len, int max_len)
{
  //解析dir 和file
 char *tokp=buf+cur_len-1;
 char *p_path=0,*e_path=0;
 char *p_file=0,*e_file=buf+cur_len-1;
 if(tokp<buf)
   return cur_len;
 int first_meet_slash=1;
 while(tokp>=buf)
 {
   if(*tokp==' ')
     break;
   if(*tokp=='/'&&first_meet_slash)
   { 
     first_meet_slash=0;
     p_file=tokp+1,e_path=tokp;

   }
   tokp--;
 }
 if(!first_meet_slash)
 p_path=tokp+1;
 else p_file=tokp+1;
 #define BUF_SIZE 128
 char path[BUF_SIZE]={0},file[BUF_SIZE]={0};
 if(p_path)
 memmove(path,p_path,e_path-p_path+1);
 else path[0]='.',path[1]=0;
 if(p_file)
 memmove(file,p_file,e_file-p_file+1);
 
  // 2. 打开当前目录
  int fd = open(path, O_RDONLY);
  if(fd < 0) return cur_len;
  int file_len=strlen(file);
  struct dirent de;
  char matched_name[DIRSIZ + 1];
  int match_count = 0;

  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;
    if(my_strncmp(file, de.name, file_len) == 0){
      match_count++;
      my_safestrcpy(matched_name, de.name, sizeof(matched_name));
    }
  }
  close(fd);

  // 3. 唯一匹配：补全剩余字符并回显
  if(match_count == 1){
    char *suffix = matched_name + file_len;
    int suffix_len = strlen(suffix);

    if(cur_len + suffix_len < max_len - 1){
      memmove(buf + cur_len, suffix, suffix_len);
      cur_len += suffix_len;
      buf[cur_len] = '\0';
      // 回显补全出来的这部分字符到终端
      write(1, suffix, suffix_len);
    }
  }
  
  return cur_len;
 }
int
getcmd_interactive(char *buf, int nbuf)
{
  write(2, "$ ", 2);
  memset(buf, 0, nbuf);
  
  int i = 0;
  char c;
  while(i < nbuf - 1){
    if(read(0, &c, 1) <= 0)
      return -1; // EOF

    if(c == '\t'){
      // 触发 Tab 补全处理
      i = do_tab_completion(buf, i, nbuf);
      continue;
    }
    //退格键盘
    if(c=='\x7f'||c=='\b')
    {
      if(i>0)
      { 
        buf[--i]=0;
        write(2,"\b \b",3);
      }
      continue;
    }
    // 遇到换行，正常结束命令输入
    if(c == '\n' || c == '\r'){
      write(2,"\n",1);
      buf[i++]='\n';
      break;
    
    }
    //正常字符 正常输出
    if(c){
      buf[i++]=c;
      write(2,&c,1);
    }
  }
  buf[i] = '\0';
  return 0;
}
int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while ((fd = open("console", O_RDWR)) >= 0) {
    if (fd >= 3) {
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  struct stat st;
  int is_tty = (fstat(0, &st) == 0 && st.type == T_DEVICE);
  while ((is_tty ? getcmd_interactive(buf, sizeof(buf)) : getcmd(buf, sizeof(buf))) >= 0) {
    char *cmd = buf;
    while (*cmd == ' ' || *cmd == '\t')
      cmd++;
    if (*cmd == '\n') // is a blank command
      continue;
    //对于cd 特别处理
    if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ') {
      // Chdir must be called by the parent, not the child.
      cmd[strlen(cmd) - 1] = 0; // chop \n
      if (chdir(cmd + 3) < 0)
        fprintf(2, "cannot cd %s\n", cmd + 3);
    } 
    else if(cmd[0]=='w'&&cmd[1]=='a'&&cmd[2]=='i'&&cmd[3]=='t'&&(cmd[4]=='\n'||cmd[4]==' '))
    {
      while(wait(0)!=-1);
    }
    else {
      //开辟子进程执行代码
      struct cmd*c=parsecmd(cmd);
      if(c->type==BACK)
      {
        if (fork1() == 0)
          runcmd(((struct backcmd*)c)->cmd);
      }
      else {
        int pid=fork1();
        if(pid==0)
          runcmd(c);
        else {
          int wpid;
           while ((wpid = wait(0)) != pid && wpid != -1);
        }
        
      }
    }
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}
//带检查的fork
int
fork1(void)
{
  int pid;

  pid = fork();
  if (pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd *
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd *)cmd;
}

struct cmd *
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd *)cmd;
}

struct cmd *
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd *)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";
//指向当前扫描位置 字符串末尾位置 q和eq记录了词的开头结束位置
int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  //跳过空白符号
  while (s < es && strchr(whitespace, *s))
    s++;
  if (q)
    *q = s;
  ret = *s;
  switch (*s) {
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if (*s == '>') {
      ret = '+';
      s++;
    }
    break;
  default:
    //获取一个完整的单词
    ret = 'a';
    while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if (eq)
    *eq = s;

  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}
//判断下一个是否是toks里面的一个
int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  //跳过空白符号 
  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es) {
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd *
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  
  while (peek(ps, es, "&")) {
    gettoken(ps, es, 0, 0);
    // cmd & 
    cmd = backcmd(cmd);
  } 
  //list cmd : cmd1;cmd2;
  if (peek(ps, es, ";")) {
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}
// cmd1 | cmd2 
struct cmd *
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|")) {
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}
//cmd  (>|>>|<  cmd)+ 
struct cmd *
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while (peek(ps, es, "<>")) {
    tok = gettoken(ps, es, 0, 0);
    if (gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch (tok) {
    case '<':
        //文件作为输入 只读 
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
        //文件作为全覆盖输出 ,需要清理原来的文件 
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE | O_TRUNC, 1);
      break;
    case '+': //>>  不清空输出
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
      break;
    }
  }
  return cmd;
}
// (cmd )
struct cmd *
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if (!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if (!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd *
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if (peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd *)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while (!peek(ps, es, "|)&;")) {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0)
      break;
    if (tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
// 把命令的输入参数设置为'\0'结尾
struct cmd *
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    return 0;

  switch (cmd->type) {
  case EXEC:
    ecmd = (struct execcmd *)cmd;
    for (i = 0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
