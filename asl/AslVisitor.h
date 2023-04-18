
// Generated from Asl.g4 by ANTLR 4.7.2

#pragma once


#include "antlr4-runtime.h"
#include "AslParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by AslParser.
 */
class  AslVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by AslParser.
   */
    virtual antlrcpp::Any visitProgram(AslParser::ProgramContext *context) = 0;

    virtual antlrcpp::Any visitFunction(AslParser::FunctionContext *context) = 0;

    virtual antlrcpp::Any visitParameters(AslParser::ParametersContext *context) = 0;

    virtual antlrcpp::Any visitReturnvalue(AslParser::ReturnvalueContext *context) = 0;

    virtual antlrcpp::Any visitDeclarations(AslParser::DeclarationsContext *context) = 0;

    virtual antlrcpp::Any visitVariable_decl(AslParser::Variable_declContext *context) = 0;

    virtual antlrcpp::Any visitType(AslParser::TypeContext *context) = 0;

    virtual antlrcpp::Any visitArraytype(AslParser::ArraytypeContext *context) = 0;

    virtual antlrcpp::Any visitBasictype(AslParser::BasictypeContext *context) = 0;

    virtual antlrcpp::Any visitStatements(AslParser::StatementsContext *context) = 0;

    virtual antlrcpp::Any visitAssignStmt(AslParser::AssignStmtContext *context) = 0;

    virtual antlrcpp::Any visitIfStmt(AslParser::IfStmtContext *context) = 0;

    virtual antlrcpp::Any visitWhileStmt(AslParser::WhileStmtContext *context) = 0;

    virtual antlrcpp::Any visitProcCall(AslParser::ProcCallContext *context) = 0;

    virtual antlrcpp::Any visitReturnStmt(AslParser::ReturnStmtContext *context) = 0;

    virtual antlrcpp::Any visitReadStmt(AslParser::ReadStmtContext *context) = 0;

    virtual antlrcpp::Any visitWriteExpr(AslParser::WriteExprContext *context) = 0;

    virtual antlrcpp::Any visitWriteString(AslParser::WriteStringContext *context) = 0;

    virtual antlrcpp::Any visitLeft_expr(AslParser::Left_exprContext *context) = 0;

    virtual antlrcpp::Any visitFunctionCall(AslParser::FunctionCallContext *context) = 0;

    virtual antlrcpp::Any visitExprIdent(AslParser::ExprIdentContext *context) = 0;

    virtual antlrcpp::Any visitArithmetic(AslParser::ArithmeticContext *context) = 0;

    virtual antlrcpp::Any visitRelational(AslParser::RelationalContext *context) = 0;

    virtual antlrcpp::Any visitArrayAccess(AslParser::ArrayAccessContext *context) = 0;

    virtual antlrcpp::Any visitUnary(AslParser::UnaryContext *context) = 0;

    virtual antlrcpp::Any visitLogic(AslParser::LogicContext *context) = 0;

    virtual antlrcpp::Any visitParenthesis(AslParser::ParenthesisContext *context) = 0;

    virtual antlrcpp::Any visitValue(AslParser::ValueContext *context) = 0;

    virtual antlrcpp::Any visitParamexp(AslParser::ParamexpContext *context) = 0;

    virtual antlrcpp::Any visitIdent(AslParser::IdentContext *context) = 0;


};

