5950 //
5951 // File descriptors
5952 //
5953 
5954 #include "types.h"
5955 #include "defs.h"
5956 #include "param.h"
5957 #include "fs.h"
5958 #include "spinlock.h"
5959 #include "sleeplock.h"
5960 #include "file.h"
5961 
5962 struct devsw devsw[NDEV];
5963 struct {
5964   struct spinlock lock;
5965   struct file file[NFILE];
5966 } ftable;
5967 
5968 void
5969 fileinit(void)
5970 {
5971   initlock(&ftable.lock, "ftable");
5972 }
5973 
5974 // Allocate a file structure.
5975 struct file*
5976 filealloc(void)
5977 {
5978   struct file *f;
5979 
5980   acquire(&ftable.lock);
5981   for(f = ftable.file; f < ftable.file + NFILE; f++){
5982     if(f->ref == 0){
5983       f->ref = 1;
5984       release(&ftable.lock);
5985       return f;
5986     }
5987   }
5988   release(&ftable.lock);
5989   return 0;
5990 }
5991 
5992 
5993 
5994 
5995 
5996 
5997 
5998 
5999 
6000 // Increment ref count for file f.
6001 struct file*
6002 filedup(struct file *f)
6003 {
6004   acquire(&ftable.lock);
6005   if(f->ref < 1)
6006     panic("filedup");
6007   f->ref++;
6008   release(&ftable.lock);
6009   return f;
6010 }
6011 
6012 // Close file f.  (Decrement ref count, close when reaches 0.)
6013 void
6014 fileclose(struct file *f)
6015 {
6016   struct file ff;
6017 
6018   acquire(&ftable.lock);
6019   if(f->ref < 1)
6020     panic("fileclose");
6021   if(--f->ref > 0){
6022     release(&ftable.lock);
6023     return;
6024   }
6025   ff = *f;
6026   f->ref = 0;
6027   f->type = FD_NONE;
6028   release(&ftable.lock);
6029 
6030   if(ff.type == FD_PIPE)
6031     pipeclose(ff.pipe, ff.writable);
6032   else if(ff.type == FD_INODE){
6033     begin_op();
6034     iput(ff.ip);
6035     end_op();
6036   }
6037 }
6038 
6039 
6040 
6041 
6042 
6043 
6044 
6045 
6046 
6047 
6048 
6049 
6050 // Get metadata about file f.
6051 int
6052 filestat(struct file *f, struct stat *st)
6053 {
6054   if(f->type == FD_INODE){
6055     ilock(f->ip);
6056     stati(f->ip, st);
6057     iunlock(f->ip);
6058     return 0;
6059   }
6060   return -1;
6061 }
6062 
6063 // Read from file f.
6064 int
6065 fileread(struct file *f, char *addr, int n)
6066 {
6067   int r;
6068 
6069   if(f->readable == 0)
6070     return -1;
6071   if(f->type == FD_PIPE)
6072     return piperead(f->pipe, addr, n);
6073   if(f->type == FD_INODE){
6074     ilock(f->ip);
6075     if((r = readi(f->ip, addr, f->off, n)) > 0)
6076       f->off += r;
6077     iunlock(f->ip);
6078     return r;
6079   }
6080   panic("fileread");
6081 }
6082 
6083 
6084 
6085 
6086 
6087 
6088 
6089 
6090 
6091 
6092 
6093 
6094 
6095 
6096 
6097 
6098 
6099 
6100 // Write to file f.
6101 int
6102 filewrite(struct file *f, char *addr, int n)
6103 {
6104   int r;
6105 
6106   if(f->writable == 0)
6107     return -1;
6108   if(f->type == FD_PIPE)
6109     return pipewrite(f->pipe, addr, n);
6110   if(f->type == FD_INODE){
6111     // write a few blocks at a time to avoid exceeding
6112     // the maximum log transaction size, including
6113     // i-node, indirect block, allocation blocks,
6114     // and 2 blocks of slop for non-aligned writes.
6115     // this really belongs lower down, since writei()
6116     // might be writing a device like the console.
6117     int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
6118     int i = 0;
6119     while(i < n){
6120       int n1 = n - i;
6121       if(n1 > max)
6122         n1 = max;
6123 
6124       begin_op();
6125       ilock(f->ip);
6126       if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
6127         f->off += r;
6128       iunlock(f->ip);
6129       end_op();
6130 
6131       if(r < 0)
6132         break;
6133       if(r != n1)
6134         panic("short filewrite");
6135       i += r;
6136     }
6137     return i == n ? n : -1;
6138   }
6139   panic("filewrite");
6140 }
6141 
6142 
6143 
6144 
6145 
6146 
6147 
6148 
6149 
