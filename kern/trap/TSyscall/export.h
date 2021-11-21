#ifndef _KERN_TRAP_TSYSCALL_H_
#define _KERN_TRAP_TSYSCALL_H_

#ifdef _KERN_

void sys_puts(tf_t *tf);
void sys_spawn(tf_t *tf);
void sys_yield(tf_t *tf);

void sys_dir(tf_t * tf);
void sys_ls(tf_t *tf);
void sys_pwd(tf_t *tf);
void sys_readline(tf_t *tf);

#endif  /* _KERN_ */

#endif  /* !_KERN_TRAP_TSYSCALL_H_ */
