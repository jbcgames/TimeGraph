#include "expr.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#ifndef M_E
#define M_E 2.71828182845904523536f
#endif

typedef enum {
    TOK_NONE,
    TOK_NUM,
    TOK_VAR_X,
    TOK_VAR_T,
    TOK_FUNC,
    TOK_OP,
    TOK_LPAREN,
    TOK_RPAREN
} TokType;

typedef struct {
    TokType type;
    ExprOpCode op;
    float val;
} Token;

static int op_prec(ExprOpCode op)
{
    switch (op) {
        case OP_ADD:
        case OP_SUB:
            return 1;
        case OP_MUL:
        case OP_DIV:
            return 2;
        case OP_NEG:
            return 3;
        case OP_POW:
            return 4;
        default:
            return 5; /* functions */
    }
}

static bool op_right_assoc(ExprOpCode op)
{
    return (op == OP_POW || op == OP_NEG);
}

static bool is_func_op(ExprOpCode op)
{
    return op >= OP_SIN && op <= OP_CEIL;
}

static bool match_func(const char *s, int len, ExprOpCode *out_op)
{
    if (len == 3 && strncmp(s, "sin", 3) == 0) { *out_op = OP_SIN; return true; }
    if (len == 3 && strncmp(s, "cos", 3) == 0) { *out_op = OP_COS; return true; }
    if (len == 3 && strncmp(s, "tan", 3) == 0) { *out_op = OP_TAN; return true; }
    if (len == 3 && strncmp(s, "abs", 3) == 0) { *out_op = OP_ABS; return true; }
    if (len == 4 && strncmp(s, "sqrt", 4) == 0) { *out_op = OP_SQRT; return true; }
    if (len == 2 && strncmp(s, "ln", 2) == 0) { *out_op = OP_LN; return true; }
    if (len == 3 && strncmp(s, "log", 3) == 0) { *out_op = OP_LOG; return true; }
    if (len == 3 && strncmp(s, "exp", 3) == 0) { *out_op = OP_EXP; return true; }
    if (len == 4 && strncmp(s, "asin", 4) == 0) { *out_op = OP_ASIN; return true; }
    if (len == 4 && strncmp(s, "acos", 4) == 0) { *out_op = OP_ACOS; return true; }
    if (len == 4 && strncmp(s, "atan", 4) == 0) { *out_op = OP_ATAN; return true; }
    if (len == 5 && strncmp(s, "floor", 5) == 0) { *out_op = OP_FLOOR; return true; }
    if (len == 4 && strncmp(s, "ceil", 4) == 0) { *out_op = OP_CEIL; return true; }
    return false;
}

static void set_err(char *err_out, int err_size, const char *msg)
{
    if (err_out && err_size > 0) {
        snprintf(err_out, (size_t)err_size, "%s", msg);
    }
}

static bool emit_inst(ExprCompiled *out, ExprOpCode op, float val, char *err_out, int err_size)
{
    if (out->len >= EXPR_MAX_OPS) {
        set_err(err_out, err_size, "Expresion muy larga");
        return false;
    }
    out->code[out->len].op = op;
    out->code[out->len].val = val;
    out->len++;
    return true;
}

bool expr_compile(const char *src, ExprCompiled *out, char *err_out, int err_size)
{
    out->len = 0;
    out->uses_t = false;

    if (!src || src[0] == '\0') {
        set_err(err_out, err_size, "Funcion vacia");
        return false;
    }

    Token toks[EXPR_MAX_OPS];
    int ntoks = 0;
    const char *p = src;

    while (*p) {
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }

        if (ntoks >= EXPR_MAX_OPS - 2) {
            set_err(err_out, err_size, "Demasiados tokens");
            return false;
        }

        Token t = { TOK_NONE, OP_NUM, 0.0f };

        if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {
            char *endp = NULL;
            float v = strtof(p, &endp);
            if (endp == p) {
                set_err(err_out, err_size, "Numero invalido");
                return false;
            }
            t.type = TOK_NUM;
            t.op = OP_NUM;
            t.val = v;
            p = endp;
        } else if (isalpha((unsigned char)*p)) {
            char ident[16];
            int ilen = 0;
            while (isalpha((unsigned char)*p) && ilen < 15) {
                ident[ilen++] = (char)tolower((unsigned char)*p);
                p++;
            }
            ident[ilen] = '\0';

            ExprOpCode fop;
            if (ilen == 1 && ident[0] == 'x') {
                t.type = TOK_VAR_X;
                t.op = OP_VAR_X;
            } else if (ilen == 1 && ident[0] == 't') {
                t.type = TOK_VAR_T;
                t.op = OP_VAR_T;
                out->uses_t = true;
            } else if (ilen == 2 && strcmp(ident, "pi") == 0) {
                t.type = TOK_NUM;
                t.op = OP_NUM;
                t.val = (float)M_PI;
            } else if (ilen == 1 && ident[0] == 'e') {
                t.type = TOK_NUM;
                t.op = OP_NUM;
                t.val = (float)M_E;
            } else if (match_func(ident, ilen, &fop)) {
                t.type = TOK_FUNC;
                t.op = fop;
            } else {
                set_err(err_out, err_size, "Simbolo desconocido");
                return false;
            }
        } else if (*p == '+') {
            t.type = TOK_OP; t.op = OP_ADD; p++;
        } else if (*p == '-') {
            t.type = TOK_OP; t.op = OP_SUB; p++;
        } else if (*p == '*') {
            t.type = TOK_OP; t.op = OP_MUL; p++;
        } else if (*p == '/') {
            t.type = TOK_OP; t.op = OP_DIV; p++;
        } else if (*p == '^') {
            t.type = TOK_OP; t.op = OP_POW; p++;
        } else if (*p == '(') {
            t.type = TOK_LPAREN; p++;
        } else if (*p == ')') {
            t.type = TOK_RPAREN; p++;
        } else {
            set_err(err_out, err_size, "Caracter invalido");
            return false;
        }

        /* Implicit multiplication: e.g. 2x, 2sin(x), (x)(t), x(x+1), sin(x)cos(t) */
        if (ntoks > 0) {
            TokType prev = toks[ntoks - 1].type;
            bool prev_is_val = (prev == TOK_NUM || prev == TOK_VAR_X || prev == TOK_VAR_T || prev == TOK_RPAREN);
            bool curr_is_val_start = (t.type == TOK_NUM || t.type == TOK_VAR_X || t.type == TOK_VAR_T ||
                                      t.type == TOK_FUNC || t.type == TOK_LPAREN);
            if (prev_is_val && curr_is_val_start) {
                toks[ntoks].type = TOK_OP;
                toks[ntoks].op = OP_MUL;
                toks[ntoks].val = 0.0f;
                ntoks++;
            }
        }

        /* Unary + / - handling */
        if (t.type == TOK_OP && (t.op == OP_SUB || t.op == OP_ADD)) {
            bool is_unary = (ntoks == 0 ||
                             toks[ntoks - 1].type == TOK_OP ||
                             toks[ntoks - 1].type == TOK_LPAREN ||
                             toks[ntoks - 1].type == TOK_FUNC);
            if (is_unary) {
                if (t.op == OP_ADD) {
                    continue; /* ignore unary + */
                }
                t.op = OP_NEG;
            }
        }

        toks[ntoks++] = t;
    }

    if (ntoks == 0) {
        set_err(err_out, err_size, "Funcion vacia");
        return false;
    }

    /* Shunting-Yard to RPN */
    Token opstack[EXPR_MAX_OPS];
    int sp = 0;

    for (int i = 0; i < ntoks; i++) {
        Token t = toks[i];
        if (t.type == TOK_NUM || t.type == TOK_VAR_X || t.type == TOK_VAR_T) {
            if (!emit_inst(out, t.op, t.val, err_out, err_size)) return false;
        } else if (t.type == TOK_FUNC) {
            opstack[sp++] = t;
        } else if (t.type == TOK_OP) {
            int p1 = op_prec(t.op);
            bool ra = op_right_assoc(t.op);
            while (sp > 0 && (opstack[sp - 1].type == TOK_OP || opstack[sp - 1].type == TOK_FUNC)) {
                int p2 = op_prec(opstack[sp - 1].op);
                if ((!ra && p1 <= p2) || (ra && p1 < p2)) {
                    if (!emit_inst(out, opstack[--sp].op, 0.0f, err_out, err_size)) return false;
                } else {
                    break;
                }
            }
            opstack[sp++] = t;
        } else if (t.type == TOK_LPAREN) {
            opstack[sp++] = t;
        } else if (t.type == TOK_RPAREN) {
            bool found_lparen = false;
            while (sp > 0) {
                Token top = opstack[--sp];
                if (top.type == TOK_LPAREN) {
                    found_lparen = true;
                    break;
                }
                if (!emit_inst(out, top.op, 0.0f, err_out, err_size)) return false;
            }
            if (!found_lparen) {
                set_err(err_out, err_size, "Parentesis ')' sin abrir");
                return false;
            }
            if (sp > 0 && opstack[sp - 1].type == TOK_FUNC) {
                if (!emit_inst(out, opstack[--sp].op, 0.0f, err_out, err_size)) return false;
            }
        }
    }

    while (sp > 0) {
        Token top = opstack[--sp];
        if (top.type == TOK_LPAREN) {
            set_err(err_out, err_size, "Falta cerrar ')'");
            return false;
        }
        if (!emit_inst(out, top.op, 0.0f, err_out, err_size)) return false;
    }

    /* Validate stack depth on compiled RPN */
    int depth = 0;
    for (int i = 0; i < out->len; i++) {
        ExprOpCode op = out->code[i].op;
        if (op == OP_NUM || op == OP_VAR_X || op == OP_VAR_T) {
            depth++;
        } else if (op == OP_NEG || is_func_op(op)) {
            if (depth < 1) {
                set_err(err_out, err_size, "Falta operando");
                return false;
            }
        } else {
            if (depth < 2) {
                set_err(err_out, err_size, "Error de sintaxis");
                return false;
            }
            depth--;
        }
        if (depth >= 64) {
            set_err(err_out, err_size, "Expresion muy anidada");
            return false;
        }
    }
    if (depth != 1) {
        set_err(err_out, err_size, "Error de sintaxis");
        return false;
    }

    return true;
}

float expr_eval(const ExprCompiled *prog, float x, float t)
{
    float stack[64];
    int sp = 0;

    for (int i = 0; i < prog->len; i++) {
        const ExprInst *ins = &prog->code[i];
        switch (ins->op) {
            case OP_NUM:   stack[sp++] = ins->val; break;
            case OP_VAR_X: stack[sp++] = x; break;
            case OP_VAR_T: stack[sp++] = t; break;
            case OP_ADD:   stack[sp - 2] += stack[sp - 1]; sp--; break;
            case OP_SUB:   stack[sp - 2] -= stack[sp - 1]; sp--; break;
            case OP_MUL:   stack[sp - 2] *= stack[sp - 1]; sp--; break;
            case OP_DIV: {
                float b = stack[--sp];
                if (fabsf(b) < 1e-12f) return NAN;
                stack[sp - 1] /= b;
                break;
            }
            case OP_POW: {
                float b = stack[--sp];
                stack[sp - 1] = powf(stack[sp - 1], b);
                break;
            }
            case OP_NEG:   stack[sp - 1] = -stack[sp - 1]; break;
            case OP_SIN:   stack[sp - 1] = sinf(stack[sp - 1]); break;
            case OP_COS:   stack[sp - 1] = cosf(stack[sp - 1]); break;
            case OP_TAN:   stack[sp - 1] = tanf(stack[sp - 1]); break;
            case OP_ABS:   stack[sp - 1] = fabsf(stack[sp - 1]); break;
            case OP_SQRT:  stack[sp - 1] = sqrtf(stack[sp - 1]); break;
            case OP_LN:    stack[sp - 1] = logf(stack[sp - 1]); break;
            case OP_LOG:   stack[sp - 1] = log10f(stack[sp - 1]); break;
            case OP_EXP:   stack[sp - 1] = expf(stack[sp - 1]); break;
            case OP_ASIN:  stack[sp - 1] = asinf(stack[sp - 1]); break;
            case OP_ACOS:  stack[sp - 1] = acosf(stack[sp - 1]); break;
            case OP_ATAN:  stack[sp - 1] = atanf(stack[sp - 1]); break;
            case OP_FLOOR: stack[sp - 1] = floorf(stack[sp - 1]); break;
            case OP_CEIL:  stack[sp - 1] = ceilf(stack[sp - 1]); break;
        }
    }
    return (sp == 1) ? stack[0] : NAN;
}
