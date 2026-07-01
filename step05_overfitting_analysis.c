#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

/*
 * Step 05: N-gram 과적합(Overfitting) 분석
 *
 * 가설: N이 클수록 과적합이 심해져서
 * - 사전 일치율 증가 (→ 100%)
 * - 창의성 감소 (→ 0%)
 * - 다양성 감소
 */

#define HANGUL_START 0xAC00
#define HANGUL_END   0xD7A3
#define BOS_TOKEN 0xFFFE
#define EOS_TOKEN 0xFFFF
#define MAX_LINE 1024
#define MAX_WORD_LEN 256
#define MAX_N 6
#define HASH_SIZE 1009
#define DICT_HASH_SIZE 10007

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

typedef struct UniqueWord {
    char word[MAX_WORD_LEN * 4];
    int count;
    struct UniqueWord *next;
} UniqueWord;

NgramEntry *ngram_tables[MAX_N][HASH_SIZE];
WordDict *word_dict[DICT_HASH_SIZE];
UniqueWord *unique_words[DICT_HASH_SIZE];

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

int count_hangul_chars(const char *word) {
    int len = 0;
    unsigned char *p = (unsigned char *)word;
    while (*p) {
        int bytes;
        uint32_t u = utf8_to_unicode(p, &bytes);
        if (u >= HANGUL_START && u <= HANGUL_END) len++;
        p += bytes;
    }
    return len;
}

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

void add_unique_word(const char *word) {
    int h = hash_word(word);
    UniqueWord *u = unique_words[h];
    while (u) {
        if (strcmp(u->word, word) == 0) {
            u->count++;
            return;
        }
        u = u->next;
    }
    u = (UniqueWord *)malloc(sizeof(UniqueWord));
    strncpy(u->word, word, sizeof(u->word) - 1);
    u->word[sizeof(u->word) - 1] = '\0';
    u->count = 1;
    u->next = unique_words[h];
    unique_words[h] = u;
}

int count_unique_words() {
    int count = 0;
    for (int i = 0; i < DICT_HASH_SIZE; i++) {
        UniqueWord *u = unique_words[i];
        while (u) {
            count++;
            u = u->next;
        }
    }
    return count;
}

void clear_unique_words() {
    for (int i = 0; i < DICT_HASH_SIZE; i++) {
        UniqueWord *u = unique_words[i];
        while (u) {
            UniqueWord *tmp = u;
            u = u->next;
            free(tmp);
        }
        unique_words[i] = NULL;
    }
}

unsigned int hash_ngram(uint32_t *context, int len) {
    unsigned int h = 17;
    for (int i = 0; i < len; i++) h = h * 31 + context[i];
    return h % HASH_SIZE;
}

NgramEntry *find_ngram(int n, uint32_t *context, int context_len) {
    int table_idx = n - 2;
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

void train_ngrams(const char *filename, int max_n) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "파일 열기 실패: %s\n", filename);
        exit(1);
    }

    printf("N-gram 학습 중 (2-gram ~ %d-gram)...\n", max_n);

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

void generate_word_ngram(int n, char *buf, int max_len) {
    buf[0] = '\0';
    uint32_t context[MAX_N];
    int context_len = n - 1;

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

        for (int i = 0; i < context_len - 1; i++) {
            context[i] = context[i + 1];
        }
        context[context_len - 1] = next;
    }
}

typedef struct {
    int n;
    int valid_count;
    int invalid_count;
    int unique_count;
    double avg_length;
    double valid_ratio;
    double diversity_ratio;
} NgramStats;

NgramStats evaluate_ngram(int n, int num_words) {
    NgramStats stats = {0};
    stats.n = n;

    char word[MAX_WORD_LEN * 4];
    int total_length = 0;
    clear_unique_words();

    // 각 N-gram별 결과를 파일로 저장
    char filename[100];
    sprintf(filename, "step05_%dgram_words.txt", n);
    FILE *fp = fopen(filename, "w");
    if (fp) {
        fprintf(fp, "============================================================\n");
        fprintf(fp, "%d-gram으로 생성된 100개 단어\n", n);
        fprintf(fp, "============================================================\n\n");
    }

    for (int i = 0; i < num_words; i++) {
        int tries = 0;
        do {
            generate_word_ngram(n, word, 20);
            tries++;
        } while (strlen(word) == 0 && tries < 10);

        if (strlen(word) == 0) { i--; continue; }

        bool ok = is_valid_word(word);
        if (ok) stats.valid_count++;
        else stats.invalid_count++;

        // 파일에 모든 단어 저장
        if (fp) {
            fprintf(fp, "%3d. %-20s %s\n", i + 1, word, ok ? "[O]" : "[X]");
        }

        add_unique_word(word);
        total_length += count_hangul_chars(word);
    }

    if (fp) {
        fprintf(fp, "\n통계:\n");
        fprintf(fp, "  실제 단어 [O]: %d개\n", stats.valid_count);
        fprintf(fp, "  생성 단어 [X]: %d개\n", stats.invalid_count);
        fclose(fp);
        printf("  → 결과 저장: %s\n", filename);
    }

    stats.unique_count = count_unique_words();
    stats.avg_length = (double)total_length / num_words;
    stats.valid_ratio = stats.valid_count * 100.0 / num_words;
    stats.diversity_ratio = stats.unique_count * 100.0 / num_words;

    return stats;
}

void print_bar(int value, int max_value, int width) {
    int bars = (value * width) / max_value;
    for (int i = 0; i < bars; i++) printf("█");
    for (int i = bars; i < width; i++) printf("░");
}

int main() {
    srand(time(NULL));

    for (int n = 0; n < MAX_N; n++) {
        for (int h = 0; h < HASH_SIZE; h++) {
            ngram_tables[n][h] = NULL;
        }
    }
    for (int h = 0; h < DICT_HASH_SIZE; h++) {
        word_dict[h] = NULL;
        unique_words[h] = NULL;
    }

    printf("============================================================\n");
    printf("Step 05: N-gram 과적합(Overfitting) 분석\n");
    printf("============================================================\n\n");

    printf("📊 데이터 분석:\n");
    printf("  • 학습 데이터: 3만 개 단어\n");
    printf("  • 평균 단어 길이: 3.21 글자\n");
    printf("  • 테스트: 각 N마다 100개 단어 생성\n\n");

    printf("가설:\n");
    printf("  N↑ → 사전일치율↑ → 100%% 수렴 (과적합)\n");
    printf("  N↑ → 창의성↓ → 0%% 수렴\n");
    printf("  N↑ → 다양성↓ (반복된 단어 증가)\n\n");

    int max_n = 6;
    train_ngrams("words_30k.txt", max_n);

    printf("============================================================\n");
    printf("실험 결과\n");
    printf("============================================================\n\n");

    NgramStats results[MAX_N];

    for (int n = 2; n <= max_n; n++) {
        printf("⏳ %d-gram 평가 중...\n", n);
        results[n - 2] = evaluate_ngram(n, 100);
    }

    printf("\n============================================================\n");
    printf("📈 종합 결과\n");
    printf("============================================================\n\n");

    printf("N  사전일치[O]  생성단어[X]  고유단어  평균길이  과적합지표\n");
    printf("----------------------------------------------------------------\n");

    for (int i = 0; i < max_n - 1; i++) {
        NgramStats s = results[i];
        printf("%d  %3d (%4.1f%%)  %3d (%4.1f%%)   %3d개    %.2f글자   ",
               s.n,
               s.valid_count, s.valid_ratio,
               s.invalid_count, 100.0 - s.valid_ratio,
               s.unique_count,
               s.avg_length);

        print_bar((int)s.valid_ratio, 100, 20);
        printf("\n");
    }

    printf("\n================================================================\n");
    printf("📊 과적합 분석\n");
    printf("================================================================\n\n");

    // 과적합 전환점 찾기
    int tipping_point = -1;
    for (int i = 0; i < max_n - 2; i++) {
        double increase = results[i + 1].valid_ratio - results[i].valid_ratio;
        if (increase > 15.0 && tipping_point == -1) {
            tipping_point = results[i + 1].n;
        }
    }

    printf("1. 사전 일치율 변화:\n");
    for (int i = 0; i < max_n - 2; i++) {
        double diff = results[i + 1].valid_ratio - results[i].valid_ratio;
        printf("   %d→%d-gram: %.1f%% → %.1f%% (%+.1f%%)\n",
               results[i].n, results[i + 1].n,
               results[i].valid_ratio, results[i + 1].valid_ratio, diff);
    }

    printf("\n2. 다양성 변화:\n");
    for (int i = 0; i < max_n - 1; i++) {
        printf("   %d-gram: %d개 고유 단어 (%.0f%% 다양성)\n",
               results[i].n, results[i].unique_count, results[i].diversity_ratio);
    }

    printf("\n3. 평균 길이 변화:\n");
    for (int i = 0; i < max_n - 1; i++) {
        printf("   %d-gram: %.2f 글자\n", results[i].n, results[i].avg_length);
    }

    printf("\n================================================================\n");
    printf("🎯 결론\n");
    printf("================================================================\n\n");

    if (tipping_point > 0) {
        printf("과적합 전환점: %d-gram\n", tipping_point);
        printf("  → %d-gram부터 급격한 과적합 발생\n\n", tipping_point);
    }

    // 최적 N 찾기
    int best_n = 2;
    double best_balance = 0;
    for (int i = 0; i < max_n - 1; i++) {
        // 창의성(생성 단어)과 품질(사전 일치)의 균형
        double creativity = 100.0 - results[i].valid_ratio;
        double quality = results[i].valid_ratio;
        double balance = (creativity > 30 && quality > 30) ?
                        (creativity + quality) / 2 : 0;

        if (balance > best_balance) {
            best_balance = balance;
            best_n = results[i].n;
        }
    }

    printf("권장 N-gram: %d-gram\n", best_n);
    printf("  → 창의성과 정확성의 최적 균형\n\n");

    printf("N값별 추천 용도:\n");
    for (int i = 0; i < max_n - 1; i++) {
        NgramStats s = results[i];
        printf("  %d-gram: ", s.n);

        if (s.valid_ratio < 40) {
            printf("창의적 단어 생성 (게임, 브랜드명)\n");
        } else if (s.valid_ratio < 60) {
            printf("균형잡힌 생성 (일반적 용도)\n");
        } else if (s.valid_ratio < 80) {
            printf("안정적 생성 (오타 교정 보조)\n");
        } else {
            printf("거의 암기 (실용성 낮음) ⚠️\n");
        }
    }

    printf("\n✅ 완료!\n");
    return 0;
}
