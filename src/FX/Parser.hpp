#pragma once

#include "Lexer.hpp"
#include "ParserNodes.hpp"

#include <stack>
#include <unordered_map>

namespace ReShade
{
	namespace FX
	{
		class Parser
		{
			Parser(const Parser &);
			Parser &operator=(const Parser &);

		public:
			explicit Parser(NodeTree &ast);

			bool Parse(const std::string &source, std::string &errors);

		protected:
			void Backup();
			void Restore();

			bool Peek(Lexer::Token::Id token) const;
			inline bool Peek(char token) const
			{
				return Peek(static_cast<Lexer::Token::Id>(token));
			}
			void Consume();
			void ConsumeUntil(Lexer::Token::Id token);
			inline void ConsumeUntil(char token)
			{
				ConsumeUntil(static_cast<Lexer::Token::Id>(token));
			}
			bool Accept(Lexer::Token::Id token);
			inline bool Accept(char token)
			{
				return Accept(static_cast<Lexer::Token::Id>(token));
			}
			bool Expect(Lexer::Token::Id token);
			inline bool Expect(char token)
			{
				return Expect(static_cast<Lexer::Token::Id>(token));
			}

		private:
			void Error(const Location &location, unsigned int code, const char *message, ...);
			void Warning(const Location &location, unsigned int code, const char *message, ...);

			// Types
			bool AcceptTypeClass(Nodes::Type &type);
			bool AcceptTypeQualifiers(Nodes::Type &type);
			bool ParseType(Nodes::Type &type);

			// Expressions
			bool AcceptUnaryOp(Nodes::Unary::Op &op);
			bool AcceptPostfixOp(Nodes::Unary::Op &op);
			bool PeekMultaryOp(Nodes::Binary::Op &op, unsigned int &precedence) const;
			bool AcceptAssignmentOp(Nodes::Assignment::Op &op);
			bool ParseExpression(Nodes::Expression *&expression);
			bool ParseExpressionUnary(Nodes::Expression *&expression);
			bool ParseExpressionMultary(Nodes::Expression *&expression, unsigned int precedence = 0);
			bool ParseExpressionAssignment(Nodes::Expression *&expression);

			// Statements
			bool ParseStatement(Nodes::Statement *&statement, bool scoped = true);
			bool ParseStatementBlock(Nodes::Statement *&statement, bool scoped = true);
			bool ParseStatementDeclaratorList(Nodes::Statement *&statement);

			// Declarations
			bool ParseTopLevel();
			bool ParseNamespace();
			bool ParseArray(int &size);
			bool ParseAnnotations(std::vector<Nodes::Annotation> &annotations);
			bool ParseStruct(Nodes::Struct *&structure);
			bool ParseFunctionResidue(Nodes::Type &type, const std::string &name, Nodes::Function *&function);
			bool ParseVariableResidue(Nodes::Type &type, const std::string &name, Nodes::Variable *&variable, bool global = false);
			bool ParseVariableAssignment(Nodes::Expression *&expression);
			bool ParseVariableProperties(Nodes::Variable *variable);
			bool ParseVariablePropertiesExpression(Nodes::Expression *&expression);
			bool ParseTechnique(Nodes::Technique *&technique);
			bool ParseTechniquePass(Nodes::Pass *&pass);
			bool ParseTechniquePassExpression(Nodes::Expression *&expression);

			// Symbol Table
			struct Scope
			{
				unsigned int Level, NamespaceLevel;
			};
			typedef Nodes::Declaration Symbol;

			void EnterScope(Symbol *parent = nullptr);
			void EnterNamespace(const std::string &name);
			void LeaveScope();
			void LeaveNamespace();
			bool InsertSymbol(Symbol *symbol, bool global = false);
			Symbol *FindSymbol(const std::string &name) const;
			Symbol *FindSymbol(const std::string &name, const Scope &scope, bool exclusive = false) const;
			bool ResolveCall(Nodes::Call *call, const Scope &scope, bool &intrinsic, bool &ambiguouss) const;
			Nodes::Expression *FoldConstantExpression(Nodes::Expression *expression);

			NodeTree &mAST;
			std::string mErrors;
			Lexer mLexer, mBackupLexer;
			Lexer::Token mToken, mNextToken, mBackupToken;
			Scope mCurrentScope;
			std::string mCurrentNamespace;
			std::stack<Symbol *> mParentStack;
			std::unordered_map<std::string, std::vector<std::pair<Scope, Symbol *>>> mSymbolStack;
		};
	}
}
