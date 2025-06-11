## 1. 新增功能

* 自动根据已算好的 First/Follow 集，构造一张“行：非终结符 × 列：真实终结符 + ‘\$’”的预测分析表。
* 打印时用“X→α”的形式显示单元格内容，也过滤掉 First/Follow 中的“ε 标记 (‘#’)”，并额外保留一列“\$”表示输入结束符。

---

## 2. 关键数据结构

1. **`vt_list[]` 和 `vt_count`**

   * 从 `grammar->terminals[]` 中筛选出所有≠‘#’ 的真实终结符，逐一拷贝到 `vt_list`，计数为 `vt_count`。
   * 这决定了表格的前 `vt_count` 列实际对应哪些终结符。

2. **`vn_count = grammar->nonterminals_count`**

   * 行数即文法中非终结符的总数，每行对应一个非终结符。

3. **`parseTable[vn_count][vt_count + 1]`**

   * 二维整型数组，所有元素初始化为 `-1`。
   * 列索引 `0..vt_count-1` 对应 `vt_list[0..vt_count-1]`；
   * 列索引 `vt_count` 专门对应“\$”。

---

## 3. 填表核心步骤

对每条编号为 `p` 的产生式 `A → α`（`rule->left_hs = A`，`rule->right_hs = α`）：

1. **计算 `FIRST(α)`**

   * 调用 `get_first_of_string(α, sets, set_count, first_alpha)`，其中 `first_alpha` 是一个字符数组，包含 α 所有可能的第一个“终结符”以及（若 α 能推 ε）一个 `'#'`。

2. **填充“非空串终结符”列**

   ```c
   for (每个 a in first_alpha) {
       if (a == '#') continue;
       col = find_terminal_index(a);    // 在 vt_list 中找 a 的列号
       parseTable[row(A)][col] = p;     // 记录产生式编号 p
   }
   ```

   * 意思：若 `a ∈ FIRST(α)` 且 `a ≠ ε`，就把这条产生式填到 `M[A, a]` 对应单元格。

3. **若 `FIRST(α)` 包含 `'#'`（ε），则填充“FOLLOW(A)”对应列**

   ```c
   if (strchr(first_alpha, '#')) {
       followA = get_follow_set(sets, set_count, A);  // 返回字符串，如 "b$"
       for (每个 b in followA) {
           if (b == '$') {
               parseTable[row(A)][vt_count] = p;      // 最后一列是 $
           } else {
               col = find_terminal_index(b);         // b 在 vt_list 的列号
               parseTable[row(A)][col] = p;
           }
       }
   }
   ```

   * 这一步处理“`A→α` 能推出 ε 时，所有 `b ∈ FOLLOW(A)` 也要用该产生式”。

---

## 4. 打印预测表

1. **表头**：

   * 先按顺序输出所有 `vt_list[0..vt_count-1]`（真实终结符），每个后面跟一个制表符；
   * 最后再输出 `"$"`；
   * 例如：`"\t d\t b\t $\n"`。

2. **每一行（对应非终结符 A）**：

   ```c
   printf("%c\t", A);  // 行首打印非终结符

   // 对应 vt_list 列
   for (j = 0; j < vt_count; j++) {
       idx = parseTable[row(A)][j];
       if (idx >= 0) {
           rule = &grammar->rules[idx];
           printf("%c->%s\t", rule->left_hs, rule->right_hs);
       } else {
           printf("-\t");
       }
   }

   // 最后一列对应 '$'
   idx = parseTable[row(A)][vt_count];
   if (idx >= 0) {
       rule = &grammar->rules[idx];
       printf("%c->%s", rule->left_hs, rule->right_hs);
   } else {
       printf("-");
   }
   printf("\n");
   ```

* 如果某单元格值为 `-1`，打印 `"-"` 表示“(A, x) 无匹配产生式”；否则取出对应的 `Rule` 并以 `A->α` 形式输出。

---


### 要点总结

* **过滤 `'#'`**：终结符列表中只保留真正会出现在输入流中的符号，`'#'`（ε 标记）不作为列头。
* **额外一列 `$`**：最后一列单独对应“输入结束符”，只在某条产生式的 FIRST(α) 包含 ε 且 b = `$ ∈ FOLLOW(A)` 时填入。
* **两步填表**：

  1. FIRST(α){ε} → 填 `(A, a)` 单元格；
  2. 若 ε ∈ FIRST(α)，再对每个 `b ∈ FOLLOW(A)` 填 `(A, b)` 或 `(A, $)`。
* **值 ≥0 表示对应产生式编号**，打印时根据编号输出完整的 “X→Y” 文本；值 `-1` 表示“无匹配”，打印为 `“-”`。

这样，一张清晰、可打印的 LL(1) 预测分析表就构造完成了，后续在语法分析阶段直接查表即可决定要用哪条产生式。
