#include "predict_table.h"
#include <stdio.h>
#include <string.h>

/*
 * build_predict_table: 将每个 (非终结符 A, 终结符 a 或 '$') 对应的产生式编号
 * 填到 table->rule_index 中，找不到时为 -1。
 *
 * 约定：
 *   - grammar->terminals 中不包括 '$'，但我们把 rule_index[*][grammar->terminals_count]
 *     这一列专门当作 “$” 列来用。
 *   - First 集里以 '#' 表示 ε。构造 First 时跳过 '#'，不会填到表中。
 *   - Follow 集里可能包含 '$'，此时代码会把它映射到列索引 grammar->terminals_count。
 */
void build_predict_table(Grammar* grammar, SymbolSet* sets, int set_count, PredictTable* table) {
    // 1) 初始化所有单元格为 -1
    memset(table->rule_index, -1, sizeof(table->rule_index));

    // 2) 遍历文法中的每条产生式
    for (int i = 0; i < grammar->rule_count; ++i) {
        Rule* rule = &grammar->rules[i];
        char A = rule->left_hs;

        // 2.1) 找到 A 对应的 First/Follow 集合
        SymbolSet* A_set = NULL;
        for (int s = 0; s < set_count; ++s) {
            if (sets[s].symbol == A) {
                A_set = &sets[s];
                break;
            }
        }
        if (!A_set) continue; // 若找不到对应的 First/Follow 集就跳过

        // 3) 对 First(右部) 中的每个非 ε 的终结符 a，将产生式 i 填到 M[A, a]
        for (int f = 0; A_set->first[f]; ++f) {
            char a = A_set->first[f];
            if (a != '#') {
                // 在 grammar->terminals 中寻找 a
                char* pos = strchr(grammar->terminals, a);
                if (pos) {
                    int row = strchr(grammar->nonterminals, A) - grammar->nonterminals;
                    int col = pos - grammar->terminals;
                    if (row >= 0 && col >= 0) {
                        table->rule_index[row][col] = i;
                    }
                }
            }
        }

        // 4) 如果 First(右部) 包含 ε（即 '#')，则对 Follow(A) 中的每个符号 b：
        //    - 若 b == '$'，映射到 col = grammar->terminals_count；
        //    - 否则 b 必须是 grammar->terminals 中的某个终结符，映射到相应列。
        if (strchr(A_set->first, '#')) {
            for (int f = 0; A_set->follow[f]; ++f) {
                char b = A_set->follow[f];
                int row = strchr(grammar->nonterminals, A) - grammar->nonterminals;
                int col;
                if (b == '$') {
                    col = grammar->terminals_count; 
                    // 约定最后一列（索引 = grammar->terminals_count）用来存 '$'
                } else {
                    char* pos = strchr(grammar->terminals, b);
                    if (!pos) continue;
                    col = pos - grammar->terminals;
                }
                if (row >= 0 && col >= 0) {
                    table->rule_index[row][col] = i;
                }
            }
        }
    }
}

/*
 * print_predict_table: 将预测分析表以可读方式打印出来：
 *   - 表头列出所有真实的终结符（跳过 '#'），后面再加一列 '$'；
 *   - 每个单元格若 rule_index[row][col] >= 0，则输出对应产生式 “A->right_hs”，否则输出 “-”。
 */
void print_predict_table(Grammar* grammar, PredictTable* table) {
    // 1) 先收集“真正要打印”的终结符：跳过 '#'，将其余放到数组 print_terms[]
    char print_terms[GRAMMAR_MAX_SYMBOLS];
    int print_count = 0;
    for (int t = 0; t < grammar->terminals_count; ++t) {
        char c = grammar->terminals[t];
        if (c == '#') continue; // 跳过用作 epsilon 标记的 '#'
        print_terms[print_count++] = c;
    }
    // print_count 个终结符后面，再加一个 '$'
    
    // 2) 打印表头
    printf("\nLL(1) 预测分析表:\n\t");
    for (int i = 0; i < print_count; ++i) {
        printf("%c\t", print_terms[i]);
    }
    printf("$\n"); // 最后一列是 '$'
    
    // 3) 对每个非终结符 n，打印一行
    for (int n = 0; n < grammar->nonterminals_count; ++n) {
        char A = grammar->nonterminals[n];
        printf("%c\t", A);

        // 3.1) 先针对每个 print_terms[i] 打印相应单元格
        for (int i = 0; i < print_count; ++i) {
            // 找到这个终结符在 grammar->terminals 中的原始列索引
            char term = print_terms[i];
            int real_col = strchr(grammar->terminals, term) - grammar->terminals;
            int idx = table->rule_index[n][real_col];
            if (idx >= 0) {
                Rule* r = &grammar->rules[idx];
                printf("%c->%s\t", r->left_hs, r->right_hs);
            } else {
                printf("-\t");
            }
        }

        // 3.2) 最后再打印 '$' 列
        int dollar_col = grammar->terminals_count;
        int idx_dollar = table->rule_index[n][dollar_col];
        if (idx_dollar >= 0) {
            Rule* r = &grammar->rules[idx_dollar];
            printf("%c->%s\t", r->left_hs, r->right_hs);
        } else {
            printf("-\t");
        }

        printf("\n");
    }
}
