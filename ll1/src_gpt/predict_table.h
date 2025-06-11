#ifndef PREDICT_TABLE_H
#define PREDICT_TABLE_H

#include "grammar.h"
#include "first_follow.h"
#include <stdbool.h>

typedef struct {
    // 这里假定 GRAMMAR_MAX_SYMBOLS 足够大，可以容纳“终结符数量 + 1”（多出的 1 用于 '$' 列）
    int rule_index[GRAMMAR_MAX_SYMBOLS][GRAMMAR_MAX_SYMBOLS];
    // [非终结符 下标][终结符 下标] 存储产生式编号，-1 表示空
    // “终结符 下标” 范围内的最后一个索引（即 grammar->terminals_count）留作 '$' 列
} PredictTable;

void build_predict_table(Grammar* grammar, SymbolSet* sets, int set_count, PredictTable* table);
void print_predict_table(Grammar* grammar, PredictTable* table);

#endif
