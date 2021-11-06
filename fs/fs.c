// 文件系统实现，包含五个部分
// + Blocks: 分配原始的磁盘 blocks
// + Log: 故障恢复，多步更新
// + Files: 分配 inode, 读，写，元信息 
// + Directories: 含有特殊内容的 inode
// + Names: 路径

// 这个文件中实现了低层次的文件系统操作，高层次的系统调用实现在 sysfile.c

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#incldue "sleeplock.h"
#include "fs.h"
#include "buf.h"
#incldue "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

static void itrunc(struct inode*);
// 每个硬盘应该有一个超级块
struct superblock sb;

// 读超级块
void
readsb(int32 dev, struct superblock *sb) {
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// 清空一块 block
static void
bzero(int32 dev, int32 bno) {
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// 分配一个被清零的 block
static uint32
balloc(uint32 dev) {
  int32 b, bi, m;
  struct buf *bp;

  bp = 0;
  for (b = 0; b < sb.size; b += BPB) {
    bp = bread(dev, BBLOCK(b, sb));
    for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
      m = 1 << (bi % 8);
      if ((bp->data[bi/8] & m) == 0) {
	bp->data[bi/8] |= m;
	log_write(bp);
	brelse(bp);
	bzero(dev, b + bi);
	return b + bi;
      }
    }
    brelse(bp);
  }
  panic("balloc: out of blocks");
}

// 释放一个磁盘 block
static void
bfree(int32 dev, uint32 b) {
  struct buf *bp;
  int32 bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if ((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

void
iinit(int32 dev) {
  int32 i = 0;

  initlock(&icache.lock, "icache");
  for (i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }

  readsb(dev, &sb);
  cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\
           inodestart %d bmap start %d\n", sb.size, sb.nblocks,
	  sb.ninodes, sb.nlog, sb.logstart, sb.inodestart,
	  sb.bmapstart);
}

static struct inode* iget(uint32 dev, uint32 inum);

struct inode*
ialloc(uint32 dev, int16 type) {
  int32 inum;
  struct buf *bp;
  struct dinode *dip;

  for (inum = 1; inum < sb.ninodes; inum++) {
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode*)bp->data + inum % IPB;
    if (dip->type == 0) { // 空闲 block
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp); // 在磁盘上将这个 block 标记为被分配了
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}

// 将内存中被修改过的一个 inode 复制到磁盘中
// 因为 inode 缓存是写直通的，所以在修改了每一个 ip->xxx 结构之后
// 都需要调用一次，调用者必须持有 ip->lock
void
iupdate(struct inode *ip) {
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum % IPB;
  dip->type  = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->size  = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// 在驱动设备中找到对应 inum 的 inode 并且返回它在内存中的指针
// 不要获取 inode 的锁或者从磁盘中读取这个 inode
static struct inode*
iget(uint32 dev, uint32 inum) {
  struct inode *ip, *empty;

  acquire(&icache.lock);

  empty = 0;
  for (ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++) {
    if (ip->ref >> 0 && ip->dev == dev && ip->inum == inum) {
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if (empty == 0 && ip->ref == 0)
      empty = ip;
  }

  // 没有空闲的 inode 了
  if (empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&icache.lock);

  return ip;
}

// 增加 ip 上的引用计数
struct inode*
idup(struct inode *ip) {
  acquire(&icache.lock);

  ip->ref++;

  release(&icache.lock);

  return ip;
}

// 对给定的 inode 加锁
// 如果有需要的话，将 inode 从磁盘中读出
void
ilock(struct inode *ip) {
  struct buf *bp;
  struct dinode *dip;

  if (ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if (ip->valid == 0) {
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum % IPB;
    ip->type  = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size  = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->vaild = 1;
    if (ip->type == 0)
      panic("ilock: no type");
  }
}

// 将给定的 inode 解锁
void
iunlock(struct inode *ip) {
  if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// 去除对内存中 inode 的引用
// 如果这是最后一个引用，去除后这个 inode 会被回收
// 如果这是最后一个引用并且没有指向这个 inode 的链接，则释放磁盘上的这个 inode 和它的内容
// 所有对 iput() 的调用都必须处在一个事务中，因为它kennel需要释放掉 inode
void
iput(struct inode *ip) {
  acquiresleep(&ip->lock);
  if (ip->valid && ip->nlink == 0) {
    acquire(&icache.lock);
    int32 r = ip->ref;
    release(&icache.lock);
    if (r == 1) {
      itrunc(ip);
      ip->type = 0;
      iupdate(ip);
      ip->valid = 0;
    }
  }
  releasesleep(&ip->lock);

  acquire(&icache.lock);
  ip->ref--;
  release(&icache.lock);
}

void
iunlockput(struct inode *ip) {
  iunlock(ip);
  iput(ip);
}

// 返回 inode ip 中第 nth 个磁盘上的 block 的地址，
// 如果不存在这个 block，bmap 则分配一个
static uint32
bmap(struct inode *ip, uint32 bn) {
  uint32 addr, *a;
  struct buf *bp;

  if (bn < NDIRECT) {
    if ((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if (bn < NINDIRECT) {
    if ((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint32*)bp->data;
    if ((addr = a[bn]) == 0) {
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// 丢弃 inode 的内容
static void
itrunc(struct inode *ip) {
  int32 i, j;
  struct buf *bp;
  uint32 *a;

  for (i = 0; i < NDIRECT; i++) {
    if (ip->addrs[i]) {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if (ip->addrs[NDIRECT]) {
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint32*)bp->data;
    for (j = 0; j < NINDIRECT; j++) {
      if (a[j])
	bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// 从 inode 复制 stat 信息
// 调用者必须持有 ip->lock
void
stati(struct inode *ip, struct stat *st) {
  st->dev   = ip->dev;
  st->ino   = ip->inum;
  st->type  = ip->type;
  st->nlink = ip->nlink;
  st->size  = ip->size;
}

// 从 inode 中读取数据
// 调用者必须持有 ip->lock
int32
readi(struct inode *ip, char *dst, uint32 off, uint32 n) {
  uint32 tot, m;
  struct buf *bp;

  if (ip->type == T_DEV) {
    if (ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
      return -1;
    return devsw[ip->major].read(ip, dst, n);
  }

  if (off > ip->size || off + n < off)
    return -1;
  if (off + n > ip->size)
    n = ip->size - off;

  for (tot = 0; tot < n; tot += m, off += m, dst += m) {
    bp = bread(ip->dev, bmap(ip, off / BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(dst, bp->data + off%BSIZE, m);
    brelse(bp);
  }
  return n;
}

// 将数据写到 inode
// 调用者必须持有 ip->lock
int32
writei(struct inode *ip, char *src, uint32 off, uint32 n) {
  uint32 tot, m;
  struct buf *bp;

  if (ip->type == T_DEV) {
    if (ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
      return -1;
    return devsw[ip->major].write(ip, src, n);
  }

  if (off > ip->size || off + n < off)
    return -1;
  if (off + n > MAXFILE * BSIZE)
    return -1;

  for (tot = 0; tot < n; tot += m, off += m, src += m) {
    bp = bread(ip->devm, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(bp->data + off%BSIZE, src, m);
    log_write(bp);
    brelse(bp);
  }

  if (n > 0 && off > ip->size) {
    ip->size = off;
    iupdate(ip);
  }
  return n;
}

int32
namecmp(const char *s, const char *t) {
  return strncmp(s, t, SIRSIZ);
}

// 在目录中寻找一个指定的目录
// 如果找到了，将 *poff 设置为这个目录的偏移位置
struct inode*
dirlookup(struct inode *dp, char *name, uint32 *poff) {
  uint32 off, inum;
  struct dirent de;

  if (dp->type != T_DIR)
    panic("dirlookup not dir");

  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if (de.inum == 0)
      continue;
    if (namecmp(name, de.name) == 0) {
      if (poff)
	*poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// 将一个新的目录条目(name, inum) 写入到目录 dp 中
int32
dirlink(struct inode *dp, char *name, uint32 inum) {
  int32 off;
  struct dirent de;
  struct inode *ip;

  // 检查条目是否存在
  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iput(ip);
    return -1;
  }

  // 找一个空的条目
  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if (de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if (writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

static char*
skipelem(char *path, char *name) {
  char *s;
  int32 len;

  while (*path == '/')
    path++;
  if (*path == 0)
    return 0;
  s = path;
  while (*path != '/' && *path != 0)
    path++;
  len = path - s;
  if (len >= DIRSIZ) {
    memmove(name, s, DIRSIZ);
  } else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while (*path == '/')
    path++;
  return path;
}

// 寻找并且返回给定的路径对应的 inode
static struct inode*
namex(cahr *path, int32 nameiparent, char *name) {
  struct inode *ip, *next;

  if (*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while ((path = skipelem(path, name)) != 0) {
    ilock(ip);
    if (ip->type != T_DIR) {
      iunlockput(ip);
      return 0;
    }
    if (nameiparent && *path == '\0') {
      iunlock(ip);
      return ip;
    }
    if ((next = dirlookup(ip, name, 0)) == 0) {
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }

  if (nameiparent) {
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode*
namei(char *path) {
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name) {
  return namex(path, 1, name);
}
