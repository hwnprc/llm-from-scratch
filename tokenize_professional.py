#!/usr/bin/env python3
"""
전문가 스타일 토큰화
- <W> 구분자: 토큰 사이 구분 (실제 데이터와 겹치지 않음)
- <S> 공백: 원본 텍스트의 공백 표시
- <BOS>, <EOS>: 문장 경계
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
    '<S>': 4,  # Space (공백)
}

def build_vocab(tokenized_sentences, min_freq=1):
    """토큰 빈도수 계산 및 vocabulary 생성"""
    token_counter = Counter()

    for sentence in tokenized_sentences:
        tokens = sentence  # 이미 리스트
        token_counter.update(tokens)

    # Special tokens + 빈도순 정렬
    vocab = SPECIAL_TOKENS.copy()
    idx = len(SPECIAL_TOKENS)

    for token, freq in token_counter.most_common():
        if freq >= min_freq and token not in vocab:
            vocab[token] = idx
            idx += 1

    return vocab, token_counter

def tokenize_with_professional_format(input_file, output_prefix):
    print("Kiwi 형태소 분석기 로딩 중...")
    kiwi = Kiwi()
    print("로딩 완료!\n")

    # 파일 읽기
    with open(input_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    tokenized_sentences = []  # 리스트의 리스트

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

        # 토큰 리스트로 저장 (공백 포함)
        tokens = []
        prev_end = 0

        for token in result:
            # 토큰 사이 공백 체크
            if token.start > prev_end:
                # 공백이 있음
                tokens.append('<S>')

            # 형태소/품사
            tokens.append(f"{token.form}/{token.tag}")
            prev_end = token.end

        if tokens:
            tokenized_sentences.append(tokens)

    # Vocabulary 생성
    print("Vocabulary 생성 중...")
    vocab, token_counter = build_vocab(tokenized_sentences)

    # 역방향 vocabulary (ID -> 토큰)
    id_to_token = {v: k for k, v in vocab.items()}

    # 1. 토큰 파일 (<W> 구분자 사용)
    with open(f"{output_prefix}_tokens.txt", 'w', encoding='utf-8') as f:
        for sentence in tokenized_sentences:
            # <BOS><W>토큰1<W>토큰2<W>...<W><EOS>
            token_str = '<BOS><W>' + '<W>'.join(sentence) + '<W><EOS>'
            f.write(token_str + '\n')

    # 2. ID 파일 (<W> 구분자 사용)
    with open(f"{output_prefix}_ids.txt", 'w', encoding='utf-8') as f:
        for sentence in tokenized_sentences:
            tokens_with_special = ['<BOS>'] + sentence + ['<EOS>']
            ids = [vocab.get(token, SPECIAL_TOKENS['<UNK>']) for token in tokens_with_special]
            # ID<W>ID<W>...
            id_str = '<W>'.join(map(str, ids))
            f.write(id_str + '\n')

    # 3. Vocabulary 파일 (JSON)
    with open(f"{output_prefix}_vocab.json", 'w', encoding='utf-8') as f:
        json.dump(vocab, f, ensure_ascii=False, indent=2)

    # 4. Vocabulary 파일 (TXT - 읽기 쉬운 형식)
    with open(f"{output_prefix}_vocab.txt", 'w', encoding='utf-8') as f:
        f.write("ID\t토큰\t빈도\n")
        f.write("-"*60 + "\n")
        # ID 순으로 정렬
        sorted_vocab = sorted(vocab.items(), key=lambda x: x[1])
        for token, idx in sorted_vocab:
            freq = token_counter.get(token, 0) if token not in SPECIAL_TOKENS else '-'
            f.write(f"{idx}\t{token}\t{freq}\n")

    # 통계 계산
    total_tokens = sum(len(s) for s in tokenized_sentences)
    space_count = sum(1 for s in tokenized_sentences for t in s if t == '<S>')

    # 통계 출력
    print("\n" + "="*70)
    print("토큰화 완료!")
    print("="*70)
    print(f"📊 통계:")
    print(f"  문장 수: {len(tokenized_sentences)}")
    print(f"  총 토큰 수: {total_tokens} (공백 {space_count}개 포함)")
    print(f"  Vocabulary 크기: {len(vocab)}")
    print(f"    - Special tokens: {len(SPECIAL_TOKENS)}")
    print(f"    - 일반 tokens: {len(vocab) - len(SPECIAL_TOKENS)}")
    print()

    print("📝 생성된 파일:")
    print(f"  1. {output_prefix}_tokens.txt  - <W> 구분자 사용")
    print(f"  2. {output_prefix}_ids.txt     - <W> 구분자 ID 시퀀스")
    print(f"  3. {output_prefix}_vocab.json  - Vocabulary (JSON)")
    print(f"  4. {output_prefix}_vocab.txt   - Vocabulary (읽기용)")
    print()

    # Top 20 frequent tokens
    print("🔥 가장 빈번한 토큰 Top 20:")
    print("-"*70)
    print(f"{'순위':<6} {'토큰':<25} {'ID':<8} {'빈도':<8}")
    print("-"*70)
    non_special = [(t, f) for t, f in token_counter.most_common() if t not in SPECIAL_TOKENS]
    for i, (token, freq) in enumerate(non_special[:20], 1):
        token_id = vocab[token]
        print(f"{i:<6} {token:<25} {token_id:<8} {freq:<8}")
    print()

    # 샘플 문장
    print("📖 샘플 문장 (처음 3개):")
    print("-"*70)
    for i, sentence in enumerate(tokenized_sentences[:3], 1):
        # 토큰 형식
        token_preview = '<W>'.join(sentence[:10])
        if len(sentence) > 10:
            token_preview += '<W>...'

        # ID 형식
        tokens_with_special = ['<BOS>'] + sentence + ['<EOS>']
        ids = [vocab.get(token, SPECIAL_TOKENS['<UNK>']) for token in tokens_with_special]
        id_preview = '<W>'.join(map(str, ids[:10]))
        if len(ids) > 10:
            id_preview += '<W>...'

        print(f"\n{i}. 토큰: <BOS><W>{token_preview}<W><EOS>")
        print(f"   ID:    {id_preview}")
    print()

    print("💡 포맷 설명:")
    print("  - <W>: 토큰 구분자 (실제 데이터와 절대 겹치지 않음)")
    print("  - <S>: 공백 토큰 (원문의 띄어쓰기)")
    print("  - <BOS>: 문장 시작")
    print("  - <EOS>: 문장 끝")
    print()

    return vocab, tokenized_sentences

if __name__ == "__main__":
    input_file = "korean_novel_sample.txt"
    output_prefix = "tokenized"

    vocab, sentences = tokenize_with_professional_format(input_file, output_prefix)

    print("✅ 완료!")
    print("\n🚀 다음 단계:")
    print("  - tokenized_tokens.txt 확인 (<W> 구분자 확인)")
    print("  - Word-level N-gram 모델 구현")
    print("  - 글자 단위 vs 단어 단위 성능 비교")
