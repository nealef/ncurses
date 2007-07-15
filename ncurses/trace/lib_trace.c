/****************************************************************************
 * Copyright (c) 1998-2006,2007 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 *     and: Thomas E. Dickey                        1996-on                 *
 ****************************************************************************/

/*
 *	lib_trace.c - Tracing/Debugging routines
 *
 * The _tracef() function is originally from pcurses (by Pavel Curtis) in 1982. 
 * pcurses allowed one to enable/disable tracing using traceon() and traceoff()
 * functions.  ncurses provides a trace() function which allows one to
 * selectively enable or disable several tracing features.
 */

#include <curses.priv.h>
#include <tic.h>

#include <ctype.h>

MODULE_ID("$Id: lib_trace.c,v 1.62 2007/07/14 19:32:54 tom Exp $")

NCURSES_EXPORT_VAR(unsigned) _nc_tracing = 0; /* always define this */

#ifdef TRACE
NCURSES_EXPORT_VAR(const char *) _nc_tputs_trace = "";
NCURSES_EXPORT_VAR(long) _nc_outchars = 0;

#define TraceFP		_nc_globals.trace_fp
#define TracePath	_nc_globals.trace_fname
#define TraceLevel	_nc_globals.trace_level

NCURSES_EXPORT(void)
trace(const unsigned int tracelevel)
{
    if ((TraceFP == 0) && tracelevel) {
	const char *mode = _nc_globals.init_trace ? "ab" : "wb";

	if (TracePath[0] == '\0') {
	    if (getcwd(TracePath, sizeof(TracePath) - 12) == 0) {
		perror("curses: Can't get working directory");
		exit(EXIT_FAILURE);
	    }
	    strcat(TracePath, "/trace");
	    if (_nc_is_dir_path(TracePath)) {
		strcat(TracePath, ".log");
	    }
	}

	_nc_globals.init_trace = TRUE;
	_nc_tracing = tracelevel;
	if (_nc_access(TracePath, W_OK) < 0
	    || (TraceFP = fopen(TracePath, mode)) == 0) {
	    perror("curses: Can't open 'trace' file");
	    exit(EXIT_FAILURE);
	}
	/* Try to set line-buffered mode, or (failing that) unbuffered,
	 * so that the trace-output gets flushed automatically at the
	 * end of each line.  This is useful in case the program dies. 
	 */
#if HAVE_SETVBUF		/* ANSI */
	(void) setvbuf(TraceFP, (char *) 0, _IOLBF, 0);
#elif HAVE_SETBUF		/* POSIX */
	(void) setbuffer(TraceFP, (char *) 0);
#endif
	_tracef("TRACING NCURSES version %s.%d (tracelevel=%#x)",
		NCURSES_VERSION,
		NCURSES_VERSION_PATCH,
		tracelevel);
    } else if (tracelevel == 0) {
	if (TraceFP != 0) {
	    fclose(TraceFP);
	    TraceFP = 0;
	}
	_nc_tracing = tracelevel;
    } else if (_nc_tracing != tracelevel) {
	_nc_tracing = tracelevel;
	_tracef("tracelevel=%#x", tracelevel);
    }
}

NCURSES_EXPORT(void)
_tracef(const char *fmt,...)
{
    static const char Called[] = T_CALLED("");
    static const char Return[] = T_RETURN("");
    va_list ap;
    bool before = FALSE;
    bool after = FALSE;
    unsigned doit = _nc_tracing;
    int save_err = errno;

    if (strlen(fmt) >= sizeof(Called) - 1) {
	if (!strncmp(fmt, Called, sizeof(Called) - 1)) {
	    before = TRUE;
	    TraceLevel++;
	} else if (!strncmp(fmt, Return, sizeof(Return) - 1)) {
	    after = TRUE;
	}
	if (before || after) {
	    if ((TraceLevel <= 1)
		|| (doit & TRACE_ICALLS) != 0)
		doit &= (TRACE_CALLS | TRACE_CCALLS);
	    else
		doit = 0;
	}
    }

    if (doit != 0) {
	if (TraceFP == 0)
	    TraceFP = stderr;
	if (before || after) {
	    int n;
	    for (n = 1; n < TraceLevel; n++)
		fputs("+ ", TraceFP);
	}
	va_start(ap, fmt);
	vfprintf(TraceFP, fmt, ap);
	fputc('\n', TraceFP);
	va_end(ap);
	fflush(TraceFP);
    }

    if (after && TraceLevel)
	TraceLevel--;
    errno = save_err;
}

/* Trace 'bool' return-values */
NCURSES_EXPORT(NCURSES_BOOL)
_nc_retrace_bool(NCURSES_BOOL code)
{
    T((T_RETURN("%s"), code ? "TRUE" : "FALSE"));
    return code;
}

/* Trace 'int' return-values */
NCURSES_EXPORT(int)
_nc_retrace_int(int code)
{
    T((T_RETURN("%d"), code));
    return code;
}

/* Trace 'unsigned' return-values */
NCURSES_EXPORT(unsigned)
_nc_retrace_unsigned(unsigned code)
{
    T((T_RETURN("%#x"), code));
    return code;
}

/* Trace 'char*' return-values */
NCURSES_EXPORT(char *)
_nc_retrace_ptr(char *code)
{
    T((T_RETURN("%s"), _nc_visbuf(code)));
    return code;
}

/* Trace 'const char*' return-values */
NCURSES_EXPORT(const char *)
_nc_retrace_cptr(const char *code)
{
    T((T_RETURN("%s"), _nc_visbuf(code)));
    return code;
}

/* Trace 'NCURSES_CONST void*' return-values */
NCURSES_EXPORT(NCURSES_CONST void *)
_nc_retrace_cvoid_ptr(NCURSES_CONST void *code)
{
    T((T_RETURN("%p"), code));
    return code;
}

/* Trace 'void*' return-values */
NCURSES_EXPORT(void *)
_nc_retrace_void_ptr(void *code)
{
    T((T_RETURN("%p"), code));
    return code;
}

/* Trace 'SCREEN *' return-values */
NCURSES_EXPORT(SCREEN *)
_nc_retrace_sp(SCREEN *code)
{
    T((T_RETURN("%p"), code));
    return code;
}

/* Trace 'WINDOW *' return-values */
NCURSES_EXPORT(WINDOW *)
_nc_retrace_win(WINDOW *code)
{
    T((T_RETURN("%p"), code));
    return code;
}
#endif /* TRACE */
