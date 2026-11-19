 #include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

 #include"kernel/param.h"

int match(char *, char *);
 const char *get_fmtname(const char*path)
 {
   const char*p = path + strlen(path);

   for (; p >= path && *p != '/'; p--)
   {}
    if(*p=='/')
    p++;
    return p;
    
 }
 int flag;
 int file_name_index;
 const char*args[MAXARG];
 void init(int argc,char*argv[])
 {
    for(file_name_index=0;file_name_index<argc-4;file_name_index++)
   {
         args[file_name_index]=argv[file_name_index+4] ;     
   }
 }
 void exec_cmd(const char*path)
 {
     int pid=fork();
     if(fork<0)
     {
       fprintf(2,"find:fork failed\n");
       exit(-1);
     }
     else if(pid==0)
     {
       //child process 
       args[file_name_index]=path;
       exec(args[0],(char**)args) ;
       fprintf(2,"find:exec %s failed\n",args[0]);
     }
     else{
       wait(0);
     } 
 }
void find(const char*current_path,const char*file_name)
 {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(current_path ,O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n",current_path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", current_path);
    close(fd);
    return;
  }

  switch (st.type) {
  case T_DEVICE:
  case T_FILE:
       // printf("cmp %s %s\n",get_fmtname(current_path),file_name);
    // if(!strcmp(get_fmtname(current_path),file_name))
    if(match((char*)file_name,(char*)get_fmtname(current_path)))
       {if(flag)
           exec_cmd(current_path);
        else 
         printf("%s\n",current_path);
       }
    break;

  case T_DIR:         
    // if(!strcmp(get_fmtname(current_path),file_name))
    if(match((char*)file_name,(char*)get_fmtname(current_path)))
        {if(flag)
           exec_cmd(current_path);
        else 
         printf("%s\n",current_path);
       } 
    if (strlen(current_path) + 1 + DIRSIZ + 1 > sizeof buf) {
      printf("find : path too long\n");
      break;
    }
    strcpy(buf, current_path);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      if(!strcmp(de.name,".")||!strcmp(de.name,".."))
      {
          continue;
      }
      // printf("%s\n",de.name);
      //构造buf
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if (stat(buf, &st) < 0) {
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      switch(st.type)
      {
        case T_FILE:
        case T_DEVICE:
           find(buf,file_name);
           break;
        case T_DIR:
           find(buf,file_name);
           break;

      }
      // printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, (int)st.size);
    }
    break;
  }
  close(fd);          
 }
 #include"kernel/param.h"
int
main(int argc, char *argv[])
{

  if (argc <3) {
    fprintf(2,"Usage:find <path> <file> [-exec <arguments>]\n");
     exit(-1);
  }
   else {
     flag=argc>3?1:0;
   }
   init(argc,argv);
  find(argv[1],argv[2]) ;
  
   exit(0);
} 
 int matchhere(char *, char *);
int matchstar(int, char *, char *);

int
match(char *re, char *text)
{
  if (re[0] == '^')
    return matchhere(re + 1, text);
  do { // must look at empty string
    if (matchhere(re, text))
      return 1;
  } while (*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int
matchhere(char *re, char *text)
{
  if (re[0] == '\0')
    return 1;
  if (re[1] == '*')
    return matchstar(re[0], re + 2, text);
  if (re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if (*text != '\0' && (re[0] == '.' || re[0] == *text))
    return matchhere(re + 1, text + 1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int
matchstar(int c, char *re, char *text)
{
  do { // a * matches zero or more instances
    if (matchhere(re, text))
      return 1;
  } while (*text != '\0' && (*text++ == c || c == '.'));
  return 0;
}
