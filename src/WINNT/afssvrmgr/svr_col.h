#ifndef SVR_COL_H
#define SVR_COL_H


/*
 * SERVER-VIEW COLUMNS ________________________________________________________
 *
 */

typedef enum
   {
   svrcolNAME,
   svrcolADDRESS,
   svrcolSTATUS,
   } SERVERCOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
SERVERCOLUMNS[] =
   {
      { IDS_SVRCOL_NAME,       150 }, // svrcolNAME
      { IDS_SVRCOL_ADDRESS,    100 }, // svrcolADDRESS
      { IDS_SVRCOL_STATUS,     400 }, // svrcolSTATUS
   };

#define nSERVERCOLUMNS  (sizeof(SERVERCOLUMNS)/sizeof(SERVERCOLUMNS[0]))


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Server_SetDefaultView_Horz (LPVIEWINFO lpviHorz);
void Server_SetDefaultView_Vert (LPVIEWINFO lpviVert);

size_t Server_GetAlertCount (LPSERVER lpServer);
LPTSTR Server_GetColumnText (LPIDENT lpi, SERVERCOLUMN col);


#endif

