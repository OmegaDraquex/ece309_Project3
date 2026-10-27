// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"
#include <stack>

namespace {

// Helper to determine operator precedence per spec §4.6
// * and / have precedence 2; + and - have precedence 1; '(' has 0
int getPrecedence(CalcToken::Kind opKind) {
    if (opKind == CalcToken::Kind::Times || opKind == CalcToken::Kind::Divide) {
        return 2;
    }
    if (opKind == CalcToken::Kind::Plus || opKind == CalcToken::Kind::Minus) {
        return 1;
    }
    return 0;
}

// Executes a single binary operator application on the value stack
bool applyOperator(CalcToken::Kind op, std::stack<double>& valStack, std::string* err) {
    // Need at least two numbers to apply a binary operator
    if (valStack.size() < 2) {
        if (err != nullptr) {
            *err = "malformed expression: not enough operands";
        }
        return false;
    }

    // Crucial order check: the first popped element is the right-hand operand!
    double rightOperand = valStack.top();
    valStack.pop();

    double leftOperand = valStack.top();
    valStack.pop();

    double opResult = 0.0;
    if (op == CalcToken::Kind::Plus) {
        opResult = leftOperand + rightOperand;
    } else if (op == CalcToken::Kind::Minus) {
        opResult = leftOperand - rightOperand;
    } else if (op == CalcToken::Kind::Times) {
        opResult = leftOperand * rightOperand;
    } else if (op == CalcToken::Kind::Divide) {
        if (rightOperand == 0.0) {
            // Exact requirement from spec §4.6: "division by zero"
            if (err != nullptr) {
                *err = "division by zero";
            }
            return false;
        }
        opResult = leftOperand / rightOperand;
    } else {
        if (err != nullptr) {
            *err = "unknown operator token";
        }
        return false;
    }

    valStack.push(opResult);
    return true;
}

} // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    if (tokens.empty()) {
        if (error != nullptr) {
            *error = "empty token list";
        }
        return false;
    }

    std::stack<double> valStack;
    std::stack<CalcToken::Kind> opStack;

    // Iterate through tokens left to right
    for (const auto& token : tokens) {
        if (token.kind == CalcToken::Kind::Number) {
            valStack.push(token.value);
        } else if (token.kind == CalcToken::Kind::LParen) {
            opStack.push(token.kind);
        } else if (token.kind == CalcToken::Kind::RParen) {
            // Apply operators until matching '(' is reached
            while (!opStack.empty() && opStack.top() != CalcToken::Kind::LParen) {
                CalcToken::Kind topOp = opStack.top();
                opStack.pop();
                if (!applyOperator(topOp, valStack, error)) {
                    return false;
                }
            }
            if (opStack.empty()) {
                if (error != nullptr) {
                    *error = "mismatched parentheses";
                }
                return false;
            }
            // Pop the matching '('
            opStack.pop();
        } else {
            // Standard binary operators (+, -, *, /)
            // While top of opStack is an operator whose precedence is >= this token's precedence, apply it
            while (!opStack.empty() && opStack.top() != CalcToken::Kind::LParen &&
                   getPrecedence(opStack.top()) >= getPrecedence(token.kind)) {
                CalcToken::Kind topOp = opStack.top();
                opStack.pop();
                if (!applyOperator(topOp, valStack, error)) {
                    return false;
                }
            }
            opStack.push(token.kind);
        }
    }

    // Process remaining operators
    while (!opStack.empty()) {
        CalcToken::Kind remainingOp = opStack.top();
        opStack.pop();
        if (remainingOp == CalcToken::Kind::LParen) {
            if (error != nullptr) {
                *error = "unclosed parenthesis";
            }
            return false;
        }
        if (!applyOperator(remainingOp, valStack, error)) {
            return false;
        }
    }

    if (valStack.size() != 1) {
        if (error != nullptr) {
            *error = "invalid final stack state";
        }
        return false;
    }

    if (out != nullptr) {
        *out = valStack.top();
    }
    return true;
}
