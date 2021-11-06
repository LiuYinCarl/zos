#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

struct devsw devsw[NDEV];

struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void) {
  initlock(&ftable.lock, "ftable");
}

struct file*
filealloc(void) {
  struct file *f;

  acquire(&ftable.lock);
  for (f = ftable.file; f < ftable.file + NFILE; f++) {
    if (f->ref == 0) {
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// 增加对文件 f 的引用计数
struct file*
filedup(struct file *f) {
  acquire(&ftable.lock);
  if (f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// 关闭文件
void
fileclose(struct file *f) {
  struct file ff;

  acquire(&ftable.lock);
  if (f->ref < 1)
    panic("fileclose");
  if (--f->ref > 0) {
    release(&ftable.lock);
    return;
  }

  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if (ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if (ff.type == FD_INODE) {
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// 获取文件的元信息
int32
filestat(struct file *f, struct stat *st) {
  if (f->type == FD_INODE) {
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// 读文件
int32
fileread(struct file *f, char *addr, int32 n) {
  int32 r;

  if (f->readable == 0)
    return -1;
  if (f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if (f->type == FD_INODE) {
    ilock(f->ip);
    if ((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

// 写文件

