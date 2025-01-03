# 基於 Redis 的社群網路推薦系統設計與實現

## **摘要**

本專案旨在設計並實現一個基於 Redis 的高效社群網路推薦系統。系統包括三大核心對象：**使用者**、**帖子** 和 **社團**，並以標籤（Tags）作為關聯和推薦的基礎。具體功能包括：

1. **使用者與帖子推薦：**
   - 考慮帖子的標籤及其發布者的標籤，基於使用者興趣生成個性化推薦。
   - 使用 Redis 的倒排索引和集合操作，快速匹配使用者與相關帖子。
2. **社團推薦：**
   - 基於社團標籤及其熱門帖子的標籤，向使用者推薦興趣相關的社團。
   - 計算社團內活躍度和熱門話題，結合 Redis 的排序功能（如 Sorted Set）優化推薦效果。
3. **觸及率優化：**
   - 在社團內發布的帖子優先觸及社團成員，透過 Redis 的分層排序機制提高其可見性。

## **TODO**

- `create_dblistnode_with_string` 不需要 strdup

- 解決命名混亂問題
