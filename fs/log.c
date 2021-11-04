#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

struct logheader {
  int32 n;
  int32 block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int32 start;
  int32 size;
  int32 outstanding; // 正在执行的系统调用数量
  int32 committing;
  int32 dev;
  struct logheader lh;
};

struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int32 dev) {
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  struct superblock sb;
  initlock(&log.lock, "log");
  readsb(dev, &sb);
  log.start = sb.logstart;
  log.size = sb.nlog;
  log.dev = dev;
  recover_from_log();
}

static void
install_trans(void) {
  int32 tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1);
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]);
    memmove(dbuf->data, lbuf->data, BSIZE);
    bwrite(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// 将日志头部从磁盘读到内存
static void
read_head(void) {
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *)(buf->data);

  log.lh.n = lh.n;
  for (int32 i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// 将日志头部从内存写到磁盘
static void
write_head(void) {
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *)(buf->data);

  hb->n = log.lh.n;
  for (int32 i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void) {
  read_head();
  install_trans(); // 如果提交了，从 log 复制到磁盘
  log.lh.n = 0;
  write_head();
}

// 这个函数在每个文件系统的系统调用开始的时候被调用
void
begin_op(void) {
  acquire(&log.lock);

  while (1) {
    if (log.committing) {
      sleep(&log, &log.lock);
    } else if (log.lh.n + (log.outstanding+1) * MAXOPBLOCKS > LOGSIZE) {
      sleep(&log, &log.lock);
    } else {
      log.outstanding++;
      release(&log.lock);
    }
  }
}

// 这个函数在每个文件系统的系统调用结束的时候被调用
// 如果这是最后一个未完成的操作，则完成之后进行提交操作
void
end_op(void) {
  int32 do_commit = 0;

  acquire(&log.lock);

  log.outstanding--;
  if (log.committing)
    panic("log.committing");
  if (log.outstanding == 0) {
    do_commit = 1;
    log.committing = 1;
  } else {
    wakeup(&log);
  }
  release(&log.lock);

  if (do_commit) {
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// 将修改过的 block 从 cache 复制到 log
static void
write_log(void) {
  for (int32 tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1);
    struct buf *from = bread(log.dev, log.lh.block[tail]);
    memmove(to->data, from->data, BSIZE);
    bwrite(to);
    bwrite(from);
    brelse(from);
    brelse(to);
  }
}

