#ifndef AFSCLASS_IDENTLIST_H
#define AFSCLASS_IDENTLIST_H

#include <WINNT/afsclass.h>


/*
 * IDENTIFIER-LIST CLASS ______________________________________________________
 *
 */

class IDENTLIST
   {
   public:
      IDENTLIST (void);
      ~IDENTLIST (void);

      void Add (LPIDENT lpi);
      void Remove (LPIDENT lpi);
      void RemoveAll (void);
      void CopyFrom (LPIDENTLIST pil);

      size_t GetCount (void);
      BOOL fIsInList (LPIDENT lpi);

      LPIDENT FindFirst (HENUM *phEnum);
      LPIDENT FindNext (HENUM *phEnum);
      void FindClose (HENUM *phEnum);

   private:
      LPHASHLIST m_lIdents;
   };


#endif  // AFSCLASS_IDENTLIST_H

