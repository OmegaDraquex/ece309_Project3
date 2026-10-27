# Project 3 Design Log
Author: Anthony Bianchi P2 code used: instructor reference

## 1. Hash table

For collision handling in ToolRegistry, i implemented separate chaining with dynamic buckets (std::vector<std::vector<std::unique_ptr<Tool>>>). I evaluated open addressing with linear probing, but chose chaining because open addressing suffers from primary clustering as the table fills. Additionally, separate chaining handles std::unique_ptr ownership cleanly without requiring tombstone slots or complex swap dances during eviction and lookup, which makes things easier.

For the hash function, i chose 64-bit FNV-1a. It hashes every character through XOR folding and multiplication by the FNV prime (1099511628211ULL). Because each character shifts the cumulative state, anagrams and nearby keys (like tool_10 versus tool_01) land in distinct buckets.

The table initializes with 16 buckets. Resizing doubles the bucket count whenever (toolCount_ + 1) / bucket_count > 0.75. Checking before insertion guarantees that load_factor() never exceeds 1.0 after add() returns.

Scale test measurements across 10,000 tools (tool_0 to tool_9999) compared to the weak hash (s[0] + s.size()):

| Metric | FNV-1a (Implementation) | Weak Hash (s[0] + s.size()) |
|---|---|---|
| Final bucket_count | 16,384 | 16,384 |
| Final load_factor | 0.6104 | 0.6104 |
| Longest chain (max_probe_length) | 6 | 9,000 |

The comparison clearly highlights collision behavior: the weak hash maps 9,000 of the 10,000 keys into one bucket because 't' and string length provide almost no entropy. FNV-1a distributes keys evenly, capping the longest chain at 6.

add() achieves amortized O(1) complexity. Doubling the table from N to 2N buckets costs O(N) work, but this event only happens after roughly 0.375N successful O(1) insertions. Distributing the O(N) rehash across those prior operations contributes an O(1) constant overhead per insert.

## 2. Pointer stability

The spec mandates that a Tool* returned by find() remains valid across table resizes.

In my design, ToolRegistry stores buckets of std::unique_ptr<Tool>. The actual Tool instances live on the heap. When rehash() executes, the bucket array is reallocated, and the std::unique_ptr wrappers are moved into their new bucket vectors. Moving a std::unique_ptr only transfers the heap address pointer; the underlying Tool object remains at its existing memory location. thus, raw pointers handed out by find() remain valid for the registry's lifetime.

## 3. Calculator trace

Evaluating 2 * (3 + 4) - 5:

1. Token 2: Number token. push 2 onto values. (values: [2], ops: [])
2. Token *: Operator token. ops is empty, push *. (values: [2], ops: [*])
3. Token (: Left paren token. push ( onto ops. (values: [2], ops: [*, (])
4. Token 3: Number token. Push 3 onto values. (values: [2, 3], ops: [*, (])
5. Token +: Operator token. Top of ops is (, push +. (values: [2, 3], ops: [*, (, +])
6. Token 4: Number token. Push 4 onto values. (values: [2, 3, 4], ops: [*, (, +])
7. Token ): Right paren token. Pop top op +. Pop operands 4 and 3. Compute 3 + 4 = 7. Push 7 onto values. Pop matching ( from ops. (values: [2, 7], ops: [*])
8. Token -: Operator token (prec 1). Top of ops is * (prec 2). Because 2 >= 1, apply *. Pop *, pop operands 7 and 2. Compute 2 * 7 = 14. Push 14. ops is empty, push -. (values: [14], ops: [-])
9. Token 5: Number token. Push 5 onto values. (values: [14, 5], ops: [-])
10. End of tokens: Empty ops. Pop -. Pop operands 5 and 14. Compute 14 - 5 = 9. Push 9.

Final answer: 9.

## 4. Errors as values

In an agent loop, tool failures (such as missing files, zero division, or invalid paths) are routine operational events rather than system errors. the language model needs to receive these failures as data so it can inspect what went wrong and attempt an alternative action. Returning errors as plain values (ToolResult::failure) keeps control flow clean and avoids expensive stack unwinding.

C++ exceptions are reserved for programmer defects and contract violations (e.g. calling as_string() on a JSON number). Those indicate bugs in C++ code rather than normal domain failures.

## 5. What I would design differently

If redesigning ToolRegistry, i would store cached 64-bit hash values alongside each std::unique_ptr<Tool>. During table resizing, the saved hash would eliminate redundant FNV-1a re-hashing over tool name strings.
