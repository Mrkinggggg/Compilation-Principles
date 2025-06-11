## 一、NFA 构建函数的修改

### 1. 原项目中相关代码

```c
// nfa.c
#include "nfa.h"
#include <stdlib.h>

State* create_state(struct Arena* arena) {
    State* s = (State*)arena_alloc(arena, sizeof(State));
    s->is_accepting = 0;
    s->transitions = NULL;
    return s;
}

void add_transition(struct Arena* arena, State* from, char symbol, State* to) {
    Transition* t = (Transition*)arena_alloc(arena, sizeof(Transition));
    t->symbol = symbol;
    t->target = to;
    t->next = from->transitions;
    from->transitions = t;
}

NFA create_char_nfa(struct Arena* arena, char c) {
    State* start = create_state(arena);
    State* accept = create_state(arena);
    add_transition(arena, start, c, accept);
    accept->is_accepting = 1;
    return (NFA){start, accept};
}

NFA create_concat_nfa(struct Arena* arena, NFA a, NFA b) {
    add_transition(arena, a.accept, '\0', b.start);
    a.accept->is_accepting = 0;
    return (NFA){a.start, b.accept};
}

NFA create_star_nfa(struct Arena* arena, NFA inner) {
    State* start = create_state(arena);
    State* accept = create_state(arena);
    add_transition(arena, start, '\0', inner.start);
    add_transition(arena, start, '\0', accept);
    add_transition(arena, inner.accept, '\0', inner.start);
    add_transition(arena, inner.accept, '\0', accept);
    inner.accept->is_accepting = 0;
    accept->is_accepting = 1;
    return (NFA){start, accept};
}
```

* **功能简述**

  1. `create_char_nfa`：针对单个字母 `c`，生成一个仅包含一条从 `start`→`accept`（符号为 `c`）的 NFA。
  2. `create_concat_nfa`：将已有的 NFA `a`、`b` 串联起来，做法是在 `a.accept`→ε→`b.start`，并取消 `a.accept` 原先的接受状态；结果返回一个新的 start（`a.start`）和接受状态（`b.accept`）。
  3. `create_star_nfa`：将传入的 NFA `inner` 扩展为 “零次或多次” 形式（`*`），新建一个外层 `start`、`accept`，在它们之间及 `inner.accept` 处添加多条 ε→连线，并把 `inner.accept` 取消最终接受标记，最后仅保留新 `accept`。

* **局限性**

  * 原项目只实现了单字符、串联、和 `*`（Kleene star）三种构造；无法直接生成 “`+`”（至少一次）、“`?`”（零次或一次）或“并集” (`|`) 这几类常见正则操作符对应的 NFA。

### 2. 新增/修改的部分

您在此基础上新增了以下三个工厂函数，用于扩展 NFA 能力，使其支持更多正则运算符：

```c
// nfa.c （新增部分）

// 1) create_plus_nfa：针对 “A+” 运算符（至少出现一次）
//    相当于先匹配一次 inner，再循环匹配 zero-or-more 次 inner
NFA create_plus_nfa(struct Arena* arena, NFA inner) {
    State* start = create_state(arena);
    State* accept = create_state(arena);
    add_transition(arena, start, '\0', inner.start);
    add_transition(arena, inner.accept, '\0', inner.start);
    add_transition(arena, inner.accept, '\0', accept);
    inner.accept->is_accepting = 0;
    accept->is_accepting = 1;
    return (NFA){start, accept};
}

// 2) create_question_nfa：针对 “A?” 运算符（零次或一次）
//    在 start 处直接可以跳到 accept（零次），
//    或者通过 inner 跳到 inner.accept 再跳到 accept（一次）
NFA create_question_nfa(struct Arena* arena, NFA inner) {
    State* start = create_state(arena);
    State* accept = create_state(arena);
    add_transition(arena, start, '\0', inner.start);
    add_transition(arena, start, '\0', accept);
    add_transition(arena, inner.accept, '\0', accept);
    inner.accept->is_accepting = 0;
    accept->is_accepting = 1;
    return (NFA){start, accept};
}

// 3) create_union_nfa：针对 “A|B” 并集运算符
//    新建一个外层 start、accept，分别 ε 跳到 a.start 与 b.start，
//    并把 a.accept, b.accept 两个地方都 ε 跳到外层 accept
NFA create_union_nfa(struct Arena* arena, NFA a, NFA b) {
    State* start = create_state(arena);
    State* accept = create_state(arena);
    add_transition(arena, start, '\0', a.start);
    add_transition(arena, start, '\0', b.start);
    add_transition(arena, a.accept, '\0', accept);
    add_transition(arena, b.accept, '\0', accept);
    a.accept->is_accepting = 0;
    b.accept->is_accepting = 0;
    accept->is_accepting = 1;
    return (NFA){start, accept};
}
```

#### 2.1 `create_plus_nfa` 的作用与实现

* **需求背景**：
  在正则表达式中，运算符 `+` 表示“至少出现一次”。对于一个子 NFA `inner`（匹配 A），`A+` 应当先匹配一次 A，然后可以无限制地继续匹配更多次 A，最后结束。

* **实现思路**：

  1. 新建外层 `start`、`accept` 状态。
  2. 从外层 `start` →ε→ `inner.start`：表示“第一次匹配”。
  3. 从 `inner.accept` →ε→ `inner.start`：表示“匹配完一次后可循环继续匹配（至少一次）”。
  4. 从 `inner.accept` →ε→ 外层 `accept`：表示“匹配完一次或多次后结束，进入最终接受”。
  5. 取消 `inner.accept` 的接受标记，仅保留外层 `accept`。

* **最终效果**：

  * 对于字符串 “A”：`start`→ε→`inner.start`（匹配一次）→`inner.accept`→ε→`accept`。
  * 对于 “AA” 或 “AAA”…：同理多次 “`inner.accept`→ε→`inner.start`→…→ε→`accept`”。

#### 2.2 `create_question_nfa` 的作用与实现

* **需求背景**：
  运算符 `?` 表示“零次或一次”。对于子 NFA `inner`（匹配 A），`A?` 要么跳过 A（匹配空串），要么匹配一次 A。

* **实现思路**：

  1. 新建外层 `start`、`accept` 状态。
  2. 从外层 `start` →ε→ `inner.start`：表示选择匹配一次。
  3. 从外层 `start` →ε→ 外层 `accept`：表示选择不匹配，直接跳到最终接受（匹配空串）。
  4. 从 `inner.accept` →ε→ 外层 `accept`：匹配完一次 A 后结束。
  5. 取消 `inner.accept` 的接受标记，仅保留外层 `accept`。

* **最终效果**：

  * 对于空串 `""`：`start`→ε→`accept`，转一次就到达 `accept`。
  * 对于单字符 “A”：`start`→ε→`inner.start`（匹配一次）→`inner.accept`→ε→`accept`。

#### 2.3 `create_union_nfa` 的作用与实现

* **需求背景**：
  运算符 `|` 表示并集（“或”），即 `A|B` 应当接受能够匹配 A 的任何串，也接受能匹配 B 的任何串。

* **实现思路**：

  1. 新建外层 `start`、`accept`。
  2. `start` →ε→ `a.start`，以及 `start` →ε→ `b.start`：表示“分叉去匹配 A 或者匹配 B”。
  3. `a.accept` →ε→ `accept`，`b.accept` →ε→ `accept`：当子 NFA 完成后都跳到同一个最终接受。
  4. 取消 `a.accept`、`b.accept` 原先的接受标记，仅保留外层 `accept`。

* **最终效果**：

  * 对于能被 `a` 接受的串：`start`→ε→`a.start`…→`a.accept`→ε→`accept`，匹配成功；
  * 对于能被 `b` 接受的串：同理跳到 `b.start` 这一支，再到 `b.accept`→ε→`accept`。

### 3. 整体改动总结

1. **函数新增**

   * 新增了 `create_plus_nfa`、`create_question_nfa`、`create_union_nfa` 三个函数，以支持 `+`、`?`、`|` 三种常见正则运算。
   * 一旦有了这三类函数，就可以在解析器阶段直接调用它们，生成更丰富的 NFA 结构。

2. **原有接口未改动**

   * 保留了 `create_char_nfa`、`create_concat_nfa`、`create_star_nfa` 三个函数，并继续使用同一个 `Arena` 内存分配策略。
   * 使用方式与原来类似：只不过新增了对更多运算符的支持。

3. **整合方式**

   * 在后续的正则解析器（见下文）里，当检测到后缀 `+`、`?`、`|` 时，就会直接调用对应新建的工厂函数，返回一个入口状态。
   * 这样，底层 NFA 构造逻辑依然集中在 `nfa.c`，解析器只需要“以符号为单位”地调用相应函数，组装更大的 NFA。

---

## 二、解析器的修改

### 1. 原项目中 `parse_regex` 的实现

```c
#include "parser.h"
#include "lexer.h"

State* parse_regex(const char* regex, struct Arena* arena) {
    State* start = create_state(arena);
    State* current = start;

    const char* p = regex;
    Token* tok;

    while ((tok = lexer_next_token(&p, arena))->type != T_EOF) {
        if (tok->type != T_CHAR) continue;

        // 创建字符NFA片段
        State* mid = create_state(arena);
        State* end = create_state(arena);
        add_transition(arena, mid, tok->value, end);

        // lookahead 判断是否是 STAR
        const char* lookahead = p;
        Token* next_tok = lexer_next_token(&lookahead, arena);
        if (next_tok->type == T_STAR) {
            // 跳过 STAR
            p = lookahead;

            // 添加循环、跳过和连接
            add_transition(arena, current, '\0', mid);  // current -> mid
            add_transition(arena, end, '\0', mid);      // end -> mid
            add_transition(arena, current, '\0', end);  // current -> end
            current = end;
        } else {
            // 非 * 情况，直接连接
            add_transition(arena, current, tok->value, end);
            current = end;
        }
    }

    current->is_accepting = 1;
    return start;
}
```

* **功能简述**

  * 该版本只支持单字符匹配及其后缀 `*`，思路是每读到一个 `T_CHAR`：

    1. 新建两个状态 `mid`、`end`，并做 `mid --c--> end`。
    2. 如果接下来是 `*`，则在 `current`→ε→`mid`、`end`→ε→`mid`、`current`→ε→`end` 三处添加 ε→，构造成可循环匹配 `c*`。并将 `current` 更新为 `end`。
    3. 否则（没有 `*`），直接在 `current`→`c`→`end` 加一条普通转移。
    4. 循环读字符直到 `T_EOF`，最后把 `current` 标记为最终接受。

* **不足之处**

  1. 只能识别一个接一个的“单字符 + 可选 `*`”，不支持并（`|`）和后缀 `+`、`?`。
  2. 串联逻辑硬编码在循环里，没有分层语法分析，无法处理带优先级的括号、`|`、多个后缀并存等复杂情况。

### 2. 新增/修改的解析器结构

您将原来简单的循环式串联实现，重构为**递归下降式**解析器，代码主要分为三层函数：

```c
static State* parse_expression(const char** regex, struct Arena* arena);
static State* parse_term(const char** regex, struct Arena* arena);
static State* parse_factor(const char** regex, struct Arena* arena);

State* parse_regex(const char* regex, struct Arena* arena) {
    const char* p = regex;
    State* start = parse_expression(&p, arena);
    return start;
}
```

#### 2.1 `parse_expression`（处理并集“|”）

```c
static State* parse_expression(const char** regex, struct Arena* arena) {
    State* start = create_state(arena);
    State* current = start;
    
    // 1) 解析第一个 term，并在外层 start --ε→ term_start
    State* term_start = parse_term(regex, arena);
    add_transition(arena, current, '\0', term_start);
    
    // 2) 检测是否有连续的 '|'
    const char* p = *regex;
    Token* tok = lexer_next_token(&p, arena);
    
    while (tok->type == T_UNION) {
        *regex = p; // 消费掉 '|'
        
        // 解析下一个 term 分支
        State* next_term = parse_term(regex, arena);
        add_transition(arena, start, '\0', next_term);
        
        p = *regex;
        tok = lexer_next_token(&p, arena);
    }
    
    return start;
}
```

* **功能**：

  * 先构造一个 **外层统一入口** `start`。
  * 调用 `parse_term` 得到第一条分支（`term_start`），并连接 `start`→ε→`term_start`。
  * 如果下一个 token 是 `|`（`T_UNION`），就“消费”掉它，再次调用 `parse_term` 建第二条分支，同样用 ε→ 扔到 `start`。如此反复，直到没有更多 `|`。
  * 最后 `start` 一共会有多条 ε→，分别指向各个 term 的入口，正好实现并集。

#### 2.2 `parse_term`（处理串联）

```c
static State* parse_term(const char** regex, struct Arena* arena) {
    State* start = create_state(arena);
    State* current = start;
    
    // 1) 先解析第一个 factor，由 start --ε→ factor_start
    State* factor = parse_factor(regex, arena);
    add_transition(arena, current, '\0', factor);
    
    // 2) 看看下一个 token 是不是 T_CHAR（意味着还有下一个 factor）
    const char* p = *regex;
    Token* tok = lexer_next_token(&p, arena);
    
    while (tok->type == T_CHAR) {
        State* next_factor = parse_factor(regex, arena);
        
        // 找到上一个 factor 的那个接受态
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
        
        // 将上一个接受态去标记，再 connect ε→ next_factor_start
        if (last_accept != NULL) {
            last_accept->is_accepting = 0;
            add_transition(arena, last_accept, '\0', next_factor);
        }
        
        p = *regex;
        tok = lexer_next_token(&p, arena);
    }
    
    return start;
}
```

* **功能**：

  1. 新建 `start`，并与第一个 `factor` 用 ε→ 串联。
  2. 如果下一个词符仍然是字母（`T_CHAR`），说明还有后续的 `factor`，需要把“当前已拼好的那段 NFA 的尾端接受态”连接到下一个 `factor` 的入口。
  3. 具体做法是：遍历 `start->transitions` 找到那条 ε→ 走到的 `factor`，再沿着它的路径找到唯一标记的 `is_accepting`，把它去标记，然后再 `ε→` 新 factor 的入口。
  4. 依此往后，把每个字面因子串成一条长链。

* **改动点**：

  * 原项目里只在循环中处理一次 `T_CHAR + 可选 *` 的情况，新版逻辑是“逐层解析 + 找尾端接受态 → 串联下一个 factor”，使得“`abc*`”能自动拆成 `a`→`b`→`c*` 三段串联。
  * 串联过程与 NFA 构造函数解耦，`parse_term` 只关心如何在尾端 `last_accept` 加 ε→，而不再需要手动写 `add_transition(current, c, next)` 之类代码。

#### 2.3 `parse_factor`（处理单字符及其后缀）

```c
static State* parse_factor(const char** regex, struct Arena* arena) {
    const char* p = *regex;
    Token* tok = lexer_next_token(&p, arena);
    
    if (tok->type != T_CHAR) {
        // 出现意外非字符，则返回一个永不接受的“错误”状态
        State* error = create_state(arena);
        error->is_accepting = 0;
        return error;
    }
    
    // 消费掉这个字符
    *regex = p;
    char c = tok->value;
    
    // 先构造“单字符 NFA”
    NFA char_nfa = create_char_nfa(arena, c);
    
    // 再查看下一个词符，有可能是 STAR / PLUS / QUESTION
    p = *regex;
    tok = lexer_next_token(&p, arena);
    
    if (tok->type == T_STAR) {
        *regex = p; // 消耗 '*'
        NFA star_nfa = create_star_nfa(arena, char_nfa);
        return star_nfa.start;
    } else if (tok->type == T_PLUS) {
        *regex = p; // 消耗 '+'
        NFA plus_nfa = create_plus_nfa(arena, char_nfa);
        return plus_nfa.start;
    } else if (tok->type == T_QUESTION) {
        *regex = p; // 消耗 '?'
        NFA question_nfa = create_question_nfa(arena, char_nfa);
        return question_nfa.start;
    }
    
    // 没有后缀，则直接返回单字符 NFA 的入口
    return char_nfa.start;
}
```

* **功能**：

  * **读取一个字面字符**（`T_CHAR`），先用 `create_char_nfa` 生成对应 NFA。
  * **查看下一个 token**，如果它是 `*`、`+`、`?`，则分别调用 `create_star_nfa`、`create_plus_nfa`、`create_question_nfa`，把“单字符 NFA”包装成带后缀的 NFA。
  * 否则“单字符 NFA”本身就是一个完整的 `factor`，直接返回该 NFA 的入口状态。

* **改动点**：

  * 原项目没有 `parse_factor`，也是直接在循环里拼接，每次都手动判断是否出现 `*`。
  * 新版通过三层递归下降，将“匹配字符 + 可选后缀”提取到一个单独函数里，逻辑更清晰，也方便以后（如果需要）支持更多后缀或分组等语法。

### 3. 与原项目的对比总结

| 方面            | 原项目简易版本                                                           | 新版递归下降解析器                                                          |                                                             |
| ------------- | ----------------------------------------------------------------- | ------------------------------------------------------------------ | ----------------------------------------------------------- |
| 支持的正则语法       | 仅支持“单字符 + 可选 `*`”                                                 | 支持“并集 \`                                                           | `、串联、后缀 `\*`、`+`、`?\`”                                      |
| 解析思路          | 一次性循环扫描，遇到 `T_CHAR` → 手动拼接 NFA                                    | 三层函数(`parse_expression`/`parse_term`/`parse_factor`) 逐级处理根据优先级分解正则 |                                                             |
| 并集(\`         | \`)                                                               | 不支持                                                                | `parse_expression` 中专门用 `while (tok->type == T_UNION)` 处理并集 |
| 串联(多个字符)      | 在循环里只处理当前字符（加 `*` 或无后缀），背后多段 NFA 之间直接平铺                           | `parse_term` 先后调用 `parse_factor` 多次，并按尾端“接受态”把它们逐一串联               |                                                             |
| 后缀运算符 `+`、`?` | 不支持                                                               | `parse_factor` 里辨别并调用 `create_plus_nfa`、`create_question_nfa`      |                                                             |
| NFA 构造函数的耦合程度 | NFA 构造与解析逻辑几乎写在一起，直接在循环里做 `add_transition(current, c, end)` 或循环连接 | 解析器只负责“识别符号并调用相应的 NFA 工厂函数”，NFA 结构生成原理封装在 `nfa.c` 里                |                                                             |

---

## 三、修改效果与使用示例

* **在原来只能写 `a*` 的基础上，现在可以写**

  * `a+`  → “至少一次 a”
  * `a?`  → “零次或一次 a”
  * `a|b` → “匹配 a 或 b”
  * `ab+cd?` → “a” 串 “b+” 串 “c” 串 “d?”，解析器会自动拆分成 4 个 factor，并生成等价的 NFA

* **示例对比**

  1. 正则串 `"ab*c"`

     * 原项目只能先把 `a`、`b*`、`c` 三段写在循环里；`b*` 的构造逻辑直接嵌在每次 loop，并没有分层函数，稍显混乱。
     * 新版递归：

       * `parse_expression` 调 `parse_term`；
       * `parse_term` 依次调用 `parse_factor("a")` → `create_char_nfa(a)`；
       * 下一字符 `b`，调用 `parse_factor("b*")` → `create_char_nfa(b)` → `create_star_nfa(b_nfa)`；
       * 下一字符 `c`，调用 `parse_factor("c")` → `create_char_nfa(c)`；
       * 串联三段 NFA。

  2. 正则串 `"x|y+"`

     * 原项目无法直接解析，会忽略 `|`。
     * 新版：

       * `parse_expression` 首先解析 `"x"` 作为第一个 `term`；
       * 检测到 `|`，再调用 `parse_term` 解析 `"y+"`：`parse_factor("y+")`→`create_char_nfa(y)`→`create_plus_nfa(y_nfa)`；
       * 最终外层 `start` 有两条 ε→，分别指向 “匹配 x” 的那条 NFA、以及 “匹配 y+” 的那条 NFA。

---

## 四、总体修改总结

1. **NFA 构造部分**

   * 扩展了对 `+`、`?`、`|` 三种正则运算符的支持，新增三个对应的构造函数：

     * `create_plus_nfa` 用于 `+`，
     * `create_question_nfa` 用于 `?`，
     * `create_union_nfa` 用于 `|`。
   * 保留并沿用了原本的单字符、串联(`concat`)、Kleene 星号(`*`)三种工厂函数，但将更复杂的运算逻辑移到独立函数里，减少了解析器的耦合度。

2. **解析器部分**

   * 从最初的“一层循环 + if 判断”改为“三层递归下降”框架：

     * `parse_expression` 处理并集。
     * `parse_term` 处理串联。
     * `parse_factor` 处理单字符及其后缀（`*`、`+`、`?`）。
   * 每次在对应层级识别到运算符，就调用相应的 NFA 构造函数，从而生成模块化且易于维护的 NFA。
   * 改造后的解析器对常见正则语法（不含括号）的覆盖度大幅提升，逻辑更清晰，也便于后续再扩充对括号、小节分组、字符集合等功能。

