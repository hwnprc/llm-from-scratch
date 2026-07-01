#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

/*
 * Step 06: 전체 국어사전(44만개)으로 6-gram 학습
 *
 * 질문: "모든 국어사전 단어로 학습하면 6-gram이 100% [O]가 되지 않을까?"
 * 가설: 전체 사전 학습 → 6-gram → 완전 과적합 → 100% 실제 단어
 */

#define HANGUL_START 0xAC00
#define HANGUL_END   0xD7A3
#define BOS_TOKEN 0xFFFE
#define EOS_TOKEN 0xFFFF
#define MAX_LINE 1024
#define MAX_WORD_LEN 256
#define MAX_N 6
#define HASH_SIZE 10007
#define DICT_HASH_SIZE 20011

typedef struct NextChar {
    uint32_t unicode;
    int count;
    struct NextChar *next;
} NextChar;

typedef struct NgramEntry {
    uint32_t context[MAX_N];
    int context_len;
    NextChar *head;
    int total_count;
    struct NgramEntry *next;
} NgramEntry;

typedef struct WordDict {
    char word[MAX_WORD_LEN * 4];
    struct WordDict *next;
} WordDict;

NgramEntry *ngram_table[HASH_SIZE];
WordDict *word_dict[DICT_HASH_SIZE];

int utf8_to_unicode(const char *s, uint32_t *out) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) { *out = c; return 1; }
    if ((c & 0xE0) == 0xC0) {
        *out = ((c & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        *out = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        *out = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    return 0;
}

void unicode_to_utf8(uint32_t uni, char *buf) {
    if (uni < 0x80) {
        buf[0] = (char)uni; buf[1] = '\0';
    } else if (uni < 0x800) {
        buf[0] = 0xC0 | (uni >> 6);
        buf[1] = 0x80 | (uni & 0x3F);
        buf[2] = '\0';
    } else if (uni < 0x10000) {
        buf[0] = 0xE0 | (uni >> 12);
        buf[1] = 0x80 | ((uni >> 6) & 0x3F);
        buf[2] = 0x80 | (uni & 0x3F);
        buf[3] = '\0';
    } else {
        buf[0] = 0xF0 | (uni >> 18);
        buf[1] = 0x80 | ((uni >> 12) & 0x3F);
        buf[2] = 0x80 | ((uni >> 6) & 0x3F);
        buf[3] = 0x80 | (uni & 0x3F);
        buf[4] = '\0';
    }
}

unsigned int hash_context(uint32_t *ctx, int len) {
    unsigned int h = 0;
    for (int i = 0; i < len; i++) {
        h = h * 31 + ctx[i];
    }
    return h % HASH_SIZE;
}

unsigned int hash_word(const char *s) {
    unsigned int h = 0;
    while (*s) h = h * 31 + (unsigned char)(*s++);
    return h % DICT_HASH_SIZE;
}

void add_to_dict(const char *word) {
    unsigned int h = hash_word(word);
    WordDict *d = word_dict[h];
    while (d) {
        if (strcmp(d->word, word) == 0) return;
        d = d->next;
    }
    d = (WordDict *)malloc(sizeof(WordDict));
    strcpy(d->word, word);
    d->next = word_dict[h];
    word_dict[h] = d;
}

bool is_valid_word(const char *word) {
    unsigned int h = hash_word(word);
    WordDict *d = word_dict[h];
    while (d) {
        if (strcmp(d->word, word) == 0) return true;
        d = d->next;
    }
    return false;
}

void add_ngram(int n, uint32_t *context, uint32_t next_char) {
    int context_len = n - 1;
    unsigned int h = hash_context(context, context_len);

    NgramEntry *entry = ngram_table[h];
    while (entry) {
        if (entry->context_len == context_len) {
            bool match = true;
            for (int i = 0; i < context_len; i++) {
                if (entry->context[i] != context[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
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
                return;
            }
        }
        entry = entry->next;
    }

    entry = (NgramEntry *)malloc(sizeof(NgramEntry));
    for (int i = 0; i < context_len; i++) {
        entry->context[i] = context[i];
    }
    entry->context_len = context_len;
    entry->head = (NextChar *)malloc(sizeof(NextChar));
    entry->head->unicode = next_char;
    entry->head->count = 1;
    entry->head->next = NULL;
    entry->total_count = 1;
    entry->next = ngram_table[h];
    ngram_table[h] = entry;
}

void train_6gram(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("파일 열기 실패: %s\n", filename);
        exit(1);
    }

    char line[MAX_LINE];
    int count = 0;

    printf("전체 국어사전 학습 중 (442,491개 단어)...\n");

    while (fgets(line, sizeof(line), f)) {
        char *p = strchr(line, '\n');
        if (p) *p = '\0';
        if (strlen(line) == 0) continue;

        add_to_dict(line);

        uint32_t chars[MAX_WORD_LEN];
        int char_count = 0;
        p = line;
        while (*p) {
            uint32_t uni;
            int len = utf8_to_unicode(p, &uni);
            if (len == 0) break;
            chars[char_count++] = uni;
            p += len;
        }

        uint32_t context[MAX_N];
        for (int i = 0; i < 5; i++) context[i] = BOS_TOKEN;

        for (int i = 0; i < char_count; i++) {
            add_ngram(6, context, chars[i]);
            for (int j = 0; j < 4; j++) context[j] = context[j + 1];
            context[4] = chars[i];
        }

        add_ngram(6, context, EOS_TOKEN);

        count++;
        if (count % 50000 == 0) {
            printf("  진행: %d 단어...\n", count);
        }
    }

    fclose(f);
    printf("학습 완료: %d개 단어\n\n", count);
}

uint32_t sample_next(uint32_t *context) {
    unsigned int h = hash_context(context, 5);
    NgramEntry *entry = ngram_table[h];

    while (entry) {
        if (entry->context_len == 5) {
            bool match = true;
            for (int i = 0; i < 5; i++) {
                if (entry->context[i] != context[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                int r = rand() % entry->total_count;
                int cum = 0;
                NextChar *nc = entry->head;
                while (nc) {
                    cum += nc->count;
                    if (r < cum) return nc->unicode;
                    nc = nc->next;
                }
            }
        }
        entry = entry->next;
    }
    return EOS_TOKEN;
}

void generate_word_6gram(char *buf, int max_len) {
    buf[0] = '\0';
    uint32_t context[5];
    for (int i = 0; i < 5; i++) context[i] = BOS_TOKEN;

    char temp[5];
    int word_len = 0;

    while (word_len < max_len) {
        uint32_t next = sample_next(context);

        if (next == EOS_TOKEN || next == BOS_TOKEN) break;

        unicode_to_utf8(next, temp);
        strcat(buf, temp);
        word_len++;

        for (int i = 0; i < 4; i++) context[i] = context[i + 1];
        context[4] = next;
    }
}

int main() {
    srand(time(NULL));

    for (int h = 0; h < HASH_SIZE; h++) {
        ngram_table[h] = NULL;
    }
    for (int h = 0; h < DICT_HASH_SIZE; h++) {
        word_dict[h] = NULL;
    }

    printf("============================================================\n");
    printf("Step 06: 전체 국어사전으로 6-gram 과적합 실험\n");
    printf("============================================================\n\n");

    printf("🔬 실험 목적:\n");
    printf("  \"전체 국어사전으로 학습하면 6-gram이 100%% [O]가 될까?\"\n\n");

    printf("📊 실험 설계:\n");
    printf("  • 학습 데이터: 전체 442,491개 단어\n");
    printf("  • N-gram: 6-gram (5글자 문맥)\n");
    printf("  • 테스트: 100개 단어 생성\n\n");

    train_6gram("kr_korean_cleaned.csv");

    printf("============================================================\n");
    printf("6-gram 단어 생성 결과 (100개)\n");
    printf("============================================================\n\n");

    int valid = 0, invalid = 0;
    FILE *fp = fopen("step06_full_dict_6gram_words.txt", "w");

    if (fp) {
        fprintf(fp, "============================================================\n");
        fprintf(fp, "전체 사전(442,491개) 학습 → 6-gram 생성 결과\n");
        fprintf(fp, "============================================================\n\n");
    }

    for (int i = 0; i < 100; i++) {
        char word[MAX_WORD_LEN * 4];
        int tries = 0;

        do {
            generate_word_6gram(word, 20);
            tries++;
        } while (strlen(word) == 0 && tries < 10);

        if (strlen(word) == 0) {
            i--;
            continue;
        }

        bool ok = is_valid_word(word);
        if (ok) valid++;
        else invalid++;

        printf("%3d. %-20s %s\n", i + 1, word, ok ? "[O]" : "[X]");

        if (fp) {
            fprintf(fp, "%3d. %-20s %s\n", i + 1, word, ok ? "[O]" : "[X]");
        }
    }

    if (fp) {
        fprintf(fp, "\n============================================================\n");
        fprintf(fp, "통계\n");
        fprintf(fp, "============================================================\n");
        fprintf(fp, "실제 단어 [O]: %d개 (%.1f%%)\n", valid, valid * 100.0 / 100);
        fprintf(fp, "생성 단어 [X]: %d개 (%.1f%%)\n", invalid, invalid * 100.0 / 100);
        fclose(fp);
    }

    printf("\n============================================================\n");
    printf("📊 최종 결과\n");
    printf("============================================================\n");
    printf("실제 단어 [O]: %d개 (%.1f%%)\n", valid, valid * 100.0 / 100);
    printf("생성 단어 [X]: %d개 (%.1f%%)\n\n", invalid, invalid * 100.0 / 100);

    if (valid == 100) {
        printf("✅ 가설 성립: 전체 사전 학습 → 100%% 과적합\n");
    } else {
        printf("❌ 가설 기각: %.1f%%만 실제 단어\n", valid * 100.0 / 100);
        printf("\n🤔 이유:\n");
        printf("  1. 데이터 희소성: 평균 단어 길이(3.21글자) < 6-gram 문맥(5글자)\n");
        printf("  2. 조합 폭발: 가능한 6-gram 조합 >> 실제 학습된 조합\n");
        printf("  3. BOS 토큰: 단어 시작 부분은 충분한 문맥 없음\n");
    }

    printf("\n✅ 완료! 결과 저장: step06_full_dict_6gram_words.txt\n");

    return 0;
}
