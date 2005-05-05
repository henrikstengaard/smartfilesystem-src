#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>

char template[]="DEVICE/A";

struct {char *device;} arglist={NULL};

UBYTE date[100];
UBYTE time[100];

void datetostr(struct DateStamp *ds);

ULONG main() {
  struct RDArgs *readarg;
  struct DeviceList *devinfo;

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    if((readarg=ReadArgs(template,(LONG *)&arglist,0))!=0) {
      devinfo=BADDR(DOSBase->dl_Root->rn_Info);
      devinfo=(struct DeviceList *)BADDR( ((struct DosInfo *)(devinfo))->di_DevInfo );

//      while(devinfo->dl_Next!=0) {
      while(devinfo!=0) {
        if((devinfo->dl_Type)==DLT_VOLUME) {
          datetostr(&devinfo->dl_VolumeDate);

          printf("VOLUMENODE %s\n",(ULONG)BADDR(devinfo->dl_Name)+1);
          printf("  Disktype      : 0x%08lx         Task          : 0x%08lx\n",devinfo->dl_DiskType,devinfo->dl_Task);
          printf("  Locklist      : 0x%08lx         unused        : 0x%08lx\n",devinfo->dl_LockList,devinfo->dl_unused);
          printf("  Creation date : %s %s\n\n",date,time);
        }
        else if((devinfo->dl_Type)==DLT_DEVICE) {
          struct DevInfo *dvi=(struct DevInfo *)devinfo;
          struct FileSysStartupMsg *fssm;

          fssm=(struct FileSysStartupMsg *)BADDR(dvi->dvi_Startup);

          printf("DEVICENODE %s\n",(ULONG)BADDR(devinfo->dl_Name)+1);
          printf("  Lock          : 0x%08lx         Task          : 0x%08lx\n",dvi->dvi_Lock,dvi->dvi_Task);
          printf("  Stacksize     : %10ld bytes   Handler       : %s\n",dvi->dvi_StackSize,(ULONG)BADDR(dvi->dvi_Handler)+1);
          printf("  Priority      : %10ld         Startup       : 0x%08lx\n",dvi->dvi_Priority,dvi->dvi_Startup);
          printf("  SegList       : 0x%08lx         GlobVec       : 0x%08lx\n\n",BADDR(dvi->dvi_SegList),dvi->dvi_GlobVec);

          if((ULONG)fssm>200 && fssm->fssm_Unit<=10) {
            struct DosEnvec *de=(struct DosEnvec *)BADDR(fssm->fssm_Environ);

            printf("  FSSM          : %s, unit %ld, flags 0x%08lx\n\n",(ULONG)BADDR(fssm->fssm_Device)+1,fssm->fssm_Unit,fssm->fssm_Flags);
            printf("  Sectorsize    : %10ld bytes   Sectors/block : %10ld\n",de->de_SizeBlock<<2,de->de_SectorPerBlock);
            printf("  LowCyl        : %10ld         HighCyl       : %10ld\n",de->de_LowCyl,de->de_HighCyl);
            printf("  Blocks/track  : %10ld         Surfaces      : %10ld tracks/cylinder\n",de->de_BlocksPerTrack,de->de_Surfaces);
            printf("  Reserved      : %10ld blocks  PreAlloc      : %10ld blocks\n",de->de_Reserved,de->de_PreAlloc);
            printf("  SecOrg        : %10ld         Interleave    : %10ld\n",de->de_SecOrg,de->de_Interleave);
            printf("  Buffers       : %10ld       ",de->de_NumBuffers);

            if(de->de_TableSize>=12) {
              printf("  BufMemType    : 0x%08lx\n",de->de_BufMemType);
              if(de->de_TableSize>=13) {
                printf("  MaxTransfer   : 0x%08lx       ",de->de_MaxTransfer);
                if(de->de_TableSize>=14) {
                  printf("  Mask          : 0x%08lx\n",de->de_Mask);
                  if(de->de_TableSize>=15) {
                    printf("  BootPri       : %10ld       ",de->de_BootPri);
                    if(de->de_TableSize>=16) {
                      printf("  DosType       : 0x%08lx\n",de->de_DosType);
                      if(de->de_TableSize>=17) {
                        printf("  Baud          : %10ld       ",de->de_Baud);
                        if(de->de_TableSize>=18) {
                          printf("  Control       : 0x%08lx\n",de->de_Control);
                          if(de->de_TableSize>=19) {
                            printf("  BootBlocks    : 0x%08lx\n",de->de_BootBlocks);
                          }
                        }
                        else {
                          printf("\n");
                        }
                      }
                    }
                    else {
                      printf("\n");
                    }
                  }
                }
                else {
                  printf("\n");
                }
              }
            }
            else {
              printf("\n");
            }

            printf("\n");
          }
        }

        devinfo=BADDR(devinfo->dl_Next);
      }
      FreeArgs(readarg);
    }
    CloseLibrary((struct Library *)DOSBase);
  }
  return(0);
}


void datetostr(struct DateStamp *ds) {
  struct DateTime dt;

  dt.dat_Stamp=*ds;
  dt.dat_Format=0;
  dt.dat_Flags=0;
  dt.dat_StrDate=date;
  dt.dat_StrDay=0;
  dt.dat_StrTime=time;

  DateToStr(&dt);
}
