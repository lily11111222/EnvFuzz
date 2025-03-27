
// 结构体定义
struct RegionSequence {
    int count;
    int capacity;
    char** regions;

     /*
     * Initialize a patch (the "constructor").
     */
    void init(void)
    {
        count = 0;
        capacity = 0;
        regions = NULL;
    }
};

struct FieldSequence {
    int count;
    int capacity;
    char** fields;
};

// 初始化 RegionSequence
static RegionSequence* initRegionSequence(int initialCapacity) {
    RegionSequence* seq = (RegionSequence*)malloc(sizeof(RegionSequence));
    seq->regions = (char**)malloc(sizeof(char*) * initialCapacity);
    seq->count = 0;
    seq->capacity = initialCapacity;
    return seq;
}

// 初始化 FieldSequence
static FieldSequence* initFieldSequence(int initialCapacity) {
    FieldSequence* seq = (FieldSequence*)malloc(sizeof(FieldSequence));
    seq->fields = (char**)malloc(sizeof(char*) * initialCapacity);
    seq->count = 0;
    seq->capacity = initialCapacity;
    return seq;
}

// 去除换行符（不使用 strcspn）
static void removeNewline(char* str) {
    char* pos;
    if ((pos = strchr(str, '\n')) != NULL) *pos = '\0';
    // if ((pos = strchr(str, '\r')) != NULL) *pos = '\0';
}

// 解析 RegionSequence
static RegionSequence* parseSeed2RegionSeq(const char* directory) {
    RegionSequence* regionSeq = initRegionSequence(100);
    DIR* dir = opendir(directory);
    if (!dir) {
        fprintf(stderr, "无法打开目录: %s\n", directory);
        return NULL;
    }

    struct my_dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".raw")) {
            char fullPath[1024];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", directory, entry->d_name);
            
            FILE* file = fopen(fullPath, "r");
            if (!file) {
                fprintf(stderr, "无法打开文件: %s\n", fullPath);
                continue;
            }

            char line[1024];
            while (fgets(line, sizeof(line), file)) {
                removeNewline(line);

                // 跳过空行
                if (line[0] == '\0') continue;

                // 扩容
                if (regionSeq->count >= regionSeq->capacity) {
                    regionSeq->capacity *= 2;
                    regionSeq->regions = (char**)realloc(regionSeq->regions, sizeof(char*) * regionSeq->capacity);
                }

                // fprintf(stderr, "line:%s\n", line);
                // 存储 region
                char *copy = strdup(line);
                if (!copy) {
                    fprintf(stderr, "strdup failed\n");
                    return NULL;
                }
                regionSeq->regions[regionSeq->count++] = copy;
                // fprintf(stderr, "regionSeq->regions[regionSeq->count-1]:%s\n", regionSeq->regions[regionSeq->count-1]);
                
                // size_t len = strlen(line);
                // regionSeq->regions[regionSeq->count++] = (char *)malloc(len+1);
                // memcpy(regionSeq->regions[regionSeq->count++], line, len);
            }
            fclose(file);
        }
    }
    closedir(dir);
    return regionSeq;
}

// 解析 FieldSequence
static FieldSequence* parseSeed2FieldSeq(const char* directory) {
    FieldSequence* fieldSeq = initFieldSequence(200);
    DIR* dir = opendir(directory);
    if (!dir) {
        fprintf(stderr, "无法打开目录: %s\n", directory);
        return NULL;
    }

    struct my_dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".raw")) {
            char fullPath[1024];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", directory, entry->d_name);
            
            FILE* file = fopen(fullPath, "r");
            if (!file) {
                fprintf(stderr, "无法打开文件: %s\n", fullPath);
                continue;
            }

            char line[1024];
            while (fgets(line, sizeof(line), file)) {
                removeNewline(line);

                // 跳过空行
                if (line[0] == '\0') continue;

                // 分割字段
                char* token = strtok(line, " ");
                while (token) {
                    // 扩容
                    if (fieldSeq->count >= fieldSeq->capacity) {
                        fieldSeq->capacity *= 2;
                        fieldSeq->fields = (char**)realloc(fieldSeq->fields, sizeof(char*) * fieldSeq->capacity);
                    }
                    // 存储字
                    // size_t len = strlen(token);
                    // fieldSeq->fields[fieldSeq->count++] = (char *)malloc(len+1);
                    // memcpy(fieldSeq->fields[fieldSeq->count++], token, len);
                    
                    char *copy = strdup(token);
                    if (!copy) {
                        fprintf(stderr, "strdup failed\n");
                        return NULL;
                    }
                    fieldSeq->fields[fieldSeq->count++] = copy;
                    // fprintf(stderr, "fieldSeq->fields[fieldSeq->count-1]:%s\n", fieldSeq->fields[fieldSeq->count-1]);
                

                    token = strtok(NULL, " ");
                }
            }
            fclose(file);
        }
    }
    closedir(dir);
    return fieldSeq;
}

// 释放 RegionSequence
static void freeRegionSequence(RegionSequence* seq) {
    for (int i = 0; i < seq->count; i++) {
        free(seq->regions[i]);
    }
    free(seq->regions);
    free(seq);
}

// 释放 FieldSequence
static void freeFieldSequence(FieldSequence* seq) {
    for (int i = 0; i < seq->count; i++) {
        free(seq->fields[i]);
    }
    free(seq->fields);
    free(seq);
}