#include <lib/debug.h>
#include <lib/pmap.h>
#include <lib/types.h>
#include <lib/x86.h>
#include <lib/trap.h>
#include <lib/syscall.h>
#include <lib/string.h>
#include <dev/intr.h>
#include <dev/console.h>
#include <pcpu/PCPUIntro/export.h>
#include <thread/PTCBIntro/export.h>
#include <fs/dir.h>
#include <fs/inode.h>

#include "import.h"

static char sys_buf[NUM_IDS][PAGESIZE];

/**
 * Copies a string from user into buffer and prints it to the screen.
 * This is called by the user level "printf" library as a system call.
 */
void sys_puts(tf_t *tf)
{
    unsigned int cur_pid;
    unsigned int str_uva, str_len;
    unsigned int remain, cur_pos, nbytes;

    cur_pid = get_curid();
    str_uva = syscall_get_arg2(tf);
    str_len = syscall_get_arg3(tf);

    if (!(VM_USERLO <= str_uva && str_uva + str_len <= VM_USERHI)) {
        syscall_set_errno(tf, E_INVAL_ADDR);
        return;
    }

    remain = str_len;
    cur_pos = str_uva;

    while (remain) {
        if (remain < PAGESIZE - 1)
            nbytes = remain;
        else
            nbytes = PAGESIZE - 1;

        if (pt_copyin(cur_pid, cur_pos, sys_buf[cur_pid], nbytes) != nbytes) {
            syscall_set_errno(tf, E_MEM);
            return;
        }

        sys_buf[cur_pid][nbytes] = '\0';
        KERN_INFO("%s", sys_buf[cur_pid]);

        remain -= nbytes;
        cur_pos += nbytes;
    }

    syscall_set_errno(tf, E_SUCC);
}

extern uint8_t _binary___obj_user_pingpong_ping_start[];
extern uint8_t _binary___obj_user_pingpong_pong_start[];
extern uint8_t _binary___obj_user_pingpong_ding_start[];
extern uint8_t _binary___obj_user_fstest_fstest_start[];
extern uint8_t _binary___obj_user_shell_shell_start[];

/**
 * Spawns a new child process.
 * The user level library function sys_spawn (defined in user/include/syscall.h)
 * takes two arguments [elf_id] and [quota], and returns the new child process id
 * or NUM_IDS (as failure), with appropriate error number.
 * Currently, we have three user processes defined in user/pingpong/ directory,
 * ping, pong, and ding.
 * The linker ELF addresses for those compiled binaries are defined above.
 * Since we do not yet have a file system implemented in mCertiKOS,
 * we statically load the ELF binaries into the memory based on the
 * first parameter [elf_id].
 * For example, ping, pong, and ding correspond to the elf_ids
 * 1, 2, 3, and 4, respectively.
 * If the parameter [elf_id] is none of these, then it should return
 * NUM_IDS with the error number E_INVAL_PID. The same error case apply
 * when the proc_create fails.
 * Otherwise, you should mark it as successful, and return the new child process id.
 */
void sys_spawn(tf_t *tf)
{
    unsigned int new_pid;
    unsigned int elf_id, quota;
    void *elf_addr;
    unsigned int curid = get_curid();

    elf_id = syscall_get_arg2(tf);
    quota = syscall_get_arg3(tf);

    if (!container_can_consume(curid, quota)) {
        syscall_set_errno(tf, E_EXCEEDS_QUOTA);
        syscall_set_retval1(tf, NUM_IDS);
        return;
    }
    else if (NUM_IDS < curid * MAX_CHILDREN + 1 + MAX_CHILDREN) {
        syscall_set_errno(tf, E_MAX_NUM_CHILDEN_REACHED);
        syscall_set_retval1(tf, NUM_IDS);
        return;
    }
    else if (container_get_nchildren(curid) == MAX_CHILDREN) {
        syscall_set_errno(tf, E_INVAL_CHILD_ID);
        syscall_set_retval1(tf, NUM_IDS);
        return;
    }

    switch (elf_id) {
    case 1:
        elf_addr = _binary___obj_user_pingpong_ping_start;
        break;
    case 2:
        elf_addr = _binary___obj_user_pingpong_pong_start;
        break;
    case 3:
        elf_addr = _binary___obj_user_pingpong_ding_start;
        break;
    case 4:
        elf_addr = _binary___obj_user_fstest_fstest_start;
        break;
    case 5:
        elf_addr = _binary___obj_user_shell_shell_start;
        break;
    default:
        syscall_set_errno(tf, E_INVAL_PID);
        syscall_set_retval1(tf, NUM_IDS);
        return;
    }

    new_pid = proc_create(elf_addr, quota);

    if (new_pid == NUM_IDS) {
        syscall_set_errno(tf, E_INVAL_PID);
        syscall_set_retval1(tf, NUM_IDS);
    } else {
        syscall_set_errno(tf, E_SUCC);
        syscall_set_retval1(tf, new_pid);
    }
}

/**
 * Yields to another thread/process.
 * The user level library function sys_yield (defined in user/include/syscall.h)
 * does not take any argument and does not have any return values.
 * Do not forget to set the error number as E_SUCC.
 */
void sys_yield(tf_t *tf)
{
    thread_yield();
    syscall_set_errno(tf, E_SUCC);
}


void sys_dir(tf_t * tf)
{
  int fd, type;
  int is_dir;
  struct file * fp;
  fd = syscall_get_arg2(tf);
  fp = tcb_get_openfiles(get_curid())[fd];
  
  if(fp == 0 || fp->ip == 0)
  {
    syscall_set_retval1(tf, -1);
    syscall_set_errno(tf, E_BADF);
    return;
  } 

  type = fp->ip->type;
  is_dir = (type == T_DIR);

  syscall_set_errno(tf, E_SUCC);
  syscall_set_retval1(tf, is_dir);
}


void sys_ls(tf_t *tf)
{
  uint32_t off, inum;
  struct dirent de;
  uint32_t de_size;
  struct inode * p_inode;

  int user_buf = syscall_get_arg2(tf);
  int buf_len = syscall_get_arg3(tf);
  int curid = get_curid();
  
  char * buf_p = sys_buf[curid];
  struct inode* dp = (struct inode*)tcb_get_cwd(curid);  
  de_size = sizeof(de);

  for(off = 0; off < dp->size; off += de_size){

    if(inode_read(dp, (char *)&de, off, de_size)!= de_size)
        KERN_PANIC("inode read length mismatch, sys_ls"); 
    
    if(de.inum == 0)
        continue;
     
    strncpy(buf_p, de.name, strnlen(de.name, 255));
    buf_p += strnlen(de.name, 255);
    *(buf_p++) = ' ';  
  }

  *(buf_p - 1) = '\0';

  int len;
  if(buf_p - sys_buf[curid] < buf_len){
    len = buf_p - sys_buf[curid];
  }
  else{
    len = buf_len;
  }
  pt_copyout(sys_buf[curid], curid, user_buf, len);
  
  syscall_set_errno(tf, E_SUCC);
  syscall_set_retval1(tf, len);
}


void sys_pwd(tf_t *tf)
{
  uint32_t poff;
  struct inode* curid = (struct inode*)tcb_get_cwd(get_curid());
  struct inode* parent = dir_lookup(curid, "..", &poff);
  char arr[20][20];

  struct dirent de;
  uint32_t de_size = sizeof(de);
  uint32_t off;
  int user_buf;

  user_buf = syscall_get_arg2(tf);
  char *p = sys_buf[get_curid()];

  
  int dir_lvl = 0;
  while (parent->inum != curid->inum){
    for(off = 0; off < parent->size; off += de_size){

      if(inode_read(parent, (char*)&de, off, de_size) != de_size)
         KERN_PANIC("inode read length mismatch, sys_pwd"); 
      
      if(de.inum == curid->inum){
        strncpy(arr[dir_lvl], de.name, 20);
        dir_lvl++;
        break;
      }
    }
    curid = parent;
    parent = dir_lookup(curid, "..", &poff);
  }

  strncpy(arr[dir_lvl], "/", 8);
  while(dir_lvl >= 0) {
    strncpy(p, arr[dir_lvl], 255);
    p += strnlen(arr[dir_lvl], 255);

    *(p++) = '/';
    // p++;
    dir_lvl--;
  }
  *(--p) = '\0';

  int len = p - sys_buf[get_curid()] + 1;
  pt_copyout(sys_buf[get_curid()], get_curid(), user_buf, len);  
  syscall_set_errno(tf, E_SUCC);
  syscall_set_retval1(tf, 0);
}


void sys_readline(tf_t *tf)
{
  char* kernbuf = readline(">:");
  unsigned int userbuf = syscall_get_arg2(tf);
  int len = strnlen(kernbuf, 100) + 1;

  if(pt_copyout((void*)kernbuf, get_curid(), userbuf, len) != len) {
    KERN_PANIC("Readline error, sys_readline\n");
    syscall_set_errno(tf, E_MEM);
    syscall_set_retval1(tf, -1);
  }
  syscall_set_errno(tf, E_SUCC);
  syscall_set_retval1(tf, 0);
}