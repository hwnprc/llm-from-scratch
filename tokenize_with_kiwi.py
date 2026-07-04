#!/usr/bin/env python3
"""
Kiwi 형태소 분석기를 사용한 토큰화
- 입력: korean_novel_sample.txt (원본 소설)
- 출력: tokenized_sentences.txt (토큰 공백 구분)
"""

from kiwipiepy import Kiwi
from tqdm import tqdm

def tokenize_file(input_file, output_file):
    # Kiwi 초기화
    print("Kiwi 형태소 분석기 로딩 중...")
    kiwi = Kiwi()
    print("로딩 완료!\n")

    # 파일 읽기
    with open(input_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    tokenized_sentences = []
    stats = {
        'total_lines': 0,
        'sentence_lines': 0,
        'total_tokens': 0,
        'unique_tokens': set()
    }

    print(f"총 {len(lines)}줄 처리 중...\n")

    for line in tqdm(lines, desc="토큰화"):
        line = line.strip()

        # 빈 줄 스킵
        if not line:
            continue

        stats['total_lines'] += 1

        # 제목/작가 줄 필터링 (문장 부호 없고 짧은 경우)
        has_punctuation = any(c in line for c in '.!?,')
        if not has_punctuation and len(line) < 30:
            continue

        # 형태소 분석
        result = kiwi.tokenize(line)

        # 토큰 추출 (형태소+품사)
        tokens = []
        for token in result:
            # 형태소/품사 형식
            token_str = f"{token.form}/{token.tag}"
            tokens.append(token_str)
            stats['unique_tokens'].add(token_str)

        if len(tokens) > 0:
            tokenized_sentences.append(' '.join(tokens))
            stats['sentence_lines'] += 1
            stats['total_tokens'] += len(tokens)

    # 결과 저장
    with open(output_file, 'w', encoding='utf-8') as f:
        for sentence in tokenized_sentences:
            f.write(sentence + '\n')

    # 통계 출력
    print("\n" + "="*60)
    print("토큰화 완료!")
    print("="*60)
    print(f"입력 파일: {input_file}")
    print(f"출력 파일: {output_file}")
    print(f"\n📊 통계:")
    print(f"  전체 줄 수: {stats['total_lines']}")
    print(f"  문장 줄 수: {stats['sentence_lines']}")
    print(f"  총 토큰 수: {stats['total_tokens']}")
    print(f"  고유 토큰 수: {len(stats['unique_tokens'])}")
    print(f"  평균 토큰/문장: {stats['total_tokens']/stats['sentence_lines']:.1f}")
    print()

    # 샘플 출력
    print("📝 샘플 (처음 5문장):")
    print("-"*60)
    for i, sentence in enumerate(tokenized_sentences[:5], 1):
        original = lines[i*2] if i*2 < len(lines) else ""
        print(f"{i}. 원문: {original.strip()}")
        # 토큰이 너무 길면 일부만 출력
        if len(sentence) > 100:
            print(f"   토큰: {sentence[:100]}...")
        else:
            print(f"   토큰: {sentence}")
        print()

    return stats

if __name__ == "__main__":
    input_file = "korean_novel_sample.txt"
    output_file = "tokenized_sentences.txt"

    tokenize_file(input_file, output_file)

    print("✅ 완료! 다음 단계:")
    print("  - tokenized_sentences.txt 확인")
    print("  - C 프로그램으로 word-level N-gram 학습")
