#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/*
 * step02: <EOS> 토큰을 사용한 단어 생성기
 *
 * 핵심 아이디어:
 * - 각 단어는 시작과 끝이 있음
 * - <EOS> (End Of Sequence) 토큰으로 끝 표시
 * - 바이그램 학습 시 단어 끝에 <EOS> 추가
 * - 생성 시 <EOS>가 나오면 멈춤
 */

#define HANGUL_START 0xAC00
#define HANGUL_END   0xD7A3
#define HANGUL_COUNT (HANGUL_END - HANGUL_START + 1)
#define EOS_TOKEN 0xFFFF    // <EOS> 특수 토큰
#define MAX_LINE 1024
#define MAX_WORD_LEN 256

// 다음 글자 후보 (연결 리스트)
typedef struct NextChar {
    uint32_t unicode;      // 다음 글자 (또는 <EOS>)
    int count;             // 빈도
    struct NextChar *next;
} NextChar;

// 각 글자별 바이그램 엔트리 (11,172 + 1개 for <EOS>)
typedef struct {
    NextChar *head;
    int total_count;
} BigramEntry;

BigramEntry *bigram_table;
int *start_char_count;     // 단어 시작 글자 빈도

// UTF-8 → Unicode
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
    } else {
        utf8[0] = (char)(0xF0 | (unicode >> 18));
        utf8[1] = (char)(0x80 | ((unicode >> 12) & 0x3F));
        utf8[2] = (char)(0x80 | ((unicode >> 6) & 0x3F));
        utf8[3] = (char)(0x80 | (unicode & 0x3F));
        utf8[4] = '\0';
    }
}

// 바이그램 추가 (next_char는 일반 글자 또는 EOS_TOKEN)
void add_bigram(uint32_t prev_char, uint32_t next_char) {
    // prev_char가 한글 범위인지 확인
    if (prev_char < HANGUL_START || prev_char > HANGUL_END) return;

    int idx = prev_char - HANGUL_START;
    BigramEntry *entry = &bigram_table[idx];

    // next_char가 리스트에 있는지 확인
    NextChar *curr = entry->head;
    while (curr != NULL) {
        if (curr->unicode == next_char) {
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

// 학습: 각 단어의 글자 바이그램 + 마지막에 <EOS>
void train_word_bigram(const char *filename) {
    FILE *input = fopen(filename, "r");
    if (input == NULL) {
        fprintf(stderr, "파일을 열 수 없습니다: %s\n", filename);
        exit(1);
    }

    printf("============================================================\n");
    printf("step02: 단어 생성기 학습 (with <EOS>)\n");
    printf("============================================================\n");

    char line[MAX_LINE];
    int word_count = 0;
    long bigram_count = 0;

    while (fgets(line, sizeof(line), input)) {
        word_count++;

        if (word_count % 100000 == 0) {
            printf("  진행: %d 단어...\n", word_count);
        }

        // 줄바꿈 제거
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;

        // 단어를 글자 배열로 변환
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

        if (char_len == 0) continue;

        // 첫 글자는 시작 글자로 카운트
        start_char_count[chars[0] - HANGUL_START]++;

        // 바이그램 생성: (글자[i], 글자[i+1])
        for (int i = 0; i < char_len - 1; i++) {
            add_bigram(chars[i], chars[i + 1]);
            bigram_count++;
        }

        // 마지막 글자 → <EOS>
        add_bigram(chars[char_len - 1], EOS_TOKEN);
        bigram_count++;
    }

    fclose(input);

    printf("\n[학습 완료]\n");
    printf("  총 단어: %d개\n", word_count);
    printf("  총 바이그램: %ld개 (EOS 포함)\n", bigram_count);
}

// 바이그램 통계를 파일로 저장
void save_bigram_stats(const char *filename) {
    printf("\n바이그램 통계 저장 중...\n");

    FILE *output = fopen(filename, "w");
    if (output == NULL) {
        fprintf(stderr, "통계 파일 생성 실패: %s\n", filename);
        return;
    }

    fprintf(output, "바이그램 학습 통계\n");
    fprintf(output, "형식: 앞글자 → 뒷글자 : 빈도\n");
    fprintf(output, "========================================\n\n");

    char prev_utf8[5], next_utf8[5];
    long total_bigrams = 0;
    int unique_bigrams = 0;

    // 모든 바이그램 순회
    for (int i = 0; i < HANGUL_COUNT; i++) {
        BigramEntry *entry = &bigram_table[i];
        if (entry->total_count == 0) continue;

        uint32_t prev_char = HANGUL_START + i;
        unicode_to_utf8(prev_char, prev_utf8);

        NextChar *curr = entry->head;
        while (curr != NULL) {
            if (curr->unicode == EOS_TOKEN) {
                fprintf(output, "%s → <EOS> : %d\n", prev_utf8, curr->count);
            } else {
                unicode_to_utf8(curr->unicode, next_utf8);
                fprintf(output, "%s → %s : %d\n", prev_utf8, next_utf8, curr->count);
            }

            total_bigrams += curr->count;
            unique_bigrams++;
            curr = curr->next;
        }
    }

    fprintf(output, "\n========================================\n");
    fprintf(output, "총 바이그램 종류: %d개\n", unique_bigrams);
    fprintf(output, "총 바이그램 빈도: %ld회\n", total_bigrams);

    fclose(output);
    printf("  저장 완료: %s\n", filename);
    printf("  고유 바이그램: %d개\n", unique_bigrams);
}

// 다음 글자 샘플링 (확률적)
uint32_t sample_next_char(uint32_t prev_char) {
    if (prev_char < HANGUL_START || prev_char > HANGUL_END) {
        return EOS_TOKEN;
    }

    int idx = prev_char - HANGUL_START;
    BigramEntry *entry = &bigram_table[idx];

    if (entry->total_count == 0) {
        return EOS_TOKEN;  // 데이터 없으면 종료
    }

    int rand_val = rand() % entry->total_count;

    int cumulative = 0;
    NextChar *curr = entry->head;
    while (curr != NULL) {
        cumulative += curr->count;
        if (rand_val < cumulative) {
            return curr->unicode;
        }
        curr = curr->next;
    }

    return EOS_TOKEN;
}

// 시작 글자 샘플링 (단어 첫 글자 빈도 기반)
uint32_t sample_start_char() {
    // 전체 시작 글자 빈도 합계
    int total = 0;
    for (int i = 0; i < HANGUL_COUNT; i++) {
        total += start_char_count[i];
    }

    if (total == 0) return HANGUL_START;

    int rand_val = rand() % total;
    int cumulative = 0;

    for (int i = 0; i < HANGUL_COUNT; i++) {
        cumulative += start_char_count[i];
        if (rand_val < cumulative) {
            return HANGUL_START + i;
        }
    }

    return HANGUL_START;
}

// 단어 하나 생성 (<EOS>까지)
void generate_word(char *buffer, int max_len) {
    buffer[0] = '\0';
    char temp[5];

    uint32_t current_char = sample_start_char();
    int len = 0;

    while (len < max_len) {
        // 현재 글자를 버퍼에 추가
        unicode_to_utf8(current_char, temp);
        strcat(buffer, temp);
        len++;

        // 다음 글자 샘플링
        uint32_t next_char = sample_next_char(current_char);

        if (next_char == EOS_TOKEN) {
            break;  // 단어 끝!
        }

        current_char = next_char;
    }
}

// 단어 빈도 카운터를 위한 해시 테이블 (간단한 버전)
typedef struct WordCount {
    char word[MAX_WORD_LEN * 4];
    int count;
    struct WordCount *next;
} WordCount;

#define HASH_SIZE 10007

WordCount *word_hash[HASH_SIZE];

// 간단한 해시 함수
unsigned int hash_word(const char *word) {
    unsigned int hash = 0;
    while (*word) {
        hash = hash * 31 + (unsigned char)(*word);
        word++;
    }
    return hash % HASH_SIZE;
}

// 단어 카운트 추가
void add_word_count(const char *word) {
    unsigned int hash = hash_word(word);
    WordCount *curr = word_hash[hash];

    // 이미 있는지 확인
    while (curr != NULL) {
        if (strcmp(curr->word, word) == 0) {
            curr->count++;
            return;
        }
        curr = curr->next;
    }

    // 없으면 새로 추가
    WordCount *new_node = (WordCount *)malloc(sizeof(WordCount));
    strncpy(new_node->word, word, sizeof(new_node->word) - 1);
    new_node->word[sizeof(new_node->word) - 1] = '\0';
    new_node->count = 1;
    new_node->next = word_hash[hash];
    word_hash[hash] = new_node;
}

// 빈도순 정렬을 위한 비교 함수
int compare_word_count(const void *a, const void *b) {
    WordCount *wa = *(WordCount **)a;
    WordCount *wb = *(WordCount **)b;
    return wb->count - wa->count;  // 내림차순
}

// 여러 단어 생성
void generate_words(int num_words, const char *output_file, const char *freq_file) {
    printf("\n============================================================\n");
    printf("단어 생성\n");
    printf("============================================================\n");

    // 해시 테이블 초기화
    memset(word_hash, 0, sizeof(word_hash));

    FILE *output = fopen(output_file, "w");
    if (output == NULL) {
        fprintf(stderr, "출력 파일 생성 실패: %s\n", output_file);
        return;
    }

    fprintf(output, "생성된 단어들 (%d개)\n\n", num_words);

    printf("\n생성된 단어:\n");
    printf("----------------------------------------\n");

    char word[MAX_WORD_LEN * 4];  // UTF-8 버퍼

    for (int i = 0; i < num_words; i++) {
        generate_word(word, 20);  // 최대 20글자

        printf("%3d. %s\n", i + 1, word);
        fprintf(output, "%s\n", word);

        // 빈도 카운트
        add_word_count(word);
    }

    printf("----------------------------------------\n");
    fclose(output);

    printf("\n결과 저장: %s\n", output_file);

    // 빈도 분석 결과 저장
    printf("빈도 분석 중...\n");

    // 해시 테이블에서 모든 단어 수집
    WordCount **word_array = (WordCount **)malloc(num_words * sizeof(WordCount *));
    int unique_count = 0;

    for (int i = 0; i < HASH_SIZE; i++) {
        WordCount *curr = word_hash[i];
        while (curr != NULL) {
            word_array[unique_count++] = curr;
            curr = curr->next;
        }
    }

    // 빈도순 정렬
    qsort(word_array, unique_count, sizeof(WordCount *), compare_word_count);

    // 빈도 파일 저장
    FILE *freq = fopen(freq_file, "w");
    if (freq != NULL) {
        fprintf(freq, "생성된 단어 빈도 분석\n");
        fprintf(freq, "총 생성: %d개\n", num_words);
        fprintf(freq, "고유 단어: %d개\n\n", unique_count);
        fprintf(freq, "순위    단어                빈도\n");
        fprintf(freq, "========================================\n");

        for (int i = 0; i < unique_count; i++) {
            fprintf(freq, "%-6d  %-18s  %d\n",
                    i + 1, word_array[i]->word, word_array[i]->count);
        }

        fclose(freq);
        printf("빈도 분석 저장: %s\n", freq_file);
        printf("  고유 단어 수: %d개\n", unique_count);
    }

    free(word_array);

    // 메모리 해제
    for (int i = 0; i < HASH_SIZE; i++) {
        WordCount *curr = word_hash[i];
        while (curr != NULL) {
            WordCount *temp = curr;
            curr = curr->next;
            free(temp);
        }
    }
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
    free(start_char_count);
}

int main() {
    srand(time(NULL));

    // 메모리 할당
    bigram_table = (BigramEntry *)calloc(HANGUL_COUNT, sizeof(BigramEntry));
    start_char_count = (int *)calloc(HANGUL_COUNT, sizeof(int));

    if (bigram_table == NULL || start_char_count == NULL) {
        fprintf(stderr, "메모리 할당 실패\n");
        return 1;
    }

    // 1. 학습
    train_word_bigram("words.txt");

    // 2. 바이그램 통계 저장
    save_bigram_stats("step02_bigram_stats.txt");

    // 3. 단어 생성 (100개 + 빈도 분석)
    generate_words(100, "step02_output.txt", "step02_frequency.txt");

    // 4. 메모리 해제
    cleanup();

    printf("\n✅ 완료!\n");

    return 0;
}
