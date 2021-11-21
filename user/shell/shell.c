#include <string.h>
#include <syscall.h>
#include <x86.h>
#include <shell.h>

struct Command
{
	const char* name;
	int (*func) (int argc, char** argv);
};

static struct Command cmds[] = {{"ls", ls}, {"pwd", pwd}, {"cd", cd}, {"cp", cp}, {"mv", mv}, {"rm", rm}, {"mkdir", shell_mkdir}, {"cat", shell_cat}, {"touch", shell_touch}, {"write", shell_write}, {"append", shell_append}};

#define BUFFERLEN 1024
#define PARSESPACE "\t\r\n "
#define MAXARGS 16
#define NUMCOMMANDS 11
char shell_buf[BUFFERLEN];

int dir_list(char* buf, char * path){
  int len;
  char pwd[BUFFERLEN];

  if(path == NULL){
    len  = sys_ls(buf, BUFFERLEN); 
  }
  else{
    sys_pwd(pwd);
    sys_chdir(path);
    len = sys_ls(buf, BUFFERLEN);
    sys_chdir(pwd); 
  }
  int i = 0;
  while(i < len){
    if(buf[i] == ' '){
        buf[i] = '\0';
    }
    i++;
  }
  return len;
}

int is_dir(char * path){
  int fd, isDir;
  if(is_exist(path)){
    fd = open(path, O_RDONLY);
  }
  isDir = sys_is_dir(fd);
  close(fd);
  return isDir;
}

int is_exist(char* path){
  int fd;
  fd = open(path, O_RDONLY);
	if(fd == -1)
    return 0;

	close(fd);
  return 1;
}

int ls(int argc, char** argv)
{
	if (argc == 1){
	  sys_ls(shell_buf, sizeof(shell_buf));
    printf("%s\n", shell_buf);
  }
  else if (argc == 2) {
    char path[100];
    sys_pwd(path);
    
    sys_chdir(argv[1]);
    sys_ls(shell_buf, sizeof(shell_buf));
    printf("%s\n", shell_buf);

    sys_chdir(path);
  }else{
    printf("ls: too many arguments\n"); 
  }
  
  return 0;
}

int pwd(int argc, char ** argv)
{
	sys_pwd(shell_buf);
  char* out = shell_buf + 1;
  printf("~%s\n", out);
	return 0;	
}

int cd(int argc, char** argv)
{
	char path[1024];
	if (argc == 1){
		printf("cd: too few arguments\n");
    return 0;
	}
	else {
		strcpy(path, argv[1]);
		sys_chdir(path);	
	}
  return 0;
}

int cp(int argc, char** argv)
{
  char * src_path, * dest_path;
  if(argc < 3){
    printf("cp: too few arguments\n");
    return 0;
  }
  else if(argc > 4){
    printf("cp: too many arguments\n");
    return 0;
  }
  if(argc == 3){
    src_path = argv[1];
    dest_path = argv[2];
    cp_r(dest_path, src_path, 0);
    return 0;
  }	
  else{
    if(strcmp(argv[1], "-r")){
      printf("cp: invalid arg. use -r for recursive cp\n");
      return 0;
    }
    src_path = argv[2];
    dest_path = argv[3];
    cp_r(dest_path, src_path, 1);
    return 0;
  }
}

int cp_r(char * dest, char * src, int rflag){
  char path[BUFFERLEN];
  char filename[100];
  char dest_path[BUFFERLEN];
  char src_path[BUFFERLEN];
  char * p;

  if(!is_exist(src)){
    printf("cp: source file does not exist\n");
    return 0;
  }
  if(!rflag){ // non recursive case
    if(is_dir(src)){
      printf("cp: use -r to copy directory\n");
      return 0;
    }
    if(is_exist(dest) && is_dir(dest)){ //file to dir
        get_filename(src, filename);
        strcpy(path, dest);
        p = path + strlen(path);
        *(p++) = '/';
        strcpy(p, filename);
        cp_r(path, src, rflag); 
    }
    else{ //file to file
      cp_file(dest, src);
    }
  }
  else{ 
    if(is_dir(src)){
      cp_file(dest, src);
      
      int len = dir_list(path, src);
      char* p = path;
      while(p - path < len){
        if(strcmp(p, ".") && strcmp(p, "..")){
          strcpy(dest_path, dest);
          strcpy(src_path, src);
          dest_path[strlen(dest)] = '/';
          src_path[strlen(src)] = '/';
          strcpy(dest_path + strlen(dest) + 1, p);
          strcpy(src_path + strlen(src) + 1, p);

          cp_r(dest_path, src_path, rflag);
        }
        p += strlen(p) + 1;
      }
    }
    else{
      cp_r(dest, src, 0);
    }
  }
  return 0;
}

int cp_file(char* dest_filename, char* src_filename) {
  if (is_dir(src_filename)) {
    sys_mkdir(dest_filename);
    return 0;
  }

  int fd = open(src_filename, O_RDONLY);

  char buf[1000];
  read(fd, buf, 1000);
  close(fd);

  fd = open(dest_filename, O_CREATE|O_RDWR);
  write(fd, buf, strlen(buf)); 
  close(fd);
  return 0;
}

int mv(int argc, char** argv)
{
  if(argc != 3){
      printf("mv: invalid arguments, exactly 2 needed\n");
      return 0;
  }
  char* src = argv[1];
  char* dest = argv[2];

  if(!is_exist(src)){
    printf("mv: source file does not exist\n");
    return 0;
  }

  if (is_dir(src)) {
    if (is_exist(dest) && !is_dir(dest)) {
      printf("mv: cannot move to a file\n");
      return 0;
    }
    cp_r(dest, src, 1);
    rm_r(src, 1);
  }else {
    cp_r(dest, src, 1);
    rm_r(src, 0);
  }
	return 0;
}

int rm(int argc, char** argv)
{
  int rflag, i;
  char * path;

  if (argc == 1) {
    printf("Too few arguments\n");
    return 0;
  }
  if(!strcmp(argv[1], "-r")){
    if (argc < 3){
      printf("rm: no path given\n");
      return 0;
    }
    rflag = 1;
    i = 2;
  }
  else{
    rflag = 0;
    i = 1;
  }

  path = argv[i];
  if(!is_exist(path)){
    printf("rm: cannot remove, no such file or directory\n", path);
    return 0;
  }
  rm_r(path, rflag);
  return 0;
}


int rm_r(char *path, int rflag){
  int len;
  char * sub_path;
  char rm_buf[BUFFERLEN];

  if(rflag){
    if(!is_dir(path)) {
      return rm_file(path);
    }
    else{
      sys_chdir(path); 
      len = dir_list(rm_buf, NULL);
      sub_path = rm_buf;
      while(sub_path - rm_buf < len){
        if(strcmp(sub_path, ".") && strcmp(sub_path, "..")){ 
          rm_r(sub_path, rflag); // recursively rm subpath
        }
        sub_path += strlen(sub_path) + 1;
      }

      sys_chdir("..");
      rm_file(path);
    }
    return 0;
  }else{
    if(is_dir(path)){
      printf("rm: cannot remove directory. try '-r' for directory\n", path);
      return 0;
    }
    else{
      return rm_file(path);
    }
  }
}

int rm_file(char * filename){
  int errno = sys_unlink(filename);
  if(errno == -1){
    printf("rm: cannot remove: sys_unlink error\n", filename);
  }
  return errno;
}


int shell_mkdir(int argc, char** argv)
{
	int i;
	if (argc == 1)
		printf ("mkdir failed, no path\n");
	
	for (i = 1; i < argc; i++){
		if (sys_mkdir(argv[i]) == 0);	
      // printf("make dir succeed.\n");
		else
			printf("make dir failed.\n");
	}
	
	return 0;
}

int shell_cat(int argc, char** argv)
{
 	if (argc == 1) {
		printf("No Path \n");
		return 0;
	}
  if (argc > 2) {
		printf("Too many arguments \n");
		return 0;
	}

  char *path = argv[1];

	int fd, size;
  char buf[100];

  fd = open(path, O_RDONLY);
  if(fd == -1){
      printf("cat, open error \n");
      return 0;
  } 
  size = read(fd, buf, sizeof(buf) - 1);
  buf[size] = '\0';
  printf("%s\n", buf);
  close(fd);
  return 0;
}

int shell_touch(int argc, char** argv)
{
	if (argc == 1) {
		printf("No path given. \n");
		return 0;
	}
	int i;
	for (i = 1; i < argc; i++)
	{
    int fd;
    fd = open(argv[i], O_RDONLY);
		if(fd >= 0){
      printf("%s file already exists\n", argv[i]);
      close(fd);
			continue;
    }
    else{
			close(open(argv[i], O_CREATE));
		}	
	}
	return 0;
}

int shell_write(int argc, char** argv) {
  if (argc == 1) {
    printf("write: too few arguments.\n");
    return 0;
  }
  int fd = open(argv[2], O_CREATE|O_RDWR);
  if (fd >= 0) {
    int n = write(fd, argv[1], strlen(argv[1]));
    if (n != strlen(argv[1])) {
      printf("write, write error\n");
    }
    close(fd);
  }else {
    printf("write, open error\n");
  }
  return 0;
}

int shell_append(int argc, char** argv) {
  if (argc == 1) {
    printf("append failed, too few arguments.\n");
    return 0;
  } 
  int fd = open(argv[2], O_RDONLY);
  if (fd >= 0) {
    char buf[1000];
    int n = read(fd, buf, 1000);
    close(fd);
    if (n == 0) {
      buf[0] = 0;
    }
    fd = open(argv[2], O_CREATE|O_RDWR);
    if (fd < 0) {
      printf("append, fd error");
      return 0;
    }
    strncpy(buf + n, argv[1], strlen(argv[1]));
    write(fd, buf, strlen(buf));
    close(fd);
  }else {
    printf("append, open error\n");
  }
  return 0;
}

int get_filename(char* path, char* filename) {
  int n = strlen(path);
  if (n == 0) return 0;
  int pos = n - 1;
  
  while (pos >= 0) {
    if (path[pos] == '/') {
      break;
    }
    pos--;
  } 

  strncpy(filename, path + pos + 1, n - (pos + 1));
  return n - pos - 1;
}

int run_command(char *buf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	argc = 0;
	argv[argc] = 0;
	
	while(1)
	{
		while (*buf && strchr(PARSESPACE, *buf))
			*buf++ = 0;
    
		if (*buf == 0)
			break;

		if (argc == MAXARGS - 1)
      break;

		argv[argc++] = buf;
		while (*buf && !strchr(PARSESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;
	if (argc == 0)
		return 0;

	for (i = 0; i < NUMCOMMANDS; i++)
	{
		if (strcmp(argv[0], cmds[i].name) == 0)
			return cmds[i].func(argc, argv);
	}

	printf("Unknown command '%s'\n", argv[0]);

	return 0;
}


int main (int argc, char** argv)
{
	char buf[1024];
	printf("\n##### Shell started #####\n");

	close(open("file1", O_CREATE | O_RDWR));
  close(open("file2", O_CREATE | O_RDWR));
  // close(open("file3", O_CREATE | O_RDWR));
  // close(open("file4", O_CREATE | O_RDWR));
  // close(open("file5", O_CREATE | O_RDWR));

	while(1)
	{
		sys_readline(buf);
		if (buf != NULL){
      int ret = run_command(buf);
			if (ret < 0)
				break;
		}
	}

  printf("\nShell ERROR, rerun\n");
}


