/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Test pthread interaction with native Win32 threads
 *
 */


#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include <pthread.h>
#include <assert.h>
#include <rx/rx_queue.h>
}


#define nTESTS         3
#define nPASSES_MAX  256


/*
 * Test1 - Tests the main thread calling pthread_self()
 *
 */

BOOL Test1 (void)
{
   BOOL rc = TRUE;
   printf ("Begin Test1:\n");

   PVOID hSelf;
   if ((hSelf = pthread_self()) == NULL)
      rc = FALSE;

   printf ("   1: pthread_self() on main thread returned 0x%08lX\n", hSelf);

   printf ("--> Test1 %s\n", (rc) ? TEXT("succeeded") : TEXT("FAILED"));
   return rc;
}


/*
 * Test2 - Tests a Win32 thread's ability to join onto a pthread
 *
 */
#define TEST2_RETURNVALUE 'Test'

DWORD WINAPI Test2_Thread1 (LPVOID lp)
{
   printf ("   1-1: waiting for thread 2 to begin\n");

   while (!*(volatile DWORD*)lp)
      ;

   printf ("   1-1: calling pthread_join (0x%08lX)\n", *(DWORD*)lp);

   DWORD status;
   int rc;
   if ((rc = pthread_join ((pthread_t)(*(DWORD*)lp), (void**)&status)) != 0)
      {
      printf ("   1-1: pthread_join failed; rc=%lu\n", rc);
      return 0;
      }

   if (status != TEST2_RETURNVALUE)
      {
      printf ("   1-1: pthread_join succeeded with wrong status (rc=%lu)\n", status);
      return 0;
      }

   printf ("   1-1: pthread_join succeeded; terminating\n");
   return 1;
}

extern "C" void *Test2_Thread2 (void *pdwCommonParam)
{
   printf ("   1-2: signalling that thread 2 began\n");
   *(volatile DWORD*)pdwCommonParam = (DWORD)pthread_self();
   Sleep (1500);

   printf ("   1-2: terminating\n");
   return (void*)TEST2_RETURNVALUE;
}

BOOL Test2 (void)
{
   BOOL rc = TRUE;
   printf ("Begin Test2:\n");

   DWORD dwCommonParam = 0;
   DWORD idThread;

   printf ("   1: creating win32 thread\n");
   HANDLE hThread1 = CreateThread (0, 0, Test2_Thread1, (LPVOID)&dwCommonParam, 0, &idThread);
   Sleep(500);

   printf ("   1: creating pthread thread\n");
   pthread_t hThread2;
   pthread_create (&hThread2, NULL, Test2_Thread2, (void *)&dwCommonParam);
   Sleep(500);

   printf ("   1: blocking until win32 thread (thread1) terminates...\n");

   WaitForSingleObject (hThread1, INFINITE);
   GetExitCodeThread (hThread1, (ULONG*)&rc);

   printf ("   1: detected thread1 termination\n");

   printf ("--> Test2 %s\n", (rc) ? TEXT("succeeded") : TEXT("FAILED"));
   return rc;
}


/*
 * Test3 - Makes sure the pthread library recognizes when native threads die
 *
 */

DWORD WINAPI Test3_Thread1 (LPVOID lp)
{
   LONG *pdw = (LONG*)lp;

   // We're awake. Do something pthready.
   //
   pthread_t pMe;
   if ((pMe = pthread_self()) == NULL)
      printf ("   3-1: could not get a pthread_self!\n");
   else
      printf ("   3-1: ready\n");

   InterlockedIncrement (pdw);

   // Now wait until we see the signal to die.
   //
   while (*(volatile LONG*)pdw != 5)
      ;

   printf ("   3-1: terminating\n");
   InterlockedIncrement (pdw);
   return 1;
}


DWORD WINAPI Test3_Thread2 (LPVOID lp)
{
   LONG *pdw = (LONG*)lp;

   // We're awake. Do something pthready.
   //
   pthread_t pMe;
   if ((pMe = pthread_self()) == NULL)
      printf ("   3-2: could not get a pthread_self!\n");
   else
      printf ("   3-2: ready\n");

   InterlockedIncrement (pdw);

   // Now wait until we see the signal to die.
   //
   while (*(volatile LONG*)pdw != 7)
      ;

   printf ("   3-2: terminating\n");
   InterlockedIncrement (pdw);
   return 1;
}


extern "C" void *Test3_Thread3 (void *pdwParam)
{
   LONG *pdw = (LONG*)pdwParam;

   // We're awake.
   //
   printf ("   3-3: ready\n");
   InterlockedIncrement (pdw);

   // Now wait until we see the signal to die.
   //
   while (*(volatile LONG*)pdw != 9)
      ;

   printf ("   3-3: terminating\n");
   InterlockedIncrement (pdw);
   return (void*)1;
}


extern "C" void *Test3_Thread4 (void *pdwParam)
{
   LONG *pdw = (LONG*)pdwParam;

   // We're awake.
   //
   printf ("   3-4: ready\n");
   InterlockedIncrement (pdw);

   // Now wait until we see the signal to die.
   //
   while (*(volatile LONG*)pdw != 11)
      ;

   printf ("   3-4: terminating\n");
   InterlockedIncrement (pdw);
   return (void*)1;
}


extern "C" void *Test3_ThreadCount (void *pdwParam)
{
   volatile LONG *pdw = (LONG*)pdwParam;

   pthread_t pMe;
   if ((pMe = pthread_self()) == NULL)
      return (void*)0;

   try {
      struct rx_queue *pNow = (rx_queue*)pMe;

      for (struct rx_queue *pWalk = pNow->next; pWalk != pNow; pWalk = pWalk->next)
         ++(*pdw);

    } catch(...) {
       *pdw = 0;
    }

   return (void*)*pdw;
}

size_t Test3_CountActiveQueue (void)
{
   DWORD dwArg = 0;
   pthread_t pThread;
   pthread_create (&pThread, NULL, Test3_ThreadCount, (void *)&dwArg);

   DWORD status;
   int rc;
   if ((rc = pthread_join (pThread, (void**)&status)) != 0)
      {
      printf ("   3: counter: pthread_join failed; rc=%lu\n", rc);
      return 0;
      }

   return dwArg;
}

BOOL Test3 (void)
{
   BOOL rc = TRUE;
   printf ("Begin Test3:\n");

   // Find the size of the active queue
   //
   size_t cInitialQueueSize;
   if ((cInitialQueueSize = Test3_CountActiveQueue()) == 0)
      {
      printf ("   3: unable to determine active queue size\n");
      rc = FALSE;
      }
   else
      {
      // Start two Win32 threads and two pthreads. We'll use a single, common
      // parameter for each thread: dwSignal. Each thread will increment the
      // signal param when it's ready for us to go; once that's done, a
      // thread will kill itself and inc the signal *again* each die we
      // increment the signal:
      //
      //    starts at 0
      //    thread 1 wakes up and incs signal to 1
      //    thread 2 wakes up and incs signal to 2
      //    thread 3 wakes up and incs signal to 3
      //    thread 4 wakes up and incs signal to 4
      //    we notice signal is 4, and we increment signal to 5
      //    thread 1 notices signal is 5, incs to 6 and dies
      //    we notice signal is 6, and we increment signal to 7
      //    thread 1 notices signal is 7, incs to 8 and dies
      //    we notice signal is 8, and we increment signal to 9
      //    thread 1 notices signal is 9, incs to 10 and dies
      //    we notice signal is 10, and we increment signal to 11
      //    thread 1 notices signal is 11, incs to 12 and dies
      //    we notice signal is 12 and finish our tests
      //
      volatile LONG dwSignal = 0;
      printf ("   3: creating test threads\n");

      DWORD idThread;
      CreateThread (0, 0, Test3_Thread1, (LPVOID)&dwSignal, 0, &idThread);
      Sleep (500);

      CreateThread (0, 0, Test3_Thread2, (LPVOID)&dwSignal, 0, &idThread);
      Sleep (500);

      pthread_attr_t attr;
      attr.is_joinable = PTHREAD_CREATE_DETACHED;

      pthread_t hThread;
      pthread_create (&hThread, &attr, Test3_Thread3, (void *)&dwSignal);
      Sleep (500);

      pthread_create (&hThread, &attr, Test3_Thread4, (void *)&dwSignal);
      Sleep (500);

      // Count the active queue again as soon as we're sure all the threads
      // are ready for us to do so.
      //
      while (dwSignal != 4)
         ;

      size_t cQueueSize;
      if ((cQueueSize = Test3_CountActiveQueue()) == 0)
         {
         printf ("   3: unable to determine active queue size\n");
         rc = FALSE;
         }
      else if (cQueueSize != cInitialQueueSize+4)
         {
         printf ("   3: error: active queue wasn't increased by 4\n");
         rc = FALSE;
         }
      else while (rc && (dwSignal < 12))
         {
         LONG dwTarget = dwSignal +2;
         InterlockedIncrement ((LONG*)&dwSignal);

         // wait for a thread to notice the change and kill itself
         while (dwSignal != dwTarget)
            ;
         Sleep (500); // give a little time for cleanup after thread incremented

         if ((cQueueSize = Test3_CountActiveQueue()) == 0)
            {
            printf ("   3: unable to determine active queue size (dwSignal=%ld)\n", dwSignal);
            rc = FALSE;
            }
         else if (cQueueSize != (cInitialQueueSize + 4 - ((dwTarget -4) /2)))
            {
            printf ("   3: active queue size did not diminish (dwSignal=%ld)\n", dwSignal);
            rc = FALSE;
            }
         }
      }

   printf ("--> Test3 %s\n", (rc) ? TEXT("succeeded") : TEXT("FAILED"));
   return rc;
}


/*
 * main - Runs all tests (unless the command-line says otherwise)
 *
 */
int main(int argc, char **argv)
{
   DWORD iRunTest[nPASSES_MAX];
   for (size_t ii = 0; ii < nPASSES_MAX; ++ii)
      iRunTest[ii] = (ii < nTESTS) ? (ii+1) : 0;

   ii = 0;
   for (--argc,++argv; argc; --argc,++argv)
      {
      DWORD iTest;
      if ( ((iTest = (size_t)atol (*argv)) > 0) && (iTest <= nTESTS) )
         {
         if (ii == 0)
            memset (iRunTest, 0x00, sizeof(iRunTest));
         if (ii < nPASSES_MAX)
            iRunTest[ ii++ ] = iTest;
         }
      }

   BOOL rc = TRUE;

   for (ii = 0; ii < nPASSES_MAX; ++ii)
      {
      if (iRunTest[ ii ] == 0)
         break;
      else switch (iRunTest[ ii ])
         {
         case 1: if (!Test1()) rc = FALSE;  break;
         case 2: if (!Test2()) rc = FALSE;  break;
         case 3: if (!Test3()) rc = FALSE;  break;
         }
      }

   return rc;
}

