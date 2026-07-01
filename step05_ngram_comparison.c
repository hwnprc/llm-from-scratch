#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

/*
 * Step 05: N-gram 모델 비교 (2-gram ~ 5-gram)
 * N값에 따른 창의성과 정확도 비교
 */

#define HANGUL_START 0xAC00
#define HANGUL_END   0xD7A3
#define HANGUL_COUNT (HANGUL_END - HANGUL_START + 1)
#define BOS_TOKEN 0xFFFE
#define EOS_TOKEN 0xFFFF
#define MAX_LINE 1024
#define MAX_WORD_LEN 256
#define MAX_N 5
#define HASH_SIZE 1009
#define DICT_HASH_SIZE 10007

// N-gram 구조체
typedef struct NextChar {
    uint32_t unicode;
    int count;
    struct NextChar *next;
} NextChar;

typedef struct NgramEntry {
    uint32_t context[MAX_N];  // N-1개의 문맥
    int context_len;
    NextChar *head;
    int total_count;
    struct NgramEntry *next;
} NgramEntry;

// 단어 사전
typedef struct WordDict {
    char word[MAX_WORD_LEN * 4];
    struct WordDict *next;
} WordDict;

// 전역 변수
NgramEntry *ngram_tables[MAX_N][HASH_SIZE];  // ngram_tables[n-2][hash]
WordDict *word_dict[DICT_HASH_SIZE];

// UTF-8 변환
uint32_t utf8_to_unicode(const unsigned char *utf8, int *bytes_read) {
    if ((utf8[0] & 0x80) == 0) {
        *bytes_read = 1;
        return utf8[0];
    } else if ((utf8[0] & 0xE0) == 0xC0) {
        *bytes_read = 2;
        return ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
    } else if ((utf8[0] & 0xF0) == 0xE0) {
        *bytes_read = 3;
        return ((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
    }
    *bytes_read = 1;
    return 0;
}

void unicode_to_utf8(uint32_t unicode, char *utf8) {
    if (unicode < 0x80) {
        utf8[0] = (char)unicode;
        utf8[1] = '\0';
    } else if (unicode < 0x800) {
        utf8[0] = (char)(0xC0 | (unicode >> 6));
        utf8[1] = (char)(0x80 | (unicode & 0x3F));
        utf8[2] = '\0';
    } else if (unicode < 0x10000) {
        utf8[0] = (char)(0xE0 | (unicode >> 12));
        utf8[1] = (char)(0x80 | ((unicode >> 6) & 0x3F));
        utf8[2] = (char)(0x80 | (unicode & 0x3F));
        utf8[3] = '\0';
    }
}

// 단어 사전 함수
unsigned int hash_word(const char *word) {
    unsigned int h = 5381;
    while (*word) h = ((h << 5) + h) + *word++;
    return h % DICT_HASH_SIZE;
}

void add_to_dict(const char *word) {
    int h = hash_word(word);
    WordDict *d = word_dict[h];
    while (d) {
        if (strcmp(d->word, word) == 0) return;
        d = d->next;
    }
    d = (WordDict *)malloc(sizeof(WordDict));
    strncpy(d->word, word, sizeof(d->word) - 1);
    d->word[sizeof(d->word) - 1] = '\0';
    d->next = word_dict[h];
    word_dict[h] = d;
}

bool is_valid_word(const char *word) {
    int h = hash_word(word);
    WordDict *d = word_dict[h];
    while (d) {
        if (strcmp(d->word, word) == 0) return true;
        d = d->next;
    }
    return false;
}

// N-gram 해시 함수
unsigned int hash_ngram(uint32_t *context, int len) {
    unsigned int h = 17;
    for (int i = 0; i < len; i++) {
        h = h * 31 + context[i];
    }
    return h % HASH_SIZE;
}

// N-gram 찾기
NgramEntry *find_ngram(int n, uint32_t *context, int context_len) {
    int table_idx = n - 2;  // 2-gram은 [0], 3-gram은 [1], ...
    int h = hash_ngram(context, context_len);
    NgramEntry *e = ngram_tables[table_idx][h];

    while (e) {
        if (e->context_len == context_len) {
            bool match = true;
            for (int i = 0; i < context_len; i++) {
                if (e->context[i] != context[i]) {
                    match = false;
                    break;
                }
            }
            if (match) return e;
        }
        e = e->next;
    }
    return NULL;
}

// N-gram 추가
void add_ngram(int n, uint32_t *context, int context_len, uint32_t next_char) {
    int table_idx = n - 2;
    int h = hash_ngram(context, context_len);
    NgramEntry *entry = find_ngram(n, context, context_len);

    if (!entry) {
        entry = (NgramEntry *)malloc(sizeof(NgramEntry));
        for (int i = 0; i < context_len; i++) {
            entry->context[i] = context[i];
        }
        entry->context_len = context_len;
        entry->head = NULL;
        entry->total_count = 0;
        entry->next = ngram_tables[table_idx][h];
        ngram_tables[table_idx][h] = entry;
    }

    NextChar *nc = entry->head;
    while (nc) {
        if (nc->unicode == next_char) {
            nc->count++;
            entry->total_count++;
            return;
        }
        nc = nc->next;
    }

    nc = (NextChar *)malloc(sizeof(NextChar));
    nc->unicode = next_char;
    nc->count = 1;
    nc->next = entry->head;
    entry->head = nc;
    entry->total_count++;
}

// 학습
void train_ngrams(const char *filename, int max_n) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "파일 열기 실패: %s\n", filename);
        exit(1);
    }

    printf("============================================================\n");
    printf("N-gram 학습 중 (2-gram ~ %d-gram)...\n", max_n);
    printf("============================================================\n");

    char line[MAX_LINE];
    int word_count = 0;

    while (fgets(line, sizeof(line), f)) {
        word_count++;
        if (word_count % 10000 == 0) printf("  진행: %d 단어...\n", word_count);

        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;

        add_to_dict(line);

        uint32_t chars[MAX_WORD_LEN + MAX_N];
        int len = 0;

        // BOS 토큰 추가 (max_n - 1개)
        for (int i = 0; i < max_n - 1; i++) {
            chars[len++] = BOS_TOKEN;
        }

        unsigned char *p = (unsigned char *)line;
        while (*p) {
            int bytes;
            uint32_t u = utf8_to_unicode(p, &bytes);
            if (u >= HANGUL_START && u <= HANGUL_END) chars[len++] = u;
            p += bytes;
        }

        chars[len++] = EOS_TOKEN;

        // 각 N에 대해 N-gram 생성
        for (int n = 2; n <= max_n; n++) {
            for (int i = 0; i < len - n + 1; i++) {
                uint32_t context[MAX_N];
                for (int j = 0; j < n - 1; j++) {
                    context[j] = chars[i + j];
                }
                add_ngram(n, context, n - 1, chars[i + n - 1]);
            }
        }
    }

    fclose(f);
    printf("학습 완료: %d개 단어\n\n", word_count);
}

// 다음 글자 샘플링
uint32_t sample_next(int n, uint32_t *context, int context_len) {
    NgramEntry *entry = find_ngram(n, context, context_len);
    if (!entry || entry->total_count == 0) return EOS_TOKEN;

    int r = rand() % entry->total_count;
    int cum = 0;
    NextChar *nc = entry->head;
    while (nc) {
        cum += nc->count;
        if (r < cum) return nc->unicode;
        nc = nc->next;
    }
    return EOS_TOKEN;
}

// 단어 생성
void generate_word_ngram(int n, char *buf, int max_len) {
    buf[0] = '\0';
    uint32_t context[MAX_N];
    int context_len = n - 1;

    // 초기 문맥: BOS 토큰들
    for (int i = 0; i < context_len; i++) {
        context[i] = BOS_TOKEN;
    }

    char temp[5];
    int word_len = 0;

    while (word_len < max_len) {
        uint32_t next = sample_next(n, context, context_len);

        if (next == EOS_TOKEN || next == BOS_TOKEN) break;

        unicode_to_utf8(next, temp);
        strcat(buf, temp);
        word_len++;

        // 문맥 업데이트 (왼쪽으로 shift)
        for (int i = 0; i < context_len - 1; i++) {
            context[i] = context[i + 1];
        }
        context[context_len - 1] = next;
    }
}

// N-gram 평가
void evaluate_ngram(int n, int num_words) {
    printf("【%d-gram 결과】\n", n);
    printf("----------------------------------------\n");

    int valid = 0, invalid = 0;
    char word[MAX_WORD_LEN * 4];

    for (int i = 0; i < num_words; i++) {
        int tries = 0;
        do {
            generate_word_ngram(n, word, 20);
            tries++;
        } while (strlen(word) == 0 && tries < 10);

        if (strlen(word) == 0) { i--; continue; }

        bool ok = is_valid_word(word);
        if (ok) valid++; else invalid++;

        if (i < 10) {  // 처음 10개만 출력
            printf("%3d. %-20s %s\n", i + 1, word, ok ? "[O]" : "[X]");
        }
    }

    if (num_words > 10) {
        printf("... (%d개 더)\n", num_words - 10);
    }

    printf("\n%d-gram 통계:\n", n);
    printf("  실제 단어: %3d개 [O] (%.1f%%)\n", valid, valid * 100.0 / num_words);
    printf("  생성 단어: %3d개 [X] (%.1f%%)\n\n", invalid, invalid * 100.0 / num_words);
}

int main() {
    srand(time(NULL));

    // 초기화
    for (int n = 0; n < MAX_N; n++) {
        for (int h = 0; h < HASH_SIZE; h++) {
            ngram_tables[n][h] = NULL;
        }
    }
    for (int h = 0; h < DICT_HASH_SIZE; h++) {
        word_dict[h] = NULL;
    }

    // 학습
    int max_n = 5;
    train_ngrams("words_30k.txt", max_n);

    // 평가
    printf("============================================================\n");
    printf("N-gram 모델 비교 (각 100개 단어 생성)\n");
    printf("============================================================\n\n");

    int results[MAX_N][2];  // [n-2][0]=valid, [1]=invalid

    for (int n = 2; n <= max_n; n++) {
        evaluate_ngram(n, 100);
        // 결과 저장은 여기서 할 수 있지만 간단히 출력만
    }

    // 최종 비교 표
    printf("============================================================\n");
    printf("📊 N-gram 비교 결과 요약\n");
    printf("============================================================\n");
    printf("N-gram    실제단어[O]    생성단어[X]    창의성\n");
    printf("------------------------------------------------------------\n");

    // 실제로는 위 evaluate에서 저장한 값을 사용해야 하지만
    // 간단히 하기 위해 다시 생성
    for (int n = 2; n <= max_n; n++) {
        int valid = 0, invalid = 0;
        char word[MAX_WORD_LEN * 4];

        for (int i = 0; i < 100; i++) {
            int tries = 0;
            do {
                generate_word_ngram(n, word, 20);
                tries++;
            } while (strlen(word) == 0 && tries < 10);

            if (strlen(word) == 0) { i--; continue; }

            if (is_valid_word(word)) valid++; else invalid++;
        }

        printf("%d-gram    %3d (%5.1f%%)    %3d (%5.1f%%)    ",
               n, valid, valid * 100.0 / 100, invalid, invalid * 100.0 / 100);

        // 창의성 바 (생성 단어 비율에 따라)
        int bars = invalid / 5;
        for (int j = 0; j < bars; j++) printf("█");
        printf("\n");
    }

    printf("\n결론:\n");
    printf("- N이 작을수록 (2-gram) 더 창의적이지만 비현실적\n");
    printf("- N이 클수록 (5-gram) 더 현실적이지만 덜 창의적\n");
    printf("- 적절한 밸런스: 3-gram 또는 4-gram\n");

    printf("\n✅ 완료!\n");
    return 0;
}
