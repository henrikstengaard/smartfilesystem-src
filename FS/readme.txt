There are a few things you should know about SFS before trying it.

- 64-bit support is present, but untested so be careful.

- No Mask support, so if you're device driver sucks then you're out
  of luck for now.

- May not work with some older programs (Workbench 1.3 most notably).
  UMS probably doesn't work either, but may work later on.

- It currently requires atleast an 020 and Kick 3.1.

- ACTION_SET_FILE_SIZE doesn't work correctly yet.  This packet isn't
  used very often, but it may give you some trouble.

- The flush timeout is currently 5 seconds.  During the flush the
  filesystem isn't accesible.

- At the moment, all changes to admin blocks are stored in a buffer
  (which grows larger as you do more modifications).  After 5 seconds
  of inactivity this buffer is flushed.  Depending on how many
  changes you made this can take quite some time.  In the future the
  flushing will be done more often.

- No notification

- No links

- Partitions larger than 2 GB haven't been tested, but should work.

- SFS is relatively safe to try on a HD which has other partitions.
  There is a very low-level check which makes sure that SFS never
  reads or writes outside its partition area (but you still must
  make sure that the mountlist is correct of course).

- Diskchanges and (physical) writeprotection may confuse the
  filesystem at the moment.


Below is an example mountlist:


TTEMP:
Device	= cybscsi.device
Filesystem	= L:FileSystemMain
Unit	= 1
Flags	= 0
Surfaces	= 1
BlockSize	= 512
BlocksPerTrack	= 793
Reserved	= 2
Interleave	= 0
Lowcyl	= 5275
Highcyl	= 6200
Buffers	= 200
Globvec	= -1
BufMemType	= 0
Mask	= 0x7FFFFFFE
Maxtransfer	= 0x100000
Mount	= 1
Dostype	= 0x53465300
StackSize	= 16384
BootPri	= -128
Priority	= 5
#
