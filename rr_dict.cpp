
// 关键词集合结构体
struct KeywordDict{
    char **keywords;  // 动态数组指针
    size_t count;     // 当前关键词数量
    size_t capacity;  // 数组容量
};

// 初始化字典
static KeywordDict *dict_create() {
    KeywordDict *dict = (KeywordDict *)malloc(sizeof(KeywordDict));
    if (!dict) return NULL;
    dict->count = 0;
    dict->capacity = 8;
    dict->keywords = (char **)malloc(dict->capacity * sizeof(char*));
    if (!dict->keywords) {
        free(dict);
        return NULL;
    }
    return dict;
}

// 释放字典内存
static void dict_free(KeywordDict *dict) {
    if (!dict) return;
    for (size_t i = 0; i < dict->count; i++) {
        free(dict->keywords[i]);
    }
    free(dict->keywords);
    free(dict);
}

// 添加关键词到字典
static int dict_add(KeywordDict *dict, const char *word) {
    // 扩容检查
    if (dict->count >= dict->capacity) {
        size_t new_cap = dict->capacity * 2;
        char **new_arr = (char **)realloc(dict->keywords, new_cap * sizeof(char*));
        if (!new_arr) return -1;
        dict->keywords = new_arr;
        dict->capacity = new_cap;
    }
    // 复制字符串
    char *copy = strdup(word);
    if (!copy) return -1;
    dict->keywords[dict->count++] = copy;
    return 0;
}

// 解析 ftp.dict 文件
static KeywordDict *parse_ftp_dict(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return NULL;

    KeywordDict *dict = dict_create();
    if (!dict) {
        fclose(fp);
        return NULL;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, fp)) != -1) {
        // 跳过空行和空白行
        if (read == 0 || line[0] == '\n') continue;

        // 去除首尾空白字符
        char *start = line;
        char *end = line + read - 1; // 排除换行符
        while (isspace(*start)) start++;
        while (end > start && isspace(*end)) end--;

        // 检查双引号格式
        if (*start != '"' || *end != '"') continue;
        start++;
        *end = '\0'; // 闭合末尾双引号

        // 提取关键词并添加
        if (dict_add(dict, start) != 0) {
            // 内存不足时终止解析
            break;
        }
    }

    free(line);
    fclose(fp);
    return dict;
}

// // 示例使用
// int main() {
//     KeywordDict *dict = parse_ftp_dict("ftp.dict");
//     if (!dict) {
//         fprintf(stderr, "Failed to parse dict file\n");
//         return 1;
//     }

//     printf("Loaded %zu keywords:\n", dict->count);
//     for (size_t i = 0; i < dict->count; i++) {
//         printf("- %s\n", dict->keywords[i]);
//     }

//     dict_free(dict);
//     return 0;
// }
