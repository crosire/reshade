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
	#include "EffectParser.hpp"

	static void													Escape(char *buffer, size_t &len);
%}

/* ----------------------------------------------------------------------------------------------
|  States																						|
---------------------------------------------------------------------------------------------- */

%option stack

%x STATE_COMMENT STATE_PP

%x STATE_FIELD
%s STATE_INITIALIZER STATE_TECHNIQUE STATE_PASS

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
	\n															{ }
}

^[ \t]*#[ \t]*													{ yy_push_state(STATE_PP, yyscanner); }
<STATE_PP>
{
	"line"{WS}+{INTEGRAL}({WS}+{STRING})?{WS}*					{
		
		char *begin = yytext + 4, *end;

		while (!::isdigit(*++begin)) continue;
			
		yylineno = static_cast<int>(::strtol(begin, &end, 10));

		if (yylineno == 0)
		{
			yyextra->Warning(*yylloc, 3000, "line numbers should be greater than zero");

			yylineno = 1;
		}

		yylineno -= 1;

		while (*end != '\0' && *++end != '"') continue;
		begin = end;
		while (*end != '\0' && *++end != '"') continue;

		const size_t len = end - begin;

		if (len != 0)
		{
			yylloc->Source = yyextra->CreateString(begin + 1, len - 1);
		}
	}
	"pragma"{WS}+.*												{

		const char *begin = yytext + 7;
		const char *end = yytext + ::strlen(yytext) - 1;

		while ((*begin == ' ' || *begin == '\t') && *begin != '\0') ++begin;
		while (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t') --end;

		yyextra->Warning(*yylloc, 3568, "'%.*s': unknown pragma ignored", end - begin + 1, begin);
	}
	"error".*													{

		const char *begin = yytext + 5;
		const char *end = yytext + ::strlen(yytext) - 1;

		while ((*begin == ' ' || *begin == '\t') && *begin != '\0') ++begin;
		while (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t') --end;

		yyextra->Error(*yylloc, 0, "%.*s", end - begin + 1, begin);
	}
	"warning".*													{

		const char *begin = yytext + 5;
		const char *end = yytext + ::strlen(yytext) - 1;

		while ((*begin == ' ' || *begin == '\t') && *begin != '\0') ++begin;
		while (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t') --end;

		yyextra->Warning(*yylloc, 0, "%.*s", end - begin + 1, begin);
	}
	[^\n\\]+													{ yyextra->Error(*yylloc, 1504, "invalid preprocessor command '%s'", yytext); }
	{LF}														{ yycolumn = 1; yy_pop_state(yyscanner); }
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

"template"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'template'"); }
"new"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'new'"); }
"try"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'try'"); }
"catch"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'catch'"); }
"operator"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'operator'"); }
"cast"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'cast'"); }
"static_cast"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'static_cast'"); }
"dynamic_cast"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'dynamic_cast'"); }
"reinterpret_cast"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'reinterpret_cast'"); }
"private"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'private'"); }
"protected"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'protected'"); }
"public"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'public'"); }
"friend"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'friend'"); }
"explicit"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'explicit'"); }
"virtual"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'virtual'"); }
"external"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'external'"); }

"technique"														{ yyextra->mNextLexerState = STATE_TECHNIQUE; return TOK_TECHNIQUE; }
<STATE_TECHNIQUE>"pass"											{ yyextra->mNextLexerState = STATE_PASS; return TOK_PASS; }
"compile"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'compile'"); }
"asm"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'asm'"); }
"asm_fragment"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'asm_fragment'"); }
"register"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'register'"); }
"packoffset"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'packoffset'"); }

"class"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'class'"); }
"struct"														{ return TOK_STRUCT; }
"union"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'union'"); }
"enum"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'enum'"); }
"interface"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'interface'"); }
"this"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'this'"); }
"typedef"														{ return TOK_TYPEDEF; }
"using"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'using'"); }
"sizeof"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sizeof'"); }

"for"															{ return TOK_FOR; }
"foreach"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'foreach'"); }
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
"goto"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'goto'");  }

"uniform"														{ return TOK_UNIFORM; }
"static"														{ return TOK_STATIC; }
"extern"														{ return TOK_EXTERN; }
"export"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'export'"); }
"shared"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'shared'"); }
"groupshared"													{ return TOK_GROUPSHARED; }
"globallycoherent"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'globallycoherent'"); }
"packed"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'packed'"); }
"inline"														{ return TOK_INLINE; }
"noinline"														{ }
"const"															{ return TOK_CONST; }
"mutable"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'mutable'"); }
"volatile"														{ return TOK_VOLATILE; }
"precise"														{ return TOK_PRECISE; }
"row_major"														{ return TOK_ROWMAJOR; }
"column_major"													{ return TOK_COLUMNMAJOR; }
"snorm"															{ return TOK_SNORM; }
"unorm"															{ return TOK_UNORM; }
"signed"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'signed'"); }
"unsigned"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'unsigned'"); }
"in"															{ return TOK_IN; }
"out"															{ return TOK_OUT; }
"inout"															{ return TOK_INOUT; }

"nointerpolation"												{ return TOK_NOINTERPOLATION; }
"noperspective"													{ return TOK_NOPERSPECTIVE; }
"linear"														{ return TOK_LINEAR; }
"centroid"														{ return TOK_CENTROID; }
"sample"														{ return TOK_SAMPLE; }

<STATE_INITIALIZER>"MinFilter"									{ yylval->l.Int = ReShade::Nodes::State::MinFilter; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"MagFilter"									{ yylval->l.Int = ReShade::Nodes::State::MagFilter; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"MipFilter"									{ yylval->l.Int = ReShade::Nodes::State::MipFilter; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"AddressU"									{ yylval->l.Int = ReShade::Nodes::State::AddressU; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"AddressV"									{ yylval->l.Int = ReShade::Nodes::State::AddressV; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"AddressW"									{ yylval->l.Int = ReShade::Nodes::State::AddressW; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"MipLODBias"									{ yylval->l.Int = ReShade::Nodes::State::MipLODBias; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"MaxAnisotropy"								{ yylval->l.Int = ReShade::Nodes::State::MaxAnisotropy; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"MinLOD"										{ yylval->l.Int = ReShade::Nodes::State::MinLOD; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"MaxLOD"|"MaxMipLevel"						{ yylval->l.Int = ReShade::Nodes::State::MaxLOD; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>[sS]"RGB"("Texture")?|"SRGBReadEnable"		{ yylval->l.Int = ReShade::Nodes::State::SRGB; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"Texture"									{ yylval->l.Int = ReShade::Nodes::State::Texture; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"Width"										{ yylval->l.Int = ReShade::Nodes::State::Width; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"Height"										{ yylval->l.Int = ReShade::Nodes::State::Height; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"MipLevels"									{ yylval->l.Int = ReShade::Nodes::State::MipLevels; return TOK_IDENTIFIER_STATE; }
<STATE_INITIALIZER>"Format"										{ yylval->l.Int = ReShade::Nodes::State::Format; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"VertexShader"										{ yylval->l.Int = ReShade::Nodes::State::VertexShader; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"HullShader"										{ yylval->l.Int = ReShade::Nodes::State::HullShader; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"DomainShader"										{ yylval->l.Int = ReShade::Nodes::State::DomainShader; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"GeometryShader"									{ yylval->l.Int = ReShade::Nodes::State::GeometryShader; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"PixelShader"										{ yylval->l.Int = ReShade::Nodes::State::PixelShader; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"ComputeShader"										{ yylval->l.Int = ReShade::Nodes::State::ComputeShader; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"RenderTarget"										{ yylval->l.Int = ReShade::Nodes::State::RenderTarget0; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"RenderTarget"[0-7]									{ yylval->l.Int = ReShade::Nodes::State::RenderTarget0 + *(yytext + 12) - '0'; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"RenderTarget"{WS}*"["{WS}*[0-7]{WS}*"]"			{ char *p = yytext + 12; while (!::isdigit(*++p)); yylval->l.Int = ReShade::Nodes::State::RenderTarget0 + *p - '0'; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"RenderTargetWriteMask"|"ColorWriteMask"			{ yylval->l.Int = ReShade::Nodes::State::RenderTargetWriteMask; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"CullMode"											{ yylval->l.Int = ReShade::Nodes::State::CullMode; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"FillMode"											{ yylval->l.Int = ReShade::Nodes::State::FillMode; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"ScissorEnable"										{ yylval->l.Int = ReShade::Nodes::State::ScissorEnable; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"AlphaToCoverageEnable"								{ yylval->l.Int = ReShade::Nodes::State::AlphaToCoverageEnable; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"AlphaBlendEnable"|"BlendEnable"					{ yylval->l.Int = ReShade::Nodes::State::BlendEnable; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"SrcBlend"											{ yylval->l.Int = ReShade::Nodes::State::SrcBlend; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"DestBlend"											{ yylval->l.Int = ReShade::Nodes::State::DestBlend; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"BlendOp"											{ yylval->l.Int = ReShade::Nodes::State::BlendOp; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"BlendOpAlpha"										{ yylval->l.Int = ReShade::Nodes::State::BlendOpAlpha; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"DepthEnable"|"ZEnable"								{ yylval->l.Int = ReShade::Nodes::State::DepthEnable; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"DepthWriteMask"|"ZWriteMask"						{ yylval->l.Int = ReShade::Nodes::State::DepthWriteMask; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"DepthFunc"|"DepthFunction"|"ZFunc"|"ZFunction"		{ yylval->l.Int = ReShade::Nodes::State::DepthFunc; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"StencilEnable"										{ yylval->l.Int = ReShade::Nodes::State::StencilEnable; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"StencilReadMask"|"StencilMask"						{ yylval->l.Int = ReShade::Nodes::State::StencilReadMask; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"StencilWriteMask"									{ yylval->l.Int = ReShade::Nodes::State::StencilWriteMask; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"StencilRef"|"StencilReference"						{ yylval->l.Int = ReShade::Nodes::State::StencilRef; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"StencilFunc"|"StencilFunction"						{ yylval->l.Int = ReShade::Nodes::State::StencilFunc; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"StencilPass"("Op")?								{ yylval->l.Int = ReShade::Nodes::State::StencilPass; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"StencilFail"("Op")?								{ yylval->l.Int = ReShade::Nodes::State::StencilFail; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>("StencilDepthFail"|"StencilZFail")("Op")?			{ yylval->l.Int = ReShade::Nodes::State::StencilZFail; return TOK_IDENTIFIER_STATE; }
<STATE_PASS>"SRGBWriteEnable"									{ yylval->l.Int = ReShade::Nodes::State::SRGBWriteEnable; return TOK_IDENTIFIER_STATE; }

 /* Literals --------------------------------------------------------------------------------- */

[tT]"rue"|"TRUE"												{ yylval->l.Int = true; return TOK_LITERAL_BOOL; }
[fF]"alse"|"FALSE"												{ yylval->l.Int = false; return TOK_LITERAL_BOOL; }
{STRING}														{ yylval->l.String.len = ::strlen(yytext) - 2; char *p = yytext + 1; Escape(p, yylval->l.String.len); yylval->l.String.p = p; return TOK_LITERAL_STRING; }
[0-9]+"."[0-9]*([eE][+-]?[0-9]+)?[hHfFlL]?						{ yylval->l.Double = ::strtod(yytext, nullptr); if (::tolower(yytext[::strlen(yytext) - 1]) == 'l') { return TOK_LITERAL_DOUBLE; } else { yylval->l.Float = static_cast<float>(yylval->l.Double); return TOK_LITERAL_FLOAT; } }
"."[0-9]+([eE][+-]?[0-9]+)?[hHfFlL]?							{ yylval->l.Double = ::strtod(yytext, nullptr); if (::tolower(yytext[::strlen(yytext) - 1]) == 'l') { return TOK_LITERAL_DOUBLE; } else { yylval->l.Float = static_cast<float>(yylval->l.Double); return TOK_LITERAL_FLOAT; } }
[0-9]+[eE][+-]?[0-9]+[hHfFlL]?									{ yylval->l.Double = ::strtod(yytext, nullptr); if (::tolower(yytext[::strlen(yytext) - 1]) == 'l') { return TOK_LITERAL_DOUBLE; } else { yylval->l.Float = static_cast<float>(yylval->l.Double); return TOK_LITERAL_FLOAT; } }
[0-9]+[hHfFlL]													{ yylval->l.Double = ::strtod(yytext, nullptr); if (::tolower(yytext[::strlen(yytext) - 1]) == 'l') { return TOK_LITERAL_DOUBLE; } else { yylval->l.Float = static_cast<float>(yylval->l.Double); return TOK_LITERAL_FLOAT; } }
1".#INF"														{ yylval->l.Float = (yytext[0] == '-') ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity(); return TOK_LITERAL_FLOAT; }
1".#IND"														{ yylval->l.Float = std::numeric_limits<float>::quiet_NaN(); return TOK_LITERAL_FLOAT; }
1".#QNAN"														{ yylval->l.Float = std::numeric_limits<float>::quiet_NaN(); return TOK_LITERAL_FLOAT; }
1".#SNAN"														{ yylval->l.Float = std::numeric_limits<float>::signaling_NaN(); return TOK_LITERAL_FLOAT; }
0[xX][a-fA-F0-9]+[uUlL]?										{ if (::tolower(yytext[::strlen(yytext) - 1]) == 'u') { yylval->l.Uint = static_cast<unsigned int>(::strtoul(yytext, nullptr, 16)); return TOK_LITERAL_INT; } else { yylval->l.Int = static_cast<int>(::strtol(yytext, nullptr, 16)); return TOK_LITERAL_INT; } }
0[0-7]+[uUlL]?													{ if (::tolower(yytext[::strlen(yytext) - 1]) == 'u') { yylval->l.Uint = static_cast<unsigned int>(::strtoul(yytext, nullptr,  8)); return TOK_LITERAL_INT; } else { yylval->l.Int = static_cast<int>(::strtol(yytext, nullptr,  8)); return TOK_LITERAL_INT; } }
{INTEGRAL}[uUlL]?												{ if (::tolower(yytext[::strlen(yytext) - 1]) == 'u') { yylval->l.Uint = static_cast<unsigned int>(::strtoul(yytext, nullptr, 10)); return TOK_LITERAL_INT; } else { yylval->l.Int = static_cast<int>(::strtol(yytext, nullptr, 10)); return TOK_LITERAL_INT; } }
"null"|"NULL"													{ yylval->l.Uint = 0; return TOK_LITERAL_INT; }

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
(?i:"SV_ClipDistance"){INTEGRAL}?								{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_CullDistance"){INTEGRAL}?								{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_Coverage")												{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_DispatchThreadID")										{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_DomainLocation")										{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_GroupID")												{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_GroupIndex")											{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_GroupThreadID")											{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_GSInstanceID")											{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_InsideTessFactor")										{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_IsFrontFace")											{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_OutputControlPointID")									{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_Position")												{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_RenderTargetArrayIndex")								{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_SampleIndex")											{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_Target"){INTEGRAL}?										{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_TessFactor")											{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_ViewportArrayIndex")									{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_InstanceID")											{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_PrimitiveID")											{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_VertexID")												{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }
(?i:"SV_Depth")													{ char *p = yytext; while (*p++ = static_cast<char>(::toupper(*p))); yylval->l.String.p = yytext; yylval->l.String.len = ::strlen(yytext); return TOK_IDENTIFIER_SEMANTIC; }

<STATE_INITIALIZER>"NONE"										{ yylval->l.Int = ReShade::Nodes::Literal::NONE; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"POINT"|"NEAREST"							{ yylval->l.Int = ReShade::Nodes::Literal::POINT; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"LINEAR"										{ yylval->l.Int = ReShade::Nodes::Literal::LINEAR; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"ANISOTROPIC"								{ yylval->l.Int = ReShade::Nodes::Literal::ANISOTROPIC; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"CLAMP"										{ yylval->l.Int = ReShade::Nodes::Literal::CLAMP; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"MIRROR"										{ yylval->l.Int = ReShade::Nodes::Literal::MIRROR; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"REPEAT"|"WRAP"								{ yylval->l.Int = ReShade::Nodes::Literal::REPEAT; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"BORDER"										{ yylval->l.Int = ReShade::Nodes::Literal::BORDER; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"R8"											{ yylval->l.Int = ReShade::Nodes::Literal::R8; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"R32F"										{ yylval->l.Int = ReShade::Nodes::Literal::R32F; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"RG8"|"R8G8"									{ yylval->l.Int = ReShade::Nodes::Literal::RG8; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"RGBA8"|"R8G8B8A8"							{ yylval->l.Int = ReShade::Nodes::Literal::RGBA8; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"RGBA16"|"R16G16B16A16"						{ yylval->l.Int = ReShade::Nodes::Literal::RGBA16; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"RGBA16F"|"R16G16B16A16F"					{ yylval->l.Int = ReShade::Nodes::Literal::RGBA16F; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"RGBA32F"|"R32G32B32A32F"					{ yylval->l.Int = ReShade::Nodes::Literal::RGBA32F; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"DXT1"|"BC1"									{ yylval->l.Int = ReShade::Nodes::Literal::DXT1; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"DXT3"|"BC2"									{ yylval->l.Int = ReShade::Nodes::Literal::DXT3; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"DXT5"|"BC3"									{ yylval->l.Int = ReShade::Nodes::Literal::DXT5; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"LATC1"|"BC4"								{ yylval->l.Int = ReShade::Nodes::Literal::LATC1; return TOK_LITERAL_ENUM; }
<STATE_INITIALIZER>"LATC2"|"BC5"								{ yylval->l.Int = ReShade::Nodes::Literal::LATC2; return TOK_LITERAL_ENUM; }
<STATE_PASS>"NONE"												{ yylval->l.Int = ReShade::Nodes::Literal::NONE; return TOK_LITERAL_ENUM; }
<STATE_PASS>"BACK"												{ yylval->l.Int = ReShade::Nodes::Literal::BACK; return TOK_LITERAL_ENUM; }
<STATE_PASS>"FRONT"												{ yylval->l.Int = ReShade::Nodes::Literal::FRONT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"POINT"												{ yylval->l.Int = ReShade::Nodes::Literal::POINT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"WIREFRAME"											{ yylval->l.Int = ReShade::Nodes::Literal::WIREFRAME; return TOK_LITERAL_ENUM; }
<STATE_PASS>"SOLID"												{ yylval->l.Int = ReShade::Nodes::Literal::SOLID; return TOK_LITERAL_ENUM; }
<STATE_PASS>"CCW"												{ yylval->l.Int = ReShade::Nodes::Literal::CCW; return TOK_LITERAL_ENUM; }
<STATE_PASS>"CW"												{ yylval->l.Int = ReShade::Nodes::Literal::CW; return TOK_LITERAL_ENUM; }
<STATE_PASS>"ONE"												{ yylval->l.Int = ReShade::Nodes::Literal::ONE; return TOK_LITERAL_ENUM; }
<STATE_PASS>"ZERO"												{ yylval->l.Int = ReShade::Nodes::Literal::ZERO; return TOK_LITERAL_ENUM; }
<STATE_PASS>"SRCCOLOR"											{ yylval->l.Int = ReShade::Nodes::Literal::SRCCOLOR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"SRCALPHA"											{ yylval->l.Int = ReShade::Nodes::Literal::SRCALPHA; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INVSRCCOLOR"										{ yylval->l.Int = ReShade::Nodes::Literal::INVSRCCOLOR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INVSRCALPHA"										{ yylval->l.Int = ReShade::Nodes::Literal::INVSRCALPHA; return TOK_LITERAL_ENUM; }
<STATE_PASS>"DESTCOLOR"											{ yylval->l.Int = ReShade::Nodes::Literal::DESTCOLOR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"DESTALPHA"											{ yylval->l.Int = ReShade::Nodes::Literal::DESTALPHA; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INVDESTCOLOR"										{ yylval->l.Int = ReShade::Nodes::Literal::INVDESTCOLOR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INVDESTALPHA"										{ yylval->l.Int = ReShade::Nodes::Literal::INVDESTALPHA; return TOK_LITERAL_ENUM; }
<STATE_PASS>"ADD"												{ yylval->l.Int = ReShade::Nodes::Literal::ADD; return TOK_LITERAL_ENUM; }
<STATE_PASS>"SUBSTRACT"											{ yylval->l.Int = ReShade::Nodes::Literal::SUBSTRACT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"REVSUBSTRACT"										{ yylval->l.Int = ReShade::Nodes::Literal::REVSUBSTRACT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"MIN"												{ yylval->l.Int = ReShade::Nodes::Literal::MIN; return TOK_LITERAL_ENUM; }
<STATE_PASS>"MAX"												{ yylval->l.Int = ReShade::Nodes::Literal::MAX; return TOK_LITERAL_ENUM; }
<STATE_PASS>"KEEP"												{ yylval->l.Int = ReShade::Nodes::Literal::KEEP; return TOK_LITERAL_ENUM; }
<STATE_PASS>"REPLACE"											{ yylval->l.Int = ReShade::Nodes::Literal::REPLACE; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INVERT"											{ yylval->l.Int = ReShade::Nodes::Literal::INVERT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INCR"												{ yylval->l.Int = ReShade::Nodes::Literal::INCR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"INCRSAT"											{ yylval->l.Int = ReShade::Nodes::Literal::INCRSAT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"DECR"												{ yylval->l.Int = ReShade::Nodes::Literal::DECR; return TOK_LITERAL_ENUM; }
<STATE_PASS>"DECRSAT"											{ yylval->l.Int = ReShade::Nodes::Literal::DECRSAT; return TOK_LITERAL_ENUM; }
<STATE_PASS>"NEVER"												{ yylval->l.Int = ReShade::Nodes::Literal::NEVER; return TOK_LITERAL_ENUM; }
<STATE_PASS>"ALWAYS"											{ yylval->l.Int = ReShade::Nodes::Literal::ALWAYS; return TOK_LITERAL_ENUM; }
<STATE_PASS>"LESS"												{ yylval->l.Int = ReShade::Nodes::Literal::LESS; return TOK_LITERAL_ENUM; }
<STATE_PASS>"GREATER"											{ yylval->l.Int = ReShade::Nodes::Literal::GREATER; return TOK_LITERAL_ENUM; }
<STATE_PASS>"LESSEQUAL"|"LEQUAL"								{ yylval->l.Int = ReShade::Nodes::Literal::LESSEQUAL; return TOK_LITERAL_ENUM; }
<STATE_PASS>"GREATEREQUAL"|"GEQUAL"								{ yylval->l.Int = ReShade::Nodes::Literal::GREATEREQUAL; return TOK_LITERAL_ENUM; }
<STATE_PASS>"EQUAL"												{ yylval->l.Int = ReShade::Nodes::Literal::EQUAL; return TOK_LITERAL_ENUM; }
<STATE_PASS>"NOTEQUAL"|"NEQUAL"									{ yylval->l.Int = ReShade::Nodes::Literal::NOTEQUAL; return TOK_LITERAL_ENUM; }
<STATE_PASS>"RED"												{ yylval->l.Int = 1 << 0; return TOK_LITERAL_INT; }
<STATE_PASS>"GREEN"												{ yylval->l.Int = 1 << 1; return TOK_LITERAL_INT; }
<STATE_PASS>"BLUE"												{ yylval->l.Int = 1 << 2; return TOK_LITERAL_INT; }
<STATE_PASS>"ALPHA"												{ yylval->l.Int = 1 << 3; return TOK_LITERAL_INT; }

 /* Types ------------------------------------------------------------------------------------ */

"vector"														{ return TOK_TYPE_VECTOR; }
"bool"[1-4]														{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Bool; yylval->l.Type.Rows = *(yytext + 4) - '0'; yylval->l.Type.Cols = 1; return TOK_TYPE_BOOLV; }
"int"[1-4]														{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Int; yylval->l.Type.Rows = *(yytext + 3) - '0'; yylval->l.Type.Cols = 1; return TOK_TYPE_INTV; }
("uint"|"dword")[1-4]											{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Uint; yylval->l.Type.Rows = *(yytext + 4) - '0'; yylval->l.Type.Cols = 1; return TOK_TYPE_UINTV; }
"half"[1-4]														{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Half; yylval->l.Type.Rows = *(yytext + 4) - '0'; yylval->l.Type.Cols = 1; return TOK_TYPE_HALFV; }
"float"[1-4]													{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Float; yylval->l.Type.Rows = *(yytext + 5) - '0'; yylval->l.Type.Cols = 1; return TOK_TYPE_FLOATV; }
"double"[1-4]													{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Double; yylval->l.Type.Rows = *(yytext + 6) - '0'; yylval->l.Type.Cols = 1; return TOK_TYPE_DOUBLEV; }

"matrix"														{ return TOK_TYPE_MATRIX; }
"bool"[1-4]x[1-4]												{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Bool; yylval->l.Type.Rows = *(yytext + 4) - '0'; yylval->l.Type.Cols = *(yytext + 6) - '0'; return TOK_TYPE_BOOLM; }
"int"[1-4]x[1-4]												{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Int; yylval->l.Type.Rows = *(yytext + 3) - '0'; yylval->l.Type.Cols = *(yytext + 5) - '0'; return TOK_TYPE_INTM; }
("uint"|"dword")[1-4]x[1-4]										{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Uint; yylval->l.Type.Rows = *(yytext + 4) - '0'; yylval->l.Type.Cols = *(yytext + 6) - '0'; return TOK_TYPE_UINTM; }
"half"[1-4]x[1-4]												{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Half; yylval->l.Type.Rows = *(yytext + 4) - '0'; yylval->l.Type.Cols = *(yytext + 6) - '0'; return TOK_TYPE_HALFM; }
"float"[1-4]x[1-4]												{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Float; yylval->l.Type.Rows = *(yytext + 5) - '0'; yylval->l.Type.Cols = *(yytext + 7) - '0'; return TOK_TYPE_FLOATM; }
"double"[1-4]x[1-4]												{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Double; yylval->l.Type.Rows = *(yytext + 6) - '0'; yylval->l.Type.Cols = *(yytext + 8) - '0'; return TOK_TYPE_DOUBLEM; }

"auto"															{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'auto'"); }
"void"															{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Void; return TOK_TYPE_VOID; }
"bool"															{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Bool; yylval->l.Type.Rows = 1; yylval->l.Type.Cols = 1; return TOK_TYPE_BOOL; }
"int"															{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Int; yylval->l.Type.Rows = 1; yylval->l.Type.Cols = 1; return TOK_TYPE_INT; }
"uint"|"dword"													{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Uint; yylval->l.Type.Rows = 1; yylval->l.Type.Cols = 1; return TOK_TYPE_UINT; }
"half"															{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Half; yylval->l.Type.Rows = 1; yylval->l.Type.Cols = 1; return TOK_TYPE_HALF; }
"float"															{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Float; yylval->l.Type.Rows = 1; yylval->l.Type.Cols = 1; return TOK_TYPE_FLOAT; }
"double"														{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Double; yylval->l.Type.Rows = 1; yylval->l.Type.Cols = 1; return TOK_TYPE_DOUBLE; }

"texture"|"texture2D"											{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Texture; yyextra->mNextLexerState = STATE_INITIALIZER; yyextra->mLexerStateAnnotation = 0; return TOK_TYPE_TEXTURE; }
[tT]"exture1D"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'Texture1D'"); }
[tT]"exture1DArray"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'Texture1DArray'"); }
"Texture2D"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'Texture2D'"); }
[tT]"exture2DArray"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'Texture2DArray'"); }
[tT]"exture2DMS"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'Texture2DMS'"); }
[tT]"exture2DMSArray"											{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'Texture2DMSArray'"); }
[tT]"exture3D"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'Texture3D'"); }
[tT]"exture"("2D")?("Rect"|"RECT")								{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'TextureRect'"); }
[tT]"exture"("Cube"|"CUBE")										{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'TextureCube'"); }
[tT]"exture"("Cube"|"CUBE")"Array"								{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'TextureCubeArray'"); }
"sampler"|"sampler2D"											{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::Sampler; yyextra->mNextLexerState = STATE_INITIALIZER; yyextra->mLexerStateAnnotation = 0; return TOK_TYPE_SAMPLER; }
"sampler1D"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler1D'"); }
"sampler1DShadow"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler1DShadow'"); }
"sampler1DArray"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler1DArray'"); }
"sampler1DArrayShadow"											{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler1DArrayShadow'"); }
"sampler2DShadow"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler2DShadow'"); }
"sampler2DArray"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler2DArray'"); }
"sampler2DArrayShadow"											{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler2DArrayShadow'"); }
"sampler2DMS"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler2DMS'"); }
"sampler2DMSArray"												{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler2DMSArray'"); }
"sampler3D"														{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler3D'"); }
"sampler"("2D")?("Rect"|"RECT")									{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'samplerRECT'"); }
"sampler"("2D")?("Rect"|"RECT")"Shadow"							{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'samplerRECTShadow'"); }
"sampler"("Cube"|"CUBE")										{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'samplerCUBE'"); }
"sampler"("Cube"|"CUBE")"Shadow"								{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'samplerCUBEShadow'"); }
"sampler"("Cube"|"CUBE")"Array"									{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'samplerCUBEArray'"); }
"sampler"("Cube"|"CUBE")"ArrayShadow"							{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'samplerCUBEArrayShadow'"); }
"SamplerState"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'SamplerState'"); }
"sampler_state"													{ yyextra->Error(*yylloc, 3000, "syntax error: unexpected reserved word 'sampler_state'"); }

"string"														{ yylval->l.Type = ReShade::Nodes::Type::Undefined; yylval->l.Type.Class = ReShade::Nodes::Type::String; return TOK_TYPE_STRING; }

{IDENTIFIER}													{ 

	yylval->l.String.p = yytext;
	yylval->l.String.len = ::strlen(yytext);

	ReShade::EffectTree::Index symbol = yyextra->GetSymbol(yytext, 0);

	if (symbol == 0)
	{
		yylval->l.Node = 0;
		
		return TOK_IDENTIFIER;
	}
	else
	{
		ReShade::EffectTree::Node &node = (*yyextra->mParserTree)[symbol];
		
		if (node.Is<ReShade::Nodes::Variable>() || node.Is<ReShade::Nodes::Function>())
		{
			yylval->l.Node = symbol;
			
			return TOK_IDENTIFIER_SYMBOL;
		}
		else if (node.Is<ReShade::Nodes::Struct>())
		{
			yylval->l.Type = ReShade::Nodes::Type::Undefined;
			yylval->l.Type.Class = ReShade::Nodes::Type::Struct;
			yylval->l.Type.Definition = symbol;

			return TOK_IDENTIFIER_TYPE;
		}
		else if (node.Is<ReShade::Nodes::Typedef>())
		{
			yylval->l.Type = node.As<ReShade::Nodes::Typedef>().Type;

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
"."																{ yy_push_state(STATE_FIELD, yyscanner); return '.'; }
":"																{ return ':'; }
","																{ return ','; }
";"																{ if (yyextra->mNextLexerState == STATE_INITIALIZER && !yyextra->mLexerStateAnnotation) yyextra->mNextLexerState = YYSTATE; return ';'; }
"?"																{ return '?'; }

"("																{ return '('; }
")"																{ return ')'; }
"{"																{ yy_push_state(yyextra->mNextLexerState, yyscanner); return '{'; }
"}"																{ yyextra->mNextLexerState = yy_top_state(yyscanner); yy_pop_state(yyscanner); return '}'; }
"<%"															{ return '{'; }
"%>"															{ return '}'; }
"["|"<:"														{ return '['; }
"]"|":>"														{ return ']'; }

"=="															{ return TOK_OPERATOR_EQUAL; }
"!="															{ return TOK_OPERATOR_NOTEQUAL; }
">="															{ return TOK_OPERATOR_GREATEREQUAL; }
"<="															{ return TOK_OPERATOR_LESSEQUAL; }
">"																{ yyextra->mLexerStateAnnotation = 0; return TOK_OPERATOR_GREATER; }
"<"																{ yyextra->mLexerStateAnnotation = 1; return TOK_OPERATOR_LESS; }
"!"																{ return TOK_OPERATOR_LOGNOT; }
"~"																{ return TOK_OPERATOR_BITNOT; }
"&&"															{ return TOK_OPERATOR_LOGAND; }
"^^"															{ return TOK_OPERATOR_LOGXOR; }
"||"															{ return TOK_OPERATOR_LOGOR; }
"+="															{ return TOK_OPERATOR_INCREMENTASSIGN; }
"-="															{ return TOK_OPERATOR_DECREMENTASSIGN; }
"*="															{ return TOK_OPERATOR_MULTIPLYASSIGN; }
"/="															{ return TOK_OPERATOR_DIVIDEASSIGN; }
"%="															{ return TOK_OPERATOR_MODULOASSIGN; }
"&="															{ return TOK_OPERATOR_BITANDASSIGN; }
"^="															{ return TOK_OPERATOR_BITXORASSIGN; }
"|="															{ return TOK_OPERATOR_BITORASSIGN; }
"<<="															{ return TOK_OPERATOR_LEFTSHIFTASSIGN; }
">>="															{ return TOK_OPERATOR_RIGHTSHIFTASSIGN; }
"="																{ return TOK_OPERATOR_ASSIGNMENT; }
"++"															{ return TOK_OPERATOR_INCREMENT; }
"--"															{ return TOK_OPERATOR_DECREMENT; }
"+"																{ return TOK_OPERATOR_PLUS; }
"-"																{ return TOK_OPERATOR_MINUS; }
"*"																{ return TOK_OPERATOR_MULTIPLY; }
"/"																{ return TOK_OPERATOR_DIVIDE; }
"%"																{ return TOK_OPERATOR_MODULO; }
"&"																{ return TOK_OPERATOR_BITAND; }
"^"																{ return TOK_OPERATOR_BITXOR; }
"|"																{ return TOK_OPERATOR_BITOR; }
"<<"															{ return TOK_OPERATOR_LEFTSHIFT; }
">>"															{ return TOK_OPERATOR_RIGHTSHIFT; }

 /* ------------------------------------------------------------------------------------------ */

<*>\r															{ }
{LF}															{ yycolumn = 1; }
<*>[\t\v\f ]+													{ }
<*>.															{ return static_cast<int>(*yytext); }

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
static void														Escape(char *buffer, size_t &len)
{
	for (size_t i = 0; i < len - 1; ++i)
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