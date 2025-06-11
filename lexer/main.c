// tmain.c
#define UNITY_BUILD // 启用 Unity Build

#include <stdio.h>
#include <stdlib.h> // For exit, EXIT_FAILURE

// --- Include ALL necessary .h and .c files ---

// Arena
#include "src/arena.h"      // Assuming path structure
#include "src/arena.c"

// NFA (Fragment Version)
#include "src/nfa.h"
#include "src/nfa.c"

// Lexer
#include "src/lexer.h"      // Make sure lexer.h defines Token struct and types
#include "src/lexer.c"

#include "src/matcher.h"      // Make sure lexer.h defines Token struct and types
#include "src/matcher.c"

#include "src/parser.h"      // Make sure lexer.h defines Token struct and types
#include "src/parser.c"

int main() {
    struct Arena* arena = arena_create(1024 * 10);

    // 测试不同的正则表达式
    const char* regex_tests[] = {
        "ab*a",      
        "aa|bb",       // 并集测试
        "ab+a",      // 加号测试
        "ab?a",      // 问号测试
    };
    
    const char* input_tests[] = {
        "abba",      // 匹配 ab*a
        "a",         // 匹配 a|b
        "abba",      // 匹配 ab+a
        "aba",        // 匹配 ab?a
    };
    
    for (int i = 0; i < 4; i++) {
        const char* regex = regex_tests[i];
        const char* input = input_tests[i];
        
        State* start = parse_regex(regex, arena);
        int match = simulate_nfa(start, input, arena);
        
        printf("Regex: \"%s\"\nInput: \"%s\"\nMatch: %s\n\n",
            regex, input, match ? "YES" : "NO");
    }

    arena_free(arena);
    return 0;
}