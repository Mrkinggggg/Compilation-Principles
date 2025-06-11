#ifndef PREDICT_TABLE_H
#define PREDICT_TABLE_H

#include "grammar.h"
#include "first_follow.h"

/**
 * 构造 LL(1) 预测分析表
 * @param grammar   已读取好的文法结构
 * @param sets      包含所有非终结符的 FIRST 和 FOLLOW 集合数组
 * @param set_count sets 数组中有效条目的数量
 */
void construct_parse_table(Grammar* grammar, SymbolSet* sets, int set_count);

/**
 * 打印 LL(1) 预测分析表到终端
 * @param grammar   已读取好的文法结构
 * @param sets      包含所有非终结符的 FIRST 和 FOLLOW 集合数组
 * @param set_count sets 数组中有效条目的数量
 */
void print_parse_table(Grammar* grammar, SymbolSet* sets, int set_count);

#endif /* PREDICT_TABLE_H */
