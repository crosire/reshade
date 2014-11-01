/**
 * ReShade
 * Copyright (C) 2014, Crosire
 *
 * This file may only be used, modified, and distributed under the terms of the license specified
 * in License.txt. By continuing to use, modify, or distribute this file you indicate that you
 * have read the license and understand and accept it completly.
 */

/*
 * Based on ANSI C lex specification by Jeff Lee 1985, Tom Stockfisch 1987, Jutta Degener 1995
 */

/* ----------------------------------------------------------------------------------------------
|  Options																						|
---------------------------------------------------------------------------------------------- */

%option reentrant
%option bison-bridge bison-locations
%option yylineno
%option nounput noyywrap
%option never-interactive
%option extra-type="ReShade::EffectParser *"

%top {
	#include <limits>
	#include <ctype.h>
	#include <stdint.h>
}

%{
	#include <boost\algorithm\string\trim.hpp>
	#include <boost\algorithm\string\case_conv.hpp>

	#include "EffectParser.hpp"

	static void													Escape(char *buffer, std::size_t &len);

	// Flex Macros
	#define YY_USER_ACTION yylloc->Line = yylineno, yylloc->Column = yycolumn, yycolumn += yyleng;
%}

/* ----------------------------------------------------------------------------------------------
|  States																						|
---------------------------------------------------------------------------------------------- */

%option stack

%x STATE_COMMENT STATE_PP STATE_PP_LINE STATE_PP_PRAGMA STATE_PP_ERROR STATE_PP_WARNING

%x STATE_FIELD
%s STATE_PROPERTY STATE_TECHNIQUE STATE_PASS

/* ----------------------------------------------------------------------------------------------
|  Rules																						|
---------------------------------------------------------------------------------------------- */

LF																(\r\n|\n)
WS																[ \t]

IDENTIFIER														[a-zA-Z_][a-zA-Z0-9_]*
INTEGRAL														(0|([1-9][0-9]*))
STRING															\"(\\.|[^\\"])*\"

%%

 /* Preprocessor ----------------------------------------------------------------------------- */

"//*"															{ yyless(1); return '/'; }
"//"[^*].*{LF}													{ }
"/*"															{ yy_push_state(STATE_COMMENT, yyscanner); }
<STATE_COMMENT>
{
	"/*"														{ }
	"*/"														{ yy_pop_state(yyscanner); }
	"*"															{ }
	[^*\n]+														{ }
	{LF}														{ }
}

^{WS}*#{WS}*													{ yy_push_state(STATE_PP, yyscanner); }
^{WS}*#{WS}*"line"{WS}+											{ yy_push_state(STATE_PP_LINE, yyscanner); }
^{WS}*#{WS}*"pragma"{WS}+										{ yyextra->mPragmas.push_back(std::string()); yy_push_state(STATE_PP_PRAGMA, yyscanner); }
^{WS}*#{WS}*"error"												{ yyextra->mPragmas.push_back(std::string()); yy_push_state(STATE_PP_ERROR, yyscanner); }
^{WS}*#{WS}*"warning"											{ yyextra->mPragmas.push_back(std::string()); yy_push_state(STATE_PP_WARNING, yyscanner); }
<STATE_PP>
{
	[^\\\n]+													{ yyextra->Error(*yylloc, 1504, "invalid preprocessor command '%s'", yytext); }
	{WS}+|\\													{ }
	\\{LF}														{ yycolumn = 1; }
	{LF}														{ yycolumn = 1; yy_pop_state(yyscanner); }
}
<STATE_PP_LINE>
{
	{INTEGRAL}													{ yy_pop_state(yyscanner);
		
		yylineno = static_cast<int>(::strtol(yytext, nullptr, 10));

		if (yylineno == 0)
		{
			yyextra->Warning(*yylloc, 3000, "line numbers should be greater than zero");
		}
		else
		{
			yylineno--;
		}
	}
	{INTEGRAL}{WS}+{STRING}										{ yy_pop_state(yyscanner);
		
		char *begin = yytext, *end;

		yylineno = static_cast<int>(::strtol(begin, &end, 10));

		if (yylineno == 0)
		{
			yyextra->Warning(*yylloc, 3000, "line numbers should be greater than zero");
		}
		else
		{
			yylineno--;
		}

		while (*end != '\0' && *++end != '"') continue;
		begin = end;
		while (*end != '\0' && *++end != '"') continue;

		const std::size_t len = end - begin;

		if (len != 0)
		{
			yylloc->Source = yyextra->mAST.AddString(begin + 1, len - 1);
		}
	}
}
<STATE_PP_PRAGMA>
{
	[^\\\n\t ]+|\\												{ yyextra->mPragmas.back() += yytext; }
	{WS}+														{ yyextra->mPragmas.back() += ' '; }
	\\{LF}														{ }
	{LF}														{ yy_pop_state(yyscanner); }
}
<STATE_PP_ERROR>
{
	[^\\\n]+|\\													{ yyextra->mPragmas.back() += yytext; }
	\\{LF}														{ }
	{LF}														{ yy_pop_state(yyscanner);
	
		std::string &message = yyextra->mPragmas.back();
		boost::algorithm::trim(message);
		
		yyextra->Error(*yylloc, 0, "%s", message.c_str());
		yyextra->mPragmas.pop_back();
	}
}
<STATE_PP_WARNING>
{
	[^\\\n]+|\\													{ yyextra->mPragmas.back() += yytext; }
	\\{LF}														{ }
	{LF}														{ yy_pop_state(yyscanner);
	
		std::string &message = yyextra->mPragmas.back();
		boost::algorithm::trim(message);
		
		yyextra->Warning(*yylloc, 0, "%s", message.c_str());
		yyextra->mPragmas.pop_back();
	}
}

<*><<EOF>>														{

	if (YYSTATE == STATE_COMMENT)
	{
		yyextra->Error(*yylloc, 1001, "comment continues past end of file");
	}

	yypop_buffer_state(yyscanner);

	if (!YY_CURRENT_BUFFER)
	{
		yyterminate();
	}
}

 /* Keywords --------------------------------------------------------------------------------- */

"template"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"new"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"delete"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"try"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"catch"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"operator"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"cast"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"static_cast"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"dynamic_cast"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"reinterpret_cast"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"const_cast"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"public"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"protected"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"private"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"friend"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"explicit"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"virtual"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"external"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }

"namespace"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"technique"														{ yyextra->mNextLexerState = STATE_TECHNIQUE; return TOK_TECHNIQUE; }
<STATE_TECHNIQUE>"pass"											{ yyextra->mNextLexerState = STATE_PASS; return TOK_PASS; }
"class"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"struct"														{ return TOK_STRUCT; }
"union"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"enum"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"interface"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"this"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"typedef"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"using"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sizeof"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"compile"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"asm"|"asm_fragment"											{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"register"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"packoffset"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }

"for"															{ return TOK_FOR; }
"foreach"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"while"															{ return TOK_WHILE; }
"do"															{ return TOK_DO; }
"if"															{ return TOK_IF; }
"else"															{ return TOK_ELSE; }
"switch"														{ return TOK_SWITCH; }
"case"															{ return TOK_CASE; }
"default"														{ return TOK_DEFAULT; }
"break"															{ return TOK_BREAK; }
"continue"														{ return TOK_CONTINUE; }
"return"														{ return TOK_RETURN; }
"discard"														{ return TOK_DISCARD; }
"goto"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext);  }

"extern"														{ yylval->l.Uint = ReShade::EffectNodes::Type::Extern; return TOK_EXTERN; }
"static"														{ yylval->l.Uint = ReShade::EffectNodes::Type::Static; return TOK_STATIC; }
"uniform"														{ yylval->l.Uint = ReShade::EffectNodes::Type::Uniform; return TOK_UNIFORM; }
"shared"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"groupshared"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"globallycoherent"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"packed"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"volatile"														{ yylval->l.Uint = ReShade::EffectNodes::Type::Volatile; return TOK_VOLATILE; }
"precise"														{ yylval->l.Uint = ReShade::EffectNodes::Type::Precise; return TOK_PRECISE; }
"in"															{ yylval->l.Uint = ReShade::EffectNodes::Type::In; return TOK_IN; }
"out"															{ yylval->l.Uint = ReShade::EffectNodes::Type::Out; return TOK_OUT; }
"inout"															{ yylval->l.Uint = ReShade::EffectNodes::Type::InOut; return TOK_INOUT; }
"inline"|"noinline"												{ }
"const"															{ yylval->l.Uint = ReShade::EffectNodes::Type::Const; return TOK_CONST; }
"mutable"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"row_major"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"column_major"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"snorm"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"unorm"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"signed"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"unsigned"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"nointerpolation"												{ yylval->l.Uint = ReShade::EffectNodes::Type::NoInterpolation; return TOK_NOINTERPOLATION; }
"noperspective"													{ yylval->l.Uint = ReShade::EffectNodes::Type::NoPerspective; return TOK_NOPERSPECTIVE; }
"linear"														{ yylval->l.Uint = ReShade::EffectNodes::Type::Linear; return TOK_LINEAR; }
"centroid"														{ yylval->l.Uint = ReShade::EffectNodes::Type::Centroid; return TOK_CENTROID; }
"sample"														{ yylval->l.Uint = ReShade::EffectNodes::Type::Sample; return TOK_SAMPLE; }

<STATE_PROPERTY>"Width"											{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::Width; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"Height"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::Height; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"MipLevels"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::MipLevels; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"Format"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::Format; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"Texture"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::Texture; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"AddressU"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::AddressU; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"AddressV"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::AddressV; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"AddressW"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::AddressW; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"MinFilter"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::MinFilter; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"MagFilter"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::MagFilter; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"MipFilter"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::MipFilter; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"MipLODBias"									{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::MipLODBias; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"MaxAnisotropy"									{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::MaxAnisotropy; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"MinLOD"										{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::MinLOD; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>"MaxLOD"|"MaxMipLevel"							{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::MaxLOD; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PROPERTY>[sS]"RGB"("Texture")?|"SRGBReadEnable"			{ yylval->l.Uint = ReShade::EffectNodes::Variable::Property::SRGBTexture; return TOK_IDENTIFIER_PROPERTY; }
<STATE_PASS>"VertexShader"										{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::VertexShader; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"PixelShader"										{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::PixelShader; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"RenderTarget"										{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::RenderTarget0; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"RenderTarget"[0-7]									{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::RenderTarget0 + *(yytext + 12) - '0'; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"RenderTargetWriteMask"|"ColorWriteEnable"			{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::ColorWriteMask; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"SRGBWriteEnable"									{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::SRGBWriteEnable; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>("Alpha")?"BlendEnable"								{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::BlendEnable; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"SrcBlend"											{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::SrcBlend; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"DestBlend"											{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::DestBlend; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"BlendOp"											{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::BlendOp; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"BlendOpAlpha"										{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::BlendOpAlpha; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>("Depth"|"Z")"Enable"								{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::DepthEnable; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"DepthWriteMask"|"ZWriteEnable"						{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::DepthWriteMask; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>("Depth"|"Z")"Func"									{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::DepthFunc; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"StencilEnable"										{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::StencilEnable; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"StencilReadMask"|"StencilMask"						{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::StencilReadMask; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"StencilWriteMask"									{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::StencilWriteMask; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"StencilRef"										{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::StencilRef; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"StencilFunc"										{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::StencilFunc; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"StencilPass"("Op")?								{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::StencilPass; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"StencilFail"("Op")?								{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::StencilFail; return TOK_IDENTIFIER_PASSSTATE; }
<STATE_PASS>"Stencil"("Depth"|"Z")"Fail"("Op")?					{ yylval->l.Uint = ReShade::EffectNodes::Pass::State::StencilDepthFail; return TOK_IDENTIFIER_PASSSTATE; }

 /* Literals --------------------------------------------------------------------------------- */

[tT]"rue"|"TRUE"												{ yylval->l.Int = true; return TOK_LITERAL_BOOL; }
[fF]"alse"|"FALSE"												{ yylval->l.Int = false; return TOK_LITERAL_BOOL; }
{STRING}														{ yylval->l.String.len = ::strlen(yytext) - 2; char *p = yytext + 1; Escape(p, yylval->l.String.len); yylval->l.String.p = p; return TOK_LITERAL_STRING; }
[0-9]+"."[0-9]*([eE][+-]?[0-9]+)?[hHfFlL]?						{ yylval->l.Float = static_cast<float>(::strtod(yytext, nullptr)); return TOK_LITERAL_FLOAT; }
"."[0-9]+([eE][+-]?[0-9]+)?[hHfFlL]?							{ yylval->l.Float = static_cast<float>(::strtod(yytext, nullptr)); return TOK_LITERAL_FLOAT; }
[0-9]+[eE][+-]?[0-9]+[hHfFlL]?									{ yylval->l.Float = static_cast<float>(::strtod(yytext, nullptr)); return TOK_LITERAL_FLOAT; }
[0-9]+[hHfFlL]													{ yylval->l.Float = static_cast<float>(::strtod(yytext, nullptr)); return TOK_LITERAL_FLOAT; }
1".#INF"														{ yylval->l.Float = std::numeric_limits<float>::infinity(); return TOK_LITERAL_FLOAT; }
1".#IND"														{ yylval->l.Float = std::numeric_limits<float>::quiet_NaN(); return TOK_LITERAL_FLOAT; }
1".#QNAN"														{ yylval->l.Float = std::numeric_limits<float>::quiet_NaN(); return TOK_LITERAL_FLOAT; }
1".#SNAN"														{ yylval->l.Float = std::numeric_limits<float>::signaling_NaN(); return TOK_LITERAL_FLOAT; }
0[xX][a-fA-F0-9]+[uUlL]?										{ if (::tolower(yytext[::strlen(yytext) - 1]) == 'u') { yylval->l.Uint = static_cast<unsigned int>(::strtoul(yytext, nullptr, 16)); return TOK_LITERAL_INT; } else { yylval->l.Int = static_cast<int>(::strtol(yytext, nullptr, 16)); return TOK_LITERAL_INT; } }
0[0-7]+[uUlL]?													{ if (::tolower(yytext[::strlen(yytext) - 1]) == 'u') { yylval->l.Uint = static_cast<unsigned int>(::strtoul(yytext, nullptr,  8)); return TOK_LITERAL_INT; } else { yylval->l.Int = static_cast<int>(::strtol(yytext, nullptr,  8)); return TOK_LITERAL_INT; } }
{INTEGRAL}[uUlL]?												{ if (::tolower(yytext[::strlen(yytext) - 1]) == 'u') { yylval->l.Uint = static_cast<unsigned int>(::strtoul(yytext, nullptr, 10)); return TOK_LITERAL_INT; } else { yylval->l.Int = static_cast<int>(::strtol(yytext, nullptr, 10)); return TOK_LITERAL_INT; } }
"NULL"															{ yylval->l.Uint = 0; return TOK_LITERAL_INT; }

"POSITION"{INTEGRAL}?											{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
"TEXCOORD"{INTEGRAL}?											{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
"NORMAL"{INTEGRAL}?												{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
"TANGENT"{INTEGRAL}?											{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
("BINORMAL"|"BITANGENT"){INTEGRAL}?								{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
"COLOR"{INTEGRAL}?												{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
"PSIZE"{INTEGRAL}?												{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
"BLENDINDICES"{INTEGRAL}?										{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
"BLENDWEIGHT"{INTEGRAL}?										{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
"TESSFACTOR"{INTEGRAL}?											{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
"INDEX"{INTEGRAL}?												{ yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_ClipDistance"){INTEGRAL}?								{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_CullDistance"){INTEGRAL}?								{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_Coverage")												{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_DispatchThreadID")										{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_DomainLocation")										{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_GroupID")												{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_GroupIndex")											{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_GroupThreadID")											{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_GSInstanceID")											{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_InsideTessFactor")										{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_IsFrontFace")											{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_OutputControlPointID")									{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_Position")												{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_RenderTargetArrayIndex")								{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_SampleIndex")											{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_Target"){INTEGRAL}?										{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_TessFactor")											{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_ViewportArrayIndex")									{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_InstanceID")											{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_PrimitiveID")											{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_VertexID")												{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_Depth")													{ boost::algorithm::to_upper(yytext); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }

<STATE_PASS,STATE_PROPERTY>"NONE"								{ yylval->l.Uint = ReShade::EffectNodes::Literal::NONE; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"POINT"|"NEAREST"								{ yylval->l.Uint = ReShade::EffectNodes::Literal::POINT; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"LINEAR"										{ yylval->l.Uint = ReShade::EffectNodes::Literal::LINEAR; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"ANISOTROPIC"									{ yylval->l.Uint = ReShade::EffectNodes::Literal::ANISOTROPIC; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"CLAMP"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::CLAMP; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"MIRROR"										{ yylval->l.Uint = ReShade::EffectNodes::Literal::MIRROR; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"REPEAT"|"WRAP"									{ yylval->l.Uint = ReShade::EffectNodes::Literal::REPEAT; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"BORDER"										{ yylval->l.Uint = ReShade::EffectNodes::Literal::BORDER; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"R8"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::R8; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"R32F"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::R32F; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"RG8"|"R8G8"									{ yylval->l.Uint = ReShade::EffectNodes::Literal::RG8; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"RGBA8"|"R8G8B8A8"								{ yylval->l.Uint = ReShade::EffectNodes::Literal::RGBA8; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"RGBA16"|"R16G16B16A16"							{ yylval->l.Uint = ReShade::EffectNodes::Literal::RGBA16; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"RGBA16F"|"R16G16B16A16F"						{ yylval->l.Uint = ReShade::EffectNodes::Literal::RGBA16F; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"RGBA32F"|"R32G32B32A32F"						{ yylval->l.Uint = ReShade::EffectNodes::Literal::RGBA32F; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"DXT1"|"BC1"									{ yylval->l.Uint = ReShade::EffectNodes::Literal::DXT1; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"DXT3"|"BC2"									{ yylval->l.Uint = ReShade::EffectNodes::Literal::DXT3; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"DXT5"|"BC3"									{ yylval->l.Uint = ReShade::EffectNodes::Literal::DXT5; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"LATC1"|"BC4"									{ yylval->l.Uint = ReShade::EffectNodes::Literal::LATC1; return TOK_LITERAL_ENUM; }
<STATE_PROPERTY>"LATC2"|"BC5"									{ yylval->l.Uint = ReShade::EffectNodes::Literal::LATC2; return TOK_LITERAL_ENUM; }
<STATE_PASS>"ZERO"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::ZERO; return TOK_LITERAL_ENUM; }
<STATE_PASS>"ONE"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::ONE; return TOK_LITERAL_ENUM; }
<STATE_PASS>"SRCCOLOR"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::SRCCOLOR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"SRCALPHA"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::SRCALPHA; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INVSRCCOLOR"										{ yylval->l.Uint = ReShade::EffectNodes::Literal::INVSRCCOLOR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INVSRCALPHA"										{ yylval->l.Uint = ReShade::EffectNodes::Literal::INVSRCALPHA; return TOK_LITERAL_ENUM; }
<STATE_PASS>"DESTCOLOR"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::DESTCOLOR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"DESTALPHA"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::DESTALPHA; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INVDESTCOLOR"										{ yylval->l.Uint = ReShade::EffectNodes::Literal::INVDESTCOLOR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INVDESTALPHA"										{ yylval->l.Uint = ReShade::EffectNodes::Literal::INVDESTALPHA; return TOK_LITERAL_ENUM; }
<STATE_PASS>"ADD"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::ADD; return TOK_LITERAL_ENUM; }
<STATE_PASS>"SUBTRACT"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::SUBTRACT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"REVSUBTRACT"										{ yylval->l.Uint = ReShade::EffectNodes::Literal::REVSUBTRACT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"MIN"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::MIN; return TOK_LITERAL_ENUM; }
<STATE_PASS>"MAX"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::MAX; return TOK_LITERAL_ENUM; }
<STATE_PASS>"KEEP"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::KEEP; return TOK_LITERAL_ENUM; }
<STATE_PASS>"REPLACE"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::REPLACE; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INVERT"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::INVERT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INCR"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::INCR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INCRSAT"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::INCRSAT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"DECR"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::DECR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"DECRSAT"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::DECRSAT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"NEVER"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::NEVER; return TOK_LITERAL_ENUM; }
<STATE_PASS>"ALWAYS"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::ALWAYS; return TOK_LITERAL_ENUM; }
<STATE_PASS>"LESS"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::LESS; return TOK_LITERAL_ENUM; }
<STATE_PASS>"GREATER"											{ yylval->l.Uint = ReShade::EffectNodes::Literal::GREATER; return TOK_LITERAL_ENUM; }
<STATE_PASS>"LESSEQUAL"|"LEQUAL"								{ yylval->l.Uint = ReShade::EffectNodes::Literal::LESSEQUAL; return TOK_LITERAL_ENUM; }
<STATE_PASS>"GREATEREQUAL"|"GEQUAL"								{ yylval->l.Uint = ReShade::EffectNodes::Literal::GREATEREQUAL; return TOK_LITERAL_ENUM; }
<STATE_PASS>"EQUAL"												{ yylval->l.Uint = ReShade::EffectNodes::Literal::EQUAL; return TOK_LITERAL_ENUM; }
<STATE_PASS>"NOTEQUAL"|"NEQUAL"									{ yylval->l.Uint = ReShade::EffectNodes::Literal::NOTEQUAL; return TOK_LITERAL_ENUM; }
<STATE_PASS>"RED"												{ yylval->l.Int = 1 << 0; return TOK_LITERAL_INT; }
<STATE_PASS>"GREEN"												{ yylval->l.Int = 1 << 1; return TOK_LITERAL_INT; }
<STATE_PASS>"BLUE"												{ yylval->l.Int = 1 << 2; return TOK_LITERAL_INT; }
<STATE_PASS>"ALPHA"												{ yylval->l.Int = 1 << 3; return TOK_LITERAL_INT; }

 /* Types ------------------------------------------------------------------------------------ */

"vector"														{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Void; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = 4; yylval->l.Type.Cols = 1; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_VECTOR; }
"matrix"														{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Void; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = 4; yylval->l.Type.Cols = 4; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_MATRIX; }

"auto"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"void"															{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Void; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = 0; yylval->l.Type.Cols = 0; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_VOID; }
"bool"															{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Bool; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = 1; yylval->l.Type.Cols = 1; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_BOOL; }
"char"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"short"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"int"															{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Int; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = 1; yylval->l.Type.Cols = 1; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_INT; }
"uint"|"dword"													{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Uint; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = 1; yylval->l.Type.Cols = 1; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_UINT; }
"long"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"half"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"float"															{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Float; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = 1; yylval->l.Type.Cols = 1; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_FLOAT; }
"double"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"fixed"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }

"bool"[1-4]														{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Bool; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = *(yytext + 4) - '0'; yylval->l.Type.Cols = 1; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_BOOLV; }
"int"[1-4]														{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Int; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = *(yytext + 3) - '0'; yylval->l.Type.Cols = 1; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_INTV; }
"uint"[1-4]														{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Uint; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = *(yytext + 4) - '0'; yylval->l.Type.Cols = 1; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_UINTV; }
"dword"[1-4]													{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Uint; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = *(yytext + 5) - '0'; yylval->l.Type.Cols = 1; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_UINTV; }
"half"[1-4]														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"float"[1-4]													{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Float; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = *(yytext + 5) - '0'; yylval->l.Type.Cols = 1; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_FLOATV; }
"double"[1-4]													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"fixed"[1-4]													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }

"bool"[1-4]x[1-4]												{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Bool; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = *(yytext + 4) - '0'; yylval->l.Type.Cols = *(yytext + 6) - '0'; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_BOOLM; }
"int"[1-4]x[1-4]												{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Int; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = *(yytext + 3) - '0'; yylval->l.Type.Cols = *(yytext + 5) - '0'; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_INTM; }
"uint"[1-4]x[1-4]												{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Uint; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = *(yytext + 4) - '0'; yylval->l.Type.Cols = *(yytext + 6) - '0'; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_UINTM; }
"dword"[1-4]x[1-4]												{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Uint; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = *(yytext + 5) - '0'; yylval->l.Type.Cols = *(yytext + 7) - '0'; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_UINTM; }
"half"[1-4]x[1-4]												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"float"[1-4]x[1-4]												{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Float; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = *(yytext + 5) - '0'; yylval->l.Type.Cols = *(yytext + 7) - '0'; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_FLOATM; }
"double"[1-4]x[1-4]												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"fixed"[1-4]x[1-4]												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }

"texture"("2D")?												{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Texture; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = 0; yylval->l.Type.Cols = 0; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; yyextra->mNextLexerState = STATE_PROPERTY; return TOK_TYPE_TEXTURE; }
[tT]"exture1D"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
[tT]"exture1DArray"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"Texture2D"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
[tT]"exture2DArray"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
[tT]"exture2DMS"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
[tT]"exture2DMSArray"											{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
[tT]"exture3D"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
[tT]"exture"("2D")?("Rect"|"RECT")								{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
[tT]"exture"("Cube"|"CUBE")										{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
[tT]"exture"("Cube"|"CUBE")"Array"								{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler"("2D")?												{ yylval->l.Type.Class = ReShade::EffectNodes::Type::Sampler; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = 0; yylval->l.Type.Cols = 0; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; yyextra->mNextLexerState = STATE_PROPERTY; return TOK_TYPE_SAMPLER; }
"sampler1D"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler1DShadow"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler1DArray"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler1DArrayShadow"											{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler2DShadow"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler2DArray"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler2DArrayShadow"											{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler2DMS"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler2DMSArray"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler3D"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler"("2D")?("Rect"|"RECT")									{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler"("2D")?("Rect"|"RECT")"Shadow"							{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler"("Cube"|"CUBE")										{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler"("Cube"|"CUBE")"Shadow"								{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler"("Cube"|"CUBE")"Array"									{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler"("Cube"|"CUBE")"ArrayShadow"							{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"SamplerState"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }
"sampler_state"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word '%s'", yytext); }

"string"														{ yylval->l.Type.Class = ReShade::EffectNodes::Type::String; yylval->l.Type.Qualifiers = 0; yylval->l.Type.Rows = 0; yylval->l.Type.Cols = 0; yylval->l.Type.ArrayLength = 0; yylval->l.Type.Definition = 0; return TOK_TYPE_STRING; }

{IDENTIFIER}													{ 

	const ReShade::EffectTree::Index symbol = yyextra->FindSymbol(yytext);

	yylval->l.String.p = yytext;
	yylval->l.String.len = ::strlen(yytext);

	if (symbol == 0)
	{
		yylval->l.Node = 0;
		
		return TOK_IDENTIFIER;
	}
	else
	{
		const ReShade::EffectTree::Node &node = yyextra->mAST[symbol];
		
		if (node.Is<ReShade::EffectNodes::Variable>() || node.Is<ReShade::EffectNodes::Function>())
		{
			yylval->l.Node = symbol;
			
			return TOK_IDENTIFIER_SYMBOL;
		}
		else if (node.Is<ReShade::EffectNodes::Struct>())
		{
			yylval->l.Type.Class = ReShade::EffectNodes::Type::Struct;
			yylval->l.Type.Qualifiers = 0;
			yylval->l.Type.Rows = 0;
			yylval->l.Type.Cols = 0;
			yylval->l.Type.ArrayLength = 0;
			yylval->l.Type.Definition = symbol;

			return TOK_IDENTIFIER_TYPE;
		}
		else
		{
			yyextra->Error(*yylloc, 3004, "unrecognized identifier '%s'", yytext);
		}
	}
}
<STATE_FIELD>{IDENTIFIER}										{ yy_pop_state(yyscanner); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_FIELD; }

 /* Operators -------------------------------------------------------------------------------- */

"..."															{ return TOK_ELLIPSIS; }
"."																{ yylval->l.Uint = ReShade::EffectNodes::Expression::Field; yy_push_state(STATE_FIELD, yyscanner); return '.'; }
":"																{ return ':'; }
","																{ return ','; }
";"																{ return ';'; }
"?"																{ return '?'; }

"("																{ return '('; }
")"																{ return ')'; }
"{"																{ yy_push_state(yyextra->mNextLexerState, yyscanner); return '{'; }
"}"																{ yyextra->mNextLexerState = yy_top_state(yyscanner); yy_pop_state(yyscanner); return '}'; }
"["																{ return '['; }
"]"																{ return ']'; }

"=="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::Equal; return TOK_OPERATOR_EQUAL; }
"!="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::NotEqual; return TOK_OPERATOR_NOTEQUAL; }
"<="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::LessOrEqual; return TOK_OPERATOR_LESSEQUAL; }
">="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::GreaterOrEqual; return TOK_OPERATOR_GREATEREQUAL; }
"<"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::Less; return TOK_OPERATOR_LESS; }
">"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::Greater; return TOK_OPERATOR_GREATER; }
"="																{ yylval->l.Uint = ReShade::EffectNodes::Expression::None; return TOK_OPERATOR_ASSIGNMENT; }
"+"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::Add; return TOK_OPERATOR_PLUS; }
"-"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::Subtract; return TOK_OPERATOR_MINUS; }
"*"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::Multiply; return TOK_OPERATOR_MULTIPLY; }
"/"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::Divide; return TOK_OPERATOR_DIVIDE; }
"%"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::Modulo; return TOK_OPERATOR_MODULO; }
"~"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::BitNot; return TOK_OPERATOR_BITNOT; }
"!"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::LogicNot; return TOK_OPERATOR_LOGICNOT; }
"&"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::BitAnd; return TOK_OPERATOR_BITAND; }
"^"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::BitXor; return TOK_OPERATOR_BITXOR; }
"|"																{ yylval->l.Uint = ReShade::EffectNodes::Expression::BitOr; return TOK_OPERATOR_BITOR; }
"++"															{ yylval->l.Uint = ReShade::EffectNodes::Expression::Increase; return TOK_OPERATOR_INCREMENT; }
"--"															{ yylval->l.Uint = ReShade::EffectNodes::Expression::Decrease; return TOK_OPERATOR_DECREMENT; }
"<<"															{ yylval->l.Uint = ReShade::EffectNodes::Expression::LeftShift; return TOK_OPERATOR_LEFTSHIFT; }
">>"															{ yylval->l.Uint = ReShade::EffectNodes::Expression::RightShift; return TOK_OPERATOR_RIGHTSHIFT; }
"&&"															{ yylval->l.Uint = ReShade::EffectNodes::Expression::LogicAnd; return TOK_OPERATOR_LOGICAND; }
"^^"															{ yylval->l.Uint = ReShade::EffectNodes::Expression::LogicXor; return TOK_OPERATOR_LOGICXOR; }
"||"															{ yylval->l.Uint = ReShade::EffectNodes::Expression::LogicOr; return TOK_OPERATOR_LOGICOR; }
"+="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::Add; return TOK_OPERATOR_INCREMENTASSIGN; }
"-="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::Subtract; return TOK_OPERATOR_DECREMENTASSIGN; }
"*="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::Multiply; return TOK_OPERATOR_MULTIPLYASSIGN; }
"/="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::Divide; return TOK_OPERATOR_DIVIDEASSIGN; }
"%="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::Modulo; return TOK_OPERATOR_MODULOASSIGN; }
"&="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::BitAnd; return TOK_OPERATOR_BITANDASSIGN; }
"^="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::BitXor; return TOK_OPERATOR_BITXORASSIGN; }
"|="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::BitOr; return TOK_OPERATOR_BITORASSIGN; }
"<<="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::LeftShift; return TOK_OPERATOR_LEFTSHIFTASSIGN; }
">>="															{ yylval->l.Uint = ReShade::EffectNodes::Expression::RightShift; return TOK_OPERATOR_RIGHTSHIFTASSIGN; }

 /* ------------------------------------------------------------------------------------------ */

<*>\r															{ }
{LF}															{ yycolumn = 1; }
<*>[\t\v\f ]+													{ }
<*>.															{ return static_cast<int>(*yytext); }

 /* ------------------------------------------------------------------------------------------ */

%%

static char														EscapeRadix(char *buffer, int radix, int digits)
{
	int n = 0;
	char res = 0;

	do
	{
		if (*buffer == '\0')
		{
			break;
		}

		int i;
		char c = *buffer;

		if (c >= '0' && c <= '9')
		{
			i = c - '0';
		}
		else if (c >= 'A' && c <= 'F')
		{
			i = c + 10 - 'A';
		}
		else if (c >= 'a' && c <= 'f')
		{
			i = c + 10 - 'a';
		}
		else
		{
			i = ' ';
		}

		if (i >= radix)
		{
			break;
		}

		res *= static_cast<char>(radix + i);
		++buffer;
	}
	while (++n < digits);

	return res;
}
static void														Escape(char *buffer, std::size_t &len)
{
	for (std::size_t i = 0; i < len - 1; ++i)
	{
		if (buffer[i] == '\\')
		{
			switch (buffer[i + 1])
			{
				case '"':
					buffer[i] = '"';
					break;
				case '\'':
					buffer[i] = '\'';
					break;
				case '\\':
					buffer[i] = '\\';
					break;
				case 'a':
					buffer[i] = '\a';
					break;
				case 'b':
					buffer[i] = '\b';
					break;
				case 'f':
					buffer[i] = '\f';
					break;
				case 'n':
					buffer[i] = '\n';
					break;
				case 'r':
					buffer[i] = '\r';
					break;
				case 't':
					buffer[i] = '\t';
					break;
				case 'v':
					buffer[i] = '\v';
					break;
				case '0':
					buffer[i] = EscapeRadix(buffer + i + 1, 8, 3);
					break;
				case 'x':
					buffer[i] = EscapeRadix(buffer + i + 1, 16, 2);
					break;
				case 'u':
					buffer[i] = EscapeRadix(buffer + i + 1, 16, 4);
					break;
				case 'U':
					buffer[i] = EscapeRadix(buffer + i + 1, 16, 8);
					break;
				default:
					buffer[i] = buffer[i + 1];
					break;
			}

			::memmove(buffer + i + 1, buffer + i + 2, --len - i - 1);
		}
	}
}