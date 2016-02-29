dnl Test getcontext() and makecontext() to ensure that we are able to
dnl copy the current user context, modify it with our own private stack
dnl and return to the original user context.
dnl
AC_DEFUN([OPENAFS_WORKING_UCONTEXT],[
  AC_MSG_CHECKING([if user context manipulation is complete])
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UCONTEXT_H
#include <ucontext.h>
#endif

#define STACK_SIZE 16384

static ucontext_t main_context, thread_context;
static char *alt_stack;

static void
thread(void)
{
	unsigned long stack_ptr;
	unsigned long offset;

	offset = (unsigned long) &stack_ptr - (unsigned long) alt_stack;
	if (offset > STACK_SIZE)
		exit(EXIT_FAILURE);
	swapcontext(&thread_context, &main_context);
	/* should never get here */
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	if (getcontext(&thread_context) == -1)
		exit(EXIT_FAILURE);
	alt_stack = malloc(STACK_SIZE);
	if (!alt_stack)
		exit(EXIT_FAILURE);
	thread_context.uc_stack.ss_sp = alt_stack;
	thread_context.uc_stack.ss_size = STACK_SIZE;
	makecontext(&thread_context, thread, 0);

	if (swapcontext(&main_context, &thread_context) == -1)
		exit(EXIT_FAILURE);

	free(alt_stack);
	exit(EXIT_SUCCESS);
}]])],[AC_MSG_RESULT(yes)
   AC_DEFINE(HAVE_WORKING_SWAPCONTEXT,1,user context manipulation is complete)],[AC_MSG_RESULT(no)],[])])
