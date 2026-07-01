#!/usr/bin/env python3
# -*- coding: utf-8 -*-

def clean_csv(input_file, output_file):
    """CSV 파일에서 깨지는 글자 제거"""

    cleaned_lines = []
    removed_count = 0

    with open(input_file, 'r', encoding='utf-8-sig') as f:  # BOM 자동 제거
        for line_num, line in enumerate(f, 1):
            # 줄바꿈 제거
            line = line.strip()

            if not line:
                continue

            # 유효한 문자만 확인
            try:
                # UTF-8로 인코딩/디코딩 테스트
                line.encode('utf-8').decode('utf-8')

                # 쉼표가 있는지 확인 (CSV 형식)
                if ',' in line:
                    word, pos = line.split(',', 1)

                    # 단어 부분이 유효한 한글/영문/숫자/기호인지 확인
                    cleaned_word = ''
                    has_invalid = False
                    for char in word:
                        char_code = ord(char)
                        # 한글(가-힣), 영문, 숫자, 하이픈, 기본 기호만 허용
                        if ('\uac00' <= char <= '\ud7a3' or  # 한글 (가-힣)
                            '\u3131' <= char <= '\u318e' or  # 한글 자모
                            'a' <= char <= 'z' or
                            'A' <= char <= 'Z' or
                            '0' <= char <= '9' or
                            char in '-·()[]{}'):
                            cleaned_word += char
                        else:
                            # 깨진 문자 발견
                            has_invalid = True
                            if removed_count < 50:  # 처음 50개만 출력
                                print(f"깨진 문자 발견 (줄 {line_num}): '{char}' (U+{char_code:04X}) in '{word}'")
                            removed_count += 1

                    # 깨진 문자가 없고 유효한 글자가 있으면 추가
                    if cleaned_word and not has_invalid:
                        cleaned_lines.append(f"{cleaned_word},{pos}")
                    elif not cleaned_word:
                        if removed_count < 50:
                            print(f"제거됨 (줄 {line_num}): {line}")

            except Exception as e:
                removed_count += 1
                print(f"오류 (줄 {line_num}): {line} - {e}")

    # 정제된 데이터 저장
    with open(output_file, 'w', encoding='utf-8') as f:
        for line in cleaned_lines:
            f.write(line + '\n')

    print(f"\n정제 완료!")
    print(f"원본 줄 수: {line_num}")
    print(f"정제된 줄 수: {len(cleaned_lines)}")
    print(f"제거된 줄 수: {removed_count}")
    print(f"결과 파일: {output_file}")

if __name__ == "__main__":
    clean_csv("kr_korean.csv", "kr_korean_cleaned.csv")
