# ImageProcessor 과제 제출

Windows에서 동작하는 C++17 CLI 이미지 처리 프로그램입니다. 제공된 `BmpParser`, `ImageBuffer`, `CommandLineParser`를 기반으로 작성했으며, C++ 표준 라이브러리만 사용합니다. 이미지 변환은 픽셀 연산으로 직접 구현하고, BMP와 로그는 표준 파일 스트림으로 처리합니다.

## 구현 범위

| 구분 | 구현 항목 |
| --- | --- |
| 기본 이미지 처리 1 | Grayscale 변환 |
| 기본 이미지 처리 2 | 이진화(Thresholding) |
| 고급 1 | `FilterBase` 추상 클래스 기반 설계 |
| 고급 2 | 로그 파일 출력 |
| 고급 3 | `std::thread`를 이용한 타일 병렬 처리 |

24비트 무압축 BMP 읽기·저장과 CLI 인자 처리를 포함합니다. 필터 파이프라인, Blur/Sharpen, 밝기/대비, 히스토그램, 크롭/리사이즈, 반전은 구현하지 않았습니다. OpenCV, stb, Boost, Qt, ImageMagick 등 외부 라이브러리나 이미지 처리 API를 사용하지 않습니다.

## 빌드

필요 환경: Windows, Visual Studio 2022의 **C++를 사용한 데스크톱 개발**, MSVC v143, Windows SDK.

1. `ImageProcessor.sln`을 엽니다.
2. 구성을 `Release`, 플랫폼을 `x64`로 선택합니다.
3. 솔루션을 빌드합니다. 실행 파일은 `x64\Release\ImageProcessor.exe`입니다.

프로젝트에 C++17과 UTF-8 소스 인코딩을 설정했습니다. 별도 패키지 설치나 외부 라이브러리 연결은 필요하지 않습니다.

Visual Studio Developer PowerShell에서 프로젝트 루트를 기준으로 빌드할 수도 있습니다.

```powershell
msbuild .\ImageProcessor.sln /m /p:Configuration=Release /p:Platform=x64
```

## 실행

아래 명령은 프로젝트 루트에서 실행합니다.

```powershell
# Grayscale, 4개 작업 스레드, 지정한 파일에 로그 추가
.\x64\Release\ImageProcessor.exe --input .\Resource\1_astronaut.bmp --output .\Resource\1_astronaut_grayscale.bmp --filter grayscale --threads 4 --log .\grayscale.log

# 이진화, 임계값 128, 4개 작업 스레드
.\x64\Release\ImageProcessor.exe --input .\Resource\4_text_page.bmp --output .\Resource\4_text_page_threshold.bmp --filter threshold --threshold 128 --threads 4 --log .\threshold.log

# 제공 코드의 단일 필터 표기법도 지원
.\x64\Release\ImageProcessor.exe -i .\Resource\5_checkerboard.bmp -o .\Resource\5_checkerboard_threshold.bmp -f threshold:128

# 도움말
.\x64\Release\ImageProcessor.exe --help
```

공백이 포함된 경로는 큰따옴표로 감쌉니다. 출력 파일이 이미 존재하면 덮어쓰며, 상위 디렉터리는 미리 존재해야 합니다.

| 옵션 | 의미 | 기본값 / 범위 |
| --- | --- | --- |
| `--input`, `-i` | 입력 BMP 경로 | 필수 |
| `--output`, `-o` | 출력 BMP 경로 | 필수 |
| `--filter`, `-f` | `grayscale`, `threshold`, `threshold:N` | 필수 |
| `--threshold` | 이진화 임계값 | 128 / 0~255 정수 |
| `--threads` | 호출 스레드를 포함한 작업 스레드 수 | 자동 / 명시할 때 1~256 정수 |
| `--log` | 로그를 추가할 파일 경로 | 현재 작업 디렉터리의 `ImageProcessor.log` |
| `--help`, `-h` | 도움말 출력 | 이미지 처리와 로그 기록 없이 종료 |

`--threshold`는 `--filter threshold`와 함께 사용합니다. `threshold:N`과 `--threshold`를 동시에 지정하거나 같은 옵션을 반복하면 오류입니다. 범위를 벗어난 값과 `128abc` 같은 숫자 뒤 문자를 허용하지 않습니다.

## 알고리즘과 구조

### Grayscale / 이진화

메모리의 픽셀은 B, G, R 순서입니다. 두 필터는 아래 명도 계산을 공유합니다.

```text
Y = (299 × R + 587 × G + 114 × B) / 1000
```

정수 나눗셈으로 소수점 이하를 버립니다. Grayscale은 B, G, R에 모두 Y를 저장합니다. 이진화는 Y가 임계값 **이상**이면 세 채널에 255, 미만이면 0을 저장합니다. 따라서 임계값 0은 모두 흰색, 255는 Y가 255인 픽셀만 흰색입니다. 이진화도 결과 파일은 24비트 BMP입니다.

### 추상 클래스와 병렬 처리

| 파일 / 클래스 | 책임 |
| --- | --- |
| `main.cpp` | 파싱 → 로그 열기 → 필터 생성 → BMP 읽기 → 필터 적용 → BMP 저장 → 로그 기록 |
| `FilterBase` | 공통 `apply()`, 타일 분할, 스레드 수명과 작업 예외 관리 |
| `GrayscaleFilter`, `ThresholdFilter` | 순수 가상 함수 `applyTile()`을 구현해 픽셀 변환 |
| `ColorUtils.h` | 두 필터가 사용하는 가중 명도 계산 |
| `BmpParser`, `ImageBuffer` | BMP 헤더·행 패딩 처리, 픽셀 메모리 소유 |
| `CommandLineParser` | CLI 옵션과 숫자 범위 검증 |
| `Logger` | 로그 경로 검사와 실행 결과 기록 |

`std::unique_ptr<FilterBase>`로 필터를 소유하고 가상 함수로 실제 연산을 호출합니다. 새 필터는 파생 클래스와 생성 분기에 추가할 수 있으며, BMP 입출력 및 공통 타일 실행 코드는 재사용합니다.

이미지를 최대 128×128 픽셀 타일로 나눕니다. 작업 번호가 k이면 k, k+스레드 수, k+2×스레드 수 순서로 타일을 담당합니다. 오른쪽·아래쪽의 남는 영역은 이미지 경계까지 처리합니다. 두 필터는 현재 픽셀만 읽고 수정하므로 서로 다른 타일 사이에 공유 쓰기가 없습니다. 이미지 크기와 버퍼는 처리 중 변경하지 않습니다.

자동 스레드 수는 `hardware_concurrency()`를 사용하며, 조회 실패 시 1, 상한은 256입니다. 실제 수는 타일 개수 이하로 제한합니다. `--threads 1`이면 새 스레드를 생성하지 않습니다.

각 작업의 예외는 `exception_ptr`로 보관한 뒤 모든 작업이 끝나면 호출 측에서 다시 던집니다. 스레드 생성 도중 실패해도 RAII 객체가 이미 시작한 스레드를 모두 `join()`합니다. 추가 메모리는 스레드 관리용 O(스레드 수)이며, 필터 적용을 위해 이미지 전체를 복사하지 않습니다.

### BMP 검증

제공 코드의 24비트 무압축 BMP 처리와 4바이트 행 정렬을 유지했습니다. Bottom-up 및 top-down 입력을 읽고 결과는 bottom-up으로 저장합니다. 헤더 크기·평면 수·픽셀 시작 위치·실제 파일 길이를 확인하며, 잘못된 높이와 크기 계산의 정수 오버플로를 방지합니다. 제공된 메모리 제한인 1억 픽셀을 유지하고, 저장 시 스트림 닫기 실패까지 확인합니다.

## 로그와 오류 처리

로그는 실행당 한 줄을 파일 끝에 추가합니다. `status`, 입력/출력 경로, 필터, 임계값, 요청/실제 스레드 수, `filter_ms`, `elapsed_ms`, 결과 메시지를 기록합니다.

- `filter_ms`: 타일 분할·스레드 생성·픽셀 처리·join을 포함한 필터 적용 시간.
- `elapsed_ms`: CLI 파싱부터 BMP 저장 또는 오류 처리까지의 시간. 마지막 로그 쓰기는 제외.
- 필터가 완료되기 전에 실패한 경우 `actual_threads=0`이며, 필터를 시작하지 않았다면 `filter_ms=0`.
- BMP 열기·저장 실패와 알 수 없는 필터도 실패 로그를 기록합니다.
- CLI 인자 오류는 경로가 확정되지 않아 콘솔에만 출력합니다. 인자 검증이 끝나고 로그 파일을 정상적으로 연 실행부터 성공/실패 기록을 남깁니다.
- 로그 경로가 입력/출력 파일과 같으면 파일 손상을 막기 위해 처리 전에 실패합니다. 로그 파일을 열거나 쓰지 못하면 콘솔에 오류를 알리고 실패 코드로 종료합니다.

| 종료 코드 | 의미 |
| --- | --- |
| 0 | 정상 처리 또는 도움말 |
| 1 | 로그 오류 등 기타 예외 |
| 2 | BMP 입출력·형식 오류 |
| 3 | 필터 생성·적용 오류 |
| 4 | CLI 인자 오류 |

## 검증

C++ 표준 라이브러리만 사용하는 테스트와 PowerShell 실행 스크립트를 제공합니다. 일반 PowerShell에서 다음 명령으로 앱·테스트를 빌드하고 검증합니다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tests\run-tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tests\run-tests.ps1 -Configuration Debug
```

검증 항목:

- 원색과 흑백의 예상 Grayscale 값, 임계값 0·128·255의 경계.
- 1×1, 한 행/열, 타일 크기의 배수가 아닌 259×257 이미지.
- 모든 픽셀이 정확히 한 번 처리되는지, 작업 예외가 호출 측으로 전달되는지.
- BMP 행 방향·패딩·저장 후 재읽기, 손상 헤더·잘린 파일·지원하지 않는 형식.
- 제공된 BMP 5개에 두 필터를 적용했을 때 1스레드와 4스레드 출력 파일의 SHA-256 일치.
- 잘못된 인자와 종료 코드, 성공/실패 로그, 입력/출력과 로그 경로 충돌.

실제 검증 환경은 Windows, Visual Studio 2026에 설치된 **MSVC v143 14.44**, C++17입니다. x64 Release/Debug 테스트와 x86 Release 빌드를 확인했습니다. 프로젝트의 Visual Studio 2022용 v143 설정은 유지했으며, Visual Studio 2022 IDE 자체에서의 실행은 별도로 확인하지 않았습니다.

### 병렬 처리 측정

테스트 실행 시 4096×4096 합성 이미지에서 두 필터의 1스레드/4스레드 시간을 출력합니다. 각 조건에서 첫 실행을 제외한 7회 중앙값이며, 이미지 복사와 BMP 입출력 시간은 제외합니다. Release, AMD Ryzen 5 7500F(6코어/12논리 프로세서)에서 측정한 한 실행의 결과입니다.

| 필터 | 1스레드 | 4스레드 | 속도 비율 |
| --- | ---: | ---: | ---: |
| Grayscale | 18.082 ms | 5.880 ms | 약 3.08배 |
| 이진화 | 22.232 ms | 7.226 ms | 약 3.08배 |

작은 이미지에서는 스레드 생성 비용으로 단일 스레드가 더 빠를 수 있습니다. 성능은 이미지 크기·CPU·시스템 부하에 따라 달라지므로 테스트의 통과 조건으로 사용하지 않습니다.

테스트 산출물은 `build/tests/`에 생성됩니다. 소스, 프로젝트, README, 원본 Resource를 제출 대상으로 두고 빌드 파일·개인 IDE 설정·로그는 `.gitignore`로 제외했습니다.
