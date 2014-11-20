/*
 * Copyright (c) 1999 Daniel Stenberg
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <time.h>
#include <stdio.h>	 
#include <ctype.h>	 
#include "cpp.h"

ReturnCode getfile(Global *global, int bufsize /* Line or define buffer size */, const char *name /* File or macro name string */, FILEINFO **file /* Common FILEINFO buffer initialization for a new file or macro. */)
{
	int size = strlen(name);

	if (!size)
	{
		name = "[stdin]";
		size = strlen(name);
	}

	*file = (FILEINFO *)malloc((int)(sizeof(FILEINFO) + bufsize + size));

	if (!*file)
	{
		return FPP_OUT_OF_MEMORY;
	}

	(*file)->parent = global->infile; /* Chain files together */
	(*file)->fp = NULL; /* No file yet */
	(*file)->filename = savestring(global, name); /* Save file/macro name */
	(*file)->progname = NULL; /* No #line seen yet */
	(*file)->unrecur = 0; /* No macro fixup */
	(*file)->bptr = (*file)->buffer; /* Initialize line ptr */
	(*file)->buffer[0] = EOS; /* Force first read */
	(*file)->line = 0; /* (Not used just yet) */

	if (global->infile != NULL)
	{
		/* If #include file, save current line */
		global->infile->line = global->line;
	}

	global->infile = (*file); /* New current file */
	global->line = 1; /* Note first line */

	return FPP_OK; /* All done. */
}
ReturnCode addfile(Global *global, FILE *fp, const char *filename) /* Initialize tables for this open file. This is called from openfile() above (for #include files),  and from the entry to cpp to open the main input file. It calls a common routine, getfile() to  build the FILEINFO structure which is used to read characters. (getfile() is also called to setup a macro replacement.)  */
{
	FILEINFO *file;
	ReturnCode ret;

	ret = getfile(global, NBUFF, filename, &file);

	if (ret)
	{
		return ret;
	}

	file->fp = fp; /* Better remember FILE * */
	file->buffer[0] = EOS; /* Initialize for first read */
	global->line = 1; /* Working on line 1 now */
	global->wrongline = TRUE; /* Force out initial #line */

	return FPP_OK;
}
ReturnCode openfile(Global *global, const char *filename) /* Open a file, add it to the linked list of open files. */
{
	FILE *fp;
	ReturnCode ret;

	if ((fp = fopen(filename, "r")) == NULL)
	{
		ret = FPP_OPEN_ERROR;
	}
	else
	{
		ret = addfile(global, fp, filename);
	}

	if (!ret && global->showincluded)
	{
		/* no error occured! */
		Error(global, "Included \"");
		Error(global, filename);
		Error(global, "\"\n");
	}

	return ret;
}
ReturnCode openinclude(Global *global, const char *filename, int searchlocal /* TRUE if #include "file" */) /* Actually open an include file. This routine is only called from doinclude() above, but was written as a separate subroutine for programmer convenience. It searches the list of directories and actually opens the file, linking it into the list of active files. Returns ReturnCode. No error message is printed. */
{
	char **incptr;
	char tmpname[NWORK]; /* Filename work area */
	int len;
	
	if (filename[0] == '/' || filename[0] == '\\')
	{
		if (openfile(global, filename) == FPP_OK)
		{
			return FPP_OK;
		}
		
	}

	if (searchlocal)
	{
		/*
		 * Look in local directory first. Try to open filename relative to the directory of the current
		 * source file (as opposed to the current directory). (ARF, SCK). Note that the fully qualified
		 * pathname is always built by discarding the last pathname component of the source file name then
		 * tacking on the #include argument.
		 */
		char *tp2 = strrchr(global->infile->filename, '/');

		if (tp2 == NULL)
		{
			tp2 = strrchr(global->infile->filename, '\\');
		}
		if (tp2 != NULL)
		{
			strncpy(tmpname, global->infile->filename, tp2 - global->infile->filename + 1);
			tmpname[tp2 - global->infile->filename + 1] = EOS;
			strcat(tmpname, filename);
		}
		else
		{
			strcpy(tmpname, filename);
		}

		if (openfile(global, tmpname) == FPP_OK)
		{
			return FPP_OK;
		}
	}

	/* Look in any directories specified by -I command line arguments, then in the builtin search list. */
	for (incptr = global->incdir; incptr < global->incend; incptr++)
	{
		len = strlen(*incptr);

		if (len + strlen(filename) >= sizeof(tmpname))
		{
			cfatal(global, FATAL_FILENAME_BUFFER_OVERFLOW);
			return FPP_FILENAME_BUFFER_OVERFLOW;
		}
		else
		{
			if ((*incptr)[len - 1] != '/')
			{
				sprintf(tmpname, "%s/%s", *incptr, filename);
			}
			else
			{
				sprintf(tmpname, "%s%s", *incptr, filename);
			}
			
			if (openfile(global, tmpname) == FPP_OK)
			{
				return FPP_OK;
			}
		}
	}

	return FPP_NO_INCLUDE;
}

ReturnCode initdefines(Global *global) /* Initialize the built-in #define's. */
{
	char **pp;
	char *tp;
	DEFBUF * dp;
	struct tm * tm;
	int i;
	time_t tvec;

	static const char months[12][4] =
	{
		"Jan",
		"Feb",
		"Mar",
		"Apr",
		"May",
		"Jun",
		"Jul",
		"Aug",
		"Sep",
		"Oct",
		"Nov",
		"Dec"
	};
	
	/* The magic pre-defines (__FILE__ and __LINE__ are initialized with negative argument counts. expand() notices this and calls the appropriate routine. DEF_NOARGS is one greater than the first "magic" definition. */
	if (!global->nflag)
	{
		for (pp = global->magic, i = DEF_NOARGS; *pp != NULL; pp++)
		{
			dp = defendel(global, *pp, FALSE);
			if (!dp)return FPP_OUT_OF_MEMORY;
			dp->nargs = --i;
		}
		
		/* Define __DATE__ as today's date. */
		dp = defendel(global, "__DATE__", FALSE);
		tp = (char *)malloc(14);

		if (!tp || !dp)
		{
			return FPP_OUT_OF_MEMORY;
		}

		dp->repl = tp;
		dp->nargs = DEF_NOARGS;
		time(&tvec);
		tm = localtime(&tvec);
		sprintf(tp, "\"%3s %2d %4d\"", /* "Aug 20 1988" */ months[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
		
		/* Define __TIME__ as this moment's time. */
		dp = defendel(global, "__TIME__", FALSE);
		tp = (char *)malloc(11);

		if (!tp || !dp)
		{
			return FPP_OUT_OF_MEMORY;
		}

		dp->repl = tp;
		dp->nargs = DEF_NOARGS;
		sprintf(tp, "\"%2d:%02d:%02d\"", /* "20:42:31" */ tm->tm_hour, tm->tm_min, tm->tm_sec);
	}

	return FPP_OK;
}
void deldefines(Global *global) /* Delete the built-in #define's. */
{
	char **pp;
	int i;
	
	/* The magic pre-defines __FILE__ and __LINE__ */
	for (pp = global->magic, i = DEF_NOARGS; *pp != NULL; pp++)
	{
		defendel(global, *pp, TRUE);
	}

	/* Undefine __DATE__. */
	defendel(global, "__DATE__", TRUE);
	
	/* Undefine __TIME__. */
	defendel(global, "__TIME__", TRUE);
}

/*
 * Generate (by hand-inspection) a set of unique values for each control operator. Note that this
 * is not guaranteed to work for non-Ascii machines. CPP won't compile if there are hash conflicts.
 */
enum
{
	L_assert = ('a' + ('s' << 1)),
	L_define = ('d' + ('f' << 1)),
	L_elif = ('e' + ('i' << 1)),
	L_else = ('e' + ('s' << 1)),
	L_endif = ('e' + ('d' << 1)),
	L_error = ('e' + ('r' << 1)),
	L_if = ('i' + (EOS << 1)),
	L_ifdef = ('i' + ('d' << 1)),
	L_ifndef = ('i' + ('n' << 1)),
	L_include = ('i' + ('c' << 1)),
	L_line = ('l' + ('n' << 1)),
	L_nogood = (EOS + (EOS << 1)), /* To catch #i */ 
	L_pragma = ('p' + ('a' << 1)),
	L_undef	= ('u' + ('d' << 1)),
};

static ReturnCode doif(Global *global, int hash) /* Process an #if, #ifdef, or #ifndef. The latter two are straightforward, while #if needs a subroutine of its own to evaluate the expression. doif() is called only if compiling is TRUE. If false, compilation is always supressed, so we don't need to evaluate anything. This supresses unnecessary warnings. */
{
	int c;
	int found;
	ReturnCode ret;

	if ((c = skipws(global)) == '\n' || c == EOF_CHAR)
	{
		unget(global);
		cerror(global, ERROR_MISSING_ARGUMENT);
		skipnl(global); /* Prevent an extra */
		unget(global); /* Error message */

		return FPP_OK;
	}

	if (hash == L_if)
	{
		unget(global);
		ret = eval(global, &found);

		if (ret)
		{
			return ret;
		}

		found = (found != 0); /* Evaluate expr, != 0 is TRUE */
		hash = L_ifdef; /* #if is now like #ifdef */
	}
	else
	{
		if (type[c] != LET) /* Next non-blank isn't letter */
		{
			/* ... is an error */
			cerror(global, ERROR_MISSING_ARGUMENT);
			skipnl(global); /* Prevent an extra */
			unget(global); /* Error message */

			return FPP_OK;
		}

		found = (lookid(global, c) != NULL); /* Look for it in symbol table */
	}

	if (found == (hash == L_ifdef))
	{
		compiling = TRUE;
		*global->ifptr |= TRUE_SEEN;
		
	}
	else
	{
		compiling = FALSE;
	}

	return FPP_OK;
	
}
static ReturnCode doinclude(Global *global) /* Process the #include control line. There are three variations:  #include "file" search somewhere relative to the current source file, if not found, treat as #include <file>. #include <file> Search in an implementation-dependent list of places. #include token Expand the token, it must be one of "file" or <file>, process as such. Note: the November 12 draft forbids '>' in the #include <file> format. This restriction is unnecessary and not implemented. */
{
	int c;
	int delim;
	ReturnCode ret;

	delim = skipws(global);

	if (ret = macroid(global, &delim))
	{
		return ret;
	}

	if (delim != '<' && delim != '"')
	{
		cerror(global, ERROR_INCLUDE_SYNTAX);
		return FPP_OK;
	}

	if (delim == '<')
	{
		delim = '>';
	}

	global->workp = global->work;

	while ((c = get(global)) != '\n' && c != EOF_CHAR)
	{
		/* Put it away. */
		if (ret = save(global, c))
		{
			return ret;
		}
	}

	unget(global); /* Force nl after include */
	
	/*  The draft is unclear if the following should be done. */
	while (--global->workp >= global->work && (*global->workp == ' ' || *global->workp == '\t'))
	{
		continue; /* Trim blanks from filename */
	}

	if (*global->workp != delim)
	{
		cerror(global, ERROR_INCLUDE_SYNTAX);
		return FPP_OK;
	}

	*global->workp = EOS; /* Terminate filename */

	ret = openinclude(global, global->work, (delim == '"'));

	if (ret && global->warnnoinclude)
	{
		/* Warn if #include file isn't there. */
		cwarn(global, WARN_CANNOT_OPEN_INCLUDE, global->work);
	}

	return FPP_OK;
}

ReturnCode control(Global *global, int *counter /* Pending newline counter */) /* Process #control lines. Simple commands are processed inline, while complex commands have their own subroutines. The counter is used to force out a newline before #line, and #pragma commands. This prevents these commands from ending up at the end of the previous line if cpp is invoked with the -C option. */
{
	int c;
	char *tp;
	int hash;
	char *ep;
	ReturnCode ret;

	c = skipws(global);

	if (c == '\n' || c == EOF_CHAR)
	{
		(*counter)++;
		return FPP_OK;
	}

	if (!isdigit(c))
	{
		scanid(global, c);	/* Get #word to tokenbuf */
	}
	else
	{
		unget(global); /* Hack -- allow #123 as a */
		strcpy(global->tokenbuf, "line"); /* synonym for #line 123 */
	}

	hash = (global->tokenbuf[1] == EOS) ? L_nogood : (global->tokenbuf[0] + (global->tokenbuf[2] << 1));

	switch (hash)
	{
		case L_assert:
			tp = "assert";
			break;
		case L_define:
			tp = "define";
			break;
		case L_elif:
			tp = "elif";
			break;
		case L_else:
			tp = "else";
			break;
		case L_endif:
			tp = "endif";
			break;
		case L_error:
			tp = "error";
			break;
		case L_if:
			tp = "if";
			break;
		case L_ifdef:
			tp = "ifdef";
			break;
		case L_ifndef:
			tp = "ifndef";
			break;
		case L_include:
			tp = "include";
			break;
		case L_line:
			tp = "line";
			break;
		case L_pragma:
			tp = "pragma";
			break;
		case L_undef:
			tp = "undef";
			break;
		default:
			hash = L_nogood;
		case L_nogood:
			tp = "";
			break;
	}

	if (strcmp(tp, global->tokenbuf) != 0)
	{
		hash = L_nogood;
	}

	/* hash is set to a unique value corresponding to the control keyword (or L_nogood if we think it's nonsense). */
	if (global->infile != NULL && global->infile->fp == NULL)
	{
		cwarn(global, WARN_CONTROL_LINE_IN_MACRO, global->tokenbuf);
	}

	if (!compiling)
	{
		/* Not compiling now */
		switch (hash)
		{
			/* These can't turn compilation on, but we must nest #if's */
			case L_if:
			case L_ifdef:
			case L_ifndef:
				if (++global->ifptr >= &global->ifstack[BLK_NEST])
				{
					cfatal(global, FATAL_TOO_MANY_NESTINGS, global->tokenbuf);
					return FPP_TOO_MANY_NESTED_STATEMENTS;
					
				}
				*global->ifptr = 0; /* !WAS_COMPILING */
			/* Many options are uninteresting if we aren't compiling. */
			case L_line:
			case L_include:
			case L_define:
			case L_undef:
			case L_assert:
			case L_error:
			/* Are pragma's always processed? */
			case L_pragma:
				skipnl(global); /* Ignore rest of line */
				(*counter)++;
				return FPP_OK;
		}
	}
	
	/*  Make sure that #line and #pragma are output on a fresh line. */
	if (*counter > 0 && (hash == L_line || hash == L_pragma))
	{
		Putchar(global, '\n');
		(*counter)--;
	}

	switch (hash)
	{
		case L_line:
			/* Parse the line to update the line number and "progname" field and line number for the next input line. Set wrongline to force it out later. */
			c = skipws(global);
			global->workp = global->work; /* Save name in work */

			while (c != '\n' && c != EOF_CHAR)
			{
				if (ret = save(global, c))
				{
					return ret;
				}

				c = get(global);
				
			}

			unget(global);

			if (ret = save(global, EOS))
			{
				return ret;
			}

			/* Split #line argument into <line-number> and <name> We subtract 1 as we want the number of the next line. */
			global->line = atoi(global->work) - 1; /* Reset line number */

			for (tp = global->work; isdigit(*tp) || type[*tp] == SPA; tp++)
			{
				continue; /* Skip over digits */
			}
			
			if (*tp != EOS)
			{
				/* Got a filename */
				if (*tp == '"' && (ep = strrchr(tp + 1, '"')) != NULL)
				{
					tp++; /* Skip over left quote */
					*ep = EOS; /* And ignore right one */
				}

				if (global->infile->progname != NULL)
				{
					/* Give up the old name if it's allocated. */
					free(global->infile->progname);
				}

				global->infile->progname = savestring(global, tp);
			}
			global->wrongline = TRUE; /* Force output later */
			break;
		case L_include:
			ret = doinclude(global);

			if (ret)
			{
				return ret;
			}
			break;
		case L_define:
			ret = dodefine(global);

			if (ret)
			{
				return ret;
			}
			break;
		case L_undef:
			doundef(global);
			break;
		case L_else:
			if (global->ifptr == &global->ifstack[0])
			{
				cerror(global, ERROR_STRING_MUST_BE_IF, global->tokenbuf);
				skipnl(global); /* Ignore rest of line */
				(*counter)++;
				return FPP_OK;
			}
			else if ((*global->ifptr & ELSE_SEEN) != 0)
			{
				cerror(global, ERROR_STRING_MAY_NOT_FOLLOW_ELSE, global->tokenbuf);
				skipnl(global); /* Ignore rest of line */
				(*counter)++;
				return FPP_OK;
			}

			*global->ifptr |= ELSE_SEEN;

			if ((*global->ifptr & WAS_COMPILING) != 0)
			{
				if (compiling || (*global->ifptr & TRUE_SEEN) != 0)
				{
					compiling = FALSE;
				}
				else
				{
					compiling = TRUE;
				}
			}
			break;
		case L_elif:
			if (global->ifptr == &global->ifstack[0])
			{
				cerror(global, ERROR_STRING_MUST_BE_IF, global->tokenbuf);
				skipnl(global); /* Ignore rest of line */
				(*counter)++;
				return FPP_OK;
			}
			else if ((*global->ifptr & ELSE_SEEN) != 0)
			{
				cerror(global, ERROR_STRING_MAY_NOT_FOLLOW_ELSE, global->tokenbuf);
				skipnl(global); /* Ignore rest of line */
				(*counter)++;
				return FPP_OK;
			}

			if ((*global->ifptr & (WAS_COMPILING | TRUE_SEEN)) != WAS_COMPILING)
			{
				compiling = FALSE; /* Done compiling stuff */
				skipnl(global); /* Skip this clause */
				(*counter)++;
				return FPP_OK;
			}

			ret = doif(global, L_if);

			if (ret)
			{
				return ret;
			}
			break;
		case L_error:
			cerror(global, ERROR_ERROR);
			break;
		case L_if:
		case L_ifdef:
		case L_ifndef:
			if (++global->ifptr < &global->ifstack[BLK_NEST])
			{
				*global->ifptr = WAS_COMPILING;
				ret = doif(global, hash);

				if (ret)
				{
					return ret;
				}
				break;
			}

			cfatal(global, FATAL_TOO_MANY_NESTINGS, global->tokenbuf);
			return FPP_TOO_MANY_NESTED_STATEMENTS;
		case L_endif:
			if (global->ifptr == &global->ifstack[0])
			{
				cerror(global, ERROR_STRING_MUST_BE_IF, global->tokenbuf);
				skipnl(global); /* Ignore rest of line */
				(*counter)++;
				return FPP_OK;
			}

			if (!compiling && (*global->ifptr & WAS_COMPILING) != 0)
			{
				global->wrongline = TRUE;
			}

			compiling = ((*global->ifptr & WAS_COMPILING) != 0);
			--global->ifptr;
			break;
		case L_assert:
		{
			int result;
			ret = eval(global, &result);

			if (ret)
			{
				return ret;
			}
			else if (result == 0)
			{
				cerror(global, ERROR_PREPROC_FAILURE);
			}
			break;
		}
		case L_pragma:
			/* #pragma is provided to pass "options" to later passes of the compiler. cpp doesn't have any yet. */
			Putstring(global, "#pragma ");

			while ((c = get(global)) != '\n' && c != EOF_CHAR)
			{
				Putchar(global, c);
			}

			unget(global);
			Putchar(global, '\n');
			break;
		default:
			/* Undefined #control keyword. Note: the correct behavior may be to warn and pass the line to a subsequent compiler pass. This would allow #asm or similar extensions. */
			if (global->warnillegalcpp)
			{
				cwarn(global, WARN_ILLEGAL_COMMAND, global->tokenbuf);
			}

			Putchar(global, '#');
			Putstring(global, global->tokenbuf);
			Putchar(global, ' ');

			while ((c = get(global)) != '\n' && c != EOF_CHAR)
			{
				Putchar(global, c);
			}

			unget(global);
			Putchar(global, '\n');
			break;
	}

	if (hash != L_include)
	{
		if (skipws(global) != '\n')
		{
			cwarn(global, WARN_UNEXPECTED_TEXT_IGNORED);
			skipnl(global);
		}
	}

	(*counter)++;

	return FPP_OK;
	
}
