//////////////////////////////////////////////////////////////////////
//
//    CodeGenVisitor - Walk the parser tree to do
//                     the generation of code
//
//    Copyright (C) 2017-2022  Universitat Politecnica de Catalunya
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU General Public License
//    as published by the Free Software Foundation; either version 3
//    of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
//    contact: José Miguel Rivero (rivero@cs.upc.edu)
//             Computer Science Department
//             Universitat Politecnica de Catalunya
//             despatx Omega.110 - Campus Nord UPC
//             08034 Barcelona.  SPAIN
//
//////////////////////////////////////////////////////////////////////

#include "CodeGenVisitor.h"
#include "antlr4-runtime.h"

#include "../common/TypesMgr.h"
#include "../common/SymTable.h"
#include "../common/TreeDecoration.h"
#include "../common/code.h"

#include <string>
#include <cstddef>    // std::size_t

// uncomment the following line to enable debugging messages with DEBUG*
// #define DEBUG_BUILD
#include "../common/debug.h"

// using namespace std;


// Constructor
CodeGenVisitor::CodeGenVisitor(TypesMgr       & Types,
                               SymTable       & Symbols,
                               TreeDecoration & Decorations) :
  Types{Types},
  Symbols{Symbols},
  Decorations{Decorations} {
}

// Accessor/Mutator to the attribute currFunctionType
TypesMgr::TypeId CodeGenVisitor::getCurrentFunctionTy() const {
  return currFunctionType;
}

void CodeGenVisitor::setCurrentFunctionTy(TypesMgr::TypeId type) {
  currFunctionType = type;
}

// Methods to visit each kind of node:
//
antlrcpp::Any CodeGenVisitor::visitProgram(AslParser::ProgramContext *ctx) {
  DEBUG_ENTER();
  code my_code;
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);
  for (auto ctxFunc : ctx->function()) {
    subroutine subr = visit(ctxFunc);
    my_code.add_subroutine(subr);
  }
  Symbols.popScope();
  DEBUG_EXIT();
  return my_code;
}

antlrcpp::Any CodeGenVisitor::visitFunction(AslParser::FunctionContext *ctx) {
  DEBUG_ENTER();
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);
  subroutine subr(ctx->ID()->getText());
  codeCounters.reset();
  std::vector<var> && lvars = visit(ctx->declarations());

  if (ctx->returnvalue()) {
    TypesMgr::TypeId tret = getTypeDecor(ctx->returnvalue()->type());
    setCurrentFunctionTy(tret);
    subr.add_param("_result", Types.to_string(tret));
  }

  if (ctx->parameters()) {
    std::vector<std::string> && lparams = visit(ctx->parameters());
    for (uint i = 0; i < lparams.size(); ++i) {
      std::string variable = ctx->parameters()->ID(i)->getText();
      //std::string vartype = ctx->parameters()->type(i)->getText();
      //if(vartype == "int") vartype = "integer";
      TypesMgr::TypeId vartype = getTypeDecor(ctx->parameters()->type(i));
      if(Types.isArrayTy(vartype)) { 
        vartype = getTypeDecor(ctx->parameters()->type(i)->arraytype()->basictype());
        subr.add_param(variable, Types.to_string(vartype), true);
      }
      else subr.add_param(variable, Types.to_string(vartype));
    }
  }

  for (auto & onevar : lvars) {
    subr.add_var(onevar);
  }
  instructionList && code = visit(ctx->statements());

  if (not ctx->returnvalue()) code = code || instruction(instruction::RETURN());
  subr.set_instructions(code);
  Symbols.popScope();
  DEBUG_EXIT();
  return subr;

}

antlrcpp::Any CodeGenVisitor::visitParameters(AslParser::ParametersContext *ctx) {
  DEBUG_ENTER();
  std::vector<std::string> lparams;
  for (auto & oneID : ctx->ID()) {
    lparams.push_back(oneID->getText());
  }
  DEBUG_EXIT();
  return lparams;
}

antlrcpp::Any CodeGenVisitor::visitDeclarations(AslParser::DeclarationsContext *ctx) {
  DEBUG_ENTER();
  std::vector<var> lvars;
  for (auto & varDeclCtx : ctx->variable_decl()) {
    std::vector<var> vars = visit(varDeclCtx);
    for (var & onevar : vars)
      lvars.push_back(onevar);
  }
  DEBUG_EXIT();
  return lvars;
}

antlrcpp::Any CodeGenVisitor::visitVariable_decl(AslParser::Variable_declContext *ctx) {
  DEBUG_ENTER();

  TypesMgr::TypeId   t1 = getTypeDecor(ctx->type());
  std::size_t      size = Types.getSizeOfType(t1);

  std::vector<var> lvars;
  for (auto & oneID : ctx->ID()) {
    std::string var_type;
    if(Types.isArrayTy(t1)) t1 = getTypeDecor(ctx->type()->arraytype()->basictype());
    var_type = Types.to_string(t1);
    
    var onevar = var{oneID->getText(), var_type, size};
    lvars.push_back(onevar);
  }

  DEBUG_EXIT();
  return lvars;
}

antlrcpp::Any CodeGenVisitor::visitParamexp(AslParser::ParamexpContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  for (auto oneParam : ctx->expr()) {
    CodeAttribs && codeAtr = visit(oneParam);
    if ((ctx->expr()).size() > 1) code = code || codeAtr.code;
    code = code || instruction::PUSH(codeAtr.addr);
  }
  DEBUG_EXIT();
  return code;
}


antlrcpp::Any CodeGenVisitor::visitStatements(AslParser::StatementsContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  for (auto stCtx : ctx->statement()) {
    instructionList && codeS = visit(stCtx);
    code = code || codeS;
  }
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitAssignStmt(AslParser::AssignStmtContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  CodeAttribs     && codAtsE1 = visit(ctx->left_expr());
  std::string           addr1 = codAtsE1.addr;
  std::string           offs1 = codAtsE1.offs;
  instructionList &     code1 = codAtsE1.code;
  TypesMgr::TypeId tid1 = getTypeDecor(ctx->left_expr());
  CodeAttribs     && codAtsE2 = visit(ctx->expr());
  std::string           addr2 = codAtsE2.addr;
  std::string           offs2 = codAtsE2.offs;
  instructionList &     code2 = codAtsE2.code;
  TypesMgr::TypeId tid2 = getTypeDecor(ctx->expr());

  // Cas que assignem un array a un altre
  if (Types.isArrayTy(tid1) and Types.isArrayTy(tid2)) {

      std::string label = codeCounters.newLabelWHILE();
      std::string labelIniWhile = "while"+label;
      std::string labelEndWhile = "endwhile"+label;
      instructionList      codeCondicio;
      instructionList      codeCos;
      std::string i = "%" + codeCounters.newTEMP();
      std::string fiWhile = "%" + codeCounters.newTEMP();
      std::string comparativa = "%" + codeCounters.newTEMP();
      std::string temp1 = "%" + codeCounters.newTEMP();

      std::string identArray1 = "%" + codeCounters.newTEMP();
      std::string identArray2 = "%" + codeCounters.newTEMP();

      if (Types.isArrayTy(tid1) and Symbols.isParameterClass(addr1))
        code1 = code1 || instruction::LOAD(identArray1, addr1);
      else identArray1 = addr1;

      if (Types.isArrayTy(tid2) and Symbols.isParameterClass(addr2))
        code2 = code2 || instruction::LOAD(identArray2, addr2);
      else identArray2 = addr2;

      codeCondicio = instruction::ILOAD(temp1, "1") ||
                     instruction::ILOAD(fiWhile, std::to_string(Types.getArraySize(tid1))) ||
                     instruction::LT(comparativa, i, fiWhile);

      std::string arrayExpr = "%" + codeCounters.newTEMP();
      codeCos = instruction::LOADX(arrayExpr, identArray2, i) ||
                instruction::XLOAD(identArray1, i, arrayExpr) ||
                instruction::ADD(i, i, temp1);

      code = code1 || code2 || instruction::ILOAD(i, "0") ||
             instruction::LABEL(labelIniWhile) || codeCondicio ||
             instruction::FJUMP(comparativa, labelEndWhile) ||
             codeCos || instruction::UJUMP(labelIniWhile) ||
             instruction::LABEL(labelEndWhile);
  }

  else {

    if(Types.isFloatTy(tid1) and Types.isIntegerTy(tid2)) {
      std::string tempaux = "%" + codeCounters.newTEMP();
      code2 = code2 || instruction::FLOAT(tempaux,addr2);
      addr2 = tempaux;
    }
    if (offs1 != "") code = code1 || code2 || instruction::XLOAD(addr1, offs1, addr2);
    else code = code1 || code2 || instruction::LOAD(addr1, addr2);
  }

  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitIfStmt(AslParser::IfStmtContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  CodeAttribs     && codAtsE = visit(ctx->expr());
  std::string          addr1 = codAtsE.addr;
  instructionList &    code1 = codAtsE.code;
  instructionList &&   code2 = visit(ctx->statements(0));
  std::string label = codeCounters.newLabelIF();
  std::string labelEndIf = "endif"+label;

  if (ctx->ELSE()) {
    std::string labelElse = "else"+label;
    instructionList && code3 = visit(ctx->statements(1));
    code = code1 || instruction::FJUMP(addr1, labelElse) ||
           code2 || instruction::UJUMP(labelEndIf) ||
           instruction::LABEL(labelElse) || code3 ||
           instruction::LABEL(labelEndIf);
  }
  else {
    code = code1 || instruction::FJUMP(addr1, labelEndIf) ||
           code2 || instruction::LABEL(labelEndIf);
  }

  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitWhileStmt(AslParser::WhileStmtContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  CodeAttribs     && codAtsE = visit(ctx->expr());
  std::string          addr1 = codAtsE.addr;
  instructionList &    code1 = codAtsE.code;
  instructionList &&   code2 = visit(ctx->statements());

  std::string label = codeCounters.newLabelWHILE();
  std::string labelIniWhile = "while"+label;
  std::string labelEndWhile = "endwhile"+label;

  code = instruction::LABEL(labelIniWhile) ||
         code1 || instruction::FJUMP(addr1, labelEndWhile) ||
         code2 || instruction::UJUMP(labelIniWhile) ||
         instruction::LABEL(labelEndWhile);
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitProcCall(AslParser::ProcCallContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  instructionList parametersCode;
  //std::string name = ctx->ident()->ID()->getSymbol()->getText();
  std::string name = ctx->ident()->getText();

  TypesMgr::TypeId tf = getTypeDecor(ctx->ident());
  TypesMgr::TypeId tret = Types.getFuncReturnType(tf);
  auto numCallParameters = (ctx->paramexp()->expr()).size();            // per tant no podem mirar si te paramatres
  auto funcParams = Types.getFuncParamsTypes(tf);

  for (uint i = 0; i < numCallParameters; ++i) {

    CodeAttribs && codeAtrExpr = visit(ctx->paramexp()->expr(i));
    TypesMgr::TypeId texpr = getTypeDecor(ctx->paramexp()->expr(i));

    parametersCode = parametersCode || codeAtrExpr.code;
    if (Types.isIntegerTy(texpr) and Types.isFloatTy(funcParams[i])) {
      std::string temp2 = "%" + codeCounters.newTEMP();
      parametersCode = parametersCode || instruction::FLOAT(temp2, codeAtrExpr.addr)
                                      || instruction::PUSH(temp2);
    }

    else if (Types.isArrayTy(texpr)) {
      auto arrayName = ctx->paramexp()->expr(i)->getText();
      if (Symbols.isParameterClass(arrayName)) {
        std::string temp2 = "%" + codeCounters.newTEMP();
        parametersCode = parametersCode || instruction::LOAD(temp2, codeAtrExpr.addr)
                                        || instruction::PUSH(temp2);
      }
      else {
        std::string temp2 = "%" + codeCounters.newTEMP();
        parametersCode = parametersCode || instruction::ALOAD(temp2, codeAtrExpr.addr)
                                      || instruction::PUSH(temp2);
      }
    }

    else parametersCode = parametersCode || instruction::PUSH(codeAtrExpr.addr);

  }

  if (Types.isVoidTy(tret)) {
    code = parametersCode || instruction::CALL(name);
    for (uint i = 0; i < numCallParameters; ++i)
      code = code || instruction::POP();
  }

  // Les funcions es poden cridar com a procediments, ignorant el resultat que retornin
  else {
    code = instruction::PUSH() || parametersCode || instruction::CALL(name);
    for (uint i = 0; i < numCallParameters; ++i)
      code = code || instruction::POP();
    code = code || instruction::POP();    // si es criden com a procediment ens la suda el que retorni per tant POP() en cap registre
  }

  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitReturnStmt(AslParser::ReturnStmtContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  if (ctx->expr()) {
    CodeAttribs && codAtsE = visit(ctx->expr());
    std::string       addr = codAtsE.addr;
    instructionList & code1 = codAtsE.code;
    TypesMgr::TypeId texpr = getTypeDecor(ctx->expr());
    TypesMgr::TypeId treturn = getCurrentFunctionTy();

    if (Types.isIntegerTy(texpr) and Types.isFloatTy(treturn)) {
      std::string temp = "%" + codeCounters.newTEMP();
      code = code1 || instruction::FLOAT(temp, addr)
                   || instruction::LOAD("_result", temp)
                   || instruction(instruction::RETURN());
    }
    else {
      code = code1 || instruction::LOAD("_result", addr)
                   || instruction(instruction::RETURN());
    }
  }
  else code = instruction::RETURN();
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitReadStmt(AslParser::ReadStmtContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs     && codAtsE = visit(ctx->left_expr());
  std::string          addr1 = codAtsE.addr;
  std::string          offs1 = codAtsE.offs;
  instructionList &    code1 = codAtsE.code;
  instructionList &     code = code1;
  TypesMgr::TypeId tid1 = getTypeDecor(ctx->left_expr());
  if(offs1 != ""){
      std::string tempaux = "%" + codeCounters.newTEMP();
      if(Types.isFloatTy(tid1)) code = code1 || instruction::READF(tempaux);
      else if (Types.isCharacterTy(tid1)) code = code1 || instruction::READC(tempaux);
      else  code = code1 || instruction::READI(tempaux);

      code = code || instruction::XLOAD(addr1,offs1,tempaux);
  }
  else{
      if(Types.isFloatTy(tid1)) code = code1 || instruction::READF(addr1);
      else if (Types.isCharacterTy(tid1)) code = code1 || instruction::READC(addr1);
      else  code = code1 || instruction::READI(addr1);
  }
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitWriteExpr(AslParser::WriteExprContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs     && codAt1 = visit(ctx->expr());
  std::string         addr1 = codAt1.addr;
  // std::string         offs1 = codAt1.offs;
  instructionList &   code = codAt1.code;
  TypesMgr::TypeId tid1 = getTypeDecor(ctx->expr());
  if (Types.isFloatTy(tid1)) code = code || instruction::WRITEF(addr1);
  else if (Types.isCharacterTy(tid1)) code = code || instruction::WRITEC(addr1);
  else code = code || instruction::WRITEI(addr1);
  //std::cout << "HERE" << std::endl;
  //std::cout << visit(ctx->expr()) << std::endl;
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitWriteString(AslParser::WriteStringContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  std::string s = ctx->STRING()->getText();
  code = code || instruction::WRITES(s);
  //std::cout << "In write String" << std::endl;
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitLeft_expr(AslParser::Left_exprContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs && codAtsIdent = visit(ctx->ident());
  if (ctx->expr()) {
    CodeAttribs && codAtsExpr = visit(ctx->expr());
    std::string resultExpr = codAtsExpr.addr;
    std::string ident = codAtsIdent.addr;
    instructionList & code = codAtsExpr.code;

    TypesMgr::TypeId t = getTypeDecor(ctx->ident());
    std::string temp = "%" + codeCounters.newTEMP();

    if (Types.isArrayTy(t) and Symbols.isParameterClass(ident)) {
      std::string tempaux = "%" + codeCounters.newTEMP();
      code = code || instruction::LOAD(tempaux, ident);
      ident = tempaux;
    }

    CodeAttribs codAts (ident, resultExpr, code);
    DEBUG_EXIT();
    return codAts;
  }
  else {
    DEBUG_EXIT();
    return codAtsIdent;
  }
}

antlrcpp::Any CodeGenVisitor::visitParenthesis(AslParser::ParenthesisContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs && codAts = visit(ctx->expr());
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitFunctionCall(AslParser::FunctionCallContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  instructionList parametersCode;
  //std::string name = ctx->ident()->ID()->getSymbol()->getText();
  std::string name = ctx->ident()->getText();

  TypesMgr::TypeId tf = getTypeDecor(ctx->ident());
  auto numCallParameters = (ctx->paramexp()->expr()).size();            // per tant no podem mirar si te paramatres
  auto funcParams = Types.getFuncParamsTypes(tf);

  for (uint i = 0; i < numCallParameters; ++i) {

    CodeAttribs && codeAtrExpr = visit(ctx->paramexp()->expr(i));
    TypesMgr::TypeId texpr = getTypeDecor(ctx->paramexp()->expr(i));

    parametersCode = parametersCode || codeAtrExpr.code;
    if (Types.isIntegerTy(texpr) and Types.isFloatTy(funcParams[i])) {
      std::string temp2 = "%" + codeCounters.newTEMP();
      parametersCode = parametersCode || instruction::FLOAT(temp2, codeAtrExpr.addr)
                                      || instruction::PUSH(temp2);
    }

    else if (Types.isArrayTy(texpr)) {
      auto arrayName = ctx->paramexp()->expr(i)->getText();
      if (Symbols.isParameterClass(arrayName)) {
        std::string temp2 = "%" + codeCounters.newTEMP();
        parametersCode = parametersCode || instruction::LOAD(temp2, codeAtrExpr.addr)
                                        || instruction::PUSH(temp2);
      }
      else {
        std::string temp2 = "%" + codeCounters.newTEMP();
        parametersCode = parametersCode || instruction::ALOAD(temp2, codeAtrExpr.addr)
                                        || instruction::PUSH(temp2);
      }
    }

    else parametersCode = parametersCode || instruction::PUSH(codeAtrExpr.addr);

  }

  auto numParams = (ctx->paramexp()->expr()).size();
  code = instruction::PUSH() || parametersCode ||
         instruction::CALL(name);
  for (uint i = 0; i < numParams; ++i)
    code = code || instruction::POP();
  std::string result = "%" + codeCounters.newTEMP();
  code = code || instruction::POP(result);

  CodeAttribs codeAtr (result, "", code);

  DEBUG_EXIT();
  return codeAtr;
}

antlrcpp::Any CodeGenVisitor::visitArrayAccess(AslParser::ArrayAccessContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs       && codeAtsIdent = visit(ctx->ident());
  std::string ident = codeAtsIdent.addr;

  CodeAttribs       && codeAtsExpr = visit(ctx->expr());
  std::string           resultExpr = codeAtsExpr.addr;

  instructionList     & code = codeAtsExpr.code;

  TypesMgr::TypeId t = getTypeDecor(ctx->ident());
  std::string temp = "%" + codeCounters.newTEMP();

  if (Types.isArrayTy(t) and Symbols.isParameterClass(ident)) {
    std::string tempaux = "%" + codeCounters.newTEMP();
    code = code || instruction::LOAD(tempaux, ident);
    code = code || instruction::LOADX(temp, tempaux, resultExpr);
  }

  else {
    code = code || instruction::LOADX(temp, ident, resultExpr);
  }

  CodeAttribs codeAts(temp, "", code);
  DEBUG_EXIT();
  return codeAts;
}


antlrcpp::Any CodeGenVisitor::visitUnary(AslParser::UnaryContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs     && codAt = visit(ctx->expr());
  std::string         addr = codAt.addr;
  instructionList &   code = codAt.code;

  std::string temp = "%" + codeCounters.newTEMP();
  TypesMgr::TypeId  t = getTypeDecor(ctx->expr());
  if (ctx->NOT())
    code = code || instruction::NOT(temp, addr);
  else if (ctx->MINUS()) {
    if (Types.isFloatTy(t))
      code = code || instruction::FNEG(temp, addr);
    else
      code = code || instruction::NEG(temp, addr);
  }
  else {
    code = code || instruction::LOAD(temp, addr);
  }
  CodeAttribs codAts(temp, "", code);
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitArithmetic(AslParser::ArithmeticContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs     && codAt1 = visit(ctx->expr(0));
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;
  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  instructionList &   code2 = codAt2.code;
  instructionList &&   code = code1 || code2;
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  // TypesMgr::TypeId  t = getTypeDecor(ctx);
  std::string temp = "%"+codeCounters.newTEMP();
  if (Types.isFloatTy(t1) or Types.isFloatTy(t2)) {

    if (Types.isIntegerTy(t1)) {
      std::string conversion = "%"+codeCounters.newTEMP();
      code = code || instruction:: FLOAT(conversion, addr1);
      addr1 = conversion;
    }

    if(ctx->PWR() and Types.isIntegerTy(t2)){
      std::string i = "0";
      std::string tmp = "%"+codeCounters.newTEMP();
      //instructionList code;
      // CodeAttribs     && codAtsE = visit(ctx->expr());
      //std::string          addr1 = codAtsE.addr;
      instructionList &&    code3 = instruction::LT(tmp, i, addr2);
      instructionList &&   code4 = instruction::MUL(temp, addr1, addr1);
      code4 = code4 || instruction::ADD(i, i, "1");

      std::string label = codeCounters.newLabelWHILE();
      std::string labelIniWhile = "while"+label;
      std::string labelEndWhile = "endwhile"+label;

      code = instruction::LABEL(labelIniWhile) ||
         code3 || instruction::FJUMP(addr1, labelEndWhile) ||
         code4 || instruction::UJUMP(labelIniWhile) ||
         instruction::LABEL(labelEndWhile);
    }
    else if (Types.isIntegerTy(t2)) {
      std::string conversion = "%"+codeCounters.newTEMP();
      code = code || instruction:: FLOAT(conversion, addr2);
      addr2 = conversion;
    }

    if (ctx->MUL())
      code = code || instruction::FMUL(temp, addr1, addr2);
    else if (ctx->DIV())
      code = code || instruction::FDIV(temp, addr1, addr2);
    else if (ctx->PLUS())
      code = code || instruction::FADD(temp, addr1, addr2);
    else     // ctx->MINUS()
      code = code || instruction::FSUB(temp, addr1, addr2);
  }

  else {
    if(ctx->PWR()){
      std::string i = "0";
      std::string tmp = "%"+codeCounters.newTEMP();
      //instructionList code;
      // CodeAttribs     && codAtsE = visit(ctx->expr());
      //std::string          addr1 = codAtsE.addr;
      instructionList &&    code3 = instruction::LT(tmp, i, addr2);
      instructionList &&   code4 = instruction::MUL(temp, addr1, addr1);
      code4 = code4 || instruction::ADD(i, i, "1");

      std::string label = codeCounters.newLabelWHILE();
      std::string labelIniWhile = "while"+label;
      std::string labelEndWhile = "endwhile"+label;

      code = instruction::LABEL(labelIniWhile) ||
         code3 || instruction::FJUMP(addr1, labelEndWhile) ||
         code4 || instruction::UJUMP(labelIniWhile) ||
         instruction::LABEL(labelEndWhile);
    }

    else if (ctx->MUL())
      code = code || instruction::MUL(temp, addr1, addr2);
    else if (ctx->DIV())
      code = code || instruction::DIV(temp, addr1, addr2);
    else if (ctx->MOD()) {
      std::string temp2 = "%"+codeCounters.newTEMP();
      std::string temp3 = "%"+codeCounters.newTEMP();
      // a |_b
      // r/  q          a = b*q + r
      // r = a mod b ==
      // r = a - q*b
      code = code || instruction::DIV(temp2, addr1, addr2); // temp2 = q = a/b
      code = code || instruction::MUL(temp3, temp2, addr2); // temp3 = q*b
      code = code || instruction::SUB(temp,  addr1, temp3); // temp = r = a - q*b
    }
    else if (ctx->PLUS())
      code = code || instruction::ADD(temp, addr1, addr2);
    else     // ctx->MINUS()
      code = code || instruction::SUB(temp, addr1, addr2);
  }

  CodeAttribs codAts(temp, "", code);
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitRelational(AslParser::RelationalContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs     && codAt1 = visit(ctx->expr(0));
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;
  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  instructionList &   code2 = codAt2.code;
  instructionList &&   code = code1 || code2;
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  // TypesMgr::TypeId  t = getTypeDecor(ctx);
  std::string temp = "%"+codeCounters.newTEMP();
  if (Types.isFloatTy(t1) or Types.isFloatTy(t2)) {

    if (Types.isIntegerTy(t1)) {
      std::string conversion = "%"+codeCounters.newTEMP();
      code = code || instruction:: FLOAT(conversion, addr1);
      addr1 = conversion;
    }

    if (Types.isIntegerTy(t2)) {
      std::string conversion = "%"+codeCounters.newTEMP();
      code = code || instruction:: FLOAT(conversion, addr2);
      addr2 = conversion;
    }

    if (ctx->EQUAL()) code = code || instruction::FEQ(temp, addr1, addr2);
    else if (ctx->LOWER()) code = code || instruction::FLT(temp, addr1, addr2);
    else if (ctx->LEQUAL()) code = code || instruction::FLE(temp, addr1, addr2);
    else if (ctx->HIGHER()) {
      std::string negar = "%"+codeCounters.newTEMP();
      code = code || instruction::FLE(negar, addr1, addr2);
      code = code || instruction::NOT(temp, negar);
    }
    else if (ctx->HEQUAL()) {
      std::string negar = "%"+codeCounters.newTEMP();
      code = code || instruction::FLT(negar, addr1, addr2);
      code = code || instruction::NOT(temp, negar);
    }
    else {
      std::string negar = "%"+codeCounters.newTEMP();
      code = code || instruction::FEQ(negar, addr1, addr2);
      code = code || instruction::NOT(temp, negar);
    }
  }

  else {

    if (ctx->EQUAL()) code = code || instruction::EQ(temp, addr1, addr2);
    else if (ctx->LOWER()) code = code || instruction::LT(temp, addr1, addr2);
    else if (ctx->LEQUAL()) code = code || instruction::LE(temp, addr1, addr2);
    else if (ctx->HIGHER()) {
      std::string negar = "%"+codeCounters.newTEMP();
      code = code || instruction::LE(negar, addr1, addr2);
      code = code || instruction::NOT(temp, negar);
    }
    else if (ctx->HEQUAL()) {
      std::string negar = "%"+codeCounters.newTEMP();
      code = code || instruction::LT(negar, addr1, addr2);
      code = code || instruction::NOT(temp, negar);
    }
    else {
      std::string negar = "%"+codeCounters.newTEMP();
      code = code || instruction::EQ(negar, addr1, addr2);
      code = code || instruction::NOT(temp, negar);
    }
  }
  CodeAttribs codAts(temp, "", code);
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitLogic(AslParser::LogicContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs     && codAt1 = visit(ctx->expr(0));
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;
  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  instructionList &   code2 = codAt2.code;
  instructionList &&   code = code1 || code2;
  std::string temp = "%"+codeCounters.newTEMP();

  if (ctx->AND())
    code = code || instruction::AND(temp, addr1, addr2);
  else  // ctx->OR()
    code = code || instruction::OR(temp, addr1, addr2);

  CodeAttribs codAts(temp, "", code);
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitValue(AslParser::ValueContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  std::string temp = "%"+codeCounters.newTEMP();

  if (ctx->getText() == "true")       code = instruction::ILOAD(temp, "1");
  else if (ctx->getText() == "false") code = instruction::ILOAD(temp, "0");
  else                                code = instruction::ILOAD(temp, ctx->getText());
  CodeAttribs codAts(temp, "", code);

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitExprIdent(AslParser::ExprIdentContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs && codAts = visit(ctx->ident());
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitIdent(AslParser::IdentContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs codAts(ctx->ID()->getText(), "", instructionList());
  DEBUG_EXIT();
  return codAts;
}


// Getters for the necessary tree node atributes:
//   Scope and Type
SymTable::ScopeId CodeGenVisitor::getScopeDecor(antlr4::ParserRuleContext *ctx) const {
  return Decorations.getScope(ctx);
}
TypesMgr::TypeId CodeGenVisitor::getTypeDecor(antlr4::ParserRuleContext *ctx) const {
  return Decorations.getType(ctx);
}


// Constructors of the class CodeAttribs:
//
CodeGenVisitor::CodeAttribs::CodeAttribs(const std::string & addr,
                                         const std::string & offs,
                                         instructionList & code) :
  addr{addr}, offs{offs}, code{code} {
}

CodeGenVisitor::CodeAttribs::CodeAttribs(const std::string & addr,
                                         const std::string & offs,
                                         instructionList && code) :
  addr{addr}, offs{offs}, code{code} {
}