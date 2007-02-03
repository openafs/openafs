/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AL_PROGRESS_H
#define AL_PROGRESS_H

#ifndef EXPORTED
#define EXPORTED
#endif

/*
 * PROGRESS DIALOGS ___________________________________________________________
 *
 * The PROGRESSDISPLAY object is probably one of the most esoteric 
 * utilities in this library. It provides a convenient way to package
 * up a background thread, and associate it with a dialog telling the user
 * what's going on. It sounds a little hokey, and honestly isn't that
 * terribly useful, but you may find you need it at some point.
 *
 * Example:
 *
 *    // Copy the files onto the user's machine.
 *    //
 *    extern LPTSTR g_apszFilename[];
 *
 *    LPPROGRESSDISPLAY pProg;
 *    pProg = new PROGRESSDISPLAY (hParent, IDD_COPYING_PROGRESS);
 *    pProg->SetProgressRange (0, nFILES(g_apszFilename));
 *    pProg->Show (fnCopyFiles, (LPARAM)g_apszFilename);
 *
 *    // By default, Show() is modal--it pumps messages until the
 *    // background thread completes.  If you have specified a
 *    // finish message ("pProg->SetFinishMessage (WM_USER+15)"),
 *    // then Show() is modeless, returning immediately. The WM_USER+15
 *    // message will be posted to the progress dialog when the background
 *    // thread completes (you can specify a dlgproc for your progress
 *    // dialog on the "new PROGRESSDISPLAY" line).
 *
 *    DWORD CALLBACK fnCopyFiles (LPPROGRESSDISPLAY pProg, LPARAM lp)
 *    {
 *       LPTSTR *apszFilename = (LPTSTR*)lp;
 *
 *       for (ii = 0; ii < nFILES(apszFilename); ++ii) {
 *          pProg->SetOperation ("Copying "+apszFilename[ii]);
 *          CopyFile (apszFilename[ii])
 *          pProg->SetProgress (ii);
 *       }
 *
 *       pProg->Close();
 *       return 0;
 *    }
 *
 */

#include <WINNT/TaLocale.h>
#include <WINNT/subclass.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef THIS_HINST
#define THIS_HINST   (HINSTANCE)GetModuleHandle(NULL)
#endif

#define IDC_OPERATION      900
#define IDC_PROGRESS       901
#define IDC_PROGRESSTEXT   902

typedef class EXPORTED PROGRESSDISPLAY PROGRESSDISPLAY, *LPPROGRESSDISPLAY;


/*
 * PROGRESSDISPLAY CLASS ______________________________________________________
 *
 */

class PROGRESSDISPLAY
   {
   public:

      PROGRESSDISPLAY (HWND hWnd);
      PROGRESSDISPLAY (HWND hParent, int iddTemplate, DLGPROC dlgproc = 0);

      static LPPROGRESSDISPLAY GetProgressDisplay (HWND hWnd);

      void GetProgressRange (int *piStart, int *piFinish);
      void SetProgressRange (int iStart, int iFinish);

      int GetProgress (void);
      void SetProgress (int iProgress);

      void GetOperation (LPTSTR pszOperation);
      void SetOperation (LPCTSTR pszOperation);

      HWND GetWindow (void);

      void SetFinishMessage (int msgFinish);
      void Show (DWORD (CALLBACK *pfn)(LPPROGRESSDISPLAY ppd, LPARAM lp), LPARAM lp);
      void Close (void);
      DWORD GetStatus (void);

   private:

      ~PROGRESSDISPLAY (void);
      void Init (HWND hWnd);
      void Finish (DWORD dwStatus = 0);
      void PROGRESSDISPLAY::OnUpdate (void);

      static HRESULT CALLBACK ProgressDisplay_StubProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
      static HRESULT CALLBACK ProgressDisplay_HookProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
      static DWORD WINAPI PROGRESSDISPLAY::ThreadProc (PVOID lp);

      BOOL m_fFinished;
      LONG m_dwStatus;

      LONG m_cRef;
      CRITICAL_SECTION m_cs;
      HWND m_hWnd;
      BOOL m_fCreatedWindow;

      int m_msgFinish;
      DWORD (CALLBACK *m_pfnUser)(LPPROGRESSDISPLAY ppd, LPARAM lp);
      LPARAM m_lpUser;
      int m_iProgressStart;
      int m_iProgressFinish;
      int m_iProgress;
      TCHAR m_szOperation[ cchRESOURCE ];
      TCHAR m_szProgressText[ cchRESOURCE ];
   };


#endif

