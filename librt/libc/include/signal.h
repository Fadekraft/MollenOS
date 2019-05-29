/* MollenOS
 *
 * Copyright 2011 - 2017, Philip Meulengracht
 *
 * This program is free software : you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ? , either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Standard C-Signal Implementation
 * - Definitions, prototypes and information needed.
 */

#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <os/osdefs.h>

#ifndef _SIG_ATOMIC_T_DEFINED
#define _SIG_ATOMIC_T_DEFINED
  typedef int sig_atomic_t;
#endif

/* Signal names (from the Unix specification on signals) */
#define SIGHUP      1  /* Hangup */
#define SIGINT      2  /* External interrupt, usually initiated by the user */
#define SIGQUIT     3  /* Quit */
#define SIGILL      4  /* Illegal instruction */
#define SIGTRAP     5  /* A breakpoint or trace instruction has been reached */
#define SIGABRT     6  /* Abnormal termination condition, as is e.g. initiated by abort() */
#define SIGEMT      7  /* Emulation trap XXX */
#define SIGFPE      8  /* Erroneous arithmetic operation such as divide by zero */
#define SIGKILL     9  /* You have been stabbed repeated with a large knife */
#define SIGBUS      10 /* Bus error (device error) */
#define SIGSEGV     11 /* Invalid memory access (segmentation fault) */
#define SIGSYS      12 /* Bad system call */
#define SIGPIPE     13 /* Attempted to read or write from a broken pipe */
#define SIGALRM     14 /* This is your wakeup call. */
#define SIGTERM     15 /* Termination request, sent to the program */
#define SIGUSR1     16 /* User Defined Signal #1 */
#define SIGUSR2     17 /* User Defined Signal #2 */
#define SIGCHLD     18 /* Child status report */
#define SIGPWR      19 /* We need moar powah! */
#define SIGWINCH    20 /* Your containing terminal has changed size */
#define SIGURG      21 /* An URGENT! event (On a socket) */
#define SIGPOLL     22 /* XXX OBSOLETE; socket i/o possible */
#define SIGSTOP     23 /* Stopped (signal) */
#define SIGTSTP     24 /* ^Z (suspend) */
#define SIGCONT     25 /* Unsuspended (please, continue) */
#define SIGTTIN     26 /* TTY input has stopped */
#define SIGTTOUT    27 /* TTY output has stopped */
#define SIGVTALRM   28 /* Virtual timer has expired */
#define SIGPROF     29 /* Profiling timer expired */
#define SIGXCPU     30 /* CPU time limit exceeded */
#define SIGXFSZ     31 /* File size limit exceeded */
#define SIGWAITING  32 /* Herp */
#define SIGDIAF     33 /* Die in a fire */
#define SIGHATE     34 /* The sending process does not like you */
#define SIGWINEVENT 35 /* Window server event */
#define SIGCAT      36 /* Everybody loves cats */
#define NUMSIGNALS  37

#ifndef __SIGTYPE_DEFINED__
#define __SIGTYPE_DEFINED__
typedef	void (*__sa_handler_t)(int);
typedef void (*__sa_process_t)(int, void*);
#endif
#define SIG_DFL (__sa_handler_t)0
#define SIG_IGN (__sa_handler_t)1
#define SIG_ERR (__sa_handler_t)-1

/* We allow handlers for SIGINT, SIGSEGV, SIGTERM, SIGILL, SIGABRT, SIGFPE. */
_CODE_BEGIN
CRTDECL(__sa_process_t, sigprocess(int sig, __sa_process_t handler));
CRTDECL(__sa_handler_t, signal(int sig, __sa_handler_t handler));
CRTDECL(int,            raise(int sig));
_CODE_END

#endif //__SIGNAL_H__
