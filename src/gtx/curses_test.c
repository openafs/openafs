/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

/*
 * Test of the curses package, to make sure I really understand how to use it.
 */

#if defined(AFS_HPUX110_ENV) && !defined(__HP_CURSES)
#define __HP_CURSES
#endif

#include <curses.h>

#include "AFS_component_version_number.c"

main(argc, argv)
    int argc;
    char **argv;

{ /*main*/

    WINDOW *mainscr;
    char str[128];
    int i;

    mainscr = initscr();
    scrollok(stdscr, TRUE);
    clear();
    addstr("This is my first curses string ever!\n");
    refresh();
    box(stdscr, '|', '-');
    standout();
    addstr("This is a standout string\n");
    refresh();
    standend();

#if 0
    box
    addstr("Enter a string and a number: ");
    refresh();
    scanw(stdscr, "%s %d", str, &i);
    wprintw(stdscr, "String was '%s', number was %d\n", str, i);
    refresh();
#endif 0

    endwin();

} /*main*/
