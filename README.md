# xunlzip

xunlzip 은 lzma 기반의 압축 포맷인 lzip 파일의 압축 해제 소스입니다.

이 소스는 교육용 목적으로 읽기 쉽게 만들어져서 lzip 컨테이너와, LZMA 알고리즘에 대해서 공부하는데 도움이 될 수 있습니다.

이 소스의 개발 과정에서 많은 부분을 lzd(https://www.nongnu.org/lzip/lzd.html) 를 참고하여 개발하였으며, 

개발 과정에서 다음 문서들을 참고하였습니다.

1. https://www.nongnu.org/lzip/manual/lzip_manual.html#Stream-format
2. https://www.7-zip.org/a/lzma-specification.7z
3. https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Markov_chain_algorithm



## lzip 포맷에 관하여 
lzip 포맷은 lzma 스트림의 앞과 뒤에 간단한 데이타가 붙어있는 포맷입니다.

다음과 같이 4바이브 시그니쳐("LZIP") + 버전 넘버(1) + 사전 크기 로 시작하고

뒤에는 CRC, 압축해제전 크기, 압축된 크기 정보가 들어 있습니다.

```c
  +--+--+--+--+----+----+=============+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  | ID string | VN | DS | LZMA stream | CRC32 |   Data size   |  Member size  |
  +--+--+--+--+----+----+=============+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

LZMA 스트림은 사전 크기 이외에도 lc, lp, pb 정보가 필요하지만, lzip 에서는 기본값(각 3, 0, 2) 만을 사용하기 때문에
LZIP 포맷에는 이와 관련된 헤더가 존재하지 않습니다.


## LZMA 알고리즘에 관하여

LZMA 알고리즘은 최대 4GiB-1의 범위를 가지는 거리 정보를 가지는 LZ77 압축 알고리즘 데이타를 다시한번 레인지 인코더로 저장하도록 설계된 압축 알고리즘 입니다.




