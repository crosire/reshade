/*
 *
 * Frexx C Preprocessor
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	/* end of taglist */
	FPPTAG_END = 0,

	/* To make the preprocessed output keep the comments */
	FPPTAG_KEEPCOMMENTS, /* data is TRUE or FALSE */

	/* To define symbols to the preprocessor */
	FPPTAG_DEFINE, /* data is the string "symbol" or "symbol=<value>" */

	/* To make the preprocessor ignore all non-fatal errors */
	FPPTAG_IGNORE_NONFATAL, /* data is TRUE or FALSE */

	/* To add an include directory to the include directory list */
	FPPTAG_INCLUDE_DIR, /* data is directory name ending with a '/' (on amiga a ':' is also valid) */
	
	/* To define predefines like __LINE__, __DATE__, etc. default is TRUE */
	FPPTAG_PREDEFINES, /* data is TRUE or FALSE */
	
	/* To make fpp leave C++ comments in the output */
	FPPTAG_IGNORE_CPLUSPLUS, /* data is TRUE or FALSE */

	/* To undefine symbols */
	FPPTAG_UNDEFINE, /* data is symbol name */

	/* Output all #defines: */
	FPPTAG_OUTPUT_DEFINES, /* data is TRUE or FALSE */

	/* Initial input file name: */
	FPPTAG_INPUT_NAME, /* data is string */

	/* Input function: */
	FPPTAG_INPUT, /* data is an input funtion "char *(*)(char *, int, void *)" */

	/* Output function: */
	FPPTAG_OUTPUT, /* data is an output function "void (*)(void *, char)" */

	/* User data, sent in the last argument to the input function: */
	FPPTAG_USERDATA, /* data is user data */

	/* Whether to exclude #line instructions in the output, default is FALSE */
	FPPTAG_LINE, /* data is TRUE or FALSE */

	/* Error function. This is called when FPP finds any warning/error/fatal: */
	FPPTAG_ERROR, /* data is function pointer to a "void (*)(void *, const char *, va_list)" */

	/* Whether to warn for illegal cpp instructions */
	FPPTAG_WARNILLEGALCPP, /* data is boolean, default is FALSE */

	/* Output the 'line' keyword on #line-lines? */
	FPPTAG_OUTPUTLINE, /* data is boolean, default is TRUE */

	/* Do not output the version information string */
	FPPTAG_IGNOREVERSION, /* data is boolean, default is FALSE */

	/* Output all included file names to stderr */
	FPPTAG_OUTPUTINCLUDES, /* data is boolean, default is FALSE */

	/* Display warning if there is any brace, bracket or parentheses unbalance */
	FPPTAG_OUTPUTBALANCE, /* data is boolean, default is FALSE */

	/* Display all whitespaces in the source */
	FPPTAG_OUTPUTSPACE, /* data is boolean, default is FALSE */

	/* Allow nested comments */
	FPPTAG_NESTED_COMMENTS, /* data is boolean, default is FALSE */

	/* Enable warnings at nested comments */
	FPPTAG_WARN_NESTED_COMMENTS, /* data is boolean, default is FALSE */

	/* Enable warnings at missing includes */
	FPPTAG_WARNMISSINCLUDE, /* data is boolean, default is TRUE */

	/* Output the main file */
	FPPTAG_OUTPUTMAIN, /* data is boolean, default is TRUE */

	/* Include file */
	FPPTAG_INCLUDE_FILE, /* data is char pointer */

	/* Include macro file */
	FPPTAG_INCLUDE_MACRO_FILE, /* data is char pointer */

	/* Evaluate the right part of a concatenate before the concat */
	FPPTAG_RIGHTCONCAT, /* data is boolean, default is FALSE */

	/* Include the specified file at the beginning of each function */
	FPPTAG_INITFUNC, /* data is char pointer or NULL */

	/* Define function to be excluded from the "beginning-function-addings" */
	FPPTAG_EXCLFUNC, /* data is char pointer */

	/* Enable output of all function names defined in the source */
	FPPTAG_DISPLAYFUNCTIONS,

	/* Switch on WWW-mode */
	FPPTAG_WEBMODE,
} fppTags;
typedef struct
{
	fppTags tag;
	void *data;
} fppTag;

int fppPreProcess(fppTag *);

#ifdef __cplusplus
}
#endif
