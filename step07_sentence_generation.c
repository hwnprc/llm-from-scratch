#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>

/*
 * Step 07: 글자 단위 N-gram 문장 생성
 *
 * 목표: 단어가 아닌 완전한 문장을 생성
 * 특징: 공백, 구두점 포함하여 자연스러운 한국어 문장 생성
 */

#define HANGUL_START 0xAC00
#define HANGUL_END   0xD7A3
#define BOS_TOKEN 0xFFFE
#define EOS_TOKEN 0xFFFF  // . ! ? 등의 종결부호
#define MAX_LINE 2048
#define MAX_SENTENCE_LEN 1024
#define MAX_N 5
#define HASH_SIZE 10007

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

NgramEntry *ngram_tables[MAX_N][HASH_SIZE];

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

bool is_sentence_end(uint32_t c) {
    return (c == '.' || c == '!' || c == '?' || c == EOS_TOKEN);
}

void add_ngram(int n, uint32_t *context, uint32_t next_char) {
    int context_len = n - 1;
    unsigned int h = hash_context(context, context_len);

    NgramEntry *entry = ngram_tables[n - 2][h];
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
    entry->next = ngram_tables[n - 2][h];
    ngram_tables[n - 2][h] = entry;
}

void train_sentence_ngrams(const char *filename, int max_n) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("파일 열기 실패: %s\n", filename);
        exit(1);
    }

    char line[MAX_LINE];
    int sentence_count = 0;

    printf("문장 단위 N-gram 학습 중 (2-gram ~ %d-gram)...\n", max_n);

    while (fgets(line, sizeof(line), f)) {
        // 빈 줄이나 제목은 스킵
        if (strlen(line) <= 1) continue;

        // 제목/작가 줄 필터링 (한글만 있고 짧은 경우)
        bool is_title = true;
        int hangul_count = 0;
        for (int i = 0; line[i]; i++) {
            unsigned char c = line[i];
            if (c == ' ' || c == '\n' || c == '\r') continue;
            if (c >= 0x80) hangul_count++; // 한글 체크
            if (c == '.' || c == ',' || c == '!' || c == '?') {
                is_title = false;
                break;
            }
        }
        if (is_title && hangul_count < 30) continue; // 짧은 제목 스킵

        // 줄바꿈 제거
        char *p = strchr(line, '\n');
        if (p) *p = '\0';
        p = strchr(line, '\r');
        if (p) *p = '\0';

        if (strlen(line) == 0) continue;

        // 문장을 유니코드 배열로 변환
        uint32_t chars[MAX_SENTENCE_LEN];
        int char_count = 0;
        p = line;
        while (*p && char_count < MAX_SENTENCE_LEN - 1) {
            uint32_t uni;
            int len = utf8_to_unicode(p, &uni);
            if (len == 0) break;
            chars[char_count++] = uni;
            p += len;
        }

        if (char_count == 0) continue;

        // 각 N-gram 학습
        for (int n = 2; n <= max_n; n++) {
            uint32_t context[MAX_N];
            int context_len = n - 1;

            // 문맥 초기화 (BOS)
            for (int i = 0; i < context_len; i++) {
                context[i] = BOS_TOKEN;
            }

            // 문장 내 모든 글자 학습
            for (int i = 0; i < char_count; i++) {
                add_ngram(n, context, chars[i]);

                // 문맥 업데이트
                for (int j = 0; j < context_len - 1; j++) {
                    context[j] = context[j + 1];
                }
                context[context_len - 1] = chars[i];
            }

            // 문장 끝 학습 (마지막 글자가 .!? 가 아니면 EOS 추가)
            if (!is_sentence_end(chars[char_count - 1])) {
                add_ngram(n, context, EOS_TOKEN);
            }
        }

        sentence_count++;
        if (sentence_count % 50 == 0) {
            printf("  진행: %d 문장...\n", sentence_count);
        }
    }

    fclose(f);
    printf("학습 완료: %d개 문장\n\n", sentence_count);
}

uint32_t sample_next(int n, uint32_t *context) {
    int context_len = n - 1;
    unsigned int h = hash_context(context, context_len);
    NgramEntry *entry = ngram_tables[n - 2][h];

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

void generate_sentence(int n, char *buf, int max_len) {
    buf[0] = '\0';
    uint32_t context[MAX_N];
    int context_len = n - 1;

    for (int i = 0; i < context_len; i++) {
        context[i] = BOS_TOKEN;
    }

    char temp[5];
    int total_bytes = 0;
    int char_count = 0;

    while (total_bytes < max_len - 10 && char_count < 200) {
        uint32_t next = sample_next(n, context);

        if (next == EOS_TOKEN) {
            // 문장이 .!? 로 끝나지 않으면 마침표 추가
            if (strlen(buf) > 0) {
                char last = buf[strlen(buf) - 1];
                if (last != '.' && last != '!' && last != '?') {
                    strcat(buf, ".");
                }
            }
            break;
        }

        if (next == BOS_TOKEN) break;

        // 문장 끝 기호 처리
        if (is_sentence_end(next)) {
            unicode_to_utf8(next, temp);
            strcat(buf, temp);
            break;
        }

        unicode_to_utf8(next, temp);
        strcat(buf, temp);
        total_bytes += strlen(temp);
        char_count++;

        for (int i = 0; i < context_len - 1; i++) {
            context[i] = context[i + 1];
        }
        context[context_len - 1] = next;
    }

    // 최대 길이 도달 시 마침표 추가
    if (char_count >= 200 || total_bytes >= max_len - 10) {
        if (strlen(buf) > 0) {
            char last = buf[strlen(buf) - 1];
            if (last != '.' && last != '!' && last != '?') {
                strcat(buf, ".");
            }
        }
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
    printf("Step 07: 글자 단위 N-gram 문장 생성\n");
    printf("============================================================\n\n");

    printf("📚 학습 데이터: 한국 고전 소설 13편\n");
    printf("🎯 목표: 완전한 문장 생성 (공백, 구두점 포함)\n\n");

    int max_n = 5;
    train_sentence_ngrams("korean_novel_sample.txt", max_n);

    // 결과를 파일로 저장
    FILE *fp = fopen("step07_generated_sentences.txt", "w");
    if (!fp) {
        printf("파일 생성 실패\n");
        return 1;
    }

    fprintf(fp, "============================================================\n");
    fprintf(fp, "글자 단위 N-gram 문장 생성 결과\n");
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
            char sentence[MAX_SENTENCE_LEN * 4];
            int tries = 0;

            do {
                generate_sentence(n, sentence, sizeof(sentence));
                tries++;
            } while (strlen(sentence) < 3 && tries < 10);

            if (strlen(sentence) >= 3) {
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
    printf("결과 파일: step07_generated_sentences.txt\n\n");

    printf("💡 다음 단계:\n");
    printf("  - 파일을 열어서 각 N-gram별 문장 품질 비교\n");
    printf("  - N이 클수록 원문과 유사해지는지 확인\n");
    printf("  - 문법적 완성도와 창의성의 균형 분석\n\n");

    return 0;
}
