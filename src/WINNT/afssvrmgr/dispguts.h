#ifndef DISPGUTS_H
#define DISPGUTS_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

/*
 *** Display_*_Internal()
 *
 * These routines should not be called directly; their interface is through
 * UpdateDisplay() [see display.h]
 *
 */

void Display_Cell_Internal (LPDISPLAYREQUEST pdr);
void Display_Servers_Internal (LPDISPLAYREQUEST pdr);
void Display_Services_Internal (LPDISPLAYREQUEST pdr);
void Display_Aggregates_Internal (LPDISPLAYREQUEST pdr);
void Display_Filesets_Internal (LPDISPLAYREQUEST pdr);
void Display_Replicas_Internal (LPDISPLAYREQUEST pdr);
void Display_ServerWindow_Internal (LPDISPLAYREQUEST pdr);


#endif

