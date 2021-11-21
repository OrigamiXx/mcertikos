#ifndef _USER_SHELL_H_
#define _USER_SHELL_H_

int ls(int argc, char ** argv);
int pwd(int argc, char **argv);
int cd(int argc, char **argv);
int cp(int argc, char **argv);
int mv(int argc, char **argv);
int rm(int argc, char **argv);
int shell_mkdir(int argc, char **argv);
int shell_cat(int argc, char **argv);
int shell_touch(int argc, char **argv);
int shell_write(int argc, char **argv);
int shell_append(int argc, char **argv);
int run_command (char *buf);

int is_dir(char * path);
int is_exist(char* path);
int rm_r(char * path, int rflag);
int cp_r(char * dest, char * src, int rflag);
int rm_file(char * filename);
int cp_file(char * dest_filename, char * src_filename);
int dir_list(char* buf, char* path);
int get_filename(char * path, char * filename);


#endif