#include <dos/dos.h>
#include <exec/nodes.h>
#include <exec/ports.h>
#include <proto/dos.h>
#include <proto/exec.h>

void kprintf(const char *,...);

LONG main() {
  struct MsgPort *port;
  struct Message *msg;
  BPTR fh,oldfh;
  struct DosLibrary *DOSBase;

  if((DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",37))!=0) {
    Forbid();

    if((port=FindPort("SFS debug output port"))==0) {
      Permit();

      if((port=CreateMsgPort())!=0) {
        port->mp_Node.ln_Pri=50;
        port->mp_Node.ln_Name="SFS debug output port";
        AddPort(port);

//        if((fh=Open("KCON:480/18/544/650/FileSystem debug window/MAXBUF-2500/KEEPCLOSED/INACTIVE/GADS/CLOSE/AUTO",MODE_NEWFILE))!=0) {

//          oldfh=SelectOutput(fh);

          kprintf("Debugger active!\n");

          while((SetSignal(0,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)==0) {
            WaitPort(port);
            while((msg=GetMsg(port))!=0) {
              kprintf(((UBYTE *)msg)+sizeof(struct Message));
//              Flush(fh);
              FreeVec(msg);
            }
          }

          /* CTRL C was pressed */

          kprintf("CTRL C pressed, exiting!\n");

//          SelectOutput(oldfh);
//          Close(fh);
//        }

        RemPort(port);
        DeleteMsgPort(port);
      }
    }
    else {
      Permit();
      kprintf("Filesystem debugger already active!\n");
    }
    CloseLibrary((struct Library *)DOSBase);
  }
  else {
    return(20);
  }
  return(0);
}
