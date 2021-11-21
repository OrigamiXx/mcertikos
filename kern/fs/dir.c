#include <kern/lib/types.h>
#include <kern/lib/debug.h>
#include <kern/lib/string.h>
#include "inode.h"
#include "dir.h"

// Directories

int dir_namecmp(const char *s, const char *t)
{
    return strncmp(s, t, DIRSIZ);
}

/**
 * Look for a directory entry in a directory.
 * If found, set *poff to byte offset of entry.
 */
struct inode *dir_lookup(struct inode *dp, char *name, uint32_t * poff)
{
    uint32_t off, inum;
    struct dirent de;

    if (dp->type != T_DIR)
      KERN_PANIC("dir_lookup not DIR");

    //TODO
    uint32_t size = sizeof(de);
    for(off = 0; off < dp->size; off += size)
    {
      if(inode_read(dp, (char*)&de, off, size) != size)        
          KERN_PANIC("read size mismatch, dir_lookup");   
      if(de.inum == 0)
          continue;
      
      if(dir_namecmp(de.name, name) == 0){
          if(poff != 0)
            *poff = off;
          
          inum = de.inum;
          return inode_get(dp->dev, inum);
      }
    }
    return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int dir_link(struct inode *dp, char *name, uint32_t inum)
{
    struct dirent de;
    struct inode* node;

    uint32_t poff;
    uint32_t off;

    // TODO: Check that name is not present.
    uint32_t size = sizeof(de);
    node = dir_lookup(dp, name, &poff);

    if(node != 0 ){
      inode_put(node);
      return -1;
    }
    // TODO: Look for an empty dirent.
    for(off = 0; off < dp->size; off += size)
    {
      if(inode_read(dp, (char*)&de, off, size) != size)
        KERN_PANIC("read size mismatch, dir_link");
      if(de.inum == 0)
        break;
    }

    de.inum = inum;
    strncpy(de.name, name, DIRSIZ);
    if(inode_write(dp, (char*)&de, off, size) != size){
      KERN_PANIC("write size mismatch, dir_link");
    }

    return 0;
}
