#ifndef EXPR_H
#define EXPR_H

#include <stdbool.h>

#define EXPR_MAX_OPS 256
#define EXPR_ERR_LEN 48

typedef enum {
    OP_NUM,
    OP_VAR_X,
    OP_VAR_T,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_POW,
    OP_NEG,
    OP_SIN,
    OP_COS,
    OP_TAN,
    OP_ABS,
    OP_SQRT,
    OP_LN,
    OP_LOG,
    OP_EXP,
    OP_ASIN,
    OP_ACOS,
    OP_ATAN,
    OP_FLOOR,
    OP_CEIL
} ExprOpCode;

typedef struct {
    ExprOpCode op;
    float val;
} ExprInst;

typedef struct {
    ExprInst code[EXPR_MAX_OPS];
    int len;
    bool uses_t;
} ExprCompiled;

/* Compiles a math expression string into RPN bytecode.
 * Returns true on success, false on error (writes message to err_out). */
bool expr_compile(const char *src, ExprCompiled *out, char *err_out, int err_size);

/* Evaluates compiled bytecode for given (x, t). Returns NAN on domain/math error. */
float expr_eval(const ExprCompiled *prog, float x, float t);

#endif /* EXPR_H */
