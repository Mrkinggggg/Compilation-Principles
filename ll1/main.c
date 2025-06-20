#define UNITY_BUILD // 启用 Unity Build

#include <stdio.h>
#include <stdlib.h> 

// Arena
#include "src/arena.h"      
#include "src/arena.c"

#include "src/first_follow.h"
#include "src/first_follow.c"

#include "src/grammar.h"
#include "src/grammar.c"

#include "src/first_set.h"
#include "src/first_set.c"

#include "src/follow_set.h"
#include "src/follow_set.c"

#include "src/predict_table.h"
#include "src/predict_table.c"

int main(){
    struct Arena* arena = arena_create(1024*10);
    char filename[] = "grammar.txt";

    GrammarResultGrammar result = read_grammar(filename, arena);
    if (result.status != GRAMMAR_OK) {
        fprintf(stderr, "Error: Failed to read grammar from file. Status code: %d\n", result.status);
        arena_free(arena);
        return 1;
    }

    Grammar* grammar = result.value;
    print_grammar(grammar);

    SymbolSet* sets = arena_alloc(arena, GRAMMAR_MAX_SYMBOLS * sizeof(SymbolSet));
    if(!sets){
        fprintf(stderr, "Error: Failed to allocate memory for sets of symbolset.\n");
        return 1; 
    }
    int set_count = 0;
    
    printf("---非终结符的First集---\n");
    compute_first_sets(grammar, sets, &set_count, arena);
    for(int i = 0; i < set_count; i++){
        printf("First set[%c] : %s\n", sets[i].symbol, sets[i].first);
    }
    
    printf("---非终结符的Follow集---\n");
    compute_follow_sets(grammar, sets, &set_count, arena);
    for(int i = 0; i < set_count; i++){
        printf("Follow set[%c] : %s\n", sets[i].symbol, sets[i].follow);
    }

    printf("--- LL(1) 预测分析表 ---\n");
        construct_parse_table(grammar, sets, set_count);
        print_parse_table(grammar, sets, set_count);

    arena_free(arena);
    return 0;
}