#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#define HANGUL_START 0xAC00
#define HANGUL_END   0xD7A3
#define HANGUL_COUNT (HANGUL_END - HANGUL_START + 1)
#define BOS_TOKEN 0xFFFE
#define EOS_TOKEN 0xFFFF
#define MAX_LINE 1024
#define MAX_WORD_LEN 256

// 다음 글자 후보
typedef struct NextChar {
    uint32_t unicode;
    int count;
    struct NextChar *next;
} NextChar;

// 트라이그램 엔트리: (c1, c2) -> 다음 글자들
typedef struct TrigramEntry {
    uint32_t c1, c2;
    NextChar *head;
    int total_count;
    struct TrigramEntry *next;
} TrigramEntry;

// 단어 사전
typedef struct WordDict {
    char word[MAX_WORD_LEN * 4];
    struct WordDict *next;
} WordDict;

#define HASH_SIZE 101
TrigramEntry *trigram_table[HASH_SIZE];
WordDict *word_dict[HASH_SIZE];

// UTF-8 → Unicode
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

// Unicode → UTF-8
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

// 해시 함수
unsigned int hash_trigram(uint32_t c1, uint32_t c2) {
    return ((c1 + c2) * 37) % HASH_SIZE;
}

unsigned int hash_word(const char *word) {
    unsigned int h = 5381;
    while (*word) h = ((h << 5) + h) + *word++;
    return h % HASH_SIZE;
}

// 트라이그램 찾기
TrigramEntry *find_trigram(uint32_t c1, uint32_t c2) {
    int h = hash_trigram(c1, c2);
    TrigramEntry *e = trigram_table[h];
    while (e) {
        if (e->c1 == c1 && e->c2 == c2) return e;
        e = e->next;
    }
    return NULL;
}

// 트라이그램 추가
void add_trigram(uint32_t c1, uint32_t c2, uint32_t c3) {
    int h = hash_trigram(c1, c2);
    TrigramEntry *entry = find_trigram(c1, c2);

    if (!entry) {
        entry = (TrigramEntry *)malloc(sizeof(TrigramEntry));
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

    nc = (NextChar *)malloc(sizeof(NextChar));
    nc->unicode = c3;
    nc->count = 1;
    nc->next = entry->head;
    entry->head = nc;
    entry->total_count++;
}

// 단어 사전에 추가
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

// 단어 검증
bool is_valid_word(const char *word) {
    int h = hash_word(word);
    WordDict *d = word_dict[h];
    while (d) {
        if (strcmp(d->word, word) == 0) return true;
        d = d->next;
    }
    return false;
}

// 학습
void train_trigram(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "파일 열기 실패: %s\n", filename);
        exit(1);
    }

    printf("============================================================\n");
    printf("트라이그램 학습 중...\n");
    printf("============================================================\n");

    char line[MAX_LINE];
    int word_count = 0;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;

        word_count++;
        add_to_dict(line);

        uint32_t chars[MAX_WORD_LEN + 2];
        int len = 0;
        chars[len++] = BOS_TOKEN;

        unsigned char *p = (unsigned char *)line;
        while (*p) {
            int bytes;
            uint32_t u = utf8_to_unicode(p, &bytes);
            if (u >= HANGUL_START && u <= HANGUL_END) {
                chars[len++] = u;
            }
            p += bytes;
        }
        chars[len++] = EOS_TOKEN;

        for (int i = 0; i < len - 2; i++) {
            add_trigram(chars[i], chars[i + 1], chars[i + 2]);
        }
    }

    fclose(f);
    printf("학습 완료: %d개 단어\n\n", word_count);
}

// 다음 글자 샘플링
uint32_t sample_next(uint32_t c1, uint32_t c2) {
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

// 단어 생성
void generate_word(char *buf, int max_len) {
    buf[0] = '\0';

    // (BOS, 첫글자) 샘플링
    // 모든 (BOS, ?) 패턴 수집
    uint32_t candidates[1000];
    int weights[1000];
    int num_candidates = 0;

    for (int h = 0; h < HASH_SIZE; h++) {
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

    // 가중치 기반 샘플링
    int total_weight = 0;
    for (int i = 0; i < num_candidates; i++) {
        total_weight += weights[i];
    }

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
        uint32_t c3 = sample_next(c1, c2);
        if (c3 == EOS_TOKEN) break;

        unicode_to_utf8(c3, temp);
        strcat(buf, temp);
        c1 = c2;
        c2 = c3;
    }
}

int main() {
    srand(time(NULL));

    for (int i = 0; i < HASH_SIZE; i++) {
        trigram_table[i] = NULL;
        word_dict[i] = NULL;
    }

    train_trigram("words_30k.txt");

    printf("============================================================\n");
    printf("트라이그램 단어 생성 (100개)\n");
    printf("============================================================\n\n");

    char word[MAX_WORD_LEN * 4];
    int valid = 0, invalid = 0;

    for (int i = 0; i < 100; i++) {
        generate_word(word, 20);

        if (strlen(word) == 0) {
            i--;
            continue;
        }

        bool ok = is_valid_word(word);
        if (ok) valid++;
        else invalid++;

        printf("%3d. %-15s %s\n", i + 1, word, ok ? "[O]" : "[X]");
    }

    printf("\n실제 단어: %d개 [O]\n", valid);
    printf("생성 단어: %d개 [X]\n", invalid);
    printf("\n✅ 완료!\n");

    return 0;
}
