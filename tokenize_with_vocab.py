#!/usr/bin/env python3
"""
LLM 스타일 토큰화: Special Token + Vocabulary ID
- <BOS>, <EOS> 추가
- Vocabulary 생성
- 토큰 → ID 변환
"""

from kiwipiepy import Kiwi
from collections import Counter
import json

# Special tokens
SPECIAL_TOKENS = {
    '<PAD>': 0,
    '<BOS>': 1,
    '<EOS>': 2,
    '<UNK>': 3,
}

def build_vocab(tokenized_sentences, min_freq=1):
    """토큰 빈도수 계산 및 vocabulary 생성"""
    token_counter = Counter()

    for sentence in tokenized_sentences:
        tokens = sentence.split()
        token_counter.update(tokens)

    # Special tokens + 빈도순 정렬
    vocab = SPECIAL_TOKENS.copy()
    idx = len(SPECIAL_TOKENS)

    for token, freq in token_counter.most_common():
        if freq >= min_freq:
            vocab[token] = idx
            idx += 1

    return vocab, token_counter

def tokenize_with_special_tokens(input_file, output_prefix):
    print("Kiwi 형태소 분석기 로딩 중...")
    kiwi = Kiwi()
    print("로딩 완료!\n")

    # 파일 읽기 및 토큰화
    with open(input_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    tokenized_sentences = []

    print("토큰화 중...")
    for line in lines:
        line = line.strip()
        if not line:
            continue

        # 제목/작가 필터링
        has_punctuation = any(c in line for c in '.!?,')
        if not has_punctuation and len(line) < 30:
            continue

        # 형태소 분석
        result = kiwi.tokenize(line)
        tokens = [f"{token.form}/{token.tag}" for token in result]

        if tokens:
            tokenized_sentences.append(' '.join(tokens))

    # Vocabulary 생성
    print("Vocabulary 생성 중...")
    vocab, token_counter = build_vocab(tokenized_sentences)

    # 1. 토큰 파일 (Special Token 포함)
    with open(f"{output_prefix}_tokens.txt", 'w', encoding='utf-8') as f:
        for sentence in tokenized_sentences:
            f.write(f"<BOS> {sentence} <EOS>\n")

    # 2. ID 파일
    with open(f"{output_prefix}_ids.txt", 'w', encoding='utf-8') as f:
        for sentence in tokenized_sentences:
            tokens = ['<BOS>'] + sentence.split() + ['<EOS>']
            ids = [vocab.get(token, SPECIAL_TOKENS['<UNK>']) for token in tokens]
            f.write(' '.join(map(str, ids)) + '\n')

    # 3. Vocabulary 파일 (JSON)
    with open(f"{output_prefix}_vocab.json", 'w', encoding='utf-8') as f:
        json.dump(vocab, f, ensure_ascii=False, indent=2)

    # 4. Vocabulary 파일 (TXT - 읽기 쉬운 형식)
    with open(f"{output_prefix}_vocab.txt", 'w', encoding='utf-8') as f:
        # ID 순으로 정렬
        sorted_vocab = sorted(vocab.items(), key=lambda x: x[1])
        for token, idx in sorted_vocab:
            freq = token_counter.get(token, 0)
            f.write(f"{idx}\t{token}\t{freq}\n")

    # 통계 출력
    print("\n" + "="*70)
    print("토큰화 완료!")
    print("="*70)
    print(f"📊 통계:")
    print(f"  문장 수: {len(tokenized_sentences)}")
    print(f"  총 토큰 수: {sum(len(s.split()) for s in tokenized_sentences)}")
    print(f"  Vocabulary 크기: {len(vocab)}")
    print(f"    - Special tokens: {len(SPECIAL_TOKENS)}")
    print(f"    - 일반 tokens: {len(vocab) - len(SPECIAL_TOKENS)}")
    print()

    print("📝 생성된 파일:")
    print(f"  1. {output_prefix}_tokens.txt  - <BOS>, <EOS> 포함 토큰")
    print(f"  2. {output_prefix}_ids.txt     - ID 시퀀스")
    print(f"  3. {output_prefix}_vocab.json  - Vocabulary (JSON)")
    print(f"  4. {output_prefix}_vocab.txt   - Vocabulary (읽기용)")
    print()

    # Top 20 frequent tokens
    print("🔥 가장 빈번한 토큰 Top 20:")
    print("-"*70)
    print(f"{'순위':<6} {'토큰':<20} {'ID':<8} {'빈도':<8}")
    print("-"*70)
    for i, (token, freq) in enumerate(token_counter.most_common(20), 1):
        token_id = vocab[token]
        print(f"{i:<6} {token:<20} {token_id:<8} {freq:<8}")
    print()

    # 샘플 문장
    print("📖 샘플 문장 (처음 3개):")
    print("-"*70)
    for i, sentence in enumerate(tokenized_sentences[:3], 1):
        tokens_with_special = ['<BOS>'] + sentence.split() + ['<EOS>']
        ids = [vocab.get(token, SPECIAL_TOKENS['<UNK>']) for token in tokens_with_special]

        print(f"\n{i}. 토큰: <BOS> {sentence[:80]}{'...' if len(sentence) > 80 else ''} <EOS>")
        print(f"   ID: {ids[:15]}{'...' if len(ids) > 15 else ''}")
    print()

    return vocab, tokenized_sentences

if __name__ == "__main__":
    input_file = "korean_novel_sample.txt"
    output_prefix = "tokenized"

    vocab, sentences = tokenize_with_special_tokens(input_file, output_prefix)

    print("✅ 완료!")
    print("\n💡 다음 단계:")
    print("  - tokenized_vocab.txt 확인 (어휘 사전)")
    print("  - tokenized_tokens.txt 확인 (토큰 시퀀스)")
    print("  - tokenized_ids.txt 확인 (ID 시퀀스)")
    print("  - C/Python으로 word-level N-gram 구현")
