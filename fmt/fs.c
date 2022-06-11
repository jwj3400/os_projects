5050 // File system implementation.  Five layers:
5051 //   + Blocks: allocator for raw disk blocks.
5052 //   + Log: crash recovery for multi-step updates.
5053 //   + Files: inode allocator, reading, writing, metadata.
5054 //   + Directories: inode with special contents (list of other inodes!)
5055 //   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
5056 //
5057 // This file contains the low-level file system manipulation
5058 // routines.  The (higher-level) system call implementations
5059 // are in sysfile.c.
5060 
5061 #include "types.h"
5062 #include "defs.h"
5063 #include "param.h"
5064 #include "stat.h"
5065 #include "mmu.h"
5066 #include "proc.h"
5067 #include "spinlock.h"
5068 #include "sleeplock.h"
5069 #include "fs.h"
5070 #include "buf.h"
5071 #include "file.h"
5072 
5073 #define min(a, b) ((a) < (b) ? (a) : (b))
5074 static void itrunc(struct inode*);
5075 // there should be one superblock per disk device, but we run with
5076 // only one device
5077 struct superblock sb;
5078 
5079 // Read the super block.
5080 void
5081 readsb(int dev, struct superblock *sb)
5082 {
5083   struct buf *bp;
5084 
5085   bp = bread(dev, 1);
5086   memmove(sb, bp->data, sizeof(*sb));
5087   brelse(bp);
5088 }
5089 
5090 
5091 
5092 
5093 
5094 
5095 
5096 
5097 
5098 
5099 
5100 // Zero a block.
5101 static void
5102 bzero(int dev, int bno)
5103 {
5104   struct buf *bp;
5105 
5106   bp = bread(dev, bno);
5107   memset(bp->data, 0, BSIZE);
5108   log_write(bp);
5109   brelse(bp);
5110 }
5111 
5112 // Blocks.
5113 
5114 // Allocate a zeroed disk block.
5115 static uint
5116 balloc(uint dev)
5117 {
5118   int b, bi, m;
5119   struct buf *bp;
5120 
5121   bp = 0;
5122   for(b = 0; b < sb.size; b += BPB){
5123     bp = bread(dev, BBLOCK(b, sb));
5124     for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
5125       m = 1 << (bi % 8);
5126       if((bp->data[bi/8] & m) == 0){  // Is block free?
5127         bp->data[bi/8] |= m;  // Mark block in use.
5128         log_write(bp);
5129         brelse(bp);
5130         bzero(dev, b + bi);
5131         return b + bi;
5132       }
5133     }
5134     brelse(bp);
5135   }
5136   panic("balloc: out of blocks");
5137 }
5138 
5139 
5140 
5141 
5142 
5143 
5144 
5145 
5146 
5147 
5148 
5149 
5150 // Free a disk block.
5151 static void
5152 bfree(int dev, uint b)
5153 {
5154   struct buf *bp;
5155   int bi, m;
5156 
5157   readsb(dev, &sb);
5158   bp = bread(dev, BBLOCK(b, sb));
5159   bi = b % BPB;
5160   m = 1 << (bi % 8);
5161   if((bp->data[bi/8] & m) == 0)
5162     panic("freeing free block");
5163   bp->data[bi/8] &= ~m;
5164   log_write(bp);
5165   brelse(bp);
5166 }
5167 
5168 // Inodes.
5169 //
5170 // An inode describes a single unnamed file.
5171 // The inode disk structure holds metadata: the file's type,
5172 // its size, the number of links referring to it, and the
5173 // list of blocks holding the file's content.
5174 //
5175 // The inodes are laid out sequentially on disk at
5176 // sb.startinode. Each inode has a number, indicating its
5177 // position on the disk.
5178 //
5179 // The kernel keeps a cache of in-use inodes in memory
5180 // to provide a place for synchronizing access
5181 // to inodes used by multiple processes. The cached
5182 // inodes include book-keeping information that is
5183 // not stored on disk: ip->ref and ip->valid.
5184 //
5185 // An inode and its in-memory representation go through a
5186 // sequence of states before they can be used by the
5187 // rest of the file system code.
5188 //
5189 // * Allocation: an inode is allocated if its type (on disk)
5190 //   is non-zero. ialloc() allocates, and iput() frees if
5191 //   the reference and link counts have fallen to zero.
5192 //
5193 // * Referencing in cache: an entry in the inode cache
5194 //   is free if ip->ref is zero. Otherwise ip->ref tracks
5195 //   the number of in-memory pointers to the entry (open
5196 //   files and current directories). iget() finds or
5197 //   creates a cache entry and increments its ref; iput()
5198 //   decrements ref.
5199 //
5200 // * Valid: the information (type, size, &c) in an inode
5201 //   cache entry is only correct when ip->valid is 1.
5202 //   ilock() reads the inode from
5203 //   the disk and sets ip->valid, while iput() clears
5204 //   ip->valid if ip->ref has fallen to zero.
5205 //
5206 // * Locked: file system code may only examine and modify
5207 //   the information in an inode and its content if it
5208 //   has first locked the inode.
5209 //
5210 // Thus a typical sequence is:
5211 //   ip = iget(dev, inum)
5212 //   ilock(ip)
5213 //   ... examine and modify ip->xxx ...
5214 //   iunlock(ip)
5215 //   iput(ip)
5216 //
5217 // ilock() is separate from iget() so that system calls can
5218 // get a long-term reference to an inode (as for an open file)
5219 // and only lock it for short periods (e.g., in read()).
5220 // The separation also helps avoid deadlock and races during
5221 // pathname lookup. iget() increments ip->ref so that the inode
5222 // stays cached and pointers to it remain valid.
5223 //
5224 // Many internal file system functions expect the caller to
5225 // have locked the inodes involved; this lets callers create
5226 // multi-step atomic operations.
5227 //
5228 // The icache.lock spin-lock protects the allocation of icache
5229 // entries. Since ip->ref indicates whether an entry is free,
5230 // and ip->dev and ip->inum indicate which i-node an entry
5231 // holds, one must hold icache.lock while using any of those fields.
5232 //
5233 // An ip->lock sleep-lock protects all ip-> fields other than ref,
5234 // dev, and inum.  One must hold ip->lock in order to
5235 // read or write that inode's ip->valid, ip->size, ip->type, &c.
5236 
5237 struct {
5238   struct spinlock lock;
5239   struct inode inode[NINODE];
5240 } icache;
5241 
5242 void
5243 iinit(int dev)
5244 {
5245   int i = 0;
5246 
5247   initlock(&icache.lock, "icache");
5248   for(i = 0; i < NINODE; i++) {
5249     initsleeplock(&icache.inode[i].lock, "inode");
5250   }
5251 
5252   readsb(dev, &sb);
5253   cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\
5254  inodestart %d bmap start %d\n", sb.size, sb.nblocks,
5255           sb.ninodes, sb.nlog, sb.logstart, sb.inodestart,
5256           sb.bmapstart);
5257 }
5258 
5259 static struct inode* iget(uint dev, uint inum);
5260 
5261 
5262 
5263 
5264 
5265 
5266 
5267 
5268 
5269 
5270 
5271 
5272 
5273 
5274 
5275 
5276 
5277 
5278 
5279 
5280 
5281 
5282 
5283 
5284 
5285 
5286 
5287 
5288 
5289 
5290 
5291 
5292 
5293 
5294 
5295 
5296 
5297 
5298 
5299 
5300 // Allocate an inode on device dev.
5301 // Mark it as allocated by  giving it type type.
5302 // Returns an unlocked but allocated and referenced inode.
5303 struct inode*
5304 ialloc(uint dev, short type)
5305 {
5306   int inum;
5307   struct buf *bp;
5308   struct dinode *dip;
5309 
5310   for(inum = 1; inum < sb.ninodes; inum++){
5311     bp = bread(dev, IBLOCK(inum, sb));
5312     dip = (struct dinode*)bp->data + inum%IPB;
5313     if(dip->type == 0){  // a free inode
5314       memset(dip, 0, sizeof(*dip));
5315       dip->type = type;
5316       log_write(bp);   // mark it allocated on the disk
5317       brelse(bp);
5318       return iget(dev, inum);
5319     }
5320     brelse(bp);
5321   }
5322   panic("ialloc: no inodes");
5323 }
5324 
5325 // Copy a modified in-memory inode to disk.
5326 // Must be called after every change to an ip->xxx field
5327 // that lives on disk, since i-node cache is write-through.
5328 // Caller must hold ip->lock.
5329 void
5330 iupdate(struct inode *ip)
5331 {
5332   struct buf *bp;
5333   struct dinode *dip;
5334 
5335   bp = bread(ip->dev, IBLOCK(ip->inum, sb));
5336   dip = (struct dinode*)bp->data + ip->inum%IPB;
5337   dip->type = ip->type;
5338   dip->major = ip->major;
5339   dip->minor = ip->minor;
5340   dip->nlink = ip->nlink;
5341   dip->size = ip->size;
5342   memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
5343   log_write(bp);
5344   brelse(bp);
5345 }
5346 
5347 
5348 
5349 
5350 // Find the inode with number inum on device dev
5351 // and return the in-memory copy. Does not lock
5352 // the inode and does not read it from disk.
5353 static struct inode*
5354 iget(uint dev, uint inum)
5355 {
5356   struct inode *ip, *empty;
5357 
5358   acquire(&icache.lock);
5359 
5360   // Is the inode already cached?
5361   empty = 0;
5362   for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
5363     if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
5364       ip->ref++;
5365       release(&icache.lock);
5366       return ip;
5367     }
5368     if(empty == 0 && ip->ref == 0)    // Remember empty slot.
5369       empty = ip;
5370   }
5371 
5372   // Recycle an inode cache entry.
5373   if(empty == 0)
5374     panic("iget: no inodes");
5375 
5376   ip = empty;
5377   ip->dev = dev;
5378   ip->inum = inum;
5379   ip->ref = 1;
5380   ip->valid = 0;
5381   release(&icache.lock);
5382 
5383   return ip;
5384 }
5385 
5386 // Increment reference count for ip.
5387 // Returns ip to enable ip = idup(ip1) idiom.
5388 struct inode*
5389 idup(struct inode *ip)
5390 {
5391   acquire(&icache.lock);
5392   ip->ref++;
5393   release(&icache.lock);
5394   return ip;
5395 }
5396 
5397 
5398 
5399 
5400 // Lock the given inode.
5401 // Reads the inode from disk if necessary.
5402 void
5403 ilock(struct inode *ip)
5404 {
5405   struct buf *bp;
5406   struct dinode *dip;
5407 
5408   if(ip == 0 || ip->ref < 1)
5409     panic("ilock");
5410 
5411   acquiresleep(&ip->lock);
5412 
5413   if(ip->valid == 0){
5414     bp = bread(ip->dev, IBLOCK(ip->inum, sb));
5415     dip = (struct dinode*)bp->data + ip->inum%IPB;
5416     ip->type = dip->type;
5417     ip->major = dip->major;
5418     ip->minor = dip->minor;
5419     ip->nlink = dip->nlink;
5420     ip->size = dip->size;
5421     memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
5422     brelse(bp);
5423     ip->valid = 1;
5424     if(ip->type == 0)
5425       panic("ilock: no type");
5426   }
5427 }
5428 
5429 // Unlock the given inode.
5430 void
5431 iunlock(struct inode *ip)
5432 {
5433   if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
5434     panic("iunlock");
5435 
5436   releasesleep(&ip->lock);
5437 }
5438 
5439 
5440 
5441 
5442 
5443 
5444 
5445 
5446 
5447 
5448 
5449 
5450 // Drop a reference to an in-memory inode.
5451 // If that was the last reference, the inode cache entry can
5452 // be recycled.
5453 // If that was the last reference and the inode has no links
5454 // to it, free the inode (and its content) on disk.
5455 // All calls to iput() must be inside a transaction in
5456 // case it has to free the inode.
5457 void
5458 iput(struct inode *ip)
5459 {
5460   acquiresleep(&ip->lock);
5461   if(ip->valid && ip->nlink == 0){
5462     acquire(&icache.lock);
5463     int r = ip->ref;
5464     release(&icache.lock);
5465     if(r == 1){
5466       // inode has no links and no other references: truncate and free.
5467       itrunc(ip);
5468       ip->type = 0;
5469       iupdate(ip);
5470       ip->valid = 0;
5471     }
5472   }
5473   releasesleep(&ip->lock);
5474 
5475   acquire(&icache.lock);
5476   ip->ref--;
5477   release(&icache.lock);
5478 }
5479 
5480 // Common idiom: unlock, then put.
5481 void
5482 iunlockput(struct inode *ip)
5483 {
5484   iunlock(ip);
5485   iput(ip);
5486 }
5487 
5488 
5489 
5490 
5491 
5492 
5493 
5494 
5495 
5496 
5497 
5498 
5499 
5500 // Inode content
5501 //
5502 // The content (data) associated with each inode is stored
5503 // in blocks on the disk. The first NDIRECT block numbers
5504 // are listed in ip->addrs[].  The next NINDIRECT blocks are
5505 // listed in block ip->addrs[NDIRECT].
5506 
5507 // Return the disk block address of the nth block in inode ip.
5508 // If there is no such block, bmap allocates one.
5509 static uint
5510 bmap(struct inode *ip, uint bn)
5511 {
5512   uint addr, *a;
5513   struct buf *bp;
5514 
5515   if(bn < NDIRECT){
5516     if((addr = ip->addrs[bn]) == 0)
5517       ip->addrs[bn] = addr = balloc(ip->dev);
5518     return addr;
5519   }
5520   bn -= NDIRECT;
5521 
5522   if(bn < NINDIRECT){
5523     // Load indirect block, allocating if necessary.
5524     if((addr = ip->addrs[NDIRECT]) == 0)
5525       ip->addrs[NDIRECT] = addr = balloc(ip->dev);
5526     bp = bread(ip->dev, addr);
5527     a = (uint*)bp->data;
5528     if((addr = a[bn]) == 0){
5529       a[bn] = addr = balloc(ip->dev);
5530       log_write(bp);
5531     }
5532     brelse(bp);
5533     return addr;
5534   }
5535 
5536   panic("bmap: out of range");
5537 }
5538 
5539 
5540 
5541 
5542 
5543 
5544 
5545 
5546 
5547 
5548 
5549 
5550 // Truncate inode (discard contents).
5551 // Only called when the inode has no links
5552 // to it (no directory entries referring to it)
5553 // and has no in-memory reference to it (is
5554 // not an open file or current directory).
5555 static void
5556 itrunc(struct inode *ip)
5557 {
5558   int i, j;
5559   struct buf *bp;
5560   uint *a;
5561 
5562   for(i = 0; i < NDIRECT; i++){
5563     if(ip->addrs[i]){
5564       bfree(ip->dev, ip->addrs[i]);
5565       ip->addrs[i] = 0;
5566     }
5567   }
5568 
5569   if(ip->addrs[NDIRECT]){
5570     bp = bread(ip->dev, ip->addrs[NDIRECT]);
5571     a = (uint*)bp->data;
5572     for(j = 0; j < NINDIRECT; j++){
5573       if(a[j])
5574         bfree(ip->dev, a[j]);
5575     }
5576     brelse(bp);
5577     bfree(ip->dev, ip->addrs[NDIRECT]);
5578     ip->addrs[NDIRECT] = 0;
5579   }
5580 
5581   ip->size = 0;
5582   iupdate(ip);
5583 }
5584 
5585 // Copy stat information from inode.
5586 // Caller must hold ip->lock.
5587 void
5588 stati(struct inode *ip, struct stat *st)
5589 {
5590   st->dev = ip->dev;
5591   st->ino = ip->inum;
5592   st->type = ip->type;
5593   st->nlink = ip->nlink;
5594   st->size = ip->size;
5595 }
5596 
5597 
5598 
5599 
5600 // Read data from inode.
5601 // Caller must hold ip->lock.
5602 int
5603 readi(struct inode *ip, char *dst, uint off, uint n)
5604 {
5605   uint tot, m;
5606   struct buf *bp;
5607 
5608   if(ip->type == T_DEV){
5609     if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
5610       return -1;
5611     return devsw[ip->major].read(ip, dst, n);
5612   }
5613 
5614   if(off > ip->size || off + n < off)
5615     return -1;
5616   if(off + n > ip->size)
5617     n = ip->size - off;
5618 
5619   for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
5620     bp = bread(ip->dev, bmap(ip, off/BSIZE));
5621     m = min(n - tot, BSIZE - off%BSIZE);
5622     memmove(dst, bp->data + off%BSIZE, m);
5623     brelse(bp);
5624   }
5625   return n;
5626 }
5627 
5628 
5629 
5630 
5631 
5632 
5633 
5634 
5635 
5636 
5637 
5638 
5639 
5640 
5641 
5642 
5643 
5644 
5645 
5646 
5647 
5648 
5649 
5650 // Write data to inode.
5651 // Caller must hold ip->lock.
5652 int
5653 writei(struct inode *ip, char *src, uint off, uint n)
5654 {
5655   uint tot, m;
5656   struct buf *bp;
5657 
5658   if(ip->type == T_DEV){
5659     if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
5660       return -1;
5661     return devsw[ip->major].write(ip, src, n);
5662   }
5663 
5664   if(off > ip->size || off + n < off)
5665     return -1;
5666   if(off + n > MAXFILE*BSIZE)
5667     return -1;
5668 
5669   for(tot=0; tot<n; tot+=m, off+=m, src+=m){
5670     bp = bread(ip->dev, bmap(ip, off/BSIZE));
5671     m = min(n - tot, BSIZE - off%BSIZE);
5672     memmove(bp->data + off%BSIZE, src, m);
5673     log_write(bp);
5674     brelse(bp);
5675   }
5676 
5677   if(n > 0 && off > ip->size){
5678     ip->size = off;
5679     iupdate(ip);
5680   }
5681   return n;
5682 }
5683 
5684 
5685 
5686 
5687 
5688 
5689 
5690 
5691 
5692 
5693 
5694 
5695 
5696 
5697 
5698 
5699 
5700 // Directories
5701 
5702 int
5703 namecmp(const char *s, const char *t)
5704 {
5705   return strncmp(s, t, DIRSIZ);
5706 }
5707 
5708 // Look for a directory entry in a directory.
5709 // If found, set *poff to byte offset of entry.
5710 struct inode*
5711 dirlookup(struct inode *dp, char *name, uint *poff)
5712 {
5713   uint off, inum;
5714   struct dirent de;
5715 
5716   if(dp->type != T_DIR)
5717     panic("dirlookup not DIR");
5718 
5719   for(off = 0; off < dp->size; off += sizeof(de)){
5720     if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
5721       panic("dirlookup read");
5722     if(de.inum == 0)
5723       continue;
5724     if(namecmp(name, de.name) == 0){
5725       // entry matches path element
5726       if(poff)
5727         *poff = off;
5728       inum = de.inum;
5729       return iget(dp->dev, inum);
5730     }
5731   }
5732 
5733   return 0;
5734 }
5735 
5736 
5737 
5738 
5739 
5740 
5741 
5742 
5743 
5744 
5745 
5746 
5747 
5748 
5749 
5750 // Write a new directory entry (name, inum) into the directory dp.
5751 int
5752 dirlink(struct inode *dp, char *name, uint inum)
5753 {
5754   int off;
5755   struct dirent de;
5756   struct inode *ip;
5757 
5758   // Check that name is not present.
5759   if((ip = dirlookup(dp, name, 0)) != 0){
5760     iput(ip);
5761     return -1;
5762   }
5763 
5764   // Look for an empty dirent.
5765   for(off = 0; off < dp->size; off += sizeof(de)){
5766     if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
5767       panic("dirlink read");
5768     if(de.inum == 0)
5769       break;
5770   }
5771 
5772   strncpy(de.name, name, DIRSIZ);
5773   de.inum = inum;
5774   if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
5775     panic("dirlink");
5776 
5777   return 0;
5778 }
5779 
5780 
5781 
5782 
5783 
5784 
5785 
5786 
5787 
5788 
5789 
5790 
5791 
5792 
5793 
5794 
5795 
5796 
5797 
5798 
5799 
5800 // Paths
5801 
5802 // Copy the next path element from path into name.
5803 // Return a pointer to the element following the copied one.
5804 // The returned path has no leading slashes,
5805 // so the caller can check *path=='\0' to see if the name is the last one.
5806 // If no name to remove, return 0.
5807 //
5808 // Examples:
5809 //   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
5810 //   skipelem("///a//bb", name) = "bb", setting name = "a"
5811 //   skipelem("a", name) = "", setting name = "a"
5812 //   skipelem("", name) = skipelem("////", name) = 0
5813 //
5814 static char*
5815 skipelem(char *path, char *name)
5816 {
5817   char *s;
5818   int len;
5819 
5820   while(*path == '/')
5821     path++;
5822   if(*path == 0)
5823     return 0;
5824   s = path;
5825   while(*path != '/' && *path != 0)
5826     path++;
5827   len = path - s;
5828   if(len >= DIRSIZ)
5829     memmove(name, s, DIRSIZ);
5830   else {
5831     memmove(name, s, len);
5832     name[len] = 0;
5833   }
5834   while(*path == '/')
5835     path++;
5836   return path;
5837 }
5838 
5839 
5840 
5841 
5842 
5843 
5844 
5845 
5846 
5847 
5848 
5849 
5850 // Look up and return the inode for a path name.
5851 // If parent != 0, return the inode for the parent and copy the final
5852 // path element into name, which must have room for DIRSIZ bytes.
5853 // Must be called inside a transaction since it calls iput().
5854 static struct inode*
5855 namex(char *path, int nameiparent, char *name)
5856 {
5857   struct inode *ip, *next;
5858 
5859   if(*path == '/')
5860     ip = iget(ROOTDEV, ROOTINO);
5861   else
5862     ip = idup(myproc()->cwd);
5863 
5864   while((path = skipelem(path, name)) != 0){
5865     ilock(ip);
5866     if(ip->type != T_DIR){
5867       iunlockput(ip);
5868       return 0;
5869     }
5870     if(nameiparent && *path == '\0'){
5871       // Stop one level early.
5872       iunlock(ip);
5873       return ip;
5874     }
5875     if((next = dirlookup(ip, name, 0)) == 0){
5876       iunlockput(ip);
5877       return 0;
5878     }
5879     iunlockput(ip);
5880     ip = next;
5881   }
5882   if(nameiparent){
5883     iput(ip);
5884     return 0;
5885   }
5886   return ip;
5887 }
5888 
5889 struct inode*
5890 namei(char *path)
5891 {
5892   char name[DIRSIZ];
5893   return namex(path, 0, name);
5894 }
5895 
5896 
5897 
5898 
5899 
5900 struct inode*
5901 nameiparent(char *path, char *name)
5902 {
5903   return namex(path, 1, name);
5904 }
5905 
5906 #define SWAPBASE	500
5907 #define SWAPMAX		(100000 - SWAPBASE)
5908 
5909 void swapread(char* ptr, int blkno)
5910 {
5911 	struct buf* bp;
5912 	int i;
5913 
5914 	if ( blkno < 0 || blkno >= SWAPMAX )
5915 		panic("swapread: blkno exceed range");
5916 
5917 	for ( i=0; i < 8; ++i ) {
5918 		bp = bread(0, blkno + SWAPBASE + i);
5919 		memmove(ptr + i * BSIZE, bp->data, BSIZE);
5920 		brelse(bp);
5921 	}
5922 }
5923 
5924 void swapwrite(char* ptr, int blkno)
5925 {
5926 	struct buf* bp;
5927 	int i;
5928 
5929 	if ( blkno < 0 || blkno >= SWAPMAX )
5930 		panic("swapread: blkno exceed range");
5931 
5932 	for ( i=0; i < 8; ++i ) {
5933 		bp = bread(0, blkno + SWAPBASE + i);
5934 		memmove(bp->data, ptr + i * BSIZE, BSIZE);
5935 		bwrite(bp);
5936 		brelse(bp);
5937 	}
5938 }
5939 
5940 
5941 
5942 
5943 
5944 
5945 
5946 
5947 
5948 
5949 
