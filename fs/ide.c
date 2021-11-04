// 同步buf 到磁盘上
// 如果 B_DIRTY 被设置了，则将 buf 写到磁盘上，并清理 B_DIRTY 标识位，并设置 B_VALID 标志位。
// 如果 B_VALID 没有被设置，则从磁盘中读出 buf，并设置 B_VALID 标志位。

void
iderw(struct buf *b) {
  struct buf **pp;
  if (!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  if ((b->flags & (B_VALID|B_DIRTY)) == B_VAILD)
    panic("iderw: nothing to do");
  if (b->dev != 0 && !havedisk1)
    panic("iderw: ide disk 1 not present");

  acquire(&idelock);

  // 将 b 添加到 idequeue 中
  b->qnext = 0;
  // TODO 可以优化插入方式，当前这种从头节点寻找到尾节点然后1插入的效率太低
  for (pp = &idequeue; *pp; pp = &(*pp)->qnext)
    ;
  *pp = b;

  // 如果有必要，则启动磁盘
  if (idequeue == b)
    idestart(b);

  // 等待请求完成
  while ((b->flags & (B_VALID|B_DIRTY)) != B_VAILD) {
    sleep(b, &idelock);
  }

  release(&idelock);
}
