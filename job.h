/* Copyright (C) 1992 Free Software Foundation, Inc.
This file is part of GNU Make.

GNU Make is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Make is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Make; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Structure describing a running or dead child process.  */

struct child
  {
    struct child *next;		/* Link in the chain.  */

    struct file *file;		/* File being remade.  */

    char **environment;		/* Environment for commands.  */

    char **command_lines;	/* Array of variable-expanded cmd lines.  */
    unsigned int command_line;	/* Index into above.  */
    char *command_ptr;		/* Ptr into command_lines[command_line].  */

    int pid;			/* Child process's ID number.  */
    unsigned int remote:1;	/* Nonzero if executing remotely.  */

    unsigned int noerror:1;	/* Nonzero if commands contained a `-'.  */

    unsigned int good_stdin:1;	/* Nonzero if this child has a good stdin.  */
    unsigned int deleted:1;	/* Nonzero if targets have been deleted.  */
  };

extern struct child *children;

extern void new_job ();
extern void wait_for_children ();
extern void push_signals_blocked_p (), pop_signals_blocked_p ();

extern char **construct_command_argv ();
extern void child_execute_job ();
extern void exec_command ();

extern unsigned int job_slots_used;
