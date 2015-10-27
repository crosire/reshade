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

/*
 * Internal Definitions for CPP
 *
 * In general, definitions in this file should not be changed.
 */

#define VERSION_TEXT \
	"Frexx C Preprocessor v1.5.1 Copyright (C) by FrexxWare 1993 - 2002.\n"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <memory.h>

#ifndef TRUE
	#define TRUE 1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

#ifndef BIG_ENDIAN
	#define BIG_ENDIAN FALSE
#endif

#ifndef EOS
	/* End of string */
	#define EOS	'\0'
#endif
#ifndef EOF_CHAR
	/* Returned by get() on eof */
	#define EOF_CHAR 0
#endif

/* Some special constants. */
#ifndef ALERT
	#define ALERT '\007' /* '\a' is "Bell" */
#endif
#ifndef VT
	#define VT '\013' /* Vertical Tab CTRL/K */
#endif

/* #define foo vs #define foo() */
#define DEF_NOARGS (-1)

/* Magic quoting operator */
#define QUOTE_PARM 0x1C
/* Magic for #defines */
#define DEF_MAGIC 0x1D
/* Token concatenation delim. */
#define TOK_SEP 0x1E
/* Magic comment separator */
#define COM_SEP 0x1F

/* The maximum number of #define parameters (31 per Standard), Note: We need another one for strings. */
#ifndef PAR_MAC
	#define PAR_MAC (31 + 1)
#endif

/* Input buffer size */
#ifndef NBUFF
	#define NBUFF 512
#endif

/* Work buffer size -- the longest macro must fit here after expansion. */
#ifndef NWORK
	#define NWORK 512
#endif

/* The nesting depth of #if expressions */
#ifndef NEXP
	#define NEXP 128
#endif

/* The number of directories that may be specified on a per-system basis, or by the -I option. */
#ifndef NINCLUDE
	#define NINCLUDE 20
#endif

#ifndef NPARMWORK
	#define NPARMWORK (NWORK * 2)
#endif

/* The number of nested #if's permitted. */
#ifndef BLK_NEST
	#define BLK_NEST 32
#endif

/* SBSIZE defines the number of hash-table slots for the symbol table. */
#ifndef SBSIZE
	#define SBSIZE  64
#endif

/* RECURSION_LIMIT may be set to -1 to disable the macro recursion test. */
#ifndef RECURSION_LIMIT
	#define RECURSION_LIMIT 1000
#endif

/* maximum number of whitespaces possible to remember */
#define MAX_SPACE_SIZE 512

/* Note -- in Ascii, the following will map macro formals onto DEL + the C1 control character region (decimal 128 .. (128 + PAR_MAC)) which will be ok as long as PAR_MAC is less than 33). Note that the last PAR_MAC value is reserved for string substitution. */
#define MAC_PARM 0x7F /* Macro formals start here */

#ifndef OS9
	#if (PAR_MAC >= 33)
		#error	"assertion fails -- PAR_MAC isn't less than 33"
	#endif
#endif

#define LASTPARM (PAR_MAC - 1)

/* Character type codes. */
#define INV	0 /* Invalid, must be zero */
#define OP_EOE INV /* End of expression */
#define DIG	1 /* Digit */
#define LET	2 /* Identifier start */
#define DOL LET
#define OP_ADD 3
#define OP_SUB 4
#define OP_MUL 5
#define OP_DIV 6
#define OP_MOD 7
#define OP_ASL 8
#define OP_ASR 9
#define OP_AND 10 /* &, not && */
#define OP_OR 11 /* |, not || */
#define OP_XOR 12
#define OP_EQ 13
#define OP_NE 14
#define OP_LT 15
#define OP_LE 16
#define OP_GE 17
#define OP_GT 18
#define OP_ANA 19 /* && */
#define OP_ORO 20 /* || */
#define OP_QUE 21 /* ? */
#define OP_COL 22 /* : */
#define OP_CMA 23 /* , (relevant?) */
#define FIRST_BINOP	OP_ADD
#define LAST_BINOP OP_CMA /* Last binary operand */
/* The following are unary. */
#define FIRST_UNOP OP_PLU /* First Unary operand */
#define OP_PLU 24 /* + (draft ANSI standard) */
#define OP_NEG 25 /* - */
#define OP_COM 26 /* ~ */
#define OP_NOT 27 /* ! */
#define LAST_UNOP OP_NOT
#define OP_LPA 28 /* ( */
#define OP_RPA 29 /* ) */
#define OP_END 30 /* End of expression marker */
#define OP_MAX (OP_END + 1) /* Number of operators */
#define OP_FAIL (OP_END + 1) /* For error returns */

/* The following are for lexical scanning only. */
#define QUO 65 /* Both flavors of quotation	*/
#define DOT 66 /* . might start a number */
#define SPA 67 /* Space and tab */
#define BSH 68 /* Just a backslash */
#define END 69 /* EOF */

/* These bits are set in ifstack[] */
#define WAS_COMPILING 1 /* TRUE if compile set at entry */
#define ELSE_SEEN 2 /* TRUE when #else processed	*/
#define TRUE_SEEN 4 /* TRUE when #if TRUE processed */

/* The DEFBUF structure stores information about #defined macros. Note that the defbuf->repl information is always in malloc storage. */
typedef struct defbuf
{
	struct defbuf *link; /* Next define in chain */
	char *repl; /* -> replacement */
	int hash; /* Symbol table hash */
	int	 nargs; /* For define(args) */
	char name[1]; /* #define name */
} DEFBUF;

/* The FILEINFO structure stores information about open files and macros being expanded. */
typedef struct fileinfo
{
	char *bptr;	/* Buffer pointer */
	int	line; /* for include or macro */
	FILE *fp; /* File if non-null */
	struct fileinfo *parent; /* Link to includer */
	char *filename;	/* File/macro name	*/
	char *progname;	/* From #line statement */
	unsigned int unrecur; /* For macro recursion */
	char buffer[1];	/* current input line */
} FILEINFO;

/* Global object */
typedef struct
{
	/*
	 * Commonly used global variables:
	 * line       is the current input line number.
	 * wrongline  is set in many places when the actual output
	 *            line is out of sync with the numbering, e.g,
	 *            when expanding a macro with an embedded newline.
	 *
	 * tokenbuf   holds the last identifier scanned (which might
	 *            be a candidate for macro expansion).
	 * errors     is the running cpp error counter.
	 * infile     is the head of a linked list of input files (extended by
	 *            #include and macros being expanded).  infile always points
	 *            to the current file/macro.  infile->parent to the includer,
	 *            etc.  infile->fd is NULL if this input stream is a macro.
	 */
	int line; /* Current line number */
	int wrongline; /* Force #line to compiler */
	char *tokenbuf; /* Buffer for current input token */
	char *functionname; /* Buffer for current function */
	int funcline; /* Line number of current function */
	int tokenbsize; /* Allocated size of tokenbuf, not counting zero at end.    */
	int errors; /* cpp error counter */
	FILEINFO *infile; /* Current input file */

	/*
	 * This counter is incremented when a macro expansion is initiated.
	 * If it exceeds a built-in value, the expansion stops -- this tests
	 * for a runaway condition:
	 *    #define X Y
	 *    #define Y X
	 *    X
	 * This can be disabled by falsifying rec_recover.  (Nothing does this
	 * currently: it is a hook for an eventual invocation flag.)
	 */
	int recursion; /* Infinite recursion counter */
	int rec_recover; /* Unwind recursive macros */

	/*
	 * instring is set TRUE when a string is scanned.  It modifies the
	 * behavior of the "get next character" routine, causing all characters
	 * to be passed to the caller (except <DEF_MAGIC>).  Note especially that
	 * comments and \<newline> are not removed from the source.  (This
	 * prevents cpp output lines from being arbitrarily long).
	 *
	 * inmacro is set by #define -- it absorbs comments and converts
	 * form-feed and vertical-tab to space, but returns \<newline>
	 * to the caller.  Strictly speaking, this is a bug as \<newline>
	 * shouldn't delimit tokens, but we'll worry about that some other
	 * time -- it is more important to prevent infinitly long output lines.
	 *
	 * instring and inmarcor are parameters to the get() routine which
	 * were made global for speed.
	 */
	int instring; /* TRUE if scanning string */
	int inmacro; /* TRUE if #defining a macro */

	/*
	 * work[] and workp are used to store one piece of text in a temporay
	 * buffer. To initialize storage, set workp = work. To store one
	 * character, call save(c); (This will fatally exit if there isn't
	 * room.) To terminate the string, call save(EOS). Note that
	 * the work buffer is used by several subroutines -- be sure your
	 * data won't be overwritten. The extra byte in the allocation is
	 * needed for string formal replacement.
	 */
	char work[NWORK + 1]; /* Work buffer */
	char *workp; /* Work buffer pointer */

	/*
	 * keepcomments is set TRUE by the -C option. If TRUE, comments
	 * are written directly to the output stream. This is needed if
	 * the output from cpp is to be passed to lint (which uses commands
	 * embedded in comments).  cflag contains the permanent state of the
	 * -C flag.  keepcomments is always falsified when processing #control
	 * commands and when compilation is supressed by a false #if
	 *
	 * If eflag is set, CPP returns "success" even if non-fatal errors
	 * were detected.
	 *
	 * If nflag is non-zero, no symbols are predefined except __LINE__.
	 * __FILE__, and __DATE__.  If nflag > 1, absolutely no symbols
	 * are predefined.
	 */
	char keepcomments; /* Write out comments flag */
	char cflag; /* -C option (keep comments) */
	char eflag; /* -E option (never fail) */
	char nflag; /* -N option (no predefines) */
	char wflag; /* -W option (write #defines) */

	/*
	 * ifstack[] holds information about nested #if's.  It is always
	 * accessed via *ifptr.  The information is as follows:
	 *    WAS_COMPILING   state of compiling flag at outer level.
	 *    ELSE_SEEN       set TRUE when #else seen to prevent 2nd #else.
	 *    TRUE_SEEN       set TRUE when #if or #elif succeeds
	 * ifstack[0] holds the compiling flag.  It is TRUE if compilation
	 * is currently enabled.  Note that this must be initialized TRUE.
	 */
	char ifstack[BLK_NEST]; /* #if information */
	char *ifptr; /* -> current ifstack[] */

	/*
	 * incdir[] stores the -i directories (and the system-specific
	 * #include <...> directories.
	 */
	char *incdir[NINCLUDE]; /* -i directories */
	char **incend; /* -> free space in incdir[] */

	/*
	 * include[] stores the -X and -x files.
	 */
	char *include[NINCLUDE];
	char includeshow[NINCLUDE]; /* show it or not! */
	char included;

	/*
	 * The value of these predefined symbols must be recomputed whenever
	 * they are evaluated. The order must not be changed.
	 */
	char *magic[5];

	/*
	 * This is the variable saying if Cpp should remove C++ style comments from
	 * the output. Default is... TRUE, yes, pronto, do it!!!
	 */
	char cplusplus;

	char *sharpfilename;

	/*
	 * parm[], parmp, and parlist[] are used to store #define() argument
	 * lists. nargs contains the actual number of parameters stored.
	 */
	char parm[NPARMWORK + 1]; /* define param work buffer */
	char *parmp; /* Free space in parm */
	char *parlist[LASTPARM]; /* -> start of each parameter */
	int nargs; /* Parameters for this macro */

	DEFBUF *macro; /* Catches start of infinite macro */

	DEFBUF *symtab[SBSIZE]; /* Symbol table queue headers */

	int evalue; /* Current value from evallex() */

	void *userdata; /* Data sent to input function */
	char *(*input)(char *, int, void *); /* Input function */
	const char *first_file; /* Preprocessed file. */
	void (*output)(void *, char); /* output function */
	char outputfile; /* output the main file */
	char out; /* should we output anything now? */
	char outputfunctions; /* output all discovered functions to stderr! */
	void (*error)(void *, const char *, va_list); /* error function */

	char linelines;
	char warnillegalcpp; /* warn for illegal preprocessor instructions? */
	char outputLINE;   /* output 'line' in #line instructions */

	char showversion;  /* display version */
	char showincluded; /* display included files */
	char showbalance; /* display paren balance */
	char showspace; /* display all whitespaces as they are */

	char comment; /* TRUE if a comment just has been written to output */

	char *spacebuf; /* Buffer to store whitespaces in if -H */

	long chpos; /* Number of whitespaces in buffer */

	char nestcomments; /* Allow nested comments */
	char warnnestcomments; /* Warn at nested comments */

	char warnnoinclude; /* Warn at missing include file */

	char rightconcat; /* should the right part of a concatenation be avaluated before the concat (TRUE) or after (FALSE) */
	char *initialfunc; /* file to include first in all functions */

	char *excludedinit[20]; /* functions (names) excluded from the initfunc */
	int excluded;

	char webmode; /* WWW process mode */
} Global;

/* Error codes */
typedef enum
{
	ERROR_CANNOT_OPEN_FILE,
	ERROR_STRING_MUST_BE_IF,
	ERROR_STRING_MAY_NOT_FOLLOW_ELSE,
	ERROR_ERROR,
	ERROR_PREPROC_FAILURE,
	ERROR_MISSING_ARGUMENT,
	ERROR_INCLUDE_SYNTAX,
	ERROR_DEFINE_SYNTAX,
	ERROR_REDEFINE,
	ERROR_ILLEGAL_UNDEF,
	ERROR_RECURSIVE_MACRO,
	ERROR_EOF_IN_ARGUMENT,
	ERROR_MISPLACED_CONSTANT,
	ERROR_IF_OVERFLOW,
	ERROR_ILLEGAL_IF_LINE,
	ERROR_OPERATOR,
	ERROR_EXPR_OVERFLOW,
	ERROR_UNBALANCED_PARENS,
	ERROR_MISPLACED,
	ERROR_STRING_IN_IF,
	ERROR_DEFINED_SYNTAX,
	ERROR_ILLEGAL_ASSIGN,
	ERROR_ILLEGAL_BACKSLASH,
	ERROR_UNTERMINATED_STRING,
	ERROR_EOF_IN_COMMENT,
	ERROR_IFDEF_DEPTH,
	ERROR_ILLEGAL_CHARACTER,
	ERROR_ILLEGAL_CHARACTER2,
	ERROR_IF_OPERAND,
	ERROR_STRANG_CHARACTER,
	ERROR_STRANG_CHARACTER2,

	BORDER_ERROR_WARN, /* below this number: errors, above: warnings */

	WARN_CONTROL_LINE_IN_MACRO,
	WARN_ILLEGAL_COMMAND,
	WARN_UNEXPECTED_TEXT_IGNORED,
	WARN_NOT_DEFINED,
	WARN_INTERNAL_ERROR,
	WARN_MACRO_NEEDS_ARGUMENTS,
	WARN_WRONG_NUMBER_ARGUMENTS,
	WARN_DIVISION_BY_ZERO,
	WARN_ILLEGAL_OCTAL,
	WARN_MULTIBYTE_NOT_PORTABLE,
	WARN_CANNOT_OPEN_INCLUDE,
	WARN_BRACKET_DEPTH,
	WARN_PAREN_DEPTH,
	WARN_BRACE_DEPTH,
	WARN_NESTED_COMMENT,

	BORDER_WARN_FATAL, /* below this number: warnings, above: fatals */

	FATAL_TOO_MANY_NESTINGS,
	FATAL_FILENAME_BUFFER_OVERFLOW,
	FATAL_TOO_MANY_INCLUDE_DIRS,
	FATAL_TOO_MANY_INCLUDE_FILES,
	FATAL_TOO_MANY_ARGUMENTS_MACRO,
	FATAL_MACRO_AREA_OVERFLOW,
	FATAL_ILLEGAL_MACRO,
	FATAL_TOO_MANY_ARGUMENTS_EXPANSION,
	FATAL_OUT_OF_SPACE_IN_ARGUMENT,
	FATAL_WORK_AREA_OVERFLOW,
	FATAL_WORK_BUFFER_OVERFLOW,
	FATAL_OUT_OF_MEMORY,
	FATAL_TOO_MUCH_PUSHBACK
} ErrorCode;
typedef enum
{
	FPP_OK,
	FPP_OUT_OF_MEMORY,
	FPP_TOO_MANY_NESTED_STATEMENTS,
	FPP_FILENAME_BUFFER_OVERFLOW,
	FPP_NO_INCLUDE,
	FPP_OPEN_ERROR,
	FPP_TOO_MANY_ARGUMENTS,
	FPP_WORK_AREA_OVERFLOW,
	FPP_ILLEGAL_MACRO,
	FPP_EOF_IN_MACRO,
	FPP_OUT_OF_SPACE_IN_MACRO_EXPANSION,
	FPP_ILLEGAL_CHARACTER,
	FPP_CANT_USE_STRING_IN_IF,
	FPP_BAD_IF_DEFINED_SYNTAX,
	FPP_IF_ERROR,
	FPP_UNTERMINATED_STRING,
	FPP_TOO_MANY_INCLUDE_DIRS,
	FPP_TOO_MANY_INCLUDE_FILES,
	FPP_INTERNAL_ERROR,

	FPP_LAST_ERROR
} ReturnCode;

/* Prototypes */
void Error(Global *, const char *, ...);
void Putchar(Global *, char);
void Putstring(Global *, const char *);
void Putint(Global *, int);
char *savestring(Global *, const char *);
ReturnCode getfile(Global *, int, const char *, FILEINFO **);
ReturnCode addfile(Global *, FILE *, const char *);
int catenate(Global *, ReturnCode *);
void cerror(Global *, ErrorCode, ...);
ReturnCode control(Global *, int *);
ReturnCode dodefine(Global *);
void doundef(Global *);
ReturnCode expand(Global *, DEFBUF *);
int get(Global *);
ReturnCode initdefines(Global *);
void outdefines(Global *);
ReturnCode save(Global *, int);
void scanid(Global *, int);
ReturnCode scannumber(Global *, int, ReturnCode(*)(Global *, int));
ReturnCode scanstring(Global *, int, ReturnCode(*)(Global *, int));
void unget(Global *);
ReturnCode ungetstring(Global *, char *);
ReturnCode eval(Global *, int *);
void skipnl(Global *);
int skipws(Global *);
ReturnCode macroid(Global *, int *);
DEFBUF *lookid(Global *, int );
DEFBUF *defendel(Global *, char *, int);
ReturnCode expstuff(Global *, char *, char *);
int cget(Global *);
void deldefines(Global *);
ReturnCode openfile(Global *, const char *);
ReturnCode openinclude(Global *, const char *, int);
ReturnCode cppmain(Global *);

/* Nasty defines to make them appear as three different functions! */
#define cwarn cerror
#define cfatal cerror

#define compiling global->ifstack[0]

/* Character classifier */
extern const char type[];
