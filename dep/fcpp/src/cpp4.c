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
#include <ctype.h>
#include <limits.h>
#include "cppdef.h"
#include "cpp.h"

/* For debug and error messages */
static char *opname[] =
{
	"end of expression", "val", "id",
	"+", "-", "*", "/", "%",
	"<<", ">>", "&",  "|", "^",
	"==", "!=", "<", "<=", ">=", ">",
	"&&", "||", "?", ":", ",",
	"unary +", "unary -", "~", "!", "(",  ")", "(none)",
};

/*
 * opdope[] has the operator precedence:
 *
 * Bits:
 *	  7	Unused (so the value is always positive)
 *	6-2	Precedence (000x .. 017x)
 *	1-0	Binary op. flags:
 *	    01	The binop flag should be set/cleared when this op is seen.
 *	    10	The new value of the binop flag.
 *
 * Note:  Expected, New binop
 * constant		0	1	Binop, end, or ) should follow constants
 * End of line	1	0	End may not be preceeded by an operator
 * binary		1	0	Binary op follows a value, value follows.
 * unary		0	0	Unary op doesn't follow a value, value follows
 *   (          0   0   Doesn't follow value, value or unop follows
 *   )			1	1	Follows value.	Op follows.
 */
static char opdope[OP_MAX] =
{
	0001, /* End of expression */
	0002, /* Digit */
	0000, /* Letter (identifier) */
	0141, 0141, 0151, 0151, 0151, /* ADD, SUB, MUL, DIV, MOD */
	0131, 0131, 0101, 0071, 0071, /* ASL, ASR, AND,  OR, XOR */
	0111, 0111, 0121, 0121, 0121, 0121,	/* EQ, NE, LT, LE, GE, GT */
	0061, 0051, 0041, 0041, 0031, /* ANA, ORO, QUE, COL, CMA */
	0160, 0160, 0160, 0160,	/* PLU, NEG, COM, NOT */
	0170, 0013, 0023, /* LPA, RPA, END */
};

/*
 * OP_QUE and OP_RPA have alternate precedences:
 */
#define OP_RPA_PREC	0013
#define OP_QUE_PREC	0034

/*
 * S_ANDOR and S_QUEST signal "short-circuit" boolean evaluation, so that
 *	#if FOO != 0 && 10 / FOO ...
 * doesn't generate an error message. They are stored in optab.skip.
 */
#define S_ANDOR 2
#define S_QUEST 1

typedef struct optab
{
	char op; /* Operator */
	char prec; /* Its precedence */
	char skip; /* Short-circuit: TRUE to skip*/
} OPTAB;

#define isbinary(op) (op >= FIRST_BINOP && op <= LAST_BINOP)
#define isunary(op) (op >= FIRST_UNOP  && op <= LAST_UNOP)

static int evalnum(Global *global, int c) /* Expand number for #if lexical analysis. Note: evalnum recognizes the unsigned suffix, but only returns a signed int value.  */
{
	int value;
	int base;
	int c1;

	if (c != '0')
	{
		base = 10;
	}
	else if ((c = cget(global)) == 'x' || c == 'X')
	{
		base = 16;
		c = cget(global);
	}
	else
	{
		base = 8;
	}

	value = 0;

	for (;;)
	{
		c1 = c;

		if (isascii(c) && isupper(c1))
		{
			c1 = tolower(c1);
		}

		if (c1 >= 'a')
		{
			c1 -= ('a' - 10);
		}
		else
		{
			c1 -= '0';
		}

		if (c1 < 0 || c1 >= base)
		{
			break;
		}

		value *= base;
		value += c1;
		c = cget(global);
	}

	if (c == 'u' || c == 'U') /* Unsigned nonsense */
	{
		c = cget(global);
	}

	unget(global);

	return value;
}
static int evalchar(Global *global, int skip /* TRUE if short-circuit evaluation */) /* Get a character constant */
{
	int c;
	int value;
	int count;
	global->instring = TRUE;

	if ((c = cget(global)) == '\\')
	{
		switch ((c = cget(global)))
		{
			case 'a':
#if ('a' == '\a' || '\a' == ALERT)
				value = ALERT; /* Use predefined value */
#else
				value = '\a'; /* Use compiler's value */
#endif
				break;
			case 'b':
				value = '\b';
				break;
			case 'f':
				value = '\f';
				break;
			case 'n':
				value = '\n';
				break;
			case 'r':
				value = '\r';
				break;
			case 't':
				value = '\t';
				break;
			case 'v':
#if ('v' == '\v' || '\v' == VT)
				value = VT; /* Use predefined value */
#else
				value = '\v'; /* Use compiler's value */
#endif
				break;
			case 'x': /* '\xFF' */
				count = 3;
				value = 0;

				while ((((c = get(global)) >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) && (--count >= 0))
				{
					value *= 16;
					value += (c <= '9') ? (c - '0') : ((c & 0xF) + 9);
				}

				unget(global);
				break;
			default:
				if (c >= '0' && c <= '7')
				{
					count = 3;
					value = 0;

					while (c >= '0' && c <= '7' && --count >= 0)
					{
						value *= 8;
						value += (c - '0');
						c = get(global);
					}

					unget(global);
				}
				else
				{
					value = c;
				}
				break;
		}
	}
	else if (c == '\'')
	{
		value = 0;
	}
	else
	{
		value = c;
	}

	/* We warn on multi-byte constants and try to hack (big|little)endian machines. */
#if BIG_ENDIAN
	count = 0;
#endif

	while ((c = get(global)) != '\'' && c != EOF_CHAR && c != '\n')
	{
		if (!skip)
		{
			cwarn(global, WARN_MULTIBYTE_NOT_PORTABLE, c);
		}

#if BIG_ENDIAN
		count += CHAR_BIT;
		value += (c << count);
#else
		value <<= CHAR_BIT;
		value += c;
#endif
	}

	global->instring = FALSE;

	return value;
}
static ReturnCode evallex(Global *global, int skip /* TRUE if short-circuit evaluation */, int *op)
{
	/*
	 * Set *op to next eval operator or value. Called from eval(). It calls a special-purpose routines for 'char' strings and numeric values:
	 *   evalchar
	 *     called to evaluate 'x'
	 *   evalnum
	 *     called to evaluate numbers.
	 */

	int c, c1, t;
	ReturnCode ret;
	char loop;

	do
	{
		/* again: */
		loop = FALSE;

		do /* Collect the token */
		{
			c = skipws(global);

			if (ret = macroid(global, &c))
			{
				return ret;
			}

			if (c == EOF_CHAR || c == '\n')
			{
				unget(global);
				*op = OP_EOE; /* End of expression */
				return FPP_OK;
			}
		}
		while ((t = type[c]) == LET && catenate(global, &ret) && !ret);

		if (ret)
		{
			/* If the loop was broken because of a fatal error! */
			return ret;
		}

		if (t == INV) /* Total nonsense */
		{
			if (!skip)
			{
				if (isascii(c) && isprint(c))
				{
					cerror(global, ERROR_ILLEGAL_CHARACTER, c);
				}
				else
				{
					cerror(global, ERROR_ILLEGAL_CHARACTER2, c);
				}
			}

			return FPP_ILLEGAL_CHARACTER;
		}
		else if (t == QUO) /* ' or " */
		{
			if (c == '\'') /* Character constant   */
			{
				global->evalue = evalchar(global, skip); /* Somewhat messy */
				*op = DIG; /* Return a value */

				return FPP_OK;
			}

			cerror(global, ERROR_STRING_IN_IF);
			return FPP_CANT_USE_STRING_IN_IF;
		}
		else if (t == LET) /* ID must be a macro */
		{
			if (strcmp(global->tokenbuf, "defined") == 0) /* Or defined name */
			{
				c1 = c = skipws(global);

				if (c == '(')
				{
					/* Allow defined(name) */
					c = skipws(global);
				}

				if (type[c] == LET)
				{
					global->evalue = (lookid(global, c) != NULL);

					if (c1 != '(' /* Need to balance */ || skipws(global) == ')') /* Did we balance? */
					{
						*op = DIG;
						return FPP_OK; /* Parsed ok */
					}
				}

				cerror(global, ERROR_DEFINED_SYNTAX);
				return FPP_BAD_IF_DEFINED_SYNTAX;
			}

			global->evalue = 0;
			*op = DIG;

			return FPP_OK;
		}
		else if (t == DIG) /* Numbers are harder   */
		{
			global->evalue = evalnum(global, c);
		}
		else if (strchr("!=<>&|\\", c) != NULL)
		{
			/* Process a possible multi-byte lexeme. */
			c1 = cget(global); /* Peek at next char */

			switch (c)
			{
				case '!':
					if (c1 == '=')
					{
						*op = OP_NE;
						return FPP_OK;
					}
					break;
				case '=':
					if (c1 != '=') /* Can't say a=b in #if */
					{
						unget(global);
						cerror(global, ERROR_ILLEGAL_ASSIGN);
						return FPP_IF_ERROR;
					}

					*op = OP_EQ;
					return FPP_OK;
				case '>':
				case '<':
					if (c1 == c)
					{
						*op = ((c == '<') ? OP_ASL : OP_ASR);
						return FPP_OK;
					}
					else if (c1 == '=')
					{
						*op = ((c == '<') ? OP_LE  : OP_GE);
						return FPP_OK;
					}
					break;
				case '|':
				case '&':
					if (c1 == c)
					{
						*op = ((c == '|') ? OP_ORO : OP_ANA);
						return FPP_OK;
					}
					break;
				case '\\':
					if (c1 == '\n') /* Multi-line if */
					{
						loop = TRUE;
						break;
					}

					cerror(global, ERROR_ILLEGAL_BACKSLASH);
					return FPP_IF_ERROR;
			}

			if (!loop)
			{
				unget(global);
			}
		}
	}
	while (loop);

	*op = t;

	return FPP_OK;
}
static int *evaleval(Global *global,  int *valp, int op, int skip /* TRUE if short-circuit evaluation */)
{
	/*
	 * Apply the argument operator to the data on the value stack.
	 * One or two values are popped from the value stack and the result
	 * is pushed onto the value stack.
	 *
	 * OP_COL is a special case.
	 *
	 * evaleval() returns the new pointer to the top of the value stack.
	 */

	int v1, v2;

	if (isbinary(op))
	{
		v2 = *--valp;
	}

	v1 = *--valp;

	switch (op)
	{
		case OP_EOE:
			break;
		case OP_ADD:
			v1 += v2;
			break;
		case OP_SUB:
			v1 -= v2;
			break;
		case OP_MUL:
			v1 *= v2;
			break;
		case OP_DIV:
		case OP_MOD:
			if (v2 == 0)
			{
				if (!skip)
				{
					cwarn(global, WARN_DIVISION_BY_ZERO, (op == OP_DIV) ? "divide" : "mod");
				}

				v1 = 0;
			}
			else if (op == OP_DIV)
			{
				v1 /= v2;
			}
			else
			{
				v1 %= v2;
			}
			break;
		case OP_ASL:
			v1 <<= v2;
			break;
		case OP_ASR:
			v1 >>= v2;
			break;
		case OP_AND:
			v1 &= v2;
			break;
		case OP_OR:
			v1 |= v2;
			break;
		case OP_XOR:
			v1 ^= v2;
			break;
		case OP_EQ:
			v1 = (v1 == v2);
			break;
		case OP_NE:
			v1 = (v1 != v2);
			break;
		case OP_LT:
			v1 = (v1 < v2);
			break;
		case OP_LE:
			v1 = (v1 <= v2);
			break;
		case OP_GE:
			v1 = (v1 >= v2);
			break;
		case OP_GT:
			v1 = (v1 > v2);
			break;
		case OP_ANA:
			v1 = (v1 && v2);
			break;
		case OP_ORO:
			v1 = (v1 || v2);
			break;
		case OP_COL:
			/* v1 has the "true" value, v2 the "false" value. The top of the value stack has the test. */
			v1 = (*--valp) ? v1 : v2;
			break;
		case OP_NEG:
			v1 = (-v1);
			break;
		case OP_PLU:
			break;
		case OP_COM:
			v1 = ~v1;
			break;
		case OP_NOT:
			v1 = !v1;
			break;
		default:
			cerror(global, ERROR_IF_OPERAND, op);
			v1 = 0;
	}

	*valp++ = v1;

	return valp;
}
ReturnCode eval(Global *global, int *eval)
{
	/*
	 * Evaluate an expression.  Straight-forward operator precedence.
	 * This is called from control() on encountering an #if statement.
	 * It calls the following routines:
	 *   evallex
	 *     Lexical analyser -- returns the type and value of the next input token.
	 * evaleval
	 *     Evaluate the current operator, given the values on the value stack.  Returns a pointer to the (new) value stack.
	 *
	 * For compatiblity with older cpp's, this return returns 1 (TRUE) if a syntax error is detected.
	 */

	int op; /* Current operator	*/
	int *valp; /* -> value vector */
	OPTAB *opp; /* Operator stack */
	int prec; /* Op precedence */
	int binop; /* Set if binary op. needed */
	int op1; /* Operand from stack */
	int skip; /* For short-circuit testing */
	int value[NEXP]; /* Value stack */
	OPTAB opstack[NEXP]; /* Operand stack */
	ReturnCode ret;
	char again = TRUE;

	valp = value;
	opp = opstack;
	opp->op = OP_END; /* Mark bottom of stack */
	opp->prec = opdope[OP_END];	/* And its precedence */
	opp->skip = 0; /* Not skipping now */
	binop = 0;

	while (again)
	{
		ret = evallex(global, opp->skip, &op);

		if (ret)
		{
			return ret;
		}

		if (op == OP_SUB && binop == 0)
		{
			op = OP_NEG; /* Unary minus */
		}
		else if (op == OP_ADD && binop == 0)
		{
			op = OP_PLU; /* Unary plus */
		}
		else if (op == OP_FAIL)
		{
			*eval = 1; /* Error in evallex */
			return FPP_OK;
		}

		if (op == DIG) /* Value? */
		{
			if (binop != 0)
			{
				cerror(global, ERROR_MISPLACED_CONSTANT);
				*eval = 1;
				return FPP_OK;
			}
			else if (valp >= &value[NEXP - 1])
			{
				cerror(global, ERROR_IF_OVERFLOW);
				*eval = 1;
				return FPP_OK;
			}
			else
			{
				*valp++ = global->evalue;
				binop = 1;
			}

			again = TRUE;
			continue;
		}
		else if (op > OP_END)
		{
			cerror(global, ERROR_ILLEGAL_IF_LINE);
			*eval = 1;
			return FPP_OK;
		}

		prec = opdope[op];

		if (binop != (prec & 1))
		{
			cerror(global, ERROR_OPERATOR, opname[op]);
			*eval = 1;
			return FPP_OK;
		}

		binop = (prec & 2) >> 1;

		do
		{
			if (prec > opp->prec)
			{
				if (op == OP_LPA)
				{
					prec = OP_RPA_PREC;
				}
				else if (op == OP_QUE)
				{
					prec = OP_QUE_PREC;
				}

				op1 = opp->skip; /* Save skip for test */

				/* Push operator onto op. stack. */
				opp++;

				if (opp >= &opstack[NEXP])
				{
					cerror(global, ERROR_EXPR_OVERFLOW, opname[op]);
					*eval = 1;
					return FPP_OK;
				}

				opp->op = op;
				opp->prec = prec;
				skip = (valp[-1] != 0); /* Short-circuit tester */

				/* Do the short-circuit stuff here. Short-circuiting  stops automagically when operators are evaluated. */
				if ((op == OP_ANA && !skip) || (op == OP_ORO && skip))
				{
					/* And/or skip starts */
					opp->skip = S_ANDOR;
				}
				else if (op == OP_QUE) /* Start of ?: operator */
				{
					opp->skip = (op1 & S_ANDOR) | ((!skip) ? S_QUEST : 0);
				}
				else if (op == OP_COL) /* : inverts S_QUEST  */
				{
					opp->skip = (op1 & S_ANDOR) | (((op1 & S_QUEST) != 0) ? 0 : S_QUEST);
				}
				else /* Other ops leave	*/
				{
					/*  skipping unchanged. */
					opp->skip = op1;
				}

				again = TRUE;
				continue;
			}

			/* Pop operator from op. stack and evaluate it. End of stack and '(' are specials. */
			skip = opp->skip; /* Remember skip value */

			switch ((op1 = opp->op)) /* Look at stacked op */
			{
				case OP_END: /* Stack end marker */
					if (op == OP_EOE)
					{
						/* Finished ok. */
						*eval = valp[-1];
						return FPP_OK;
					}

					/* Read another op. */
					again = TRUE;
					continue;
				case OP_LPA: /* ( on stack */
					if (op != OP_RPA) /* Matches ) on input? */
					{
						cerror(global, ERROR_UNBALANCED_PARENS, opname[op]);
						*eval = 1;
						return FPP_OK;
					}

					opp--; /* Unstack it */
					/* -- Fall through 	*/
				case OP_QUE:
					/* Evaluate true expr.	*/
					again = TRUE;
					continue;
				case OP_COL: /* : on stack.	*/
					opp--; /* Unstack it */

					if (opp->op != OP_QUE) /* Matches ? on stack? */
					{
						cerror(global, ERROR_MISPLACED, opname[opp->op]);
						*eval = 1;
						return FPP_OK;
					}
					/* Evaluate op1. */
				default: /* Others:	 */
					opp--; /* Unstack the operator */
					valp = evaleval(global, valp, op1, skip);
					again = FALSE;
					break;
			}
		}
		while (!again);
	}

	return FPP_OK;
}
