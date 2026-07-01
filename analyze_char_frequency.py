#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
한국어 글자 빈도 분석 프로그램

단계:
1. 데이터 읽기: CSV 파일에서 단어 추출
2. 글자 카운팅: 각 글자의 출현 빈도 계산
3. 자모 분석: 초성/중성/종성 분해 및 빈도 계산
4. 정렬: 빈도순 정렬
5. 결과 저장: 텍스트 파일로 출력
"""

from collections import Counter
import sys

def decompose_hangul(char):
    """
    한글 글자를 초성/중성/종성으로 분해

    한글 유니코드 구조:
    - 가 = 0xAC00 (44032)
    - 힣 = 0xD7A3 (55203)
    - 총 11,172자

    계산 공식:
    초성 = (code - 0xAC00) // 588
    중성 = ((code - 0xAC00) % 588) // 28
    종성 = (code - 0xAC00) % 28
    """

    # 초성 19개
    CHOSUNG = ['ㄱ', 'ㄲ', 'ㄴ', 'ㄷ', 'ㄸ', 'ㄹ', 'ㅁ', 'ㅂ', 'ㅃ',
               'ㅅ', 'ㅆ', 'ㅇ', 'ㅈ', 'ㅉ', 'ㅊ', 'ㅋ', 'ㅌ', 'ㅍ', 'ㅎ']

    # 중성 21개
    JUNGSUNG = ['ㅏ', 'ㅐ', 'ㅑ', 'ㅒ', 'ㅓ', 'ㅔ', 'ㅕ', 'ㅖ', 'ㅗ', 'ㅘ',
                'ㅙ', 'ㅚ', 'ㅛ', 'ㅜ', 'ㅝ', 'ㅞ', 'ㅟ', 'ㅠ', 'ㅡ', 'ㅢ', 'ㅣ']

    # 종성 28개 (첫번째는 받침 없음)
    JONGSUNG = ['', 'ㄱ', 'ㄲ', 'ㄳ', 'ㄴ', 'ㄵ', 'ㄶ', 'ㄷ', 'ㄹ', 'ㄺ',
                'ㄻ', 'ㄼ', 'ㄽ', 'ㄾ', 'ㄿ', 'ㅀ', 'ㅁ', 'ㅂ', 'ㅄ', 'ㅅ',
                'ㅆ', 'ㅇ', 'ㅈ', 'ㅊ', 'ㅋ', 'ㅌ', 'ㅍ', 'ㅎ']

    if not ('가' <= char <= '힣'):
        return None, None, None

    code = ord(char) - 0xAC00
    cho_idx = code // 588
    jung_idx = (code % 588) // 28
    jong_idx = code % 28

    return CHOSUNG[cho_idx], JUNGSUNG[jung_idx], JONGSUNG[jong_idx]


def analyze_character_frequency(input_file):
    """
    단계 1-3: 데이터 읽기, 글자 카운팅, 자모 분석
    """
    print("=" * 60)
    print("한국어 글자 빈도 분석 시작")
    print("=" * 60)

    # 카운터 초기화
    char_counter = Counter()      # 전체 글자
    chosung_counter = Counter()   # 초성
    jungsung_counter = Counter()  # 중성
    jongsung_counter = Counter()  # 종성

    total_words = 0
    total_chars = 0

    print("\n[단계 1] 데이터 읽기 중...")

    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                if line_num % 100000 == 0:
                    print(f"  처리 중: {line_num:,}줄...")

                line = line.strip()
                if not line or ',' not in line:
                    continue

                # 쉼표 앞의 단어만 추출
                word = line.split(',')[0]
                total_words += 1

                # 단계 2: 각 글자 카운팅
                for char in word:
                    if '가' <= char <= '힣':  # 한글만
                        char_counter[char] += 1
                        total_chars += 1

                        # 단계 3: 자모 분해
                        cho, jung, jong = decompose_hangul(char)
                        if cho:
                            chosung_counter[cho] += 1
                        if jung:
                            jungsung_counter[jung] += 1
                        if jong:  # 빈 문자열이 아닌 경우만
                            jongsung_counter[jong] += 1

    except FileNotFoundError:
        print(f"오류: '{input_file}' 파일을 찾을 수 없습니다.")
        sys.exit(1)

    print(f"\n[단계 1 완료]")
    print(f"  총 단어 수: {total_words:,}개")
    print(f"  총 글자 수: {total_chars:,}개")
    print(f"  고유 글자 수: {len(char_counter):,}개")

    return {
        'char': char_counter,
        'chosung': chosung_counter,
        'jungsung': jungsung_counter,
        'jongsung': jongsung_counter,
        'total_words': total_words,
        'total_chars': total_chars
    }


def save_results(results, output_file):
    """
    단계 4-5: 정렬 및 결과 저장
    """
    print("\n[단계 4-5] 결과 정렬 및 저장 중...")

    with open(output_file, 'w', encoding='utf-8') as f:
        # 헤더
        f.write("=" * 80 + "\n")
        f.write("한국어 글자 빈도 분석 결과\n")
        f.write("=" * 80 + "\n\n")

        # 통계 정보
        f.write("## 통계 정보\n")
        f.write(f"총 단어 수: {results['total_words']:,}개\n")
        f.write(f"총 글자 수: {results['total_chars']:,}개\n")
        f.write(f"고유 글자 수: {len(results['char']):,}개\n")
        f.write("\n" + "=" * 80 + "\n\n")

        # 1. 전체 글자 빈도 (상위 100개)
        f.write("## 1. 글자 빈도 분석 (상위 100개)\n\n")
        f.write(f"{'순위':<6} {'글자':<8} {'빈도':<12} {'비율(%)':<10} {'누적(%)':<10}\n")
        f.write("-" * 60 + "\n")

        cumulative = 0
        for rank, (char, count) in enumerate(results['char'].most_common(100), 1):
            percentage = count / results['total_chars'] * 100
            cumulative += percentage
            f.write(f"{rank:<6} {char:<8} {count:<12,} {percentage:<10.4f} {cumulative:<10.2f}\n")

        f.write("\n" + "=" * 80 + "\n\n")

        # 2. 초성 빈도
        f.write("## 2. 초성 빈도 분석\n\n")
        f.write(f"{'순위':<6} {'초성':<8} {'빈도':<12} {'비율(%)':<10}\n")
        f.write("-" * 50 + "\n")

        total_cho = sum(results['chosung'].values())
        for rank, (cho, count) in enumerate(results['chosung'].most_common(), 1):
            percentage = count / total_cho * 100
            f.write(f"{rank:<6} {cho:<8} {count:<12,} {percentage:<10.4f}\n")

        f.write("\n" + "=" * 80 + "\n\n")

        # 3. 중성 빈도
        f.write("## 3. 중성 빈도 분석\n\n")
        f.write(f"{'순위':<6} {'중성':<8} {'빈도':<12} {'비율(%)':<10}\n")
        f.write("-" * 50 + "\n")

        total_jung = sum(results['jungsung'].values())
        for rank, (jung, count) in enumerate(results['jungsung'].most_common(), 1):
            percentage = count / total_jung * 100
            f.write(f"{rank:<6} {jung:<8} {count:<12,} {percentage:<10.4f}\n")

        f.write("\n" + "=" * 80 + "\n\n")

        # 4. 종성 빈도
        f.write("## 4. 종성 빈도 분석\n\n")
        f.write(f"{'순위':<6} {'종성':<8} {'빈도':<12} {'비율(%)':<10}\n")
        f.write("-" * 50 + "\n")

        total_jong = sum(results['jongsung'].values())
        for rank, (jong, count) in enumerate(results['jongsung'].most_common(), 1):
            percentage = count / total_jong * 100
            f.write(f"{rank:<6} {jong:<8} {count:<12,} {percentage:<10.4f}\n")

        f.write("\n" + "=" * 80 + "\n")

    print(f"  결과 파일 저장 완료: {output_file}")


def print_summary(results):
    """
    콘솔에 요약 정보 출력
    """
    print("\n" + "=" * 60)
    print("분석 완료!")
    print("=" * 60)

    print(f"\n총 단어 수: {results['total_words']:,}개")
    print(f"총 글자 수: {results['total_chars']:,}개")
    print(f"고유 글자 수: {len(results['char']):,}개")

    print("\n📊 상위 20개 글자:")
    print(f"{'순위':<6} {'글자':<8} {'빈도':<12} {'비율(%)':<10}")
    print("-" * 50)

    for rank, (char, count) in enumerate(results['char'].most_common(20), 1):
        percentage = count / results['total_chars'] * 100
        print(f"{rank:<6} {char:<8} {count:<12,} {percentage:<10.4f}")

    print("\n📊 초성 빈도 (상위 10개):")
    for rank, (cho, count) in enumerate(results['chosung'].most_common(10), 1):
        total = sum(results['chosung'].values())
        percentage = count / total * 100
        print(f"{rank}. {cho}: {count:,}회 ({percentage:.2f}%)")

    print("\n📊 중성 빈도 (상위 10개):")
    for rank, (jung, count) in enumerate(results['jungsung'].most_common(10), 1):
        total = sum(results['jungsung'].values())
        percentage = count / total * 100
        print(f"{rank}. {jung}: {count:,}회 ({percentage:.2f}%)")


def main():
    input_file = "kr_korean_cleaned.csv"
    output_file = "char_frequency_result.txt"

    # 분석 실행
    results = analyze_character_frequency(input_file)

    # 결과 저장
    save_results(results, output_file)

    # 요약 출력
    print_summary(results)

    print(f"\n✅ 상세 결과는 '{output_file}' 파일을 확인하세요.")


if __name__ == "__main__":
    main()
