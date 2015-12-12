#pragma once

#include "Lexer.hpp"

#include <stack>
#include <unordered_map>

namespace ReShade
{
	namespace FX
	{
		class Parser
		{
		public:
			explicit Parser(NodeTree &ast);

			bool Parse(const std::string &source, std::string &errors);

		protected:
			void Backup();
			void Restore();

			bool Peek(char token) const
			{
				return Peek(static_cast<Lexer::TokenId>(token));
			}
			bool Peek(Lexer::TokenId token) const;
			void Consume();
			void ConsumeUntil(char token)
			{
				ConsumeUntil(static_cast<Lexer::TokenId>(token));
			}
			void ConsumeUntil(Lexer::TokenId token);
			bool Accept(char token)
			{
				return Accept(static_cast<Lexer::TokenId>(token));
			}
			bool Accept(Lexer::TokenId token);
			bool Expect(char token)
			{
				return Expect(static_cast<Lexer::TokenId>(token));
			}
			bool Expect(Lexer::TokenId token);

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
			bool ParseFunctionResidue(Nodes::Type &type, std::string name, Nodes::Function *&function);
			bool ParseVariableResidue(Nodes::Type &type, std::string name, Nodes::Variable *&variable, bool global = false);
			bool ParseVariableAssignment(Nodes::Expression *&expression);
			bool ParseVariableProperties(Nodes::Variable *variable);
			bool ParseVariablePropertiesExpression(Nodes::Expression *&expression);
			bool ParseTechnique(Nodes::Technique *&technique);
			bool ParseTechniquePass(Nodes::Pass *&pass);
			bool ParseTechniquePassExpression(Nodes::Expression *&expression);

			// Symbol Table
			struct Scope
			{
				std::string Name;
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
			Nodes::Expression *FoldConstantExpression(Nodes::Expression *expression) const;

			NodeTree *_ast;
			std::string *_errors;
			Lexer _lexer, _lexerBackup;
			Lexer::Token _token, _tokenNext, _tokenBackup;
			Scope _currentScope;
			std::stack<Symbol *> _parentStack;
			std::unordered_map<std::string, std::vector<std::pair<Scope, Symbol *>>> _symbolStack;
		};
	}
}
