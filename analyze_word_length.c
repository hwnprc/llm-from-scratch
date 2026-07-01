#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HANGUL_START 0xAC00
#define HANGUL_END   0xD7A3
#define MAX_LINE 1024

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

int main() {
    FILE *f = fopen("words_30k.txt", "r");
    if (!f) {
        fprintf(stderr, "파일 열기 실패\n");
        return 1;
    }

    char line[MAX_LINE];
    int length_dist[50] = {0};  // 길이별 분포
    long total_chars = 0;
    int word_count = 0;
    int max_length = 0;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;

        int len = 0;
        unsigned char *p = (unsigned char *)line;
        while (*p) {
            int bytes;
            uint32_t u = utf8_to_unicode(p, &bytes);
            if (u >= HANGUL_START && u <= HANGUL_END) len++;
            p += bytes;
        }

        if (len > 0 && len < 50) {
            length_dist[len]++;
            total_chars += len;
            word_count++;
            if (len > max_length) max_length = len;
        }
    }
    fclose(f);

    printf("============================================================\n");
    printf("단어 길이 분석 (3만 개 단어)\n");
    printf("============================================================\n\n");

    printf("총 단어 수: %d개\n", word_count);
    printf("평균 길이: %.2f 글자\n", (double)total_chars / word_count);
    printf("최대 길이: %d 글자\n\n", max_length);

    printf("길이별 분포:\n");
    printf("길이   단어수    비율     누적비율\n");
    printf("--------------------------------------------\n");

    int cumulative = 0;
    for (int i = 1; i <= max_length && i < 50; i++) {
        if (length_dist[i] > 0) {
            cumulative += length_dist[i];
            double pct = length_dist[i] * 100.0 / word_count;
            double cum_pct = cumulative * 100.0 / word_count;
            printf("%2d글자  %5d개   %5.1f%%    %5.1f%%\n",
                   i, length_dist[i], pct, cum_pct);
        }
    }

    return 0;
}
