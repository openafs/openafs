#include <windows.h>

typedef class HOURGLASS
{
   protected:
      HCURSOR m_OldCursor;

   public:
      HOURGLASS (LPCSTR idCursor = IDC_WAIT)
      {
         m_OldCursor = GetCursor();
         SetCursor (LoadCursor (NULL, idCursor));
      }

      virtual ~HOURGLASS (void)
      {
         SetCursor (m_OldCursor);
      }

} HOURGLASS, *PHOURGLASS;

