#pragma once

#include "EffectTree.hpp"
#include "EffectLexer.hpp"

#include <stack>
#include <unordered_map>
#include <boost\noncopyable.hpp>

namespace ReShade
{
	namespace FX
	{
		class Parser : boost::noncopyable
		{
		public:
			Parser(Lexer &lexer, Tree &ast);

			bool Parse();

		protected:
			void Backup();
			void Restore();

			inline bool Peek(unsigned int token)
			{
				return Peek(static_cast<Lexer::Token::Id>(token));
			}
			inline bool Peek(Lexer::Token::Id token)
			{
				return this->mNextToken == token;
			}
			inline void Consume()
			{
				this->mToken = this->mNextToken;
				this->mNextToken = this->mLexer.Lex();
			}
			inline bool Accept(unsigned int token)
			{
				return Accept(static_cast<Lexer::Token::Id>(token));
			}
			inline bool Accept(Lexer::Token::Id token)
			{
				if (Peek(token))
				{
					Consume();

					return true;
				}

				return false;
			}
			bool AcceptIdentifier(std::string &identifier);
			inline bool Expect(unsigned int token)
			{
				return Expect(static_cast<Lexer::Token::Id>(token));
			}
			bool Expect(Lexer::Token::Id token);
			bool ExpectIdentifier(std::string &identifier);

		private:
			// Types
			bool AcceptTypeClass(Nodes::Type &type);
			bool AcceptTypeQualifiers(Nodes::Type &type);
			bool ParseType(Nodes::Type &type);

			// Statements
			bool ParseStatement(Nodes::Statement *&statement, bool scoped = true);
			bool ParseStatementCompound(Nodes::Statement *&statement, bool scoped = true);

			// Expressions
			bool AcceptUnaryOp(Nodes::Unary::Op &op);
			bool AcceptPostfixOp(Nodes::Unary::Op &op);
			bool PeekMultaryOp(Nodes::Binary::Op &op, unsigned int &precedence);
			bool AcceptAssignmentOp(Nodes::Assignment::Op &op);
			bool ParseExpression(Nodes::Expression *&expression);
			bool ParseExpressionUnary(Nodes::Expression *&expression);
			bool ParseExpressionMultary(Nodes::Expression *&expression, unsigned int precedence = 0);
			bool ParseExpressionAssignment(Nodes::Expression *&expression);

			// Declarations
			bool ParseTopLevel();
			bool ParseArray(int &size);
			bool ParseAnnotations(std::vector<Nodes::Annotation> &annotations);
			bool ParseStruct(Nodes::Struct *&structure);
			bool ParseDeclaration(Nodes::DeclaratorList *&declarators);
			bool ParseFunctionResidue(Nodes::Type &type, const std::string &name, Nodes::Function *&function);
			bool ParseVariableResidue(Nodes::Type &type, const std::string &name, Nodes::Variable *&variable);
			bool ParseVariableAssignment(Nodes::Expression *&expression);
			bool ParseVariableProperties(Nodes::Variable *variable);
			bool ParseVariablePropertiesExpression(Nodes::Expression *&expression);
			bool ParseTechnique(Nodes::Technique *&technique);
			bool ParseTechniquePass(Nodes::Pass *&pass);
			bool ParseTechniquePassExpression(Nodes::Expression *&expression);

			// Symbol Table
			void EnterScope(Node *parent = nullptr);
			void LeaveScope();
			bool InsertSymbol(Node *symbol);
			Node *FindSymbol(const std::string &name) const;
			Node *FindSymbol(const std::string &name, unsigned int scope, bool exclusive = false) const;
			bool ResolveCall(Nodes::Call *call, bool &intrinsic, bool &ambiguouss) const;
			Nodes::Expression *FoldConstantExpression(Nodes::Expression *expression);

			Tree &mAST;
			Lexer mLexer, mBackupLexer, &mOrigLexer;
			Lexer::Token mToken, mNextToken, mBackupToken;
			unsigned int mCurrentScope;
			std::stack<Node *> mParentStack;
			std::unordered_map<std::string, std::vector<std::pair<unsigned int, Node *>>> mSymbolStack;
		};
	}
}