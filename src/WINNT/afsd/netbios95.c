/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Netbios function for DJGPP: calls interrupt 5Ch for Netbios function.
   NCB and buffer space must be in DOS memory (below 1MB). */

#include <stdio.h>
#include <stdlib.h>
#include <dpmi.h>
#include <go32.h>
#include "dosdefs95.h"
#include "netbios95.h"

extern int handler_seg, handler_off;

int Netbios(NCB *Ncb, dos_ptr dos_ncb)
{
  __dpmi_regs regs;
  int asynch = 1;
  dos_ptr oldncb_buffer;

#if 1
  if (Ncb->ncb_command == NCBRESET ||
      Ncb->ncb_command == NCBCANCEL ||
      Ncb->ncb_command == NCBUNLINK ||
      Ncb->ncb_command == NCBADDNAME ||
      Ncb->ncb_command == NCBENUM ||
      Ncb->ncb_command == NCBDELNAME) /* temp */
    asynch = 0;
#else
  if (1)
    asynch = 0;
#endif
  else
    /* set to asynchronous */
    Ncb->ncb_command |= ASYNCH;

  /* adjust ncb_buffer pointer to be a segment:zero-offset pointer
     for __dpmi_int */
  oldncb_buffer = Ncb->ncb_buffer;
  Ncb->ncb_buffer = Ncb->ncb_buffer << 12;

  /*if (asynch)
    Ncb->ncb_post = (handler_seg << 16) | handler_off;*/

  /* copy to DOS space */
  dosmemput(Ncb, sizeof(NCB), dos_ncb);

  /* set address of NCB in registers */
  memset(&regs, 0, sizeof(regs));
  regs.d.ebx = 0;
  regs.x.ds = regs.x.es = dos_ncb/16;

  __dpmi_int(0x5c,&regs);
  /*dosmemget(__tb, sizeof(NCB), Ncb);*/
  
  if (asynch)
    IOMGR_NCBSelect(Ncb, dos_ncb, NULL);

  /* undo the change to ncb_buffer */
  Ncb->ncb_buffer = oldncb_buffer;

  return regs.x.ax;
}

