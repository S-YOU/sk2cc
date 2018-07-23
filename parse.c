#include "cc.h"

Map *func_symbols, *symbols;

Type *type_new() {
  Type *type = (Type *) malloc(sizeof(Type));
  return type;
}

Type *int_type() {
  Type *type = type_new();
  type->type = INT;
  return type;
}

Type *pointer_to(Type *type) {
  Type *pointer = type_new();
  pointer->type = POINTER;
  pointer->pointer_of = type;
  return pointer;
}

Symbol *symbol_new() {
  Symbol *symbol = (Symbol *) malloc(sizeof(Symbol));
  return symbol;
}

Node *node_new() {
  Node *node = (Node *) malloc(sizeof(Node));
  return node;
}

Node *assignment_expression();
Node *expression();

Node *primary_expression() {
  Token *token = get_token();
  Node *node;

  if (token->type == tINT_CONST) {
    node = node_new();
    node->type = CONST;
    node->value_type = int_type();
    node->int_value = token->int_value;
  } else if (token->type == tIDENTIFIER) {
    char *identifier = token->identifier;
    Symbol *symbol;
    if (map_lookup(func_symbols, identifier)) {
      symbol = (Symbol *) map_lookup(func_symbols, identifier);
    } else if (map_lookup(symbols, identifier)) {
      symbol = (Symbol *) map_lookup(symbols, identifier);
    } else {
      symbol = NULL;
    }
    node = node_new();
    node->type = IDENTIFIER;
    node->value_type = symbol ? symbol->value_type : int_type();
    node->identifier = identifier;
    node->symbol = symbol;
  } else if (token->type == tLPAREN) {
    node = expression();
    expect_token(tRPAREN);
  } else {
    error("unexpected primary expression.");
  }

  return node;
}

Node *postfix_expression() {
  Node *node = primary_expression();

  while (1) {
    if (read_token(tLPAREN)) {
      if (node->type != IDENTIFIER) {
        error("invalid function call.");
      }

      Symbol *symbol = (Symbol *) map_lookup(func_symbols, node->identifier);

      Node *parent = node_new();
      parent->type = FUNC_CALL;
      parent->value_type = symbol ? symbol->value_type : int_type();
      parent->left = node;
      parent->args = vector_new();
      if (peek_token()->type != tRPAREN) {
        do {
          if (parent->args->length >= 6) {
            error("too many arguments.");
          }
          vector_push(parent->args, assignment_expression());
        } while (read_token(tCOMMA));
      }
      expect_token(tRPAREN);

      node = parent;
    } else {
      break;
    }
  }

  return node;
}

Node *unary_expression() {
  Node *node;

  if (read_token(tAND)) {
    Node *expr = unary_expression();
    if (expr->type != IDENTIFIER) {
      error("invalid operand type.");
    }
    node = node_new();
    node->type = ADDRESS;
    node->value_type = pointer_to(expr->value_type);
    node->left = expr;
  } else if (read_token(tMUL)) {
    Node *expr = unary_expression();
    if (expr->value_type->type != POINTER) {
      error("invalid operand type.");
    }
    node = node_new();
    node->type = INDIRECT;
    node->value_type = expr->value_type->pointer_of;
    node->left = expr;
  } else if (read_token(tADD)) {
    Node *expr = unary_expression();
    if (expr->value_type->type != INT) {
      error("invalid operand type.");
    }
    node = node_new();
    node->type = UPLUS;
    node->value_type = expr->value_type;
    node->left = expr;
  } else if (read_token(tSUB)) {
    Node *expr = unary_expression();
    if (expr->value_type->type != INT) {
      error("invalid operand type.");
    }
    node = node_new();
    node->type = UMINUS;
    node->value_type = expr->value_type;
    node->left = expr;
  } else if (read_token(tNOT)) {
    Node *expr = unary_expression();
    if (expr->value_type->type != INT) {
      error("invalid operand type.");
    }
    node = node_new();
    node->type = NOT;
    node->value_type = expr->value_type;
    node->left = expr;
  } else if (read_token(tLNOT)) {
    Node *expr = unary_expression();
    if (expr->value_type->type != INT) {
      error("invalid operand type.");
    }
    node = node_new();
    node->type = LNOT;
    node->value_type = expr->value_type;
    node->left = expr;
  } else {
    node = postfix_expression();
  }

  return node;
}

Node *multiplicative_expression(Node *unary_exp) {
  Node *node = unary_exp;

  while (1) {
    NodeType type;
    if (read_token(tMUL)) type = MUL;
    else if (read_token(tDIV)) type = DIV;
    else if (read_token(tMOD)) type = MOD;
    else break;

    Node *left = node;
    Node *right = unary_expression();

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = type;
    node->value_type = value_type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *additive_expression(Node *unary_exp) {
  Node *node = multiplicative_expression(unary_exp);

  while (1) {
    NodeType type;
    if (read_token(tADD)) type = ADD;
    else if (read_token(tSUB)) type = SUB;
    else break;

    Node *left = node;
    Node *right = multiplicative_expression(unary_expression());

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else if (left->value_type->type == POINTER && right->value_type->type == INT) {
      value_type = left->value_type;
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = type;
    node->value_type = value_type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *shift_expression(Node *unary_exp) {
  Node *node = additive_expression(unary_exp);

  while (1) {
    NodeType type;
    if (read_token(tLSHIFT)) type = LSHIFT;
    else if (read_token(tRSHIFT)) type = RSHIFT;
    else break;

    Node *left = node;
    Node *right = additive_expression(unary_expression());

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = type;
    node->value_type = value_type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *relational_expression(Node *unary_exp) {
  Node *node = shift_expression(unary_exp);

  while (1) {
    NodeType type;
    if (read_token(tLT)) type = LT;
    else if (read_token(tGT)) type = GT;
    else if (read_token(tLTE)) type = LTE;
    else if (read_token(tGTE)) type = GTE;
    else break;

    Node *left = node;
    Node *right = shift_expression(unary_expression());

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = type;
    node->value_type = value_type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *equality_expression(Node *unary_exp) {
  Node *node = relational_expression(unary_exp);

  while (1) {
    NodeType type;
    if (read_token(tEQ)) type = EQ;
    else if (read_token(tNEQ)) type = NEQ;
    else break;

    Node *left = node;
    Node *right = relational_expression(unary_expression());

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = type;
    node->value_type = value_type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *and_expression(Node *unary_exp) {
  Node *node = equality_expression(unary_exp);

  while (read_token(tAND)) {
    Node *left = node;
    Node *right = equality_expression(unary_expression());

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = AND;
    node->value_type = value_type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *exclusive_or_expression(Node *unary_exp) {
  Node *node = and_expression(unary_exp);

  while (read_token(tXOR)) {
    Node *left = node;
    Node *right = and_expression(unary_expression());

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = XOR;
    node->value_type = value_type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *inclusive_or_expression(Node *unary_exp) {
  Node *node = exclusive_or_expression(unary_exp);

  while (read_token(tOR)) {
    Node *left = node;
    Node *right = exclusive_or_expression(unary_expression());

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = OR;
    node->value_type = value_type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *logical_and_expression(Node *unary_exp) {
  Node *node = inclusive_or_expression(unary_exp);

  while (read_token(tLAND)) {
    Node *left = node;
    Node *right = inclusive_or_expression(unary_expression());

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = LAND;
    node->value_type = value_type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *logical_or_expression(Node *unary_exp) {
  Node *node = logical_and_expression(unary_exp);

  while (read_token(tLOR)) {
    Node *left = node;
    Node *right = logical_and_expression(unary_expression());

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = LOR;
    node->value_type = value_type;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *conditional_expression(Node *unary_exp) {
  Node *node = logical_or_expression(unary_exp);

  if (read_token(tQUESTION)) {
    Node *control = node;
    Node *left = expression();
    expect_token(tCOLON);
    Node *right = conditional_expression(unary_expression());

    Type *value_type;
    if (left->value_type->type == INT && right->value_type->type == INT) {
      value_type = int_type();
    } else {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = CONDITION;
    node->value_type = value_type;
    node->control = control;
    node->left = left;
    node->right = right;
  }

  return node;
}

Node *assignment_expression() {
  Node *node;

  Node *unary_exp = unary_expression();
  if (read_token(tASSIGN)) {
    Node *left = unary_exp;
    Node *right = assignment_expression();

    if (left->type != IDENTIFIER && left->type != INDIRECT) {
      error("left side of assignment operator should be identifier or indirect operator.");
    }
    if (left->value_type->type != right->value_type->type) {
      error("invalid operand type.");
    }

    node = node_new();
    node->type = ASSIGN;
    node->value_type = left->value_type;
    node->left = left;
    node->right = right;
  } else {
    node = conditional_expression(unary_exp);
  }

  return node;
}

Node *expression() {
  return assignment_expression();
}

void declaration() {
  Type *type = int_type();
  expect_token(tINT);
  while (read_token(tMUL)) {
    type = pointer_to(type);
  }

  Token *token = expect_token(tIDENTIFIER);
  expect_token(tSEMICOLON);

  if (!map_lookup(symbols, token->identifier)) {
    Symbol *symbol = symbol_new();
    symbol->value_type = type;
    symbol->offset = map_count(symbols) * 8 + 8;
    map_put(symbols, token->identifier, symbol);
  }
}

Node *statement();

Node *compound_statement() {
  Node *node = node_new();
  node->type = COMP_STMT;
  node->statements = vector_new();

  expect_token(tLBRACE);
  while (1) {
    Token *token = peek_token();
    if (token->type == tRBRACE || token->type == tEND) break;
    if (peek_token()->type == tINT) {
      declaration();
    } else {
      Node *stmt = statement();
      vector_push(node->statements, (void *) stmt);
    }
  }
  expect_token(tRBRACE);

  return node;
}

Node *expression_statement() {
  Node *node = node_new();
  node->type = EXPR_STMT;
  node->expression = expression();
  expect_token(tSEMICOLON);

  return node;
}

Node *selection_statement() {
  Node *node = node_new();

  if (read_token(tIF)) {
    node->type = IF_STMT;
    expect_token(tLPAREN);
    node->control = expression();
    expect_token(tRPAREN);
    node->if_body = statement();
    if (read_token(tELSE)) {
      node->type = IF_ELSE_STMT;
      node->else_body = statement();
    }
  }

  return node;
}

Node *iteration_statement() {
  Node *node = node_new();

  if (read_token(tWHILE)) {
    node->type = WHILE_STMT;
    expect_token(tLPAREN);
    node->control = expression();
    expect_token(tRPAREN);
    node->loop_body = statement();
  } else if (read_token(tDO)) {
    node->type = DO_WHILE_STMT;
    node->loop_body = statement();
    expect_token(tWHILE);
    expect_token(tLPAREN);
    node->control = expression();
    expect_token(tRPAREN);
    expect_token(tSEMICOLON);
  } else if (read_token(tFOR)) {
    node->type = FOR_STMT;
    expect_token(tLPAREN);
    node->init = peek_token()->type != tSEMICOLON ? expression() : NULL;
    expect_token(tSEMICOLON);
    node->control = peek_token()->type != tSEMICOLON ? expression() : NULL;
    expect_token(tSEMICOLON);
    node->afterthrough = peek_token()->type != tRPAREN ? expression() : NULL;
    expect_token(tRPAREN);
    node->loop_body = statement();
  }

  return node;
}

Node *jump_statement() {
  Node *node = node_new();

  if (read_token(tCONTINUE)) {
    node->type = CONTINUE_STMT;
    expect_token(tSEMICOLON);
  } else if (read_token(tBREAK)) {
    node->type = BREAK_STMT;
    expect_token(tSEMICOLON);
  } else if (read_token(tRETURN)) {
    node->type = RETURN_STMT;
    node->expression = peek_token()->type != tSEMICOLON ? expression() : NULL;
    expect_token(tSEMICOLON);
  }

  return node;
}

Node *statement() {
  Node *node;

  if (peek_token()->type == tLBRACE) {
    node = compound_statement();
  } else if (peek_token()->type == tIF) {
    node = selection_statement();
  } else if (peek_token()->type == tWHILE) {
    node = iteration_statement();
  } else if (peek_token()->type == tDO) {
    node = iteration_statement();
  } else if (peek_token()->type == tFOR) {
    node = iteration_statement();
  } else if (peek_token()->type == tCONTINUE) {
    node = jump_statement();
  } else if (peek_token()->type == tBREAK) {
    node = jump_statement();
  } else if (peek_token()->type == tRETURN) {
    node = jump_statement();
  } else {
    node = expression_statement();
  }

  return node;
}

Node *function_definition() {
  map_clear(symbols);

  expect_token(tINT);
  Type *type = int_type();
  while (read_token(tMUL)) {
    type = pointer_to(type);
  }

  Token *id = expect_token(tIDENTIFIER);
  Symbol *func_sym = symbol_new();
  func_sym->value_type = type;
  if (map_lookup(func_symbols, id->identifier)) {
    error("duplicated function definition.");
  }
  map_put(func_symbols, id->identifier, (void *) func_sym);

  Vector *params = vector_new();
  expect_token(tLPAREN);
  if (peek_token()->type != tRPAREN) {
    do {
      if (params->length >= 6) {
        error("too many parameters.");
      }
      expect_token(tINT);
      Type *param_type = int_type();
      while (read_token(tMUL)) {
        param_type = pointer_to(param_type);
      }
      Token *token = expect_token(tIDENTIFIER);
      if (map_lookup(symbols, token->identifier)) {
        error("duplicated parameter definition.");
      }
      Symbol *symbol = symbol_new();
      symbol->value_type = param_type;
      symbol->offset = map_count(symbols) * 8 + 8;
      map_put(symbols, token->identifier, symbol);
      vector_push(params, symbol);
    } while (read_token(tCOMMA));
  }
  expect_token(tRPAREN);

  Node *node = node_new();
  node->type = FUNC_DEF;
  node->identifier = id->identifier;
  node->function_body = compound_statement();
  node->local_vars_size = map_count(symbols) * 8;
  node->params = params;

  return node;
}

Node *translate_unit() {
  Node *node = node_new();
  node->type = TLANS_UNIT;
  node->definitions = vector_new();

  while (peek_token()->type != tEND) {
    Node *def = function_definition();
    vector_push(node->definitions, (void *) def);
  }

  return node;
}

Node *parse() {
  return translate_unit();
}

void parse_init() {
  func_symbols = map_new();
  symbols = map_new();
}
