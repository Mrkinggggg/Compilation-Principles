#include "parser.h"
#include "lexer.h"

// 前向声明
static State* parse_expression(const char** regex, struct Arena* arena);
static State* parse_term(const char** regex, struct Arena* arena);
static State* parse_factor(const char** regex, struct Arena* arena);

// 解析表达式：term | term | ...
static State* parse_expression(const char** regex, struct Arena* arena) {
    State* start = create_state(arena);
    State* current = start;
    
    // 解析第一个 term
    State* term_start = parse_term(regex, arena);
    add_transition(arena, current, '\0', term_start);
    
    // 查找后续的 | 操作符
    const char* p = *regex;
    Token* tok = lexer_next_token(&p, arena);
    
    while (tok->type == T_UNION) {
        *regex = p; // 消费 | 符号
        
        // 解析下一个 term
        State* next_term = parse_term(regex, arena);
        add_transition(arena, start, '\0', next_term);
        
        // 检查是否还有更多 | 操作符
        p = *regex;
        tok = lexer_next_token(&p, arena);
    }
    
    return start;
}

// 解析项：factor factor ...
static State* parse_term(const char** regex, struct Arena* arena) {
    State* start = create_state(arena);
    State* current = start;
    
    // 解析第一个 factor
    State* factor = parse_factor(regex, arena);
    add_transition(arena, current, '\0', factor);
    
    // 查找后续的 factor
    const char* p = *regex;
    Token* tok = lexer_next_token(&p, arena);
    
    while (tok->type == T_CHAR) {
        // 解析下一个 factor
        State* next_factor = parse_factor(regex, arena);
        
        // 找到当前链中的最后一个接受状态
        State* last_accept = NULL;
        for (Transition* t = current->transitions; t != NULL; t = t->next) {
            if (t->symbol == '\0') {
                State* s = t->target;
                while (s->transitions != NULL) {
                    Transition* next_t = s->transitions;
                    if (next_t->target->is_accepting) {
                        last_accept = next_t->target;
                        break;
                    }
                    s = next_t->target;
                }
            }
        }
        
        if (last_accept != NULL) {
            last_accept->is_accepting = 0;
            add_transition(arena, last_accept, '\0', next_factor);
        }
        
        // 检查是否还有更多 factor
        p = *regex;
        tok = lexer_next_token(&p, arena);
    }
    
    return start;
}

// 解析因子：char | char* | char+ | char?
static State* parse_factor(const char** regex, struct Arena* arena) {
    const char* p = *regex;
    Token* tok = lexer_next_token(&p, arena);
    
    if (tok->type != T_CHAR) {
        // 错误处理：期望一个字符
        State* error = create_state(arena);
        error->is_accepting = 0;
        return error;
    }
    
    *regex = p; // 消费字符
    char c = tok->value;
    
    // 创建基本的字符 NFA
    NFA char_nfa = create_char_nfa(arena, c);
    
    // 检查后缀操作符
    p = *regex;
    tok = lexer_next_token(&p, arena);
    
    if (tok->type == T_STAR) {
        *regex = p; // 消费 *
        NFA star_nfa = create_star_nfa(arena, char_nfa);
        return star_nfa.start;
    } else if (tok->type == T_PLUS) {
        *regex = p; // 消费 +
        NFA plus_nfa = create_plus_nfa(arena, char_nfa);
        return plus_nfa.start;
    } else if (tok->type == T_QUESTION) {
        *regex = p; // 消费 ?
        NFA question_nfa = create_question_nfa(arena, char_nfa);
        return question_nfa.start;
    }
    
    return char_nfa.start;
}

State* parse_regex(const char* regex, struct Arena* arena) {
    const char* p = regex;
    State* start = parse_expression(&p, arena);
    return start;
}