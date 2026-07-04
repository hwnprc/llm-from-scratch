#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

/*
 * Step 08: 단어 단위 N-gram 문장 생성
 *
 * 글자 단위 vs 단어 단위:
 * - 글자: "나" "는" " " "고" "향" ...
 * - 단어: "나/NP" "는/JX" "<S>" "고향/NNG" ...
 *
 * 장점: 더 자연스러운 문법, 빠른 학습
 */

#define MAX_LINE 8192
#define MAX_TOKEN_LEN 256
#define MAX_N 5
#define HASH_SIZE 10007
#define MAX_SENTENCE_TOKENS 500

// Special tokens
#define BOS_TOKEN "<BOS>"
#define EOS_TOKEN "<EOS>"
#define SPACE_TOKEN "<S>"
#define UNK_TOKEN "<UNK>"

typedef struct NextToken {
    char token[MAX_TOKEN_LEN];
    int count;
    struct NextToken *next;
} NextToken;

typedef struct NgramEntry {
    char context[MAX_N][MAX_TOKEN_LEN];
    int context_len;
    NextToken *head;
    int total_count;
    struct NgramEntry *next;
} NgramEntry;

NgramEntry *ngram_tables[MAX_N][HASH_SIZE];

unsigned int hash_tokens(char context[][MAX_TOKEN_LEN], int len) {
    unsigned int h = 0;
    for (int i = 0; i < len; i++) {
        for (char *p = context[i]; *p; p++) {
            h = h * 31 + (unsigned char)(*p);
        }
    }
    return h % HASH_SIZE;
}

void add_ngram(int n, char context[][MAX_TOKEN_LEN], const char *next_token) {
    int context_len = n - 1;
    unsigned int h = hash_tokens(context, context_len);

    NgramEntry *entry = ngram_tables[n - 2][h];

    // 기존 엔트리 검색
    while (entry) {
        if (entry->context_len == context_len) {
            bool match = true;
            for (int i = 0; i < context_len; i++) {
                if (strcmp(entry->context[i], context[i]) != 0) {
                    match = false;
                    break;
                }
            }
            if (match) {
                // 다음 토큰 검색
                NextToken *nt = entry->head;
                while (nt) {
                    if (strcmp(nt->token, next_token) == 0) {
                        nt->count++;
                        entry->total_count++;
                        return;
                    }
                    nt = nt->next;
                }
                // 새 다음 토큰 추가
                nt = (NextToken *)malloc(sizeof(NextToken));
                strncpy(nt->token, next_token, MAX_TOKEN_LEN - 1);
                nt->token[MAX_TOKEN_LEN - 1] = '\0';
                nt->count = 1;
                nt->next = entry->head;
                entry->head = nt;
                entry->total_count++;
                return;
            }
        }
        entry = entry->next;
    }

    // 새 엔트리 생성
    entry = (NgramEntry *)malloc(sizeof(NgramEntry));
    for (int i = 0; i < context_len; i++) {
        strncpy(entry->context[i], context[i], MAX_TOKEN_LEN - 1);
        entry->context[i][MAX_TOKEN_LEN - 1] = '\0';
    }
    entry->context_len = context_len;

    entry->head = (NextToken *)malloc(sizeof(NextToken));
    strncpy(entry->head->token, next_token, MAX_TOKEN_LEN - 1);
    entry->head->token[MAX_TOKEN_LEN - 1] = '\0';
    entry->head->count = 1;
    entry->head->next = NULL;
    entry->total_count = 1;

    entry->next = ngram_tables[n - 2][h];
    ngram_tables[n - 2][h] = entry;
}

void train_word_ngrams(const char *filename, int max_n) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("파일 열기 실패: %s\n", filename);
        exit(1);
    }

    char line[MAX_LINE];
    int sentence_count = 0;

    printf("단어 단위 N-gram 학습 중 (2-gram ~ %d-gram)...\n", max_n);

    while (fgets(line, sizeof(line), f)) {
        // 줄바꿈 제거
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';

        if (strlen(line) == 0) continue;

        // <W> 구분자로 토큰 분리
        char *tokens[MAX_SENTENCE_TOKENS];
        int token_count = 0;

        char *p = line;
        char *start = p;

        while (*p && token_count < MAX_SENTENCE_TOKENS) {
            if (p[0] == '<' && p[1] == 'W' && p[2] == '>') {
                if (p > start) {
                    int len = p - start;
                    tokens[token_count] = (char *)malloc(len + 1);
                    strncpy(tokens[token_count], start, len);
                    tokens[token_count][len] = '\0';
                    token_count++;
                }
                p += 3; // <W> 스킵
                start = p;
            } else {
                p++;
            }
        }

        // 마지막 토큰
        if (p > start && token_count < MAX_SENTENCE_TOKENS) {
            int len = p - start;
            tokens[token_count] = (char *)malloc(len + 1);
            strncpy(tokens[token_count], start, len);
            tokens[token_count][len] = '\0';
            token_count++;
        }

        if (token_count < 2) {
            for (int i = 0; i < token_count; i++) free(tokens[i]);
            continue;
        }

        // 각 N-gram 학습
        for (int n = 2; n <= max_n; n++) {
            char context[MAX_N][MAX_TOKEN_LEN];
            int context_len = n - 1;

            // BOS로 초기화
            for (int i = 0; i < context_len; i++) {
                strcpy(context[i], BOS_TOKEN);
            }

            // 문장 학습
            for (int i = 0; i < token_count; i++) {
                add_ngram(n, context, tokens[i]);

                // 문맥 업데이트
                for (int j = 0; j < context_len - 1; j++) {
                    strcpy(context[j], context[j + 1]);
                }
                strcpy(context[context_len - 1], tokens[i]);
            }
        }

        // 메모리 해제
        for (int i = 0; i < token_count; i++) {
            free(tokens[i]);
        }

        sentence_count++;
        if (sentence_count % 50 == 0) {
            printf("  진행: %d 문장...\n", sentence_count);
        }
    }

    fclose(f);
    printf("학습 완료: %d개 문장\n\n", sentence_count);
}

void sample_next_token(int n, char context[][MAX_TOKEN_LEN], char *result) {
    int context_len = n - 1;
    unsigned int h = hash_tokens(context, context_len);
    NgramEntry *entry = ngram_tables[n - 2][h];

    while (entry) {
        if (entry->context_len == context_len) {
            bool match = true;
            for (int i = 0; i < context_len; i++) {
                if (strcmp(entry->context[i], context[i]) != 0) {
                    match = false;
                    break;
                }
            }
            if (match) {
                int r = rand() % entry->total_count;
                int cum = 0;
                NextToken *nt = entry->head;
                while (nt) {
                    cum += nt->count;
                    if (r < cum) {
                        strcpy(result, nt->token);
                        return;
                    }
                    nt = nt->next;
                }
            }
        }
        entry = entry->next;
    }

    strcpy(result, EOS_TOKEN);
}

void generate_sentence(int n, char *buf, int max_len) {
    buf[0] = '\0';
    char context[MAX_N][MAX_TOKEN_LEN];
    int context_len = n - 1;

    // BOS로 초기화
    for (int i = 0; i < context_len; i++) {
        strcpy(context[i], BOS_TOKEN);
    }

    char next_token[MAX_TOKEN_LEN];
    int token_count = 0;

    while (token_count < 100 && strlen(buf) < max_len - 300) {
        sample_next_token(n, context, next_token);

        if (strcmp(next_token, EOS_TOKEN) == 0) {
            break;
        }

        if (strcmp(next_token, BOS_TOKEN) == 0) {
            break;
        }

        // 공백 토큰 처리
        if (strcmp(next_token, SPACE_TOKEN) == 0) {
            if (strlen(buf) > 0) {
                strcat(buf, " ");
            }
        } else {
            // 형태소/품사에서 형태소만 추출
            char *slash = strchr(next_token, '/');
            if (slash) {
                int len = slash - next_token;
                strncat(buf, next_token, len);
            } else {
                strcat(buf, next_token);
            }
        }

        // 문맥 업데이트
        for (int i = 0; i < context_len - 1; i++) {
            strcpy(context[i], context[i + 1]);
        }
        strcpy(context[context_len - 1], next_token);

        token_count++;
    }

    // 마침표 추가
    if (strlen(buf) > 0 && buf[strlen(buf) - 1] != '.') {
        strcat(buf, ".");
    }
}

int main() {
    srand(time(NULL));

    // 초기화
    for (int n = 0; n < MAX_N; n++) {
        for (int h = 0; h < HASH_SIZE; h++) {
            ngram_tables[n][h] = NULL;
        }
    }

    printf("============================================================\n");
    printf("Step 08: 단어 단위 N-gram 문장 생성\n");
    printf("============================================================\n\n");

    printf("📚 학습 데이터: Kiwi 형태소 분석 결과\n");
    printf("🎯 목표: 단어 단위로 자연스러운 문장 생성\n\n");

    int max_n = 5;
    train_word_ngrams("tokenized_tokens.txt", max_n);

    // 결과 파일
    FILE *fp = fopen("step08_word_ngram_sentences.txt", "w");
    if (!fp) {
        printf("파일 생성 실패\n");
        return 1;
    }

    fprintf(fp, "============================================================\n");
    fprintf(fp, "단어 단위 N-gram 문장 생성 결과\n");
    fprintf(fp, "============================================================\n\n");

    printf("============================================================\n");
    printf("문장 생성 중...\n");
    printf("============================================================\n\n");

    for (int n = 2; n <= max_n; n++) {
        printf("📝 %d-gram 문장 생성 중...\n", n);

        fprintf(fp, "============================================================\n");
        fprintf(fp, "%d-gram 생성 문장 (20개)\n", n);
        fprintf(fp, "============================================================\n\n");

        int total_len = 0;
        int completed = 0;

        for (int i = 0; i < 20; i++) {
            char sentence[MAX_LINE];
            int tries = 0;

            do {
                generate_sentence(n, sentence, sizeof(sentence));
                tries++;
            } while (strlen(sentence) < 5 && tries < 10);

            if (strlen(sentence) >= 5) {
                fprintf(fp, "%2d. %s\n", i + 1, sentence);
                total_len += strlen(sentence);
                completed++;
            } else {
                i--; // 재시도
            }
        }

        double avg_len = completed > 0 ? (double)total_len / completed : 0;
        fprintf(fp, "\n통계:\n");
        fprintf(fp, "  평균 길이: %.1f 바이트\n", avg_len);
        fprintf(fp, "  완성된 문장: %d/20\n\n", completed);

        printf("  → 완료: %d개 문장 (평균 %.1f 바이트)\n\n", completed, avg_len);
    }

    fclose(fp);

    printf("============================================================\n");
    printf("✅ 완료!\n");
    printf("============================================================\n");
    printf("결과 파일: step08_word_ngram_sentences.txt\n\n");

    printf("💡 비교:\n");
    printf("  - step07: 글자 단위 N-gram\n");
    printf("  - step08: 단어 단위 N-gram\n");
    printf("  → 어느 쪽이 더 자연스러운가?\n\n");

    return 0;
}
