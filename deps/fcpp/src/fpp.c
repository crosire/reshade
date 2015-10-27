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

#include <stdio.h>
#include "fpp.h"
#include "cpp.h"

static ReturnCode dooptions(Global *global, fppTag *tags) /* dooptions is called to process command line arguments (-Detc). It is called only at cpp startup. */
{
	DEFBUF *dp;
	char end = FALSE; /* end of taglist */

	while (tags && !end)
	{
		switch (tags->tag)
		{
			case FPPTAG_END:
				end = TRUE;
				break;
			case FPPTAG_INITFUNC:
				global->initialfunc = (char *)tags->data;
				break;
			case FPPTAG_DISPLAYFUNCTIONS:
				global->outputfunctions = tags->data ? 1 : 0;
				break;
			case FPPTAG_RIGHTCONCAT:
				global->rightconcat = tags->data ? 1 : 0;
				break;
			case FPPTAG_OUTPUTMAIN:
				global->outputfile = tags->data ? 1 : 0;
				break;
			case FPPTAG_NESTED_COMMENTS:
				global->nestcomments = tags->data ? 1 : 0;
				break;
			case FPPTAG_WARNMISSINCLUDE:
				global->warnnoinclude = tags->data ? 1 : 0;
				break;
			case FPPTAG_WARN_NESTED_COMMENTS:
				global->warnnestcomments =  tags->data ? 1 : 0;
				break;
			case FPPTAG_OUTPUTSPACE:
				global->showspace = tags->data ? 1 : 0;
				break;
			case FPPTAG_OUTPUTBALANCE:
				global->showbalance = tags->data ? 1 : 0;
				break;
			case FPPTAG_OUTPUTINCLUDES:
				global->showincluded = tags->data ? 1 : 0;
				break;
			case FPPTAG_IGNOREVERSION:
				global->showversion = tags->data ? 0 : 1;
				break;
			case FPPTAG_WARNILLEGALCPP:
				global->warnillegalcpp = tags->data ? 1 : 0;
				break;
			case FPPTAG_OUTPUTLINE:
				global->outputLINE = tags->data ? 1 : 0;
				break;
			case FPPTAG_KEEPCOMMENTS:
				if (tags->data)
				{
					global->cflag = TRUE;
					global->keepcomments = TRUE;
				}
				break;
			case FPPTAG_DEFINE:
				{
					char *symbol = (char *)tags->data;
					char *text = symbol;

					while (*text != EOS && *text != '=')
					{
						text++;
					}

					/* If the option is just "-Dfoo", make it -Dfoo=1 */
					if (*text == EOS)
					{
						text = "1";
					}
					else
					{
						*text++ = EOS;
					}

					/* Now, save the word and its definition. */
					if (!(dp = defendel(global, symbol, FALSE)))
					{
						return FPP_OUT_OF_MEMORY;
					}

					dp->repl = savestring(global, text);
					dp->nargs = DEF_NOARGS;
					break;
				}
			case FPPTAG_IGNORE_NONFATAL:
				global->eflag = TRUE;
				break;
			case FPPTAG_INCLUDE_DIR:
				if (global->incend >= &global->incdir[NINCLUDE])
				{
					cfatal(global, FATAL_TOO_MANY_INCLUDE_DIRS);
					return FPP_TOO_MANY_INCLUDE_DIRS;
				}

				*global->incend++ = (char *)tags->data;
				break;
			case FPPTAG_INCLUDE_FILE:
			case FPPTAG_INCLUDE_MACRO_FILE:
				if (global->included >= NINCLUDE)
				{
					cfatal(global, FATAL_TOO_MANY_INCLUDE_FILES);
					return FPP_TOO_MANY_INCLUDE_FILES;
				}

				global->include[global->included] = (char *)tags->data;
				global->includeshow[global->included] = (tags->tag == FPPTAG_INCLUDE_FILE);
				global->included++;
				break;
			case FPPTAG_PREDEFINES:
				global->nflag = 1;
				break;
			case FPPTAG_IGNORE_CPLUSPLUS:
				global->cplusplus = !tags->data;
				break;
			case FPPTAG_UNDEFINE:
				if (defendel(global, (char *)tags->data, TRUE) == NULL)
				{
					cwarn(global, WARN_NOT_DEFINED, tags->data);
				}
				break;
			case FPPTAG_OUTPUT_DEFINES:
				global->wflag++;
				break;
			case FPPTAG_INPUT_NAME:
				strcpy(global->work, (const char *)tags->data); /* Remember input filename */
				global->first_file = (const char *)tags->data;
				break;
			case FPPTAG_INPUT:
				global->input = (char *(*)(char *, int, void *))tags->data;
				break;
			case FPPTAG_OUTPUT:
				global->output = (void (*)(void *, char))tags->data;
				break;
			case FPPTAG_ERROR:
				global->error = (void (*)(void *, const char *, va_list))tags->data;
				break;
			case FPPTAG_USERDATA:
				global->userdata = tags->data;
				break;
			case FPPTAG_LINE:
				global->linelines = tags->data ? 1 : 0;
				break;
			case FPPTAG_EXCLFUNC:
				global->excludedinit[global->excluded++] = (char *)tags->data;
				break;
			case FPPTAG_WEBMODE:
				global->webmode=(tags->data?1:0);
				break;
			default:
				cwarn(global, WARN_INTERNAL_ERROR, NULL);
				break;
		}

		tags++;
	}

	return FPP_OK;
}

int fppPreProcess(fppTag *tags)
{
	int i = 0;
	ReturnCode ret; /* cpp return code */
	Global *global;

	global = (Global *)malloc(sizeof(Global));

	if (!global)
	{
		return FPP_OUT_OF_MEMORY;
	}

	memset(global, 0, sizeof(Global));

	global->rec_recover = TRUE;
	global->ifstack[0] = TRUE; /* #if information */
	global->ifptr = global->ifstack;
	global->incend = global->incdir;
	global->cplusplus = 1;
	global->linelines = TRUE;
	global->warnillegalcpp = FALSE;
	global->outputLINE = TRUE;
	global->warnnoinclude = TRUE;
	global->showversion = TRUE;
	global->showincluded = FALSE;
	global->showspace = FALSE;
	global->nestcomments = FALSE;
	global->warnnestcomments = FALSE;
	global->outputfile = TRUE;

	/* Note: order is important */
	global->magic[0] = "__LINE__";
	global->magic[1] = "__FILE__";
	global->magic[2] = "__FUNCTION__";
	global->magic[3] = "__FUNC_LINE__";
	global->magic[4] = NULL; /* Must be last */

	ret = initdefines(global);

	if (ret != FPP_OK)
	{
		return ret;
	}

	dooptions(global, tags);

	/* open main file */
	if (global->first_file)
	{
		ret = openfile(global, global->first_file);

		if (ret != FPP_OK)
		{
			cerror(global, ERROR_CANNOT_OPEN_FILE, global->first_file);
			return ret;
		}
	}
	else
	{
		ret = addfile(global, stdin, global->work);
	}

	global->out = global->outputfile;

	if (ret == FPP_OK)
	{
		ret = cppmain(global); /* process main file */
	}

	if ((i = (global->ifptr - global->ifstack)) != 0)
	{
		cerror(global, ERROR_IFDEF_DEPTH, i);
	}

	return global->errors > 0 && !global->eflag ? !FPP_OK : FPP_OK;
}
