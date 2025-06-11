#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "predict_table.h"
#include "grammar.h"
#include "first_follow.h"

/* ──────────────── 全局静态变量 ──────────────── */

/* parseTable[i][j] 存储：第 i 个非终结符（grammar->nonterminals[i]）在遇到
 * j-th 终结符时所对应的产生式编号（rule index）。若为 -1，表示该单元无产生式。 */
static int **parseTable = NULL;

/* 非终结符数量（行数） */
static int vn_count = 0;

/* 终结符数量（不含 '#'，不含 '$'），列数 = vt_count + 1 */
static int vt_count = 0;

/* 终结符列表（不含 '#'，不含 '$'），长度为 vt_count */
static char *vt_list = NULL;

/* ──────────────── 工具函数 ──────────────── */

/**
 * 在 grammar->nonterminals 数组中查找符号 A 的索引。
 * 找不到返回 -1。
 */
static int find_nonterminal_index(Grammar* grammar, char A) {
    for (int i = 0; i < grammar->nonterminals_count; i++) {
        if (grammar->nonterminals[i] == A) return i;
    }
    return -1;
}

/**
 * 在 vt_list 中查找终结符 t 的索引（不含 '$'）。
 * 找不到返回 -1。
 */
static int find_terminal_index(char t) {
    for (int i = 0; i < vt_count; i++) {
        if (vt_list[i] == t) return i;
    }
    return -1;
}

/**
 * 从 sets 数组里取出某非终结符 A 的 FIRST 集合字符串
 * （以 '\0' 结尾，内容可能含 '#' 表示空串）。
 * 找不到返回空字符串。
 */
static const char* get_first_set(SymbolSet* sets, int set_count, char A) {
    for (int i = 0; i < set_count; i++) {
        if (sets[i].symbol == A) {
            return sets[i].first;
        }
    }
    return "";
}

/**
 * 从 sets 数组里取出某非终结符 A 的 FOLLOW 集合字符串
 * （以 '\0' 结尾，内容含 '$' 表示输入结束符）。
 * 找不到返回空字符串。
 */
static const char* get_follow_set(SymbolSet* sets, int set_count, char A) {
    for (int i = 0; i < set_count; i++) {
        if (sets[i].symbol == A) {
            return sets[i].follow;
        }
    }
    return "";
}

/**
 * 计算文法右部字符串 alpha 的 FIRST(alpha)，结果写入 first_alpha 缓冲区中，
 * 以 '\0' 结尾。first_alpha 的容量应至少为 GRAMMAR_MAX_SYMBOLS+1。
 * 
 * 算法：
 *   1. 从 alpha 的第一个非空白字符开始：
 *      - 如果是 '#'(epsilon)，则先把 '#' 加入 first_alpha，但继续看下一个符号；
 *      - 如果是终结符 a (不含 '#')，则把 a 加入 first_alpha，停止；
 *      - 如果是非终结符 B，则把 FIRST(B) 中除了 '#' 以外的符号全加入 first_alpha，
 *        如果 FIRST(B) 中含 '#'，则继续看 alpha 的下一个符号；否则停止。
 *   2. 如果 alpha 所有符号都能推出 epsilon（或 alpha 为空串），也把 '#' 加入 first_alpha。
 */
static void get_first_of_string(const char* alpha, SymbolSet* sets, int set_count, char* first_alpha) {
    int pos = 0;
    int epsilon_possible = 1;  /* 标记从当前位置开始其余部分是否可能都产生 epsilon */
    size_t len = strlen(alpha);

    /* 遍历 alpha 中的每个字符 (跳过空白) */
    for (size_t i = 0; i < len; i++) {
        char X = alpha[i];
        if (isspace((unsigned char)X)) continue;

        /* 如果遇到 epsilon 符号 '#' */
        if (X == '#') {
            /* 把 '#' 加入 first_alpha（仅一次） */
            if (!strchr(first_alpha, '#')) {
                first_alpha[pos++] = '#';
                first_alpha[pos] = '\0';
            }
            /* 继续看后续符号，epsilon_possible 保持为 1 */
            continue;
        }

        /* 如果是终结符（排除了 '#'） */
        if (grammar_is_terminal(X)) {
            if (!strchr(first_alpha, X)) {
                first_alpha[pos++] = X;
                first_alpha[pos] = '\0';
            }
            epsilon_possible = 0;  /* 终结符一旦加入，后继部分不再能推出 epsilon */
            break;
        }

        /* X 是非终结符 */
        {
            const char* firstX = get_first_set(sets, set_count, X);
            int has_epsilon = 0;
            for (int k = 0; firstX[k]; k++) {
                if (firstX[k] == '#') {
                    has_epsilon = 1;
                } else {
                    if (!strchr(first_alpha, firstX[k])) {
                        first_alpha[pos++] = firstX[k];
                        first_alpha[pos] = '\0';
                    }
                }
            }
            if (!has_epsilon) {
                /* FIRST(X) 中不含 epsilon，就不再往后看 */
                epsilon_possible = 0;
                break;
            }
            /* 否则继续看看 alpha 的下一个符号 */
        }
    }

    /* 如果 alpha 整体都能推出 epsilon，就加入 '#' */
    if (epsilon_possible) {
        if (!strchr(first_alpha, '#')) {
            first_alpha[pos++] = '#';
            first_alpha[pos] = '\0';
        }
    }
}

/* ──────────────── 公有函数 ──────────────── */

/**
 * 构造 LL(1) 预测分析表
 */
void construct_parse_table(Grammar* grammar, SymbolSet* sets, int set_count) {
    /* 1. 从 grammar->terminals 中收集实际用于 parse table 的终结符（排除 '#'） */
    vt_count = 0;
    /* 最多 grammar->terminals_count 个位置 */
    vt_list = (char*)malloc(grammar->terminals_count * sizeof(char));
    for (int i = 0; i < grammar->terminals_count; i++) {
        char t = grammar->terminals[i];
        if (t == '#') continue;
        /* 其余都是有效的终结符 */
        vt_list[vt_count++] = t;
    }
    /* 表的列数 = vt_count + 1，其中最后一列是 '$' */

    /* 2. 非终结符数量 */
    vn_count = grammar->nonterminals_count;

    /* 3. 分配 parseTable，大�点 vn_count 行, vt_count+1 列 */
    parseTable = (int**)malloc(vn_count * sizeof(int*));
    for (int i = 0; i < vn_count; i++) {
        parseTable[i] = (int*)malloc((vt_count + 1) * sizeof(int));
        for (int j = 0; j <= vt_count; j++) {
            parseTable[i][j] = -1;
        }
    }

    /* 4. 遍历每个规则，填充 parseTable */
    for (int p = 0; p < grammar->rule_count; p++) {
        Rule* rule = &grammar->rules[p];
        char A = rule->left_hs;
        int row = find_nonterminal_index(grammar, A);
        if (row < 0) continue;  /* 保险起见 */

        /* 4.1 先求 FIRST(alpha) */
        char first_alpha[GRAMMAR_MAX_SYMBOLS + 1];
        first_alpha[0] = '\0';
        get_first_of_string(rule->right_hs, sets, set_count, first_alpha);

        /* 4.2 FIRST(alpha) 中除 '#' 外的终结符，直接填表 */
        for (int k = 0; first_alpha[k]; k++) {
            char a = first_alpha[k];
            if (a == '#') continue;
            int col = find_terminal_index(a);
            if (col >= 0) {
                parseTable[row][col] = p;
            }
        }

        /* 4.3 如果 FIRST(alpha) 包含 epsilon('#')，则对 FOLLOW(A) 中所有符号 b 填表 */
        if (strchr(first_alpha, '#')) {
            const char* followA = get_follow_set(sets, set_count, A);
            for (int k = 0; followA[k]; k++) {
                char b = followA[k];
                if (b == '$') {
                    /* '$' 列索引为 vt_count */
                    parseTable[row][vt_count] = p;
                } else {
                    int col = find_terminal_index(b);
                    if (col >= 0) {
                        parseTable[row][col] = p;
                    }
                }
            }
        }
    }
}

/**
 * 打印 LL(1) 预测分析表到终端
 *
 * 修改后：当无对应产生式时，单元格显示 "-"
 *
 * 输出格式：
 *      （制表符）t1   t2   ...   tn   $ 
 *    A1  A1->right ...   -   ...  ...
 *    A2  ...
 */
void print_parse_table(Grammar* grammar, SymbolSet* sets, int set_count) {
    /* 打印第一行：所有终结符 + '$' */
    printf("\t");
    for (int j = 0; j < vt_count; j++) {
        printf("%c\t\t", vt_list[j]);
    }
    printf("$\n");

    /* 每一行对应一个非终结符 */
    for (int i = 0; i < vn_count; i++) {
        char A = grammar->nonterminals[i];
        printf("%c\t", A);

        /* 打印每个终结符列，若无产生式则输出 "-" */
        for (int j = 0; j < vt_count; j++) {
            int prod_idx = parseTable[i][j];
            if (prod_idx >= 0) {
                Rule* rule = &grammar->rules[prod_idx];
                printf("%c->%s\t", rule->left_hs, rule->right_hs);
            } else {
                /* 无产生式时显示 "-" */
                printf("-\t\t");
            }
        }

        /* 打印 '$' 列，若无产生式则输出 "-" */
        int prod_idx = parseTable[i][vt_count];
        if (prod_idx >= 0) {
            Rule* rule = &grammar->rules[prod_idx];
            printf("%c->%s", rule->left_hs, rule->right_hs);
        } else {
            /* 无产生式时显示 "-" */
            printf("-");
        }
        printf("\n");
    }
}
