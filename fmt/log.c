4800 #include "types.h"
4801 #include "defs.h"
4802 #include "param.h"
4803 #include "spinlock.h"
4804 #include "sleeplock.h"
4805 #include "fs.h"
4806 #include "buf.h"
4807 
4808 // Simple logging that allows concurrent FS system calls.
4809 //
4810 // A log transaction contains the updates of multiple FS system
4811 // calls. The logging system only commits when there are
4812 // no FS system calls active. Thus there is never
4813 // any reasoning required about whether a commit might
4814 // write an uncommitted system call's updates to disk.
4815 //
4816 // A system call should call begin_op()/end_op() to mark
4817 // its start and end. Usually begin_op() just increments
4818 // the count of in-progress FS system calls and returns.
4819 // But if it thinks the log is close to running out, it
4820 // sleeps until the last outstanding end_op() commits.
4821 //
4822 // The log is a physical re-do log containing disk blocks.
4823 // The on-disk log format:
4824 //   header block, containing block #s for block A, B, C, ...
4825 //   block A
4826 //   block B
4827 //   block C
4828 //   ...
4829 // Log appends are synchronous.
4830 
4831 // Contents of the header block, used for both the on-disk header block
4832 // and to keep track in memory of logged block# before commit.
4833 struct logheader {
4834   int n;
4835   int block[LOGSIZE];
4836 };
4837 
4838 struct log {
4839   struct spinlock lock;
4840   int start;
4841   int size;
4842   int outstanding; // how many FS sys calls are executing.
4843   int committing;  // in commit(), please wait.
4844   int dev;
4845   struct logheader lh;
4846 };
4847 
4848 
4849 
4850 struct log log;
4851 
4852 static void recover_from_log(void);
4853 static void commit();
4854 
4855 void
4856 initlog(int dev)
4857 {
4858   if (sizeof(struct logheader) >= BSIZE)
4859     panic("initlog: too big logheader");
4860 
4861   struct superblock sb;
4862   initlock(&log.lock, "log");
4863   readsb(dev, &sb);
4864   log.start = sb.logstart;
4865   log.size = sb.nlog;
4866   log.dev = dev;
4867   recover_from_log();
4868 }
4869 
4870 // Copy committed blocks from log to their home location
4871 static void
4872 install_trans(void)
4873 {
4874   int tail;
4875 
4876   for (tail = 0; tail < log.lh.n; tail++) {
4877     struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
4878     struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
4879     memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
4880     bwrite(dbuf);  // write dst to disk
4881     brelse(lbuf);
4882     brelse(dbuf);
4883   }
4884 }
4885 
4886 // Read the log header from disk into the in-memory log header
4887 static void
4888 read_head(void)
4889 {
4890   struct buf *buf = bread(log.dev, log.start);
4891   struct logheader *lh = (struct logheader *) (buf->data);
4892   int i;
4893   log.lh.n = lh->n;
4894   for (i = 0; i < log.lh.n; i++) {
4895     log.lh.block[i] = lh->block[i];
4896   }
4897   brelse(buf);
4898 }
4899 
4900 // Write in-memory log header to disk.
4901 // This is the true point at which the
4902 // current transaction commits.
4903 static void
4904 write_head(void)
4905 {
4906   struct buf *buf = bread(log.dev, log.start);
4907   struct logheader *hb = (struct logheader *) (buf->data);
4908   int i;
4909   hb->n = log.lh.n;
4910   for (i = 0; i < log.lh.n; i++) {
4911     hb->block[i] = log.lh.block[i];
4912   }
4913   bwrite(buf);
4914   brelse(buf);
4915 }
4916 
4917 static void
4918 recover_from_log(void)
4919 {
4920   read_head();
4921   install_trans(); // if committed, copy from log to disk
4922   log.lh.n = 0;
4923   write_head(); // clear the log
4924 }
4925 
4926 // called at the start of each FS system call.
4927 void
4928 begin_op(void)
4929 {
4930   acquire(&log.lock);
4931   while(1){
4932     if(log.committing){
4933       sleep(&log, &log.lock);
4934     } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
4935       // this op might exhaust log space; wait for commit.
4936       sleep(&log, &log.lock);
4937     } else {
4938       log.outstanding += 1;
4939       release(&log.lock);
4940       break;
4941     }
4942   }
4943 }
4944 
4945 
4946 
4947 
4948 
4949 
4950 // called at the end of each FS system call.
4951 // commits if this was the last outstanding operation.
4952 void
4953 end_op(void)
4954 {
4955   int do_commit = 0;
4956 
4957   acquire(&log.lock);
4958   log.outstanding -= 1;
4959   if(log.committing)
4960     panic("log.committing");
4961   if(log.outstanding == 0){
4962     do_commit = 1;
4963     log.committing = 1;
4964   } else {
4965     // begin_op() may be waiting for log space,
4966     // and decrementing log.outstanding has decreased
4967     // the amount of reserved space.
4968     wakeup(&log);
4969   }
4970   release(&log.lock);
4971 
4972   if(do_commit){
4973     // call commit w/o holding locks, since not allowed
4974     // to sleep with locks.
4975     commit();
4976     acquire(&log.lock);
4977     log.committing = 0;
4978     wakeup(&log);
4979     release(&log.lock);
4980   }
4981 }
4982 
4983 // Copy modified blocks from cache to log.
4984 static void
4985 write_log(void)
4986 {
4987   int tail;
4988 
4989   for (tail = 0; tail < log.lh.n; tail++) {
4990     struct buf *to = bread(log.dev, log.start+tail+1); // log block
4991     struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
4992     memmove(to->data, from->data, BSIZE);
4993     bwrite(to);  // write the log
4994     brelse(from);
4995     brelse(to);
4996   }
4997 }
4998 
4999 
5000 static void
5001 commit()
5002 {
5003   if (log.lh.n > 0) {
5004     write_log();     // Write modified blocks from cache to log
5005     write_head();    // Write header to disk -- the real commit
5006     install_trans(); // Now install writes to home locations
5007     log.lh.n = 0;
5008     write_head();    // Erase the transaction from the log
5009   }
5010 }
5011 
5012 // Caller has modified b->data and is done with the buffer.
5013 // Record the block number and pin in the cache with B_DIRTY.
5014 // commit()/write_log() will do the disk write.
5015 //
5016 // log_write() replaces bwrite(); a typical use is:
5017 //   bp = bread(...)
5018 //   modify bp->data[]
5019 //   log_write(bp)
5020 //   brelse(bp)
5021 void
5022 log_write(struct buf *b)
5023 {
5024   int i;
5025 
5026   if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
5027     panic("too big a transaction");
5028   if (log.outstanding < 1)
5029     panic("log_write outside of trans");
5030 
5031   acquire(&log.lock);
5032   for (i = 0; i < log.lh.n; i++) {
5033     if (log.lh.block[i] == b->blockno)   // log absorbtion
5034       break;
5035   }
5036   log.lh.block[i] = b->blockno;
5037   if (i == log.lh.n)
5038     log.lh.n++;
5039   b->flags |= B_DIRTY; // prevent eviction
5040   release(&log.lock);
5041 }
5042 
5043 
5044 
5045 
5046 
5047 
5048 
5049 
