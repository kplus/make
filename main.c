/* Copyright (C) 1988, 1989, 1990, 1991 Free Software Foundation, Inc.
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

#include "make.h"
#include "commands.h"
#include "dep.h"
#include "file.h"
#include "variable.h"
#include "job.h"
#include <time.h>
#include "getopt.h"


extern char *version_string;

extern struct dep *read_all_makefiles ();

extern void print_variable_data_base ();
extern void print_dir_data_base ();
extern void print_rule_data_base ();
extern void print_file_data_base ();
extern void print_vpath_data_base ();

#ifndef	HAVE_UNISTD_H
extern int chdir ();
#endif
#ifndef	STDC_HEADERS
extern void exit ();
extern double atof ();
#endif
extern char *mktemp ();

static void log_working_directory ();
static void print_data_base (), print_version ();
static void decode_switches (), decode_env_switches ();
static void define_makeflags ();


#if 0 /* dummy tag */
flags () {}
#endif
/* Flags:
 *	-b ignored for compatibility with System V Make
 *	-C change directory
 *	-d debug
 *	-e env_overrides
 *	-f makefile
 *	-i ignore_errors
 *	-j job_slots
 *	-k keep_going
 *	-l max_load_average
 *	-m ignored for compatibility with something or other
 *	-n just_print
 *	-o consider file old
 *	-p print_data_base
 *	-q question
 *	-r no_builtin_rules
 *	-s silent
 *	-S turn off -k
 *	-t touch
 *	-v print version information
 *	-w log working directory
 *	-W consider file new (with -n, `what' if effect)
 */

/* The structure that describes an accepted command switch.  */

struct command_switch
  {
    char c;			/* The switch character.  */
    enum			/* Type of the value.  */
      {
	flag,			/* Turn int flag on.  */
	flag_off,		/* Turn int flag off.  */
	string,			/* One string per switch.  */
	positive_int,		/* A positive integer.  */
	floating,		/* A floating-point number (double).  */
	ignore			/* Ignored.  */
      } type;

    char *value_ptr;	/* Pointer to the value-holding variable.  */

    unsigned int env:1;		/* Can come from MAKEFLAGS.  */
    unsigned int toenv:1;	/* Should be put in MAKEFLAGS.  */
    unsigned int no_makefile:1;	/* Don't propagate when remaking makefiles.  */

    char *noarg_value;	/* Pointer to value used if no argument is given.  */
    char *default_value;/* Pointer to default value.  */

    char *long_name;		/* Long option name.  */
    char *argdesc;		/* Descriptive word for argument.  */
    char *description;		/* Description for usage message.  */
  };


/* The structure used to hold the list of strings given
   in command switches of a type that takes string arguments.  */

struct stringlist
  {
    char **list;	/* Nil-terminated list of strings.  */
    unsigned int idx;	/* Index into above.  */
    unsigned int max;	/* Number of pointers allocated.  */
  };


/* The recognized command switches.  */

/* Nonzero means do not print commands to be executed (-s).  */

int silent_flag;

/* Nonzero means just touch the files
   that would appear to need remaking (-t)  */

int touch_flag;

/* Nonzero means just print what commands would need to be executed,
   don't actually execute them (-n).  */

int just_print_flag;

/* Print debugging trace info (-d).  */

int debug_flag = 0;

/* Environment variables override makefile definitions.  */

int env_overrides = 0;

/* Nonzero means ignore status codes returned by commands
   executed to remake files.  Just treat them all as successful (-i).  */

int ignore_errors_flag = 0;

/* Nonzero means don't remake anything, just print the data base
   that results from reading the makefile (-p).  */

int print_data_base_flag = 0;

/* Nonzero means don't remake anything; just return a nonzero status
   if the specified targets are not up to date (-q).  */

int question_flag = 0;

/* Nonzero means do not use any of the builtin rules (-r).  */

int no_builtin_rules_flag = 0;

/* Nonzero means keep going even if remaking some file fails (-k).  */

int keep_going_flag;
int default_keep_going_flag = 0;

/* Nonzero means print working directory
   before starting and after finishing make. */

int print_directory_flag = 0;

/* Nonzero means print version information.  */

int print_version_flag = 0;

/* List of makefiles given with -f switches.  */

static struct stringlist *makefiles = 0;


/* Number of job slots (commands that can be run at once).  */

unsigned int job_slots = 1;
unsigned int default_job_slots = 1;

/* Value of job_slots that means no limit.  */

static unsigned int inf_jobs = 0;

/* Maximum load average at which multiple jobs will be run.
   Negative values mean unlimited, while zero means limit to
   zero load (which could be useful to start infinite jobs remotely
   but one at a time locally).  */

double max_load_average = -1.0;
double default_load_average = -1.0;

/* List of directories given with -c switches.  */

static struct stringlist *directories = 0;

/* List of include directories given with -I switches.  */

static struct stringlist *include_directories = 0;

/* List of files given with -o switches.  */

static struct stringlist *old_files = 0;

/* List of files given with -W switches.  */

static struct stringlist *new_files = 0;

/* The table of command switches.  */

static struct command_switch switches[] =
  {
    { 'b', ignore, 0, 0, 0, 0, 0, 0,
	0, 0,
	"Ignored for compatibility" },
    { 'C', string, (char *) &directories, 0, 0, 0, 0, 0,
	"directory", "DIRECTORY",
	"Change to DIRECTORY before doing anything" },
    { 'd', flag, (char *) &debug_flag, 1, 1, 0, 0, 0,
	"debug", 0,
	"Print lots of debugging information" },
    { 'e', flag, (char *) &env_overrides, 1, 1, 0, 0, 0,
	"environment-overrides", 0,
	"Environment variables override makefiles" },
    { 'f', string, (char *) &makefiles, 0, 0, 0, 0, 0,
	"file", "FILE",
	"Read FILE as a makefile" },
    { 'i', flag, (char *) &ignore_errors_flag, 1, 1, 0, 0, 0,
	"ignore-errors", 0,
	"Ignore errors from commands" },
    { 'I', string, (char *) &include_directories, 0, 0, 0, 0, 0,
	"include-dir", "DIRECTORY",
	"Search DIRECTORY for included makefiles" },
    { 'j', positive_int, (char *) &job_slots, 1, 1, 0,
	(char *) &inf_jobs, (char *) &default_job_slots,
	"jobs", "N",
	"Allow N jobs at once; infinite jobs with no arg" },
    { 'k', flag, (char *) &keep_going_flag, 1, 1, 0,
	0, (char *) &default_keep_going_flag,
	"keep-going", 0,
	"Keep going when some targets can't be made" },
    { 'l', floating, (char *) &max_load_average, 1, 1, 0,
	(char *) &default_load_average, (char *) &default_load_average,
	"load-average", "N",
	"Don't start multiple jobs unless load is below N" },
    { 'm', ignore, 0, 0, 0, 0, 0, 0,
	0, 0,
	"-b" },
    { 'n', flag, (char *) &just_print_flag, 1, 1, 1, 0, 0,
	"just-print", 0,
	"Don't actually run any commands; just print them" },
    { 'o', string, (char *) &old_files, 0, 0, 0, 0, 0,
	"old-file", "FILE",
	"Consider FILE to be very old and don't remake it" },
    { 'p', flag, (char *) &print_data_base_flag, 1, 1, 0, 0, 0,
	"print-data-base", 0,
	"Print make's internal database" },
    { 'q', flag, (char *) &question_flag, 1, 1, 1, 0, 0,
	"question", 0,
	"Run no commands; exit status says if up to date" },
    { 'r', flag, (char *) &no_builtin_rules_flag, 1, 1, 0, 0, 0,
	"no-builtin-rules", 0,
	"Disable the built-in implicit rules" },
    { 's', flag, (char *) &silent_flag, 1, 1, 0, 0, 0,
	"silent", 0,
	"Don't echo commands" },
    { 'S', flag_off, (char *) &keep_going_flag, 1, 1, 0,
	0, (char *) &default_keep_going_flag,
	"no-keep-going", 0,
	"Turns off -k" },
    { 't', flag, (char *) &touch_flag, 1, 1, 1, 0, 0,
	"touch", 0,
	"Touch targets instead of remaking them" },
    { 'v', flag, (char *) &print_version_flag, 0, 0, 0, 0, 0,
	"version", 0,
	"Print the version number of make" },
    { 'w', flag, (char *) &print_directory_flag, 1, 1, 0, 0, 0,
	"print-directory", 0,
	"Print the current directory" },
    { 'W', string, (char *) &new_files, 0, 0, 0, 0, 0,
	"what-if", "FILE",
	"Consider FILE to be infinitely new" },
    { '\0', }
  };

/* Secondary long names for options.  */

static struct option long_option_aliases[] =
  {
    { "quiet",		no_argument,		0, 's' },
    { "new",		required_argument,	0, 'W' },
    { "assume-new",	required_argument,	0, 'W' },
    { "assume-old",	required_argument,	0, 'o' },
    { "max-load",	optional_argument,	0, 'l' },
    { "dry-run",	no_argument,		0, 'n' },
    { "recon",		no_argument,		0, 'n' },
    { "makefile",	required_argument,	0, 'f' },
  };

/* List of non-switch arguments.  */

struct stringlist *other_args = 0;

/* The name we were invoked with.  */

char *program;

/* Value of the MAKELEVEL variable at startup (or 0).  */

unsigned int makelevel;

/* First file defined in the makefile whose name does not
   start with `.'.  This is the default to remake if the
   command line does not specify.  */

struct file *default_goal_file;

/* Pointer to structure for the file .DEFAULT
   whose commands are used for any file that has none of its own.
   This is zero if the makefiles do not define .DEFAULT.  */

struct file *default_file;

/* Mask of signals that are being caught with fatal_error_signal.  */

#ifdef	POSIX
sigset_t fatal_signal_set;
#else
#ifdef	HAVE_SIGSETMASK
int fatal_signal_mask;
#endif
#endif

int
main (argc, argv, envp)
     int argc;
     char **argv;
     char **envp;
{
#ifndef	HAVE_SYS_SIGLIST
  extern void init_siglist ();
#endif
  extern RETSIGTYPE fatal_error_signal (), child_handler ();
  register struct file *f;
  register unsigned int i;
  register char *cmd_defs;
  register unsigned int cmd_defs_len, cmd_defs_idx;
  char **p;
  time_t now;
  struct dep *goals = 0;
  register struct dep *lastgoal;
  struct dep *read_makefiles;
  PATH_VAR (current_directory);

  default_goal_file = 0;
  reading_filename = 0;
  reading_lineno_ptr = 0;
  
#ifndef	HAVE_SYS_SIGLIST
  init_siglist ();
#endif

#ifdef	POSIX
  sigemptyset (&fatal_signal_set);
#define	ADD_SIG(sig)	sigaddset (&fatal_signal_set, sig)
#else
#ifdef	HAVE_SIGSETMASK
  fatal_signal_mask = 0;
#define	ADD_SIG(sig)	fatal_signal_mask |= sigmask (sig)
#else
#define	ADD_SIG(sig)
#endif
#endif

#define	FATAL_SIG(sig)							      \
  if (signal ((sig), fatal_error_signal) == SIG_IGN)			      \
    (void) signal ((sig), SIG_IGN);					      \
  else									      \
    ADD_SIG (sig);

  FATAL_SIG (SIGHUP);
  FATAL_SIG (SIGQUIT);
  FATAL_SIG (SIGINT);
  FATAL_SIG (SIGTERM);

#ifdef	SIGDANGER
  FATAL_SIG (SIGDANGER);
#endif
#ifdef SIGXCPU
  FATAL_SIG (SIGXCPU);
#endif
#ifdef SIGXFSZ
  FATAL_SIG (SIGXFSZ);
#endif

#undef	FATAL_SIG

  /* Make sure stdout is line-buffered.  */

#ifdef	HAVE_SETLINEBUF
  setlinebuf (stdout);
#else
#ifndef	SETVBUF_REVERSED
  setvbuf (stdout, (char *) 0, _IOLBF, BUFSIZ);
#else	/* setvbuf not reversed.  */
  /* Some buggy systems lose if we pass 0 instead of allocating ourselves.  */
  setvbuf (stdout, _IOLBF, xmalloc (BUFSIZ), BUFSIZ);
#endif	/* setvbuf reversed.  */
#endif	/* setlinebuf missing.  */

  /* Set up to access user data (files).  */
  user_access ();

  /* Figure out where this program lives.  */

  if (argv[0] == 0)
    argv[0] = "";
  if (argv[0][0] == '\0')
    program = "make";
  else 
    {
      program = rindex (argv[0], '/');
      if (program == 0)
	program = argv[0];
      else
	++program;
    }

  /* Figure out where we are.  */

  if (getcwd (current_directory, GET_PATH_MAX) == 0)
    {
#ifdef	HAVE_GETCWD
      perror_with_name ("getcwd: ", "");
#else
      error ("getwd: %s", current_directory);
#endif
      current_directory[0] = '\0';
    }

  /* Read in variables from the environment.  It is important that this be
     done before `MAKE' and `MAKEOVERRIDES' are figured out so their
     definitions will not be ones from the environment.  */

  for (i = 0; envp[i] != 0; ++i)
    {
      register char *ep = envp[i];
      while (*ep++ != '=')
	;
      (void) define_variable (envp[i], ep - envp[i] - 1, ep, o_env, 1);
    }

  /* Decode the switches.  */

  decode_switches (argc, argv);

  /* Print version information.  */

  if (print_version_flag || print_data_base_flag || debug_flag)
    print_version ();

  /* Search for command line arguments that define variables,
     and do the definitions.  Also save up the text of these
     arguments in CMD_DEFS so we can put them into the values
     of $(MAKEOVERRIDES) and $(MAKE).  */

  cmd_defs_len = 200;
  cmd_defs = (char *) xmalloc (cmd_defs_len);
  cmd_defs_idx = 0;

  for (i = 1; other_args->list[i] != 0; ++i)
    {
      if (other_args->list[i][0] == '\0')
	/* Ignore empty arguments, so they can't kill enter_file.  */
	continue;

      /* Try a variable definition.  */
      if (try_variable_definition (other_args->list[i], o_command))
	{
	  /* It succeeded.  The variable is already defined.
	     Backslash-quotify it and append it to CMD_DEFS, then clobber it
	     to 0 in the list so that it won't be taken for a goal target.  */
	  register char *p = other_args->list[i];
	  unsigned int l = strlen (p);
	  if (cmd_defs_idx + (l * 2) + 1 > cmd_defs_len)
	    {
	      if (l * 2 > cmd_defs_len)
		cmd_defs_len += l * 2;
	      else
		cmd_defs_len *= 2;
	      cmd_defs = (char *) xrealloc (cmd_defs, cmd_defs_len);
	    }
	  
	  while (*p != '\0')
	    {
	      if (index (";'\"*?[]$<>(){}|&~`\\ \t\r\n\f\v", *p) != 0)
		cmd_defs[cmd_defs_idx++] = '\\';
	      cmd_defs[cmd_defs_idx++] = *p++;
	    }
	  cmd_defs[cmd_defs_idx++] = ' ';
	}
      else
	{
	  /* It was not a variable definition, so it is a target to be made.
	     Enter it as a file and add it to the dep chain of goals.  */
	  f = enter_file (other_args->list[i]);
	  f->cmd_target = 1;
	  
	  if (goals == 0)
	    {
	      goals = (struct dep *) xmalloc (sizeof (struct dep));
	      lastgoal = goals;
	    }
	  else
	    {
	      lastgoal->next = (struct dep *) xmalloc (sizeof (struct dep));
	      lastgoal = lastgoal->next;
	    }
	  lastgoal->name = 0;
	  lastgoal->file = f;
	}
    }

  if (cmd_defs_idx > 0)
    {
      cmd_defs[cmd_defs_idx - 1] = '\0';
      (void) define_variable ("MAKEOVERRIDES", 13, cmd_defs, o_override, 0);
    }

  /* Set the "MAKE" variable to the name we were invoked with.
     (If it is a relative pathname with a slash, prepend our directory name
     so the result will run the same program regardless of the current dir.
     If it is a name with no slash, we can only hope that PATH did not
     find it in the current directory.)

     Append the command-line variable definitions gathered above
     so sub-makes will get them as command-line definitions.  */

  if (current_directory[0] != '\0'
      && argv[0] != 0 && argv[0][0] != '/' && index (argv[0], '/') != 0)
    argv[0] = concat (current_directory, "/", argv[0]);

  if (cmd_defs_idx > 0)
    {
      char *str = concat (argv[0], " ", cmd_defs);
      (void) define_variable ("MAKE", 4, str, o_env, 0);
      free (str);
    }
  else
    (void) define_variable ("MAKE", 4, argv[0], o_env, 0);

  free (cmd_defs);

  /* If there were -c flags, move ourselves about.  */

  if (directories != 0)
    for (i = 0; directories->list[i] != 0; ++i)
      {
	char *dir = directories->list[i];
	if (chdir (dir) < 0)
	  pfatal_with_name (dir);
      }

  /* Figure out the level of recursion.  */
  {
    struct variable *v = lookup_variable ("MAKELEVEL", 9);
    if (v != 0 && *v->value != '\0' && *v->value != '-')
      makelevel = (unsigned int) atoi (v->value);
    else
      makelevel = 0;
  }

  /* Always do -w in sub-makes and under -C.  */
  print_directory_flag |= directories != 0 || makelevel > 0;

  /* Construct the list of include directories to search.  */

  construct_include_path (include_directories == 0 ? (char **) 0
			  : include_directories->list);

  /* Tell the user where he is.  */

  if (print_directory_flag)
    log_working_directory (1);

  /* Read any stdin makefiles into temporary files.  */

  if (makefiles != 0)
    {
      register unsigned int i;
      for (i = 0; i < makefiles->idx; ++i)
	if (makefiles->list[i][0] == '-' && makefiles->list[i][1] == '\0')
	  {
	    /* This makefile is standard input.  Since we may re-exec
	       and thus re-read the makefiles, we read standard input
	       into a temporary file and read from that.  */
	    static char name[] = "/tmp/GmXXXXXX";
	    FILE *outfile;

	    /* Free the storage allocated for "-".  */
	    free (makefiles->list[i]);

	    /* Make a unique filename.  */
	    (void) mktemp (name);

	    outfile = fopen (name, "w");
	    if (outfile == 0)
	      pfatal_with_name ("fopen (temporary file)");
	    while (!feof (stdin))
	      {
		char buf[2048];
		int n = fread (buf, 1, sizeof(buf), stdin);
		if (n > 0 && fwrite (buf, 1, n, outfile) != n)
		  pfatal_with_name ("fwrite (temporary file)");
	      }
	    /* Try to make sure we won't remake the temporary
	       file when we are re-exec'd.  Kludge-o-matic!  */
	    fprintf (outfile, "%s:;\n", name);
	    (void) fclose (outfile);

	    /* Replace the name that read_all_makefiles will
	       see with the name of the temporary file.  */
	    makefiles->list[i] = savestring (name, sizeof name - 1);

	    /* Make sure the temporary file will not be remade.  */
	    f = enter_file (savestring (name, sizeof name - 1));
	    f->updated = 1;
	    f->update_status = 0;
	    /* Let it be removed when we're done.  */
	    f->intermediate = 1;
	    /* But don't mention it.  */
	    f->dontcare = 1;
	  }
    }

  /* Set up to handle children dying.  This must be done before
     reading in the makefiles so that `shell' function calls will work.  */

#if	defined (SIGCHLD) && !defined (USG)
  (void) signal (SIGCHLD, child_handler);
#else
  (void) signal (SIGCLD, child_handler);
#endif

  /* Define the initial list of suffixes for old-style rules.  */

  set_default_suffixes ();

  /* Define some internal and special variables.  */

  define_automatic_variables ();

  /* Set up the MAKEFLAGS and MFLAGS variables
     so makefiles can look at them.  */

  define_makeflags (0, 0);

  /* Define the default variables.  */
  define_default_variables ();

  /* Read all the makefiles.  */

  default_file = enter_file (".DEFAULT");

  read_makefiles
    = read_all_makefiles (makefiles == 0 ? (char **) 0 : makefiles->list);

  decode_env_switches ("MAKEFLAGS", 9);
  decode_env_switches ("MFLAGS", 6);

  /* Set up MAKEFLAGS and MFLAGS again, so they will be right.  */

  define_makeflags (1, 0);

  ignore_errors_flag |= lookup_file (".IGNORE") != 0;

  silent_flag |= lookup_file (".SILENT") != 0;

  /* Make each `struct dep' point at the
     `struct file' for the file depended on.  */

  snap_deps ();

  /* Install the default implicit rules.
     This used to be done before reading the makefiles.
     But in that case, built-in pattern rules were in the chain
     before user-defined ones, so they matched first.  */

  install_default_implicit_rules ();

  /* Convert old-style suffix rules to pattern rules.  */

  convert_to_pattern ();

  /* Compute implicit rule limits.  */

  count_implicit_rule_limits ();

  /* Construct the listings of directories in VPATH lists.  */

  build_vpath_lists ();

  /* Mark files given with -o flags as very old (00:00:01.00 Jan 1, 1970)
     and as having been updated already, and files given with -W flags
     as brand new (time-stamp of now).  */

  if (old_files != 0)
    for (p = old_files->list; *p != 0; ++p)
      {
	f = enter_file (*p);
	f->last_mtime = (time_t) 1;
	f->updated = 1;
	f->update_status = 0;
      }

  if (new_files != 0)
    {
      now = time ((time_t *) 0);
      for (p = new_files->list; *p != 0; ++p)
	{
	  f = enter_file (*p);
	  f->last_mtime = now;
	}
    }

  if (read_makefiles != 0)
    {
      /* Update any makefiles if necessary.  */

      time_t *makefile_mtimes = 0;
      unsigned int mm_idx = 0;

      if (debug_flag)
	puts ("Updating makefiles....");

      /* Remove any makefiles we don't want to try to update.
	 Also record the current modtimes so we can compare them later.  */
      {
	register struct dep *d, *last;
	last = 0;
	d = read_makefiles;
	while (d != 0)
	  {
	    register struct file *f = d->file;
	    if (f->double_colon)
	      {
		do
		  {
		    if (f->deps == 0 && f->cmds != 0)
		      {
			/* This makefile is a :: target with commands, but
			   no dependencies.  So, it will always be remade.
			   This might well cause an infinite loop, so don't
			   try to remake it.  (This will only happen if
			   your makefiles are written exceptionally
			   stupidly; but if you work for Athena, that's how
			   you write your makefiles.)  */

			if (debug_flag)
			  printf ("Makefile `%s' might loop; not remaking it.\n",
				  f->name);

			if (last == 0)
			  read_makefiles = d->next;
			else
			  last->next = d->next;

			/* Free the storage.  */
			free ((char *) d);

			d = last == 0 ? 0 : last->next;

			break;
		      }
		    f = f->prev;
		  }
		while (f != NULL);
	      }
	    if (f == NULL || !f->double_colon)
	      {
		if (makefile_mtimes == 0)
		  makefile_mtimes = (time_t *) xmalloc (sizeof (time_t));
		else
		  makefile_mtimes = (time_t *)
		    xrealloc ((char *) makefile_mtimes,
			      (mm_idx + 1) * sizeof (time_t));
		makefile_mtimes[mm_idx++] = file_mtime_no_search (d->file);
		last = d;
		d = d->next;
	      }
	  }
      }	

      /* Set up `MAKEFLAGS' specially while remaking makefiles.  */
      define_makeflags (1, 1);

      switch (update_goal_chain (read_makefiles, 1))
	{
	default:
	  abort ();
	  
	case -1:
	  /* Did nothing.  */
	  break;
	  
	case 1:
	  /* Failed to update.  Figure out if we care.  */
	  {
	    /* Nonzero if any makefile was successfully remade.  */
	    int any_remade = 0;
	    /* Nonzero if any makefile we care about failed
	       in updating or could not be found at all.  */
	    int any_failed = 0;
	    register unsigned int i;

	    for (i = 0; read_makefiles != 0; ++i)
	      {
		struct dep *d = read_makefiles;
		read_makefiles = d->next;
		if (d->file->updated)
		  {
		    /* This makefile was updated.  */
		    if (d->file->update_status == 0)
		      {
			/* It was successfully updated.  */
			any_remade |= (file_mtime_no_search (d->file)
				       != makefile_mtimes[i]);
		      }
		    else if (d->changed != 1)
		      {
			time_t mtime;
			/* The update failed and this makefile was not
			   from the MAKEFILES variable, so we care.  */
			error ("Failed to remake makefile `%s'.",
			       d->file->name);
			mtime = file_mtime_no_search (d->file);
			any_remade |= (mtime != (time_t) -1
				       && mtime != makefile_mtimes[i]);
		      }
		  }
		else
		  /* This makefile was not found at all.  */
		  switch (d->changed)
		    {
		    case 0:
		      /* A normal makefile.  We must die later.  */
		      error ("Makefile `%s' was not found", dep_name (d));
		      any_failed = 1;
		      break;
		    case 1:
		      /* A makefile from the MAKEFILES variable.
			 We don't care.  */
		      break;
		    case 2:
		      /* An included makefile.  We don't need
			 to die, but we do want to complain.  */
		      error ("Included makefile `%s' was not found.",
			     dep_name (d));
		      break;
		    }

		free ((char *) d);
	      }

	    if (any_remade)
	      goto re_exec;
	    else if (any_failed)
	      die (1);
	    else
	      break;
	  }

	case 0:
	re_exec:;
	  /* Updated successfully.  Re-exec ourselves.  */
	  if (print_directory_flag)
	    log_working_directory (0);
	  if (debug_flag)
	    puts ("Re-execing myself....");
	  if (makefiles != 0)
	    {
	      /* These names might have changed.  */
	      register unsigned int i, j = 0;
	      for (i = 1; i < argc; ++i)
		if (!strcmp (argv[i], "-f"))
		    {
		      char *p = &argv[i][2];
		      if (*p == '\0')
			argv[++i] = makefiles->list[j];
		      else
			argv[i] = concat ("-f", makefiles->list[j], "");
		      ++j;
		    }
	    }
	  if (directories != 0 && directories->idx > 0)
	    {
	      char bad;
	      if (current_directory[0] != '\0')
		{
		  if (chdir (current_directory) < 0)
		    {
		      perror_with_name ("chdir", "");
		      bad = 1;
		    }
		  else
		    bad = 0;
		}
	      else
		bad = 1;
	      if (bad)
		fatal ("Couldn't change back to original directory.");
	    }
	  fflush (stdout);
	  fflush (stderr);
	  for (p = environ; *p != 0; ++p)
	    if (!strncmp (*p, "MAKELEVEL=", 10))
	      {
		*p = (char *) alloca (40);
		sprintf (*p, "MAKELEVEL=%u", makelevel);
		break;
	      }
	  exec_command (argv, environ);
	  /* NOTREACHED */
	}
    }

  /* Set up `MAKEFLAGS' again for the normal targets.  */
  define_makeflags (1, 0);

  {
    int status;

    /* If there were no command-line goals, use the default.  */
    if (goals == 0)
      {
	if (default_goal_file != 0)
	  {
	    goals = (struct dep *) xmalloc (sizeof (struct dep));
	    goals->next = 0;
	    goals->name = 0;
	    goals->file = default_goal_file;
	  }
      }
    else
      lastgoal->next = 0;

    if (goals != 0)
      {
	/* Update the goals.  */

	if (debug_flag)
	  puts ("Updating goal targets....");

	switch (update_goal_chain (goals, 0))
	  {
	  case -1:
	    /* Nothing happened.  */
	  case 0:
	    /* Updated successfully.  */
	    status = 0;
	    break;
	  case 1:
	    /* Updating failed.  */
	    status = 1;
	    break;
	  default:
	    abort ();
	  }
      }

    /* Print the data base under -p.  */
    if (print_data_base_flag)
      print_data_base ();

    if (goals == 0)
      {
	if (read_makefiles == 0)
	  fatal ("No targets specified and no makefile found");
	else
	  fatal ("No targets");
      }

    /* Exit.  */
    die (status);
  }

  return 0;
}

static void
decode_switches (argc, argv)
     int argc;
     char **argv;
{
  char bad = 0;
  register unsigned int i;
  register struct command_switch *cs;
  register struct stringlist *sl;
  char *p;
  char options[sizeof (switches) / sizeof (switches[0]) * 3];
  struct option long_options[(sizeof (switches) / sizeof (switches[0])) +
			     (sizeof (long_option_aliases) /
			      sizeof (long_option_aliases[0]))];
  register int c;

  decode_env_switches ("MAKEFLAGS", 9);
  decode_env_switches ("MFLAGS", 6);

  other_args = (struct stringlist *) xmalloc (sizeof (struct stringlist));
  other_args->max = argc + 1;
  other_args->list = (char **) xmalloc ((argc + 1) * sizeof (char *));
  other_args->idx = 1;
  other_args->list[0] = savestring (argv[0], strlen (argv[0]));

  /* Fill in the string and vector for getopt.  */
  p = options;
  *p++ = '-';			/* Non-option args are returned in order.  */
  for (i = 0; switches[i].c != '\0'; ++i)
    {
      long_options[i].name = (switches[i].long_name == 0 ? "" :
			      switches[i].long_name);
      long_options[i].flag = 0;
      *p++ = long_options[i].val = switches[i].c;
      switch (switches[i].type)
	{
	case flag:
	case flag_off:
	case ignore:
	  long_options[i].has_arg = no_argument;
	  break;

	case string:
	case positive_int:
	case floating:
	  *p++ = ':';
	  if (switches[i].noarg_value != 0)
	    {
	      *p++ = ':';
	      long_options[i].has_arg = optional_argument;
	    }
	  else
	    long_options[i].has_arg = required_argument;
	  break;
	}
    }
  *p = '\0';
  for (c = 0; c < (sizeof (long_option_aliases) /
		   sizeof (long_option_aliases[0]));
       ++c)
    long_options[i++] = long_option_aliases[c];
  long_options[i].name = 0;

  /* getopt does most of the parsing for us.  */
  while ((c = getopt_long (argc, argv,
			   options, long_options, (int *) 0)) != EOF)
    {
      if (c == '?')
	/* Bad option.  We will print a usage message and die later.
	   But continue to parse the other options so the user can
	   see all he did wrong.  */
	bad = 1;
      else if (c == 1)
	{
	  /* This is a non-option argument.  */
	  other_args->list[other_args->idx++] = optarg;
	  if (getenv ("POSIXLY_CORRECT") != 0)
	    /* POSIX.2 says all the options must come first.
	       All the remaining args are non-options.  */
	    break;
	}
      else
	for (cs = switches; cs->c != '\0'; ++cs)
	  if (cs->c == c)
	    {
	      switch (cs->type)
		{
		default:
		  abort ();

		case ignore:
		  break;

		case flag:
		case flag_off:
		  *(int *) cs->value_ptr = cs->type == flag;
		  break;

		case string:
		  if (optarg == 0)
		    optarg = cs->noarg_value;

		  sl = *(struct stringlist **) cs->value_ptr;
		  if (sl == 0)
		    {
		      sl = (struct stringlist *)
			xmalloc (sizeof (struct stringlist));
		      sl->max = 5;
		      sl->idx = 0;
		      sl->list = (char **) xmalloc (5 * sizeof (char *));
		      *(struct stringlist **) cs->value_ptr = sl;
		    }
		  else if (sl->idx == sl->max - 1)
		    {
		      sl->max += 5;
		      sl->list = (char **)
			xrealloc ((char *) sl->list,
				  sl->max * sizeof (char *));
		    }
		  sl->list[sl->idx++] = savestring (optarg, strlen (optarg));
		  sl->list[sl->idx] = 0;
		  break;

		case positive_int:
		  if (optarg == 0 && argc > optind
		      && isdigit (argv[optind][0]))
		    optarg = argv[optind++];
		  if (optarg != 0)
		    {
		      int i = atoi (optarg);
		      if (i < 1)
			{
			  error ("the `-%c' option requires a \
positive integral argument",
				 cs->c);
			  bad = 1;
			}
		      else
			*(unsigned int *) cs->value_ptr = i;
		    }
		  else
		    *(unsigned int *) cs->value_ptr
		      = *(unsigned int *) cs->noarg_value;
		  break;

		case floating:
		  *(double *) cs->value_ptr
		    = (optarg != 0 ? atof (optarg)
		       : (optind < argc && (isdigit (argv[optind][0])
					    || argv[optind][0] == '.'))
		       ? atof (argv[optind++])
		       : *(double *) cs->noarg_value);
		  break;
		}
	    
	      /* We've found the switch.  Stop looking.  */
	      break;
	    }
    }

  while (optind < argc)
    other_args->list[other_args->idx++] = argv[optind++];
  other_args->list[other_args->idx] = 0;

  if (bad)
    {
      /* Print a nice usage message.  */
      fprintf (stderr, "Usage: %s [options] [target] ...\n", program);
      fputs ("Options:\n", stderr);
      for (cs = switches; cs->c != '\0'; ++cs)
	{
	  char buf[1024], arg[50];

	  if (cs->description[0] == '-')
	    continue;

	  switch (long_options[cs - switches].has_arg)
	    {
	    case no_argument:
	      arg[0] = '\0';
	      break;
	    case required_argument:
	      sprintf (arg, " %s", cs->argdesc);
	      break;
	    case optional_argument:
	      sprintf (arg, " [%s]", cs->argdesc);
	      break;
	    }

	  p = buf;
	  sprintf (buf, "  -%c%s", cs->c, arg);
	  p += strlen (p);
	  if (cs->long_name != 0)
	    {
	      sprintf (p, ", --%s%s", cs->long_name, arg);
	      p += strlen (p);
	      for (i = 0; i < (sizeof (long_option_aliases) /
			       sizeof (long_option_aliases[0]));
		   ++i)
		if (long_option_aliases[i].val == cs->c)
		  {
		    sprintf (p, ", --%s%s", long_option_aliases[i].name, arg);
		    p += strlen (p);
		  }
	    }
	  {
	    struct command_switch *ncs = cs;
	    while ((++ncs)->c != '\0')
	      if (ncs->description[0] == '-' &&
		  ncs->description[1] == cs->c)
		{
		  /* This is another switch that does the same
		     one as the one we are processing.  We want
		     to list them all together on one line.  */
		  sprintf (p, ", -%c%s", ncs->c, arg);
		  p += strlen (p);
		  if (ncs->long_name != 0)
		    {
		      sprintf (p, ", --%s%s", ncs->long_name, arg);
		      p += strlen (p);
		    }
		}
	  }

	  if (p - buf >= 30)
	    {
	      fprintf (stderr, "%s\n", buf);
	      buf[0] = '\0';
	    }

	  fprintf (stderr, "%-30s%s.\n", buf, cs->description);
	}

      die (1);
    }
}

static void
decode_env_switches (envar, len)
     char *envar;
     unsigned int len;
{
  struct variable *v;
  register char *args;
  register struct command_switch *cs;

  v = lookup_variable (envar, len);
  if (v == 0 || *v->value == '\0')
    return;

  for (args = v->value; *args != '\0'; ++args)
    for (cs = switches; cs->c != '\0'; ++cs)
      if (cs->c == *args)
	if (cs->env)
	  switch (cs->type)
	    {
	    case string:
	      /* None of these allow environment changes.  */
	    default:
	      abort ();
	    case flag:
	    case flag_off:
	      *(int *) cs->value_ptr = cs->type == flag;
	      break;
	    case positive_int:
	      while (isspace (args[1]))
		++args;
	      if (isdigit(args[1]))
		{
		  int i = atoi (&args[1]);
		  while (isdigit (args[1]))
		    ++args;
		  if (i >= 1)
		    *(unsigned int *) cs->value_ptr = i;
		}
	      else
		*(unsigned int *) cs->value_ptr
		  = *(unsigned int *) cs->noarg_value;
	      break;
	    case floating:
	      while (isspace (args[1]))
		++args;
	      if (args[1] == '.' || isdigit (args[1]))
		{
		  *(double *) cs->value_ptr = atof (&args[1]);
		  while (args[1] == '.' || isdigit (args[1]))
		    ++args;
		}
	      else
		*(double *) cs->value_ptr = *(double *) cs->noarg_value;
	      break;
	    }
}

/* Define the MAKEFLAGS and MFLAGS variables to reflect the settings of the
   command switches.  Include positive_int and floating options if PF.
   Don't include options with the `no_makefile' flag is if MAKEFILE.  */

static void
define_makeflags (pf, makefile)
     int pf, makefile;
{
  register struct command_switch *cs;
  char flags[200];
  register unsigned int i;

  i = 0;
  for (cs = switches; cs->c != '\0'; ++cs)
    if (cs->toenv && (!makefile || !cs->no_makefile))
      {
	if (i == 0 || flags[i - 1] == ' ')
	  flags[i++] = '-';
	switch (cs->type)
	  {
	  default:
	    abort ();

	  case ignore:
	    break;

	  case flag:
	  case flag_off:
	    if (!*(int *) cs->value_ptr == (cs->type == flag_off)
		&& (cs->default_value == 0
		    || *(int *) cs->value_ptr != *(int *) cs->default_value))
	      flags[i++] = cs->c;
	    break;

	  case positive_int:
	    if (pf)
	      {
		if ((cs->default_value != 0
		     && (*(unsigned int *) cs->value_ptr
			 == *(unsigned int *) cs->default_value)))
		  break;
		else if (cs->noarg_value != 0
			 && (*(unsigned int *) cs->value_ptr ==
			     *(unsigned int *) cs->noarg_value))
		  flags[i++] = cs->c;
		else
		  {
		    unsigned int value;
		    if (cs->c == 'j')
		      value = 1;
		    else
		      value = *(unsigned int *) cs->value_ptr;
		    sprintf (&flags[i], "%c%u ", cs->c, value);
		    i += strlen (&flags[i]);
		  }
	      }
	    break;

	  case floating:
	    if (pf)
	      {
		if (cs->default_value != 0
		    && (*(double *) cs->value_ptr
			== *(double *) cs->default_value))
		  break;
		else if (cs->noarg_value != 0
			 && (*(double *) cs->value_ptr
			     == *(double *) cs->noarg_value))
		  flags[i++] = cs->c;
		else
		  {
		    sprintf (&flags[i], "%c%f ",
			     cs->c, *(double *) cs->value_ptr);
		    i += strlen (&flags[i]);
		  }
	      }
	    break;
	  }
      }

  if (i == 0)
    flags[0] = flags[1] = '\0';
  else if (flags[i - 1] == ' ' || flags[i - 1] == '-')
    flags[i - 1] = '\0';
  flags[i] = '\0';

  /* On Sun, the value of MFLAGS starts with a `-' but the
     value of MAKEFLAGS lacks the `-'.  Be compatible.  */
  (void) define_variable ("MAKEFLAGS", 9, &flags[1], o_env, 0);
  (void) define_variable ("MFLAGS", 6, flags, o_env, 0);
}

static int printed_version = 0;

/* Print version information.  */

static void
print_version ()
{
  extern char *remote_description;
  char *precede = print_data_base_flag ? "# " : "";

  printf ("%sGNU Make version %s", precede, version_string);
  if (remote_description != 0 && *remote_description != '\0')
    printf ("-%s", remote_description);

  printf (", by Richard Stallman and Roland McGrath.\n\
%sCopyright (C) 1988, 1989, 1990, 1991, 1992 Free Software Foundation, Inc.\n\
%sThis is free software; see the source for copying conditions.\n\
%sThere is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A\n\
%sPARTICULAR PURPOSE.\n\n", precede, precede, precede, precede);

  printed_version = 1;

  /* Flush stdout so the user doesn't have to wait to see the
     version information while things are thought about.  */
  fflush (stdout);
}

/* Print a bunch of information about this and that.  */

static void
print_data_base ()
{
  extern char *ctime ();
  time_t when;

  when = time ((time_t *) 0);
  printf ("\n# Make data base, printed on %s", ctime (&when));

  print_variable_data_base ();
  print_dir_data_base ();
  print_rule_data_base ();
  print_file_data_base ();
  print_vpath_data_base ();

  when = time ((time_t *) 0);
  printf ("\n# Finished Make data base on %s\n", ctime (&when));
}

/* Exit with STATUS, cleaning up as necessary.  */

void
die (status)
     int status;
{
  static char dying = 0;

  if (!dying)
    {
      int err;

      dying = 1;

      if (print_version_flag && !printed_version)
	print_version ();

      /* Wait for children to die.  */
      for (err = status != 0; job_slots_used > 0; err = 0)
	reap_children (1, err);

      /* Remove the intermediate files.  */
      remove_intermediates (0);

      if (print_directory_flag)
	log_working_directory (0);
    }

  exit (status);
}

/* Write a message indicating that we've just entered or
   left (according to ENTERING) the current directory.  */

static void
log_working_directory (entering)
     int entering;
{
  static int entered = 0;
  PATH_VAR (pwdbuf);
  char *message = entering ? "Entering" : "Leaving";

  if (entering)
    entered = 1;
  else if (!entered)
    /* Don't print the leaving message if we
       haven't printed the entering message.  */
    return;

  if (print_data_base_flag)
    fputs ("# ", stdout);

  if (makelevel == 0)
    printf ("%s: %s ", program, message);
  else
    printf ("%s[%u]: %s ", program, makelevel, message);

  if (getcwd (pwdbuf, GET_PATH_MAX) == 0)
    {
#ifdef	HAVE_GETCWD
      perror_with_name ("getcwd: ", "");
#else
      error ("getwd: %s", pwdbuf);
#endif
      puts ("an unknown directory");
    }
  else
    printf ("directory `%s'\n", pwdbuf);
}
