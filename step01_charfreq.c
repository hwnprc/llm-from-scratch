#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
 * 완성형 한글 글자 빈도 분석 프로그램
 *
 * 한글 유니코드 범위: U+AC00 (가) ~ U+D7A3 (힣)
 * 총 11,172개의 완성형 한글 글자
 */

#define HANGUL_START 0xAC00    // '가'
#define HANGUL_END   0xD7A3    // '힣'
#define HANGUL_COUNT (HANGUL_END - HANGUL_START + 1)  // 11,172
#define MAX_LINE 1024

// 글자 빈도 구조체
typedef struct {
    uint32_t unicode;   // 유니코드 값
    int count;          // 출현 횟수
} CharFreq;

// UTF-8에서 유니코드 코드포인트 추출
uint32_t utf8_to_unicode(const unsigned char *utf8, int *bytes_read) {
    uint32_t codepoint = 0;

    if ((utf8[0] & 0x80) == 0) {  // 1바이트 (ASCII)
        codepoint = utf8[0];
        *bytes_read = 1;
    } else if ((utf8[0] & 0xE0) == 0xC0) {  // 2바이트
        codepoint = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
        *bytes_read = 2;
    } else if ((utf8[0] & 0xF0) == 0xE0) {  // 3바이트 (한글은 여기)
        codepoint = ((utf8[0] & 0x0F) << 12) |
                    ((utf8[1] & 0x3F) << 6) |
                    (utf8[2] & 0x3F);
        *bytes_read = 3;
    } else if ((utf8[0] & 0xF8) == 0xF0) {  // 4바이트
        codepoint = ((utf8[0] & 0x07) << 18) |
                    ((utf8[1] & 0x3F) << 12) |
                    ((utf8[2] & 0x3F) << 6) |
                    (utf8[3] & 0x3F);
        *bytes_read = 4;
    } else {
        *bytes_read = 1;  // 잘못된 바이트는 스킵
    }

    return codepoint;
}

// 유니코드를 UTF-8로 변환
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

// 빈도순 정렬을 위한 비교 함수
int compare_freq(const void *a, const void *b) {
    CharFreq *fa = (CharFreq *)a;
    CharFreq *fb = (CharFreq *)b;
    return fb->count - fa->count;  // 내림차순
}

int main() {
    FILE *input, *output;
    char line[MAX_LINE];
    int *char_count;
    int total_words = 0;
    long total_chars = 0;
    int unique_chars = 0;

    printf("============================================================\n");
    printf("완성형 한글 글자 빈도 분석\n");
    printf("============================================================\n");

    // 한글 글자 카운트 배열 할당 (11,172개)
    char_count = (int *)calloc(HANGUL_COUNT, sizeof(int));
    if (char_count == NULL) {
        fprintf(stderr, "메모리 할당 실패\n");
        return 1;
    }

    // 입력 파일 열기
    input = fopen("kr_korean_cleaned.csv", "r");
    if (input == NULL) {
        fprintf(stderr, "kr_korean_cleaned.csv 파일을 열 수 없습니다.\n");
        free(char_count);
        return 1;
    }

    printf("[단계 1] 데이터 읽기 및 글자 카운팅 중...\n");

    // 파일 읽기 및 글자 카운팅
    while (fgets(line, sizeof(line), input)) {
        total_words++;

        if (total_words % 100000 == 0) {
            printf("  처리 중: %d줄...\n", total_words);
        }

        // 쉼표 이전 부분만 추출 (단어)
        char *comma = strchr(line, ',');
        if (comma) *comma = '\0';

        // UTF-8 바이트를 유니코드로 변환하며 카운팅
        unsigned char *p = (unsigned char *)line;
        while (*p) {
            int bytes_read;
            uint32_t unicode = utf8_to_unicode(p, &bytes_read);

            // 한글 완성형인지 확인
            if (unicode >= HANGUL_START && unicode <= HANGUL_END) {
                int idx = unicode - HANGUL_START;
                char_count[idx]++;
                total_chars++;
            }

            p += bytes_read;
        }
    }
    fclose(input);

    printf("[단계 1 완료]\n");
    printf("  총 단어 수: %d개\n", total_words);
    printf("  총 글자 수: %ld개\n", total_chars);

    // 고유 글자 수 계산 및 배열 생성
    printf("\n[단계 2] 빈도순 정렬 중...\n");

    for (int i = 0; i < HANGUL_COUNT; i++) {
        if (char_count[i] > 0) {
            unique_chars++;
        }
    }

    CharFreq *freq_array = (CharFreq *)malloc(unique_chars * sizeof(CharFreq));
    if (freq_array == NULL) {
        fprintf(stderr, "메모리 할당 실패\n");
        free(char_count);
        return 1;
    }

    // 빈도 배열에 데이터 복사
    int idx = 0;
    for (int i = 0; i < HANGUL_COUNT; i++) {
        if (char_count[i] > 0) {
            freq_array[idx].unicode = HANGUL_START + i;
            freq_array[idx].count = char_count[i];
            idx++;
        }
    }

    // 빈도순 정렬
    qsort(freq_array, unique_chars, sizeof(CharFreq), compare_freq);

    printf("  고유 글자 수: %d개\n", unique_chars);

    // 결과 파일 저장
    printf("\n[단계 3] 결과 파일 저장 중...\n");

    output = fopen("char_frequency.txt", "w");
    if (output == NULL) {
        fprintf(stderr, "결과 파일을 생성할 수 없습니다.\n");
        free(char_count);
        free(freq_array);
        return 1;
    }

    fprintf(output, "================================================================================\n");
    fprintf(output, "완성형 한글 글자 빈도 분석 결과\n");
    fprintf(output, "================================================================================\n\n");
    fprintf(output, "총 단어 수: %d개\n", total_words);
    fprintf(output, "총 글자 수: %ld개\n", total_chars);
    fprintf(output, "고유 글자 수: %d개\n\n", unique_chars);
    fprintf(output, "================================================================================\n\n");
    fprintf(output, "순위    글자    빈도        비율(%%)    누적(%%)\n");
    fprintf(output, "----------------------------------------------------------------\n");

    double cumulative = 0.0;
    for (int i = 0; i < unique_chars; i++) {
        char utf8_char[5];
        unicode_to_utf8(freq_array[i].unicode, utf8_char);

        double percentage = (double)freq_array[i].count / total_chars * 100.0;
        cumulative += percentage;

        fprintf(output, "%-6d  %s      %-10d  %6.4f      %6.2f\n",
                i + 1, utf8_char, freq_array[i].count, percentage, cumulative);
    }

    fclose(output);

    // 콘솔에 상위 30개 출력
    printf("  결과 파일 저장 완료: char_frequency.txt\n");
    printf("\n============================================================\n");
    printf("분석 완료!\n");
    printf("============================================================\n");
    printf("\n📊 상위 30개 글자:\n\n");
    printf("순위    글자    빈도        비율(%%)\n");
    printf("----------------------------------------\n");

    for (int i = 0; i < 30 && i < unique_chars; i++) {
        char utf8_char[5];
        unicode_to_utf8(freq_array[i].unicode, utf8_char);

        double percentage = (double)freq_array[i].count / total_chars * 100.0;
        printf("%-6d  %s      %-10d  %6.4f\n",
               i + 1, utf8_char, freq_array[i].count, percentage);
    }

    printf("\n✅ 상세 결과는 'char_frequency.txt' 파일을 확인하세요.\n");

    // 메모리 해제
    free(char_count);
    free(freq_array);

    return 0;
}
