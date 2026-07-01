#include <stdio.h>

int main() {
    FILE *fp = fopen("output.txt", "w");
    if (fp == NULL) {
        printf("파일을 열 수 없습니다.\n");
        return 1;
    }

    fprintf(fp, "Hello, World!\n");
    fclose(fp);

    printf("output.txt 파일에 저장되었습니다.\n");
    return 0;
}
