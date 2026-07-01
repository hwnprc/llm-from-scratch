#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#define HANGUL_START 0xAC00
#define HANGUL_END   0xD7A3
#define HANGUL_COUNT (HANGUL_END - HANGUL_START + 1)
#define EOS_TOKEN 0xFFFF
#define BOS_TOKEN 0xFFFE
#define MAX_LINE 1024
#define MAX_WORD_LEN 256
#define DICT_HASH_SIZE 10007

// 공통 구조체
typedef struct NextChar {
    uint32_t unicode;
    int count;
    struct NextChar *next;
} NextChar;

typedef struct WordDict {
    char word[MAX_WORD_LEN * 4];
    struct WordDict *next;
} WordDict;

// 바이그램 구조체
typedef struct {
    NextChar *head;
    int total_count;
} BigramEntry;

// 트라이그램 구조체
typedef struct TrigramEntry {
    uint32_t c1, c2;
    NextChar *head;
    int total_count;
    struct TrigramEntry *next;
} TrigramEntry;

// 전역 변수
BigramEntry *bigram_table;
int *start_char_count;
#define TRIGRAM_HASH_SIZE 101
TrigramEntry *trigram_table[TRIGRAM_HASH_SIZE];
WordDict *word_dict[DICT_HASH_SIZE];

// UTF-8 변환 함수들
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

// 바이그램 함수들
void add_bigram(uint32_t prev_char, uint32_t next_char) {
    if (prev_char < HANGUL_START || prev_char > HANGUL_END) return;
    int idx = prev_char - HANGUL_START;
    BigramEntry *entry = &bigram_table[idx];

    NextChar *curr = entry->head;
    while (curr) {
        if (curr->unicode == next_char) {
            curr->count++;
            entry->total_count++;
            return;
        }
        curr = curr->next;
    }

    NextChar *new_node = malloc(sizeof(NextChar));
    new_node->unicode = next_char;
    new_node->count = 1;
    new_node->next = entry->head;
    entry->head = new_node;
    entry->total_count++;
}

uint32_t sample_next_bigram(uint32_t prev_char) {
    if (prev_char < HANGUL_START || prev_char > HANGUL_END) return EOS_TOKEN;
    int idx = prev_char - HANGUL_START;
    BigramEntry *entry = &bigram_table[idx];
    if (entry->total_count == 0) return EOS_TOKEN;

    int rand_val = rand() % entry->total_count;
    int cumulative = 0;
    NextChar *curr = entry->head;
    while (curr) {
        cumulative += curr->count;
        if (rand_val < cumulative) return curr->unicode;
        curr = curr->next;
    }
    return EOS_TOKEN;
}

uint32_t sample_start_char() {
    int total = 0;
    for (int i = 0; i < HANGUL_COUNT; i++) total += start_char_count[i];
    if (total == 0) return HANGUL_START;

    int rand_val = rand() % total;
    int cumulative = 0;
    for (int i = 0; i < HANGUL_COUNT; i++) {
        cumulative += start_char_count[i];
        if (rand_val < cumulative) return HANGUL_START + i;
    }
    return HANGUL_START;
}

void generate_word_bigram(char *buffer, int max_len) {
    buffer[0] = '\0';
    char temp[5];
    uint32_t current_char = sample_start_char();
    int len = 0;

    while (len < max_len) {
        unicode_to_utf8(current_char, temp);
        strcat(buffer, temp);
        len++;
        uint32_t next_char = sample_next_bigram(current_char);
        if (next_char == EOS_TOKEN) break;
        current_char = next_char;
    }
}

// 트라이그램 함수들
unsigned int hash_trigram(uint32_t c1, uint32_t c2) {
    return ((c1 + c2) * 37) % TRIGRAM_HASH_SIZE;
}

TrigramEntry *find_trigram(uint32_t c1, uint32_t c2) {
    int h = hash_trigram(c1, c2);
    TrigramEntry *e = trigram_table[h];
    while (e) {
        if (e->c1 == c1 && e->c2 == c2) return e;
        e = e->next;
    }
    return NULL;
}

void add_trigram(uint32_t c1, uint32_t c2, uint32_t c3) {
    int h = hash_trigram(c1, c2);
    TrigramEntry *entry = find_trigram(c1, c2);

    if (!entry) {
        entry = malloc(sizeof(TrigramEntry));
        entry->c1 = c1;
        entry->c2 = c2;
        entry->head = NULL;
        entry->total_count = 0;
        entry->next = trigram_table[h];
        trigram_table[h] = entry;
    }

    NextChar *nc = entry->head;
    while (nc) {
        if (nc->unicode == c3) {
            nc->count++;
            entry->total_count++;
            return;
        }
        nc = nc->next;
    }

    nc = malloc(sizeof(NextChar));
    nc->unicode = c3;
    nc->count = 1;
    nc->next = entry->head;
    entry->head = nc;
    entry->total_count++;
}

uint32_t sample_next_trigram(uint32_t c1, uint32_t c2) {
    TrigramEntry *entry = find_trigram(c1, c2);
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

void generate_word_trigram(char *buf, int max_len) {
    buf[0] = '\0';
    uint32_t candidates[1000];
    int weights[1000];
    int num_candidates = 0;

    for (int h = 0; h < TRIGRAM_HASH_SIZE; h++) {
        TrigramEntry *e = trigram_table[h];
        while (e) {
            if (e->c1 == BOS_TOKEN && e->c2 >= HANGUL_START && e->c2 <= HANGUL_END) {
                candidates[num_candidates] = e->c2;
                weights[num_candidates] = e->total_count;
                num_candidates++;
            }
            e = e->next;
        }
    }

    if (num_candidates == 0) return;

    int total_weight = 0;
    for (int i = 0; i < num_candidates; i++) total_weight += weights[i];

    int r = rand() % total_weight;
    int cum = 0;
    uint32_t first_char = candidates[0];
    for (int i = 0; i < num_candidates; i++) {
        cum += weights[i];
        if (r < cum) {
            first_char = candidates[i];
            break;
        }
    }

    char temp[5];
    unicode_to_utf8(first_char, temp);
    strcat(buf, temp);

    uint32_t c1 = BOS_TOKEN;
    uint32_t c2 = first_char;

    for (int i = 0; i < max_len; i++) {
        uint32_t c3 = sample_next_trigram(c1, c2);
        if (c3 == EOS_TOKEN) break;
        unicode_to_utf8(c3, temp);
        strcat(buf, temp);
        c1 = c2;
        c2 = c3;
    }
}

// 학습 함수
void train_models(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "파일 열기 실패: %s\n", filename);
        exit(1);
    }

    printf("============================================================\n");
    printf("바이그램 & 트라이그램 학습 중...\n");
    printf("============================================================\n");

    char line[MAX_LINE];
    int word_count = 0;

    while (fgets(line, sizeof(line), f)) {
        word_count++;
        if (word_count % 10000 == 0) printf("  진행: %d 단어...\n", word_count);

        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;

        add_to_dict(line);

        uint32_t chars[MAX_WORD_LEN + 2];
        int len = 0;
        chars[len++] = BOS_TOKEN;

        unsigned char *p = (unsigned char *)line;
        while (*p) {
            int bytes;
            uint32_t u = utf8_to_unicode(p, &bytes);
            if (u >= HANGUL_START && u <= HANGUL_END) chars[len++] = u;
            p += bytes;
        }
        chars[len++] = EOS_TOKEN;

        if (len < 3) continue;

        // 바이그램 학습
        start_char_count[chars[1] - HANGUL_START]++;
        for (int i = 1; i < len - 1; i++) {
            add_bigram(chars[i], chars[i + 1]);
        }

        // 트라이그램 학습
        for (int i = 0; i < len - 2; i++) {
            add_trigram(chars[i], chars[i + 1], chars[i + 2]);
        }
    }

    fclose(f);
    printf("학습 완료: %d개 단어\n\n", word_count);
}

// 평가 함수
void evaluate_models(int num_words) {
    printf("============================================================\n");
    printf("모델 비교 평가 (각 %d개 단어 생성)\n", num_words);
    printf("============================================================\n\n");

    // 바이그램 평가
    printf("【바이그램 결과】\n");
    printf("----------------------------------------\n");
    int bigram_valid = 0, bigram_invalid = 0;
    char word[MAX_WORD_LEN * 4];

    for (int i = 0; i < num_words; i++) {
        generate_word_bigram(word, 20);
        if (strlen(word) == 0) { i--; continue; }
        bool ok = is_valid_word(word);
        if (ok) bigram_valid++; else bigram_invalid++;
        printf("%3d. %-20s %s\n", i + 1, word, ok ? "[O]" : "[X]");
    }

    printf("\n바이그램 통계:\n");
    printf("  실제 단어: %d개 [O] (%.1f%%)\n", bigram_valid, bigram_valid * 100.0 / num_words);
    printf("  생성 단어: %d개 [X] (%.1f%%)\n\n", bigram_invalid, bigram_invalid * 100.0 / num_words);

    // 트라이그램 평가
    printf("【트라이그램 결과】\n");
    printf("----------------------------------------\n");
    int trigram_valid = 0, trigram_invalid = 0;

    for (int i = 0; i < num_words; i++) {
        generate_word_trigram(word, 20);
        if (strlen(word) == 0) { i--; continue; }
        bool ok = is_valid_word(word);
        if (ok) trigram_valid++; else trigram_invalid++;
        printf("%3d. %-20s %s\n", i + 1, word, ok ? "[O]" : "[X]");
    }

    printf("\n트라이그램 통계:\n");
    printf("  실제 단어: %d개 [O] (%.1f%%)\n", trigram_valid, trigram_valid * 100.0 / num_words);
    printf("  생성 단어: %d개 [X] (%.1f%%)\n\n", trigram_invalid, trigram_invalid * 100.0 / num_words);

    // 비교 결과
    printf("============================================================\n");
    printf("📊 비교 결과\n");
    printf("============================================================\n");
    printf("           바이그램      트라이그램\n");
    printf("실제단어   %3d (%.1f%%)   %3d (%.1f%%)\n",
           bigram_valid, bigram_valid * 100.0 / num_words,
           trigram_valid, trigram_valid * 100.0 / num_words);
    printf("생성단어   %3d (%.1f%%)   %3d (%.1f%%)\n",
           bigram_invalid, bigram_invalid * 100.0 / num_words,
           trigram_invalid, trigram_invalid * 100.0 / num_words);
    printf("\n");

    if (bigram_invalid > trigram_invalid) {
        printf("✅ 바이그램이 더 창의적입니다 (생성 단어가 %d개 더 많음)\n",
               bigram_invalid - trigram_invalid);
    } else if (trigram_invalid > bigram_invalid) {
        printf("✅ 트라이그램이 더 창의적입니다 (생성 단어가 %d개 더 많음)\n",
               trigram_invalid - bigram_invalid);
    } else {
        printf("✅ 두 모델의 창의성이 비슷합니다\n");
    }
}

int main() {
    srand(time(NULL));

    // 메모리 할당 및 초기화
    bigram_table = calloc(HANGUL_COUNT, sizeof(BigramEntry));
    start_char_count = calloc(HANGUL_COUNT, sizeof(int));
    for (int i = 0; i < TRIGRAM_HASH_SIZE; i++) trigram_table[i] = NULL;
    for (int i = 0; i < DICT_HASH_SIZE; i++) word_dict[i] = NULL;

    // 학습
    train_models("words_30k.txt");

    // 평가
    evaluate_models(100);

    printf("\n✅ 완료!\n");
    return 0;
}
