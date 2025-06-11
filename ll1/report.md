

# LL(1) 预测分析表功能报告

## 一、功能背景与目的

在编译原理课程或相关项目中，LL(1) 预测分析作为自顶向下语法分析方法之一，需要事先构造一个“预测分析表”（Parse Table），用于在分析过程中根据当前栈顶的非终结符和下一个输入符号快速定位对应的产生式。

* **原有情况**：项目已经实现了文法的读取、First 集和 Follow 集的计算，但缺少一个直观易用的预测分析表模块。分析时只能通过单步查找 First/Follow 与产生式的方式来判断，而没有一个“表格化”的可视结构。
* **新增目的**：在项目中增加预测分析表模块，将 LL(1) 填表算法和打印功能封装成两个简单易用的函数，便于后续直接调用该表格进行语法分析演示、调试或输出结果的教学展示。具体目标包括：

  1. 根据已经计算好的 First/Follow 集自动构造整张预测表；
  2. 将预测表以“终结符列 + ‘\$’结束符”+“非终结符行”的方式打印出来，每个单元格显示完整的“X→α” 形式，而不是仅输出产生式编号；
  3. 过滤掉 First/Follow 中表示“ε”的 `'#'` 字符，并额外保留 `$` 列，用于表示输入结束符；
  4. 保持原有文法、First/Follow 模块接口不变，仅在新文件 `predict_table.h/.c` 内完成相关实现，尽量与已有系统解耦，以便维护和移植。

---

## 二、模块接口与文件结构

### 2.1 头文件：`predict_table.h`

```c
#ifndef PREDICT_TABLE_H
#define PREDICT_TABLE_H

#include "grammar.h"
#include "first_follow.h"
#include <stdbool.h>

/**
 * 构造 LL(1) 预测分析表，并将结果保存在内部结构中。
 * @param grammar   已读取并存储的文法结构指针，包含规则集、终结符/非终结符数组等
 * @param sets      已计算好的 FIRST 和 FOLLOW 集合数组，元素类型为 SymbolSet
 * @param set_count sets 数组中有效条目的数量
 */
void construct_parse_table(Grammar* grammar, SymbolSet* sets, int set_count);

/**
 * 打印当前已构造的 LL(1) 预测分析表。
 * - 表头依次列出所有真实终结符（跳过 '#'），尾部再加一列 '$' 。
 * - 单元格内容若有产生式，则打印“左部->右部”的完整文本；否则输出“-”。
 * @param grammar   用于从产生式编号映射回完整产生式文本
 * @param sets      
 * @param set_count sets 数组长度
 */
void print_parse_table(Grammar* grammar, SymbolSet* sets, int set_count);

#endif /* PREDICT_TABLE_H */
```

* **`construct_parse_table`**：主入口函数，完成预测表的分配、初始化与填充。填表逻辑直接依赖于外部先前计算好的 First 和 Follow 集。
* **`print_parse_table`**：打印函数，将内存中保存的预测表以“表格”形式输出到标准终端，便于人工查看。

### 2.2 源文件：`predict_table.c`

核心实现包括以下几个部分：

1. **全局静态变量声明**

   ```c
   static int **parseTable = NULL;   // 保存预测表条目的二维数组
   static int vn_count = 0;          // 文法中非终结符数量（行数）
   static int vt_count = 0;          // 真实终结符数量（不含 '#'，暂不计 '$'）——列数 = vt_count + 1
   static char *vt_list = NULL;      // 长度为 vt_count，用来存储真实终结符字符列表
   ```

   * `parseTable[i][j]` 处存储一个整型值：若 ≥0，表示对应的产生式编号；若为 `-1`，表示该单元格“无对应产生式”。
   * `vn_count` 等于 `grammar->nonterminals_count`；
   * `vt_list` 是从 `grammar->terminals`（可能包含 `'#'`）中筛选出的所有非 `'#'` 的终结符字符，按顺序收集到一个长度为 `vt_count` 的数组中；
   * 留出额外一列（`col = vt_count`）专门用来存储“输入结束符 `$`”时的产生式编号。

2. **查找函数**

   * `find_nonterminal_index(Grammar* grammar, char A)`：遍历 `grammar->nonterminals[]`，如果某个位置字符等于 `A`，返回该下标；否则返回 `-1`。用于将符号 `A` 映射为“行号”。
   * `find_terminal_index(char t)`：遍历 `vt_list[0..vt_count-1]`，若 `vt_list[i] == t`，返回 `i`，否则返回 `-1`。用于将真实终结符 `t` 映射为“列号”。
   * `get_first_of_string(const char* alpha, SymbolSet* sets, int set_count, char* first_alpha)`：给定一个右部字符串 `alpha`（例如 `"D B"`、`"dA"`、`"#"` 等），将其所有可能的“第一个终结符”收集到 `first_alpha` 数组中（以 `'\0'` 结束）。若该串可推出 ε，则 `first_alpha` 末尾也加上 `'#'`。
   * `get_first_set(...)` 与 `get_follow_set(...)`：根据某个非终结符 `A` 在 `sets[]` 中查找对应的 FIRST(A) 或 FOLLOW(A) 字符串，并返回字符串指针。

3. **构造函数：`construct_parse_table`**

   ```c
   void construct_parse_table(Grammar* grammar, SymbolSet* sets, int set_count) {
       // （1）从 grammar->terminals 过滤出真实终结符，把它们存到 vt_list[] 中
       vt_count = 0;
       vt_list = malloc(grammar->terminals_count * sizeof(char));
       for (int i = 0; i < grammar->terminals_count; i++) {
           char t = grammar->terminals[i];
           if (t == '#') continue; // 过滤掉 '#'（ε 标记）
           vt_list[vt_count++] = t;
       }
       // 最终 vt_count = 真实终结符个数；我们后续会在此基础上额外加一个 '$' 列

       // （2）确定非终结符行数：vn_count = grammar->nonterminals_count
       vn_count = grammar->nonterminals_count;

       // （3）动态分配 parseTable[vn_count][vt_count + 1]，并全部初始化为 -1
       parseTable = malloc(vn_count * sizeof(int*));
       for (int i = 0; i < vn_count; i++) {
           parseTable[i] = malloc((vt_count + 1) * sizeof(int));
           for (int j = 0; j <= vt_count; j++) {
               parseTable[i][j] = -1;
           }
       }

       // （4）遍历每一条产生式，填充 parseTable
       for (int p = 0; p < grammar->rule_count; p++) {
           Rule* rule = &grammar->rules[p];
           char A = rule->left_hs;                    // 产生式左部
           int row = find_nonterminal_index(grammar, A);
           if (row < 0) continue;                     // 如果找不到对应行，则跳过

           // 4.1) 计算右部串 RIGHT = rule->right_hs 的 FIRST(α)
           char first_alpha[GRAMMAR_MAX_SYMBOLS + 1];
           first_alpha[0] = '\0';
           get_first_of_string(rule->right_hs, sets, set_count, first_alpha);
           // first_alpha 示例：{"d","b","#"} 中任选其一，放到数组里是否合法？

           // 4.2) FIRST(α) 中非 '#' 的终结符 a  => parseTable[row][ col(a) ] = p
           for (int k = 0; first_alpha[k]; k++) {
               char a = first_alpha[k];
               if (a == '#') continue;                 // 排除 ε 标记
               int col = find_terminal_index(a);
               if (col >= 0) {
                   parseTable[row][col] = p;
               }
           }

           // 4.3) 如果 FIRST(α) 包含 '#'，则对 FOLLOW(A) 中的每个符号 b 做填表
           if (strchr(first_alpha, '#')) {
               const char* followA = get_follow_set(sets, set_count, A);
               for (int k = 0; followA[k]; k++) {
                   char b = followA[k];
                   if (b == '$') {
                       // 如果 b == '$'，映射到“额外最后一列”
                       parseTable[row][vt_count] = p;
                   } else {
                       // 否则 b 应是某个真实终结符，找它在 vt_list 中的索引
                       int col = find_terminal_index(b);
                       if (col >= 0) {
                           parseTable[row][col] = p;
                       }
                   }
               }
           }
       }
   }
   ```

   * **（1）过滤终结符**：将 `grammar->terminals` 中的所有非 `'#'` 字符依次放进 `vt_list`，得到一份“真实终结符列表”。

   * **（2）确定行数**：直接用 `grammar->nonterminals_count`。

   * **（3）分配内存并初始化**：`parseTable[i][j] = -1` 表示“暂时无规则”。

   * **（4）对每条产生式 `p : A→α`**：

     * **(4.1) 计算 FIRST(α)**：调用 `get_first_of_string`，其作用是从 `α` 的字符序列一路提取“第一个可能出现的终结符”，若整个 α 都能跑到末尾且都能推 ε，则 `first_alpha` 内还得有 `'#'`。
     * **(4.2) FIRST(α){#}**：对于 `first_alpha` 中的每个非 `'#'` 的符号 `a`，执行 `parseTable[row(A)][col(a)] = 产生式编号 p`。

       * 这样就保证“当当前栈顶是 A，下一个输入是 a 时，要用编号 p 的产生式”。
     * **(4.3) 若 `first_alpha` 包含 `'#'`**（说明 α 能推出 ε），就要把该产生式放到 FOLLOW(A) 中的所有符号列上。遍历 `FOLLOW(A)`：

       * 如果 b == `$`，对应 `col = vt_count`；
       * 否则 b 必然是 `vt_list` 之一，用 `find_terminal_index(b)` 找实际列索引；
       * 再令 `parseTable[row(A)][col] = p`。

   * 最终，经过遍历所有生成式后，`parseTable` 二维数组就完成了对整个预测表的“逻辑填充”。

4. **打印函数：`print_parse_table`**

   ```c
   void print_parse_table(Grammar* grammar, SymbolSet* sets, int set_count) {
       // （1）打印表头：真实终结符 + “$”
       printf("\t");
       for (int j = 0; j < vt_count; j++) {
           printf("%c\t", vt_list[j]);
       }
       printf("$\n");

       // （2）对每行（每个非终结符）：
       for (int i = 0; i < vn_count; i++) {
           char A = grammar->nonterminals[i];
           printf("%c\t", A); // 打印行首非终结符

           // (2.1) 打印真实终结符列
           for (int j = 0; j < vt_count; j++) {
               int prod_idx = parseTable[i][j];
               if (prod_idx >= 0) {
                   Rule* rule = &grammar->rules[prod_idx];
                   printf("%c->%s\t", rule->left_hs, rule->right_hs);
               } else {
                   printf("-\t");
               }
           }

           // (2.2) 最后一列打印 “$” 列
           int prod_idx = parseTable[i][vt_count];
           if (prod_idx >= 0) {
               Rule* rule = &grammar->rules[prod_idx];
               printf("%c->%s", rule->left_hs, rule->right_hs);
           } else {
               printf("-");
           }

           printf("\n");
       }
   }
   ```

   * **（1）输出列头**：先输出一个制表符 `"\t"`，再依次将 `vt_list[j]` 中的每个真实终结符打印在一行，最后加上 `"$"`，并换行。
   * **（2）输出每一行**：

     1. 打印行首的非终结符 `A`（`grammar->nonterminals[i]`）。
     2. 按顺序遍历 `j = 0..vt_count-1`：若 `parseTable[i][j] >=0`，把对应的产生式取出并打印 “`left->right`”；否则打印 “-”。
     3. 再打印 “\$” 列：检查 `parseTable[i][vt_count]`，若 ≥0 则打印该产生式，否则输出 “-”。
   * 这样就形成了一张“行非终结符，列真实终结符+ `$`”的预测表可视输出。

---

## 三、关键逻辑详解

### 3.1 过滤 `'#'`，保留真实终结符

* **原因**：在 LL(1) 填表时，`FIRST(A)` 中的 `'#'`（或 \`"ε"）标记虽然告诉我们“某条产生式能推 ε”，但是 ε 本身不会成为输入流中的符号，不应当在预测表的列头出现。
* **做法**：遍历 `grammar->terminals[]`（例如原本可能有 `{ 'd', 'b', '#' }`），跳过 `'#'`，把其他符号一次存入 `vt_list[]`。最终 `vt_list = { 'd', 'b' }`，`vt_count = 2`。
* **结果**：预测表列依次是 `d, b`，行尾另外加一个 “`$` 列”。在视觉上看，预测表形如：

  ```
      d    b    $
  A   …    …    …
  B   …    …    …
  ```

### 3.2 计算 RIGHT 部分的 FIRST(α)

对于每个产生式 `A→α`，需要先知道“当 α 的最左符号可能是哪些终结符”——这是“预测表第一步填表”的依据。

* **函数：`get_first_of_string(const char* alpha, …, char* first_alpha)`**

  1. 初始化：`first_alpha[0] = '\0'`，并假设 `epsilon_possible = true`。
  2. 按照 `alpha` 的字符依次扫描：

     * 若遇到终结符 `a ≠ '#'`，则把它加入 `first_alpha`，将 `epsilon_possible = false`，循环结束。
     * 若遇到非终结符 `B`，则取出 `FIRST(B)`：

       * 将 `FIRST(B)\{#}`（所有非 ε 项）加到 `first_alpha` 中；
       * 如果 `FIRST(B)` 中含有 `'#'`，说明 B 可以推出 ε，就设 `epsilon_possible = true` 并继续看 α 中的下一个符号；否则就设 `epsilon_possible = false` 并结束循环。
     * 若遇到 `'#'`（手写产生式右部直接写 `"#"`），说明 α 本身就是 ε，就把 `'#'` 放到 `first_alpha` 中，并保持 `epsilon_possible = true`，然后结束循环（因为 α 只含 ε，没其他字符可遍历）。
  3. 整个 `α` 扫描完后，如果 `epsilon_possible == true`（说明所有层级都能推出 ε），在 `first_alpha` 中再加一个 `'#'`。
  4. 最后以 `'\0'` 结尾。

* **示例**：若 `α = "D B"`，`FIRST(D) = { d, '#' }`，`FIRST(B) = { b }`，则：

  1. 见到 `D`，把 `d` 加入；因 `FIRST(D)` 含 `'#'`，继续；
  2. 见到 `B`，把 `b` 加入；`FIRST(B)` 不含 `'#'`，结束。
  3. `epsilon_possible` 此时为 `false`，不再添加 `'#'`；
  4. 结果 `first_alpha = "db"`（以某种顺序，可能是 `{'d','b','\0'}`）。

### 3.3 填充预测表格

以产生式编号 `p : A→α` 为例：

1. **取出 `row = find_nonterminal_index(grammar, A)` 获得 A 在非终结符数组中的行索引**。

   * 如果返回 `-1` 说明出错或该产生式左部不属于已注册的非终结符，直接跳过。
2. **取出 `FIRST(α)` 字符串 `first_alpha`**（参照 3.2）。
3. **遍历 `first_alpha`**：

   * 如果字符 `x == '#'`，跳过；
   * 否则 `x` 是某个真实终结符（例如 `'d'`、`'b'`），用 `find_terminal_index(x)` 查到列号 `col`，执行 `parseTable[row][col] = p`。
4. **如果 `first_alpha` 包含 `'#'`**（即 α 能推 ε），就遍历 `FOLLOW(A)`：

   * 用 `get_follow_set(sets, set_count, A)` 拿到形如 `"b$"`、`"d$"`、`"$"` 等 C 字符串；
   * 对每个 `b`：

     * 若 `b == '$'`，令 `col = vt_count`；
     * 否则用 `col = find_terminal_index(b)`；
     * 再 `parseTable[row][col] = p`。
   * 这一步将所有“当 A 能推出 ε 并且下一个输入符号在 FOLLOW(A) 中时”对应的单元格填成“产生式 p”。

完成上述所有产生式的遍历，就完成了预测表的全部填充。

### 3.4 打印与展示

1. **打印第一行（列头）**：

   * 先打一个制表符 `"\t"`，对齐行首。
   * 再依次把 `vt_list[0..vt_count-1]` 输出成 `"d\tb\t…"` 这样的形式。
   * 最后加一个 `"$\n"`，表示输入结束符。

2. **打印每一行**（`i = 0..vn_count-1`）：

   * 打印行首的非终结符 `grammar->nonterminals[i]`，再加上一个 `"\t"`；
   * 对于 `j = 0..vt_count-1`：

     * 若 `parseTable[i][j] == -1`，打印 `"-\t"`；
     * 否则取出 `Rule* rule = &grammar->rules[ parseTable[i][j] ]`，打印 `"%c->%s\t"`，也就是完整的 “左部->右部”。
   * 最后一列 `j = vt_count`（对应 `$`）：

     * 若 `parseTable[i][vt_count] == -1`，打印 `"-"`；
     * 否则打印对应产生式（同样 “`A->α`” 形式）。
   * 最后打一行换行 `"\n"`，进入下一个非终结符。

打印完后，一张排版美观的 LL(1) 预测分析表就摆在终端上，方便开发者或同学对照检查。

---

## 四、使用示例

假设我们有如下文法（与报告示例保持一致）：

```
(1) A → D B
(2) A → #
(3) D → #
(4) D → d A
(5) D → d
(6) B → b
```

* **非终结符**：`A, D, B`
* **终结符**：假设用 `{ 'd', 'b', '#' }` 表示，其中 `'#'` 用于表示 ε，最后输出不显示 `'#'`。
* 已计算出来的 First/Follow（示例）：

  ```
  First(A) = { d, b, # }
  First(D) = { d, # }
  First(B) = { b }
  Follow(A) = { $, b }
  Follow(D) = { d, b, $ }
  Follow(B) = { $, d }
  ```

按照新功能提供的 `construct_parse_table` 和 `print_parse_table`，执行流程如下：

1. **先调用**

   ```c
   construct_parse_table(&grammar, sets, set_count);
   ```

   * `vt_list = { 'd', 'b' }`，`vt_count = 2`

   * `vn_count = 3`（`A、D、B` 三个非终结符）

   * 分配 `parseTable[3][3]`，全部置 `-1`

   * 遍历每条产生式：

     1. `A → D B`（p=0）：

        * `FIRST("DB") = { d, b }`；
        * 填 `parseTable[row(A)][col('d')] = 0`，`parseTable[row(A)][col('b')] = 0`
        * `FIRST` 中不含 `'#'`，跳过 FOLLOW(A) 步骤。
     2. `A → #`（p=1）：

        * `FIRST("#") = { '#' }`；
        * 因 `'#' ∈ FIRST`，取 `FOLLOW(A) = { b, $ }`：

          * `parseTable[row(A)][col('b')] = 1`（覆盖掉原来 `0`）；
          * `parseTable[row(A)][vt_count=2] = 1` （即 `parseTable[row(A)][2] = 1`）。
     3. `D → #`（p=2）：

        * `FIRST("#") = { '#' }`；
        * `FOLLOW(D) = { d, b, $ }`：

          * `parseTable[row(D)][col('d')] = 2`
          * `parseTable[row(D)][col('b')] = 2`
          * `parseTable[row(D)][2] = 2`
     4. `D → d A`（p=3）：

        * `FIRST("dA") = { d }`；
        * `parseTable[row(D)][col('d')] = 3`（覆盖掉原来 `2`）；
        * `FIRST` 不含 `'#'`，跳过 FOLLOW(D)。
     5. `D → d`（p=4）：

        * `FIRST("d") = { d }`；
        * `parseTable[row(D)][col('d')] = 4` （再次覆盖掉原来 `3`）。
     6. `B → b`（p=5）：

        * `FIRST("b") = { b }`；
        * `parseTable[row(B)][col('b')] = 5`
        * `FIRST` 不含 `'#'`，跳过 FOLLOW(B)。

   * 最终 `parseTable`（以行号与列号对应的产生式编号展示，行顺序：A=0、D=1、B=2；列顺序：d=0、b=1、\$=2）：

     ```
       col→   d    b    $
     row
     A(0)    0    1    1
     D(1)    4    2    2
     B(2)   -1    5   -1
     ```

     对应内容：

     * `parseTable[0][0]=0` → “A→DB”
     * `parseTable[0][1]=1` → “A→#”
     * `parseTable[0][2]=1` → “A→#”
     * `parseTable[1][0]=4` → “D→d”
     * `parseTable[1][1]=2` → “D→#”
     * `parseTable[1][2]=2` → “D→#”
     * `parseTable[2][1]=5` → “B→b”

2. **再调用**

   ```c
   print_parse_table(&grammar, sets, set_count);
   ```

   终端输出：

   ```
        d       b       $
   A    A->DB   A->#    A->#
   D    D->d    D->#    D->#
   B     -      B->b     -
   ```

* **解读**：

  * 第一行列头是 `d, b, $`；
  * “A 行”在 `d` 列对应 `A→DB`，在 `b/$` 列对应 `A→#`；
  * “D 行”在 `d` 列对应 `D→d`，在 `b/$` 列对应 `D→#`；
  * “B 行”仅在 `b` 列对应 `B→b`，其余列输出 `-`。
* 这样就形成了一张易于阅读、能直接用于 LL(1) 分析的预测表。

---


## 五、小结

* 本次新增的 `predict_table` 模块 **整体封装了 LL(1) 预测分析表的构造与打印功能**，核心逻辑分为：

  1. **过滤真实终结符**（剔除 `'#'`）；
  2. **动态分配并初始化表格**；
  3. **对每条产生式** 计算 `FIRST(右部)` 并 填写“非空串终结符”对应单元格；若能推 ε，则遍历 `FOLLOW(A)` 填写“空串情形”对应单元格；
  4. **打印** 时按“终结符 + \$”列头、每行非终结符顺序输出，单元格若有产生式就展示 `A->α` 文本，否则打印 `-`。
* 该模块实现简单清晰、解耦性强，只需在项目里先计算好 First/Follow 集，然后调用 `construct_parse_table(...)` 和 `print_parse_table(...)` 即可生成并展示预测表。开发者可在此基础上轻松扩展冲突检测、文件导出、动态查询等功能。

