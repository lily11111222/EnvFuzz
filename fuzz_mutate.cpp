/*
 *  ____  ____  _____              
 * |  _ \|  _ \|  ___|   _ ________
 * | |_) | |_) | |_ | | | |_  /_  /
 * |  _ <|  _ <|  _|| |_| |/ / / / 
 * |_| \_\_| \_\_|   \__,_/___/___|
 *
 * Copyright (C) National University of Singapore
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define PADDING     1024

/*
 * Crude approximation of n*log2(x+1)
 */
static size_t NLOG2(size_t n, size_t x)
{
    x++;
    size_t z;
    asm ("lzcnt %1, %0": "=r"(z): "r"(x));
    size_t r = (x & (0xFFFFFFFFFFFFFFFFull >> (z+1)));
    z = 63 - z;
    size_t t = (x & (0xFFFFFFFFFFFFFFFFull << z));
    return n * z + (n * r) / (2 * t);
}

#define BSWAP_U16(x)                                    \
    ((((x) & 0x00FF) << 8) |                            \
     (((x) & 0xFF00) >> 8))
#define BSWAP_U32(x)                                    \
    ((((x) & 0x000000FF) << 24) |                       \
     (((x) & 0x0000FF00) << 8) |                        \
     (((x) & 0x00FF0000) >> 8) |                        \
     (((x) & 0xFF000000) >> 24))
#define BSWAP_U64(x)                                    \
    ((((x) & 0x00000000000000FFull) << 56) |            \
     (((x) & 0x000000000000FF00ull) << 40) |            \
     (((x) & 0x0000000000FF0000ull) << 24) |            \
     (((x) & 0x00000000FF000000ull) << 8)  |            \
     (((x) & 0x000000FF00000000ull) >> 8)  |            \
     (((x) & 0x0000FF0000000000ull) >> 24) |            \
     (((x) & 0x00FF000000000000ull) >> 40) |            \
     (((x) & 0xFF00000000000000ull) >> 56))
static uint16_t BSWAP16(uint16_t x)
{
    return BSWAP_U16(x);
}
static uint32_t BSWAP32(uint32_t x)
{
    return BSWAP_U32(x);
}
static uint64_t BSWAP64(uint64_t x)
{
    return BSWAP_U64(x);
}

static MSG *neighbour(RNG &R, MSG *M)
{
    MSG *N = NULL;
    const size_t MAX = 32;
    size_t i = 0;
    if (R.flip())
    {   // Forward:
        N = M->next;
        for (; N != NULL && i < MAX; i++)
            N = N->next;
        i = R.bias(0, i, 1);
        N = M->next;
        for (; i > 0; i--)
            N = N->next;
    }
    else
    {   // Backward:
        N = M->prev;
        for (; N != NULL && i < MAX; i++)
            N = N->prev;
        i = R.bias(0, i, 1);
        N = M->prev;
        for (; i > 0; i--)
            N = N->prev;
    }
    return N;
}

/*
 * Clone the given message.
 */
static MSG *clone(MSG *M, size_t padding = 0)
{
    size_t size = sizeof(MSG) + M->len + padding;
    MSG *N = (MSG *)pmalloc(size);
    memcpy(N, M, sizeof(MSG));
    N->next = N->prev = N;
    memcpy(N->payload, M->payload, M->len);
    return N;
}

// 解析 P->payload （real_msg） 成 cur_region_seq (RegionSequence)
static void parse_regions(RegionSequence *cur_region_seq, MSG *P, size_t padding) {
    // fprintf(stderr, "parse_regions, P->payload: %s\n", (char *)P->payload);
    // fprintf(stderr, "parse_regions, P->len: %zu\n", P->len);
    // fprintf(stderr, "parse_regions, P->payload length: %zu\n", strlen((char *)P->payload));
    cur_region_seq->regions = (char**)pmalloc(sizeof(char*) * 10);
    cur_region_seq->count = 0;
    cur_region_seq->capacity = 10;
    //// 解析 P->payload 填充 cur_region_seq
    char *start = pstrdup_l((char *)P->payload, P->len);
    // fprintf(stderr, "start: %s\n", start);
    char *saveptr = NULL;
    char *line = strtok_r(start, "\n", &saveptr);
    // if (line && strlen(line) == 0) {
    //     fprintf(stderr, "line is empty\n");
    // }
    // 不要parse多了！！！
    while (line) {
        // fprintf(stderr, "line: %d\n", strlen(line));
        char* pos;
        if ((pos = strchr(line, '\n')) != NULL) *pos = '\0';
        // if (line && (pos = strchr(line, '\r')) != NULL) *pos = '\0';
        // fprintf(stderr, "line-aft: %d\n", strlen(line));
        // 跳过空行
        if (line[0] == '\0') continue;
        // 扩容
        if (cur_region_seq->count >= cur_region_seq->capacity) {
            cur_region_seq->capacity += 5;
            cur_region_seq->regions = (char**)prealloc(cur_region_seq->regions, sizeof(char*) * cur_region_seq->capacity);
        }
        // 存储 region
        char *copy = pstrdup(line);
        if (!copy) {
            fprintf(stderr, "pstrdup failed\n");
            break ;
        }
        cur_region_seq->regions[cur_region_seq->count++] = copy;
        line = strtok_r(NULL, "\n", &saveptr);
    }
    saveptr = NULL;
    pfree(start);
}

void replace_region(MSG *P, RegionSequence *cur_region_seq, RNG &R, size_t *padding) {
    // fprintf(stderr, "replace_region P: %s\n", P->payload);
    if (cur_region_seq->count == 0) return;

    // fprintf(stderr, "cur_region_seq->count in replace: %zu\n", cur_region_seq->count);
    uint32_t replace_idx = R.rand(0, cur_region_seq->count - 1);  // 选择 P->payload 里的一个 region，不是第几个，而是region[i]
    uint32_t new_idx = R.rand(0, region_seq->count - 1);  // 选择 region_seq 里的一个 region 替换

    // fprintf(stderr, "replace_idx: %u, new_idx: %u\n", replace_idx, new_idx);

    char *target = cur_region_seq->regions[replace_idx];
    char *replacement = region_seq->regions[new_idx];

    fprintf(stderr, "target: %s, replacement: %s\n", target, replacement);
    size_t old_len = strlen(target);
    size_t new_len = strlen(replacement);

    fprintf(stderr, "old_len: %zu, new_len: %zu\n", old_len, new_len);

    if (new_len > old_len && new_len - old_len > *padding) return ;

    char *start = (char *)P->payload;
    size_t offset = 0;
    // fprintf(stderr, "start: %s\n", start);
    for (size_t i = 0; i < replace_idx; i++) {
        size_t len = strlen(cur_region_seq->regions[i]);
        offset += len + 1;
        // if(cur_region_seq->regions[i][len - 1] == '\r') {
        //     offset += len + 2;
        // }
        // else {
        //     offset += len + 1;
        // }
    }
    // char *replace_pos = strchr(start, target);
    // fprintf(stderr, "offset: %zu\n", offset);
    // fprintf(stderr, "P->len: %zu\n", P->len);
    // fprintf(stderr, "P->slen: %zd\n", P->slen);
    // fprintf(stderr, "start + offset: %s\n", start + offset);
    // fprintf(stderr, "start + offset + old_len: %s\n", start + offset + old_len);
    // fprintf(stderr, "P->len - (offset + old_len): %zu\n", P->len - (offset + old_len));

    memmove(start + offset + new_len, start + offset + old_len, P->len - (offset + old_len));
    // fprintf(stderr, "memmove\n");
    memcpy(start + offset, replacement, new_len);
    // fprintf(stderr, "memcpy\n");

    P->len += (new_len - old_len);
    // fprintf(stderr, "P->len: %zu\n", P->len);
    *padding -= (new_len - old_len);
    // fprintf(stderr, "P->payload: %s\n", P->payload);
}

void insert_region(MSG *P, RegionSequence *cur_region_seq, RNG &R, size_t &padding) {
    // fprintf(stderr, "insert_region P: %s\n", P->payload);

    if (cur_region_seq->count == 0) return;

    // 随机选择插入点 (插入到 P->payload 的某个 Region 之后)
    size_t insert_idx = R.rand(0, cur_region_seq->count - 1); 
    // 随机选择插入的 Region
    char *inserted = region_seq->regions[R.rand(0, region_seq->count - 1)];  

    // 计算要插入的总长度
    size_t insert_len = strlen(inserted) + 1;  // 包括换行符

    fprintf(stderr, "insert_idx: %d, inserted: %s\n", insert_idx, inserted);

    // 检查 padding 是否足够
    if (insert_len > padding) return;

    char *start = (char *)P->payload;

    // 计算插入的偏置
     size_t offset = 0;
    for (size_t i = 0; i <= insert_idx; i++) {
        size_t len = strlen(cur_region_seq->regions[i]);
        offset += len + 1;
        // if(cur_region_seq->regions[i][len - 1] == '\r') {
        //     offset += len + 2;
        // }
        // else {
        //     offset += len + 1;
        // }
    }

    // 移动数据腾出空间
    memmove(start + offset + insert_len, start + offset, P->len - offset);

    // 插入 Region
    memcpy(start + offset, inserted, insert_len-1);
    start[offset + insert_len - 1] = '\n';
    // 更新 P->len 和 padding
    P->len += insert_len;
    padding -= insert_len;
}

void copy_region(MSG *P, RegionSequence *cur_region_seq, RNG &R, size_t &padding) {
    // fprintf(stderr, "copy_region P: %s\n", P->payload);

    if (cur_region_seq->count == 0) return;

    // 选择要复制的 Region
    size_t copy_idx = R.rand(0, cur_region_seq->count - 1);
    char *copied = cur_region_seq->regions[copy_idx];
    fprintf(stderr, "copied: %s\n", copied);

    // 随机选择复制到的Region之后
    size_t copy_to_idx = R.rand(0, cur_region_seq->count - 1);
    fprintf(stderr, "copy_to_idx: %d\n", copy_to_idx);
    // 计算复制的长度
    size_t copy_len = strlen(copied) + 1;

    // 确保 padding 足够
    if (copy_len > padding) return;

    char *start = (char *)P->payload;
    // 计算插入的偏置
     size_t offset = 0;
    for (size_t i = 0; i <= copy_to_idx; i++) {
        size_t len = strlen(cur_region_seq->regions[i]);
        offset += len + 1;
        // if(cur_region_seq->regions[i][len - 1] == '\r') {
        //     offset += len + 2;
        // }
        // else {
        //     offset += len + 1;
        // }
    }

    // 移动数据腾出空间
    memmove(start + offset + copy_len, start + offset, P->len - offset);

    // 复制 Region
    memcpy(start + offset, copied, copy_len-1);
    start[offset+copy_len-1] = '\n';

    // 更新 P->len 和 padding
    P->len += copy_len;
    padding -= copy_len;
}

void delete_region(MSG *P, RegionSequence *cur_region_seq, RNG &R, size_t &padding) {
    // fprintf(stderr, "delete_region P: %s\n", P->payload);

    if (cur_region_seq->count == 0) return;

    // 选择要删除的 Region
    size_t delete_idx = R.rand(0, cur_region_seq->count - 1);
    fprintf(stderr, "delete_idx: %d\n", delete_idx);
    char *deleted = cur_region_seq->regions[delete_idx];

    // 计算删除的长度
    size_t delete_len = strlen(deleted) + 1;

    // 计算删除的起点 (确保不越界)
    size_t delete_pos = 0;
    for (size_t i = 0; i < delete_idx; i++) {
        delete_pos += strlen(cur_region_seq->regions[i]) + 1;
    }

    // delete_pos = MIN(delete_pos, P->len - delete_len);
    if(delete_pos + delete_len > P->len) {
        fprintf(stderr, "delete_pos + delete_len > P->len\n");
        return ;
    }

    char *start = (char *)P->payload;

    // 移动数据，删除目标 Region
    memmove(start + delete_pos, start + delete_pos + delete_len, P->len - (delete_pos + delete_len));

    // 更新 P->len 和 padding
    P->len -= delete_len;
    padding += delete_len;
}


/*
 * Given a message, generate a mutant version.
 */
static MSG *mutate(RNG &R, MSG *M, size_t depth, size_t stage,
    bool copy = false, bool is_real_msg = false)
{
    if (M->outbound)
        return (copy? clone(M): M);

    // n is the number of mutations.
    // depth == 0 --> at least one mutation
    // depth > 0  --> n can be zero, and n increases with depth
    size_t n = (depth == 0) + R.bias(0, NLOG2(1, stage) + depth, 4);
    if (n == 0)
        return (copy? clone(M): M);     // No mutation applied

    // Clone M into N so that N can be mutated.
    size_t padding = 64 + R.bias(0, PADDING, 2);
    size_t size = sizeof(MSG) + M->len + padding;
    MSG *P = clone(M, padding), *N;

    // Apply random mutations:
    for (size_t i = 0; i < n; i++)
    {
        if (P->len == 0 && padding == 0)
            break;
        ssize_t j, k, l;
        int8_t i8    = 0x0; int16_t i16 = 0x0; int32_t i32 = 0x0;
        int64_t i64  = 0x0;
        int16_t *p16 = NULL; int32_t *p32 = NULL; int64_t *p64 = NULL;
        
        uint32_t rng;
        RegionSequence *cur_region_seq = NULL;
        // fprintf(stderr, "org M->len: %zu\n", M->len);
        // fprintf(stderr, "org P->len: %zu\n", P->len);
        if(is_real_msg) {
            rng = R.rand(0, 22);
            if(rng >= 19 ) {
                fprintf(stderr, "is_real_msg\n");
                // 解析 P->payload 填充 cur_region_seq
                cur_region_seq = (RegionSequence *)pmalloc(sizeof(RegionSequence));
                cur_region_seq->init();
                parse_regions(cur_region_seq, P, padding);
                // 解析 P->payload 填充 cur_region_seq
                fprintf(stderr, "cur_region_seq->count: %d\n", cur_region_seq->count);
                for (size_t i = 0; i < cur_region_seq->count; i++) {
                    fprintf(stderr, "cur_region_seq->regions[%zu]: %s\n", i, cur_region_seq->regions[i]);
                }
            }
        }
        else {
            rng = R.rand(0, 17);
            // fprintf(stderr, " not is_real_msg\n");
        }

        switch (rng)
        {
            case 0:     // flip bits int8_t
            flip_8:
                if (P->len == 0) goto insert;
                j = R.bias(1, 8, 4);
                for (ssize_t k = 0; k < j; k++)
                    i8 |= (0x1 << R.rand(0, 8-1));
                j = R.rand(0, P->len-1);
                P->payload[j] ^= i8;
                break;
            case 1:     // flip bits int16_t
            flip_16:
                if (P->len < sizeof(int16_t)) goto flip_8;
                j = R.bias(1, 16, 4);
                for (ssize_t k = 0; k < j; k++)
                    i16 |= (0x1 << R.rand(0, 16-1));
                j = R.rand(0, P->len-sizeof(int16_t));
                j -= (R.flip(8)? j % sizeof(int16_t): 0);
                p16 = (int16_t *)(P->payload + j);
                *p16 ^= i16;
                break;
            case 2:     // flip bits int32_t
            flip_32:
                if (P->len < sizeof(int32_t)) goto flip_16;
                j = R.bias(1, 32, 4);
                for (ssize_t k = 0; k < j; k++)
                    i32 |= (0x1 << R.rand(0, 32-1));
                j = R.rand(0, P->len-sizeof(int32_t));
                j -= (R.flip(8)? j % sizeof(int32_t): 0);
                p32 = (int32_t *)(P->payload + j);
                *p32 ^= i32;
                break;
            case 3:     // flip bits int64_t
                if (P->len < sizeof(int64_t)) goto flip_32;
                j = R.bias(1, 64, 4);
                for (ssize_t k = 0; k < j; k++)
                    i64 |= (0x1ull << R.rand(0, 64-1));
                j = R.rand(0, P->len-sizeof(int64_t));
                j -= (R.flip(8)? j % sizeof(int64_t): 0);
                p64 = (int64_t *)(P->payload + j);
                *p64 ^= i64;
                break;
            case 4:     // add int8_t
            add_8:
                if (P->len == 0) goto insert;
                k = 1 + R.bias(0, INT8_MAX-1, 4);
                k = (R.flip()? -k: k);
                j = R.rand(0, P->len-1);
                P->payload[j] += (int8_t)k;
                break;
            case 5:     // add int16_t
            add_16:
                if (P->len < sizeof(int16_t)) goto add_8;
                k = 1 + R.bias(0, INT16_MAX-1, 6);
                k = (R.flip()? -k: k);
                j = R.rand(0, P->len-sizeof(int16_t));
                j -= (R.flip(8)? j % sizeof(int16_t): 0);
                p16 = (int16_t *)(P->payload + j);
                *p16 += (R.flip()? *p16 + (int16_t)k:
                    BSWAP16(BSWAP16(*p16) + (int16_t)BSWAP16(k)));
                break;
            case 6:     // add int32_t
            add_32:
                if (P->len < sizeof(int32_t)) goto add_16;
                k = 1 + R.bias(0, INT32_MAX-1, 8);
                k = (R.flip()? -k: k);
                j = R.rand(0, P->len-sizeof(int32_t));
                j -= (R.flip(8)? j % sizeof(int32_t): 0);
                p32 = (int32_t *)(P->payload + j);
                *p32 = (R.flip()?  *p32 + (int32_t)k:
                    BSWAP32(BSWAP32(*p32) + (int32_t)BSWAP32(k)));
                break;
            case 7:     // add int64_t
                if (P->len < sizeof(int64_t)) goto add_32;
                k = 1 + R.bias(0, INT32_MAX-1, 8);
                k = (R.flip()? -k: k);
                j = R.rand(0, P->len-sizeof(int64_t));
                j -= (R.flip(8)? j % sizeof(int64_t): 0);
                p64 = (int64_t *)(P->payload + j);
                *p64 = (R.flip()? *p64 + (int64_t)k:
                    BSWAP64(BSWAP64(*p64) + (int64_t)BSWAP64(k)));
                break;
            case 8:     // Random bytes
            rand:
                if (P->len == 0 && padding == 0) break;
                if (P->len == 0) goto insert;
                j = R.rand(0, P->len-1);
                k = j + R.bias(0, 128, 4);
                k = MIN(k, P->slen-1);
                l = R.bias(2, 16, 2);
                for (ssize_t m = j; m <= k; m++)
                {
                    if (R.flip(n)) continue;
                    P->payload[m] ^= (uint8_t)R.rand(1, UINT8_MAX);
                }
                break;
            case 9:     // Delete bytes
                if (P->len == 0) goto insert;
                j = R.rand(0, P->len-1);
                k = 1 + j + R.bias(0, 128, 2);
                k = MIN(k, P->slen-1);
                memcpy(P->payload + j, P->payload + k, P->len - k);
                P->len -= (k - j);
                break;
            case 10:    // Copy bytes
                if (P->len == 0) goto insert;
                j = R.rand(0, P->len-1);
                k = R.rand(0, P->len-1);
                l = 1 + R.bias(0, 1024, 2);
                l = MIN(l, P->slen - j - 1);
                l = MIN(l, P->slen - k - 1);
                if (l == 0) goto rand;
                memmove(P->payload + j, P->payload + k, l);
                break;
            case 11:    // Set bytes
                if (P->len == 0) goto insert;
                j = R.rand(0, P->len-1);
                l = 2 + R.bias(0, 256, 4);
                l = MIN(l, P->slen - j - 1);
                if (l == 0) goto rand;
                k = R.rand(0, UINT8_MAX);
                memset(P->payload + j, (int)k, l);
                break;
            case 12:    // Insert random byte
                if (padding == 0) goto rand;
                j = R.rand(0, P->len);
                l = 1 + R.bias(0, padding-1, 2);
                k = R.rand(0, UINT8_MAX);
                memmove(P->payload + j + l, P->payload + j, P->len - j);
                memset(P->payload + j, (int)k, l);
                P->len  += l;
                padding -= l;
                break;
            case 13:    // Insert random bytes
            insert:
                if (padding == 0) goto rand;
                j = R.rand(0, P->len);
                l = 1 + R.bias(0, padding-1, 2);
                k = R.rand(0, UINT8_MAX);
                memmove(P->payload + j + l, P->payload + j, P->len - j);
                for (ssize_t m = j; m < j + l; m++)
                    P->payload[m] = (uint8_t)R.rand(0, UINT8_MAX);
                P->len  += l;
                padding -= l;
                break;
            case 14:    // Splice bytes (same message)
            splice:
                if (padding == 0) goto rand;
                if (P->len == 0)  goto insert;
                j = R.rand(0, P->len);
                k = R.rand(0, P->len-1);
                l = 1 + R.bias(0, padding-1, 3);
                l = MIN(l, P->slen - k - 1);
                if (l == 0) goto rand;
                memmove(P->payload + j + l, P->payload + j, P->len - j);
                memmove(P->payload + j, P->payload + k, l);
                P->len  += l;
                padding -= l;
                break;
            case 15:    // Splice messages
                if (padding == 0) goto rand;
                N = neighbour(R, M);
                if (N == NULL || N->len == 0) goto splice;
                j = R.rand(0, P->len);
                k = R.rand(0, N->len-1);
                l = 1 + R.bias(0, padding-1, 3);
                l = MIN(l, N->slen - k - 1);
                memmove(P->payload + j + l, P->payload + j, P->len - j);
                memmove(P->payload + j, N->payload + k, l);
                P->len  += l;
                padding -= l;
                break;
            case 16:    // Copy messages
                if (P->len == 0) goto insert;
                N = neighbour(R, M);
                if (N == NULL || N->len == 0) goto splice;
                j = R.rand(0, P->len-1);
                k = (j >= N->slen || R.flip(4)? R.rand(0, N->len-1): j);
                l = 1 + R.bias(0, 1024, 3);
                l = MIN(l, P->slen - j - 1);
                l = MIN(l, N->slen - k - 1);
                if (l == 0) goto rand;
                memmove(P->payload + j, N->payload + k, l);
                break;
            case 17:    // Blend messages
                if (P->len == 0) goto insert;
                N = neighbour(R, M);
                if (N == NULL || N->len == 0) goto splice;
                k = R.rand(1, 4);
                for (j = 0; j < N->slen && j < P->slen; j++)
                    if (R.flip(k))
                        P->payload[j] = N->payload[j];
                break;
            case 18:    // insert keywords
                fprintf(stderr, "case 18\n");
                if (dict && dict->count > 0) {  
                    // perror("doing insert keywords");
                    // 随机选择一个关键词
                    size_t keyword_index = R.rand(0, dict->count - 1);
                    const char *keyword = dict->keywords[keyword_index];
                    size_t keyword_len = strlen(keyword);
                    
                    if (keyword_len > 0 && padding >= keyword_len) {
                        // 随机选择插入位置
                        j = R.rand(0, P->len);
            
                        // 移动后面的数据为关键词腾出空间
                        memmove((char *)P->payload + j + keyword_len, 
                                (char *)P->payload + j, 
                                P->len - j);
            
                        // 插入关键词
                        memcpy((char *)P->payload + j, keyword, keyword_len);
            
                        // 更新长度信息
                        P->len += keyword_len;
                        padding -= keyword_len;
                        // fprintf(stderr, "insert keywords complete: %s", (char *)P->payload);
                    }
                }
                break;
            case 19:    // 替换region
                // fprintf(stderr, "case 19 original payload: %s\n", P->payload);
                replace_region(P, cur_region_seq, R, &padding);
                fprintf(stderr, "case 19 complete payload: %s\n", P->payload);
                break;
            case 20:    // 插入region
                insert_region(P, cur_region_seq, R, padding);
                fprintf(stderr, "case 20 complete payload: %s\n", P->payload);
                break;
            case 21:    // 复制region
                copy_region(P, cur_region_seq, R, padding);
                fprintf(stderr, "case 21 complete payload: %s\n", P->payload);
                break;
            case 22:    // 删除region
                delete_region(P, cur_region_seq, R, padding);
                fprintf(stderr, "case 22 complete payload: %s\n", P->payload);
                break;
        }
        
        if(is_real_msg && rng >= 19) {
            // fprintf(stderr, "cur_region_seq->count: %d\n", cur_region_seq->count);
            for(int i = 0; i < cur_region_seq->count; i++) {
                // fprintf(stderr, "cur_region_seq->regions[%d]: %s\n", i, cur_region_seq->regions[i]);
                pfree((void *)cur_region_seq->regions[i]);
            }
            pfree((void *)cur_region_seq);
        }
    }

    if (size != sizeof(MSG) + P->len)
    {
        // fprintf(stderr, "size != sizeof(MSG) + P->len\n");
        size = sizeof(MSG) + P->len;
        P = (MSG *)prealloc((void *)P, size);
    }

    return P;
}

