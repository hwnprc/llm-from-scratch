#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/*
 * step02: 바이그램(Bigram) 모델로 텍스트 생성
 *
 * 바이그램: 연속된 두 글자의 조합
 * 학습: "A 다음에 B가 몇 번 나왔는가?" 카운트
 * 생성: 이전 글자를 보고 다음 글자를 확률적으로 선택
 */

#define HANGUL_START 0xAC00
#define HANGUL_END   0xD7A3
#define HANGUL_COUNT (HANGUL_END - HANGUL_START + 1)
#define MAX_LINE 1024
#define MAX_WORD_LEN 256

// 다음 글자 후보 (연결 리스트)
typedef struct NextChar {
    uint32_t unicode;      // 다음에 올 글자
    int count;             // 빈도
    struct NextChar *next;
} NextChar;

// 각 글자별 바이그램 엔트리
typedef struct {
    NextChar *head;        // 다음 글자 리스트의 시작
    int total_count;       // 이 글자 다음에 온 총 글자 수
} BigramEntry;

BigramEntry *bigram_table;

// UTF-8 → Unicode 변환
uint32_t utf8_to_unicode(const unsigned char *utf8, int *bytes_read) {
    uint32_t codepoint = 0;

    if ((utf8[0] & 0x80) == 0) {
        codepoint = utf8[0];
        *bytes_read = 1;
    } else if ((utf8[0] & 0xE0) == 0xC0) {
        codepoint = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
        *bytes_read = 2;
    } else if ((utf8[0] & 0xF0) == 0xE0) {
        codepoint = ((utf8[0] & 0x0F) << 12) |
                    ((utf8[1] & 0x3F) << 6) |
                    (utf8[2] & 0x3F);
        *bytes_read = 3;
    } else if ((utf8[0] & 0xF8) == 0xF0) {
        codepoint = ((utf8[0] & 0x07) << 18) |
                    ((utf8[1] & 0x3F) << 12) |
                    ((utf8[2] & 0x3F) << 6) |
                    (utf8[3] & 0x3F);
        *bytes_read = 4;
    } else {
        *bytes_read = 1;
    }

    return codepoint;
}

// Unicode → UTF-8 변환
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
    } else {
        utf8[0] = (char)(0xF0 | (unicode >> 18));
        utf8[1] = (char)(0x80 | ((unicode >> 12) & 0x3F));
        utf8[2] = (char)(0x80 | ((unicode >> 6) & 0x3F));
        utf8[3] = (char)(0x80 | (unicode & 0x3F));
        utf8[4] = '\0';
    }
}

// 바이그램 추가: prev_char 다음에 next_char가 나왔음
void add_bigram(uint32_t prev_char, uint32_t next_char) {
    // 한글 범위 체크
    if (prev_char < HANGUL_START || prev_char > HANGUL_END) return;
    if (next_char < HANGUL_START || next_char > HANGUL_END) return;

    int idx = prev_char - HANGUL_START;
    BigramEntry *entry = &bigram_table[idx];

    // 이미 next_char가 리스트에 있는지 확인
    NextChar *curr = entry->head;
    while (curr != NULL) {
        if (curr->unicode == next_char) {
            // 이미 있으면 카운트만 증가
            curr->count++;
            entry->total_count++;
            return;
        }
        curr = curr->next;
    }

    // 없으면 새로 추가
    NextChar *new_node = (NextChar *)malloc(sizeof(NextChar));
    new_node->unicode = next_char;
    new_node->count = 1;
    new_node->next = entry->head;
    entry->head = new_node;
    entry->total_count++;
}

// 1단계: 바이그램 학습
void train_bigram(const char *filename) {
    FILE *input = fopen(filename, "r");
    if (input == NULL) {
        fprintf(stderr, "파일을 열 수 없습니다: %s\n", filename);
        exit(1);
    }

    printf("============================================================\n");
    printf("1단계: 바이그램 학습\n");
    printf("============================================================\n");

    char line[MAX_LINE];
    int word_count = 0;
    long bigram_count = 0;

    while (fgets(line, sizeof(line), input)) {
        word_count++;

        if (word_count % 100000 == 0) {
            printf("  진행: %d 단어...\n", word_count);
        }

        // 쉼표 이전 부분만 추출 (단어 부분)
        char *comma = strchr(line, ',');
        if (comma) *comma = '\0';

        // 단어의 모든 글자를 배열에 저장
        uint32_t chars[MAX_WORD_LEN];
        int char_len = 0;

        unsigned char *p = (unsigned char *)line;
        while (*p && char_len < MAX_WORD_LEN) {
            int bytes_read;
            uint32_t unicode = utf8_to_unicode(p, &bytes_read);

            if (unicode >= HANGUL_START && unicode <= HANGUL_END) {
                chars[char_len++] = unicode;
            }

            p += bytes_read;
        }

        // 연속된 두 글자로 바이그램 생성
        for (int i = 0; i < char_len - 1; i++) {
            add_bigram(chars[i], chars[i + 1]);
            bigram_count++;
        }
    }

    fclose(input);

    printf("\n[학습 완료]\n");
    printf("  총 단어: %d개\n", word_count);
    printf("  총 바이그램: %ld개\n", bigram_count);
}

// 2단계: 다음 글자 샘플링 (확률적 선택)
uint32_t sample_next_char(uint32_t prev_char) {
    if (prev_char < HANGUL_START || prev_char > HANGUL_END) {
        return 0;
    }

    int idx = prev_char - HANGUL_START;
    BigramEntry *entry = &bigram_table[idx];

    if (entry->total_count == 0) {
        return 0;  // 다음 글자 정보 없음
    }

    // 0 ~ (total_count - 1) 범위의 랜덤 숫자
    int rand_val = rand() % entry->total_count;

    // 누적 확률로 선택
    int cumulative = 0;
    NextChar *curr = entry->head;
    while (curr != NULL) {
        cumulative += curr->count;
        if (rand_val < cumulative) {
            return curr->unicode;
        }
        curr = curr->next;
    }

    return 0;
}

// 랜덤 시작 글자 선택
uint32_t random_start_char() {
    // 바이그램 데이터가 있는 글자 중에서 랜덤 선택
    int attempts = 0;
    while (attempts < 100) {
        int rand_idx = rand() % HANGUL_COUNT;
        if (bigram_table[rand_idx].total_count > 0) {
            return HANGUL_START + rand_idx;
        }
        attempts++;
    }
    return HANGUL_START;  // 실패시 '가'
}

// 3단계: 텍스트 생성
void generate_text(int num_chars, const char *output_file) {
    printf("\n============================================================\n");
    printf("2단계: 텍스트 생성\n");
    printf("============================================================\n");

    FILE *output = fopen(output_file, "w");
    if (output == NULL) {
        fprintf(stderr, "출력 파일 생성 실패: %s\n", output_file);
        return;
    }

    fprintf(output, "바이그램 모델 생성 결과 (%d글자)\n\n", num_chars);

    // 시작 글자
    uint32_t current_char = random_start_char();
    char utf8_char[5];

    printf("\n생성된 텍스트:\n");
    printf("----------------------------------------\n");

    for (int i = 0; i < num_chars; i++) {
        // 현재 글자 출력
        unicode_to_utf8(current_char, utf8_char);
        printf("%s", utf8_char);
        fprintf(output, "%s", utf8_char);

        // 다음 글자 샘플링
        uint32_t next_char = sample_next_char(current_char);

        if (next_char == 0) {
            // 다음 글자가 없으면 랜덤 재시작
            next_char = random_start_char();
            printf(" ");
            fprintf(output, " ");
        }

        current_char = next_char;
    }

    printf("\n----------------------------------------\n");
    fprintf(output, "\n");
    fclose(output);

    printf("\n결과 저장: %s\n", output_file);
}

// 메모리 해제
void cleanup() {
    for (int i = 0; i < HANGUL_COUNT; i++) {
        NextChar *curr = bigram_table[i].head;
        while (curr != NULL) {
            NextChar *temp = curr;
            curr = curr->next;
            free(temp);
        }
    }
    free(bigram_table);
}

int main() {
    // 랜덤 시드
    srand(time(NULL));

    // 바이그램 테이블 초기화
    bigram_table = (BigramEntry *)calloc(HANGUL_COUNT, sizeof(BigramEntry));
    if (bigram_table == NULL) {
        fprintf(stderr, "메모리 할당 실패\n");
        return 1;
    }

    // 1단계: 학습
    train_bigram("kr_korean_cleaned.csv");

    // 2단계: 생성 (300글자)
    generate_text(300, "generated_text.txt");

    // 메모리 해제
    cleanup();

    printf("\n✅ 완료!\n");

    return 0;
}
