# ImageProcessor 코드 학습 노트

이 문서는 **현재 프로젝트 코드를 처음 읽는 사람**을 위한 설명서입니다. 목표는 코드를 외우는 것이 아니라, 실행 명령 하나가 어떤 함수를 거쳐 픽셀을 바꾸고 파일로 저장되는지 직접 설명할 수 있게 되는 것입니다.

처음에는 1~6장을 읽고 단일 스레드로 실행해 보세요. 그다음 7~10장에서 구조와 오류 처리를 배우고, 11장의 디버깅 실습으로 실제 값을 확인하면 됩니다. 코드 조각은 설명에 필요한 부분을 발췌했으며, 생략된 부분은 연결된 소스 파일에서 볼 수 있습니다.

## 1. 이 프로그램은 무엇을 하는가?

우리가 구현한 이미지 변환은 두 가지입니다.

| 기능 | 하는 일 |
| --- | --- |
| Grayscale | 색상 이미지를 여러 밝기의 회색으로 바꾼다. |
| 이진화, Thresholding | 밝기를 기준값과 비교해 완전한 검정 또는 흰색으로 바꾼다. |

여기에 추상 클래스 설계, 로그 파일 출력, 멀티스레드 병렬 처리가 들어 있습니다. BMP 읽기·쓰기와 CLI 파서는 제공된 코드를 활용하고 필요한 검증과 옵션을 보완했습니다. 이미지 변환은 C++ 표준 라이브러리와 직접 작성한 계산으로 처리합니다.

CLI는 창의 버튼 대신 **실행 명령에 값을 붙여 조작하는 방식**입니다. 다음 명령을 예로 보겠습니다. 프로젝트 루트에서 Release 빌드 후 실행하는 명령입니다.

```powershell
.\x64\Release\ImageProcessor.exe --input .\Resource\1_astronaut.bmp --output .\Resource\study_gray.bmp --filter grayscale --threads 1 --log .\study.log
```

뜻은 다음과 같습니다.

```text
1_astronaut.bmp를 읽는다.
grayscale 필터를 적용한다.
작업 스레드는 1개를 사용한다.
study_gray.bmp로 저장한다.
실행 결과를 study.log에 추가한다.
```

`--threads 1`부터 시작하는 이유는 한 실행 흐름을 순서대로 따라가기 쉽기 때문입니다. 기능을 이해한 뒤 4로 바꿔 비교하면 됩니다.

## 2. 파일을 어떤 순서로 읽으면 좋을까?

| 읽는 순서 | 파일 | 처음 확인할 내용 |
| --- | --- | --- |
| 1 | [main.cpp](ImageProcessor/main.cpp) | 프로그램 전체 실행 순서 |
| 2 | [CommandLineParser.h](ImageProcessor/CommandLineParser.h) | 사용자의 요청을 담는 `ProgramOptions` |
| 3 | [ImageBuffer.h](ImageProcessor/ImageBuffer.h) | 이미지 크기와 픽셀을 보관하는 방법 |
| 4 | [ColorUtils.h](ImageProcessor/ColorUtils.h) | 색상에서 밝기를 계산하는 식 |
| 5 | [GrayscaleFilter.cpp](ImageProcessor/GrayscaleFilter.cpp), [ThresholdFilter.cpp](ImageProcessor/ThresholdFilter.cpp) | 실제 픽셀을 바꾸는 반복문 |
| 6 | [FilterBase.h](ImageProcessor/FilterBase.h), [FilterBase.cpp](ImageProcessor/FilterBase.cpp) | 추상 클래스와 병렬 실행 |
| 7 | [BmpParser.cpp](ImageProcessor/BmpParser.cpp) | 파일의 바이트와 메모리의 픽셀을 연결하는 방법 |
| 8 | [Logger.cpp](ImageProcessor/Logger.cpp), [Exceptions.h](ImageProcessor/Exceptions.h) | 기록과 실패 처리 |
| 9 | [ImageProcessorTests.cpp](Tests/ImageProcessorTests.cpp) | 구현이 맞는지 검증하는 방법 |

### `.h`와 `.cpp`는 왜 나뉘어 있을까?

`.h`에는 다른 코드가 알아야 할 클래스 모양과 함수 선언을 주로 적습니다. `.cpp`에는 그 함수가 실제로 어떻게 동작하는지를 주로 적습니다. 이 프로젝트의 `ColorUtils.h`처럼 짧은 함수의 구현을 헤더에 넣는 경우도 있습니다.

빌드할 때는 각 `.cpp`와 그 파일이 포함한 헤더를 묶어 컴파일합니다. 이 묶음을 번역 단위라고 합니다. 컴파일한 결과들을 연결하는 링크 단계를 거쳐 하나의 실행 파일이 만들어집니다. 함수 선언이 “이 함수를 이렇게 호출할 수 있다”고 알리면, 함수 정의는 실행할 실제 코드를 제공합니다.

```cpp
// ThresholdFilter.h: 이런 생성자를 사용할 수 있다고 알린다.
explicit ThresholdFilter(int threshold = 128);
```

```cpp
// ThresholdFilter.cpp: 값을 저장하고 범위를 검사하는 실제 동작이다.
ThresholdFilter::ThresholdFilter(int threshold) : m_threshold(threshold) {
    if (threshold < 0 || threshold > 255) {
        throw FilterError("Threshold must be between 0 and 255");
    }
}
```

`생성자`는 객체를 만들 때 실행되는 함수입니다. `: m_threshold(threshold)`는 객체의 멤버 변수에 받은 값을 초기화합니다. `explicit`은 정수가 의도치 않게 필터 객체로 자동 변환되는 것을 막습니다.

`ImageProcessor.sln`은 Visual Studio 솔루션 파일이고, `.vcxproj`는 컴파일할 파일·C++ 버전·빌드 설정을 담습니다. `.vcxproj.user`는 개인 디버깅 설정 같은 정보를 담습니다. 알고리즘 학습은 위 표의 C++ 파일부터 시작하면 됩니다.

## 3. `main.cpp`를 실행 순서대로 읽기

정상 처리의 순서는 다음과 같습니다.

```text
main 시작
  ↓
CommandLineParser::parse()   명령행을 ProgramOptions로 변환
  ↓
Logger::open()               로그 경로 검사 및 파일 열기
  ↓
createFilter()               선택한 필터 객체 생성
  ↓
BmpParser::loadFromFile()    BMP를 ImageBuffer로 읽기
  ↓
FilterBase::apply()          타일을 나누고 실제 필터 실행
  ↓
BmpParser::saveToFile()      바뀐 ImageBuffer를 BMP로 저장
  ↓
Logger::write()              시간과 결과 기록
  ↓
return exitCode              종료 코드를 운영체제에 전달
```

### 3-1. `argc`, `argv`는 명령행을 전달받는다

```cpp
int main(int argc, char* argv[])
```

`argc`는 전달된 문자열 개수입니다. `argv`는 그 문자열들에 접근하는 배열입니다. `argv[0]`에는 보통 실행 파일 이름 또는 경로가 들어갑니다.

예를 들어 `ImageProcessor.exe --filter grayscale`이라는 짧은 명령의 전달 값은 다음과 같습니다. 이 명령 자체는 입력·출력 옵션이 없어서 실제 실행하면 인자 오류입니다.

```text
argc = 3
argv[0] = "ImageProcessor.exe"
argv[1] = "--filter"
argv[2] = "grayscale"
```

파서는 이 문자열들을 읽어 아래 구조체를 채웁니다.

```cpp
struct ProgramOptions {
    std::string inputPath;
    std::string outputPath;
    std::string filterName;
    int threshold = 128;
    unsigned int threadCount = 0;
    std::string logPath = "ImageProcessor.log";
    bool showHelp = false;
};
```

구조체는 관련된 값을 한 묶음으로 보관합니다. `options.inputPath`는 그 묶음 안의 입력 경로를 읽는 표현입니다. 내부의 `threadCount = 0`은 자동 선택을 뜻합니다. 사용자가 CLI에 직접 `--threads 0`을 적는 것은 허용하지 않습니다.

### 3-2. 문자열이 실제 필터 객체로 바뀐다

```cpp
const auto filter = createFilter(options);
```

`createFilter()`는 `filterName`에 따라 `GrayscaleFilter` 또는 `ThresholdFilter`를 만듭니다. 알 수 없는 이름이면 `FilterError`를 던집니다. `auto`는 우변을 보고 컴파일러가 변수의 자료형을 정하게 합니다. 여기서는 `std::unique_ptr<ip::FilterBase>`입니다.

### 3-3. 이미지가 메모리로 들어온다

```cpp
ip::ImageBuffer image = ip::BmpParser::loadFromFile(options.inputPath);
```

이 줄 이후 `image`는 너비·높이·픽셀 데이터를 갖고 있습니다. 필터는 파일을 직접 열지 않고 이 메모리 안의 픽셀을 바꿉니다.

### 3-4. 필터가 이미지 원본을 수정한다

```cpp
record.actualThreads = filter->apply(image, options.threadCount);
```

`->`는 포인터가 가리키는 객체의 멤버에 접근합니다. `apply()`는 전달받은 `image`를 수정하고, 실제 작업 스레드 수를 반환합니다. 처리된 이미지를 반환하는 함수가 아니라는 점을 구분하세요.

### 3-5. 수정된 이미지를 저장하고 기록한다

```cpp
ip::BmpParser::saveToFile(options.outputPath, image);
record.success = true;
record.message = "Completed";
```

저장까지 성공해야 성공 상태를 설정합니다. 이후 로그를 쓰고 종료 코드를 반환합니다. 중간에 예외가 발생하면 해당 `try`의 남은 문장은 건너뛰고 대응하는 `catch`로 이동합니다.

## 4. 이미지가 메모리에서는 어떻게 생겼을까?

### 4-1. 픽셀 하나는 B, G, R 세 숫자다

24비트 컬러 이미지의 픽셀 하나는 8비트 채널 세 개로 구성됩니다. 채널 값은 각각 0~255입니다. 현재 프로젝트는 **B, G, R 순서**로 저장합니다.

| 색 | B | G | R |
| --- | ---: | ---: | ---: |
| 검정 | 0 | 0 | 0 |
| 흰색 | 255 | 255 | 255 |
| 빨강 | 0 | 0 | 255 |
| 초록 | 0 | 255 | 0 |
| 파랑 | 255 | 0 | 0 |
| 회색 | 128 | 128 | 128 |

`std::uint8_t`는 여기서 채널 하나를 저장하는 8비트 부호 없는 정수입니다. 화면에 표시할 때는 문자처럼 출력될 수 있으므로 숫자를 확인할 때 `static_cast<int>(값)`을 사용하면 편합니다.

### 4-2. 2차원 그림을 1차원 배열에 넣는다

너비 2, 높이 2인 이미지를 생각해 보세요. 좌표는 왼쪽 위가 `(0, 0)`이고, x는 오른쪽, y는 아래쪽으로 증가합니다.

```text
화면 좌표:      (0, 0)       (1, 0)
                (0, 1)       (1, 1)

메모리 위치:    0  1  2      3  4  5      6  7  8      9 10 11
저장 채널:      B  G  R      B  G  R      B  G  R      B  G  R
픽셀 좌표:      (0, 0)       (1, 0)       (0, 1)       (1, 1)
```

그래서 필요한 픽셀 메모리 크기는 `너비 × 높이 × 3`바이트입니다. `ImageBuffer`는 이것을 `std::vector<std::uint8_t>`에 저장합니다. `vector`는 연속된 메모리를 소유하고 객체가 사라질 때 해제합니다.

좌표 `(x, y)`의 첫 채널 위치는 다음과 같습니다.

```text
offset = (y × width + x) × 3
```

너비 2에서 `(1, 1)`은 `(1 × 2 + 1) × 3 = 9`이므로 인덱스 9부터 시작합니다.

### 4-3. `rowStride()`와 `rowPtr(y)`

`stride`는 다음 행으로 이동할 때 건너뛸 바이트 수입니다. 메모리의 `rowStride()`는 `width × 3`입니다. `rowPtr(y)`는 y번째 행이 시작하는 주소를 반환합니다.

```cpp
return m_data.data() + static_cast<std::size_t>(y) * rowStride();
```

`m_data.data()`는 첫 바이트의 주소입니다. 거기에 y개 행의 길이를 더합니다. `rowPtr()`는 y의 범위를 검사하지만, 그 주소에 더하는 x 값의 검사는 호출하는 코드의 책임입니다. 필터에서는 타일 경계를 유효한 이미지 범위로 제한합니다.

## 5. Grayscale 반복문을 한 줄씩 이해하기

먼저 [ColorUtils.h](ImageProcessor/ColorUtils.h)의 계산을 봅니다.

```cpp
inline std::uint8_t luminance(const std::uint8_t* bgr) noexcept {
    return static_cast<std::uint8_t>(
        (299u * bgr[2] + 587u * bgr[1] + 114u * bgr[0]) / 1000u);
}
```

이 함수는 BGR 픽셀의 주소를 받아 밝기 하나를 계산합니다.

```text
Y = 0.299 × R + 0.587 × G + 0.114 × B
```

각 색 채널의 기여도를 다르게 반영하는 가중합입니다. 코드에서는 같은 계수를 정수로 표현하기 위해 299, 587, 114를 곱하고 1000으로 나눕니다. 정수끼리 나누므로 소수점 이하는 버립니다. `u`는 부호 없는 정수 리터럴이라는 뜻입니다.

빨간 픽셀 `(B=0, G=0, R=255)`이라면:

```text
Y = (299 × 255 + 587 × 0 + 114 × 0) / 1000
  = 76245 / 1000
  = 76
```

그다음 세 채널을 같은 값으로 바꿉니다.

```cpp
const auto gray = luminance(pixel);
pixel[0] = pixel[1] = pixel[2] = gray;
```

`(0, 0, 255)`가 `(76, 76, 76)`으로 바뀌는 것입니다. 이 연결된 대입은 세 채널에 모두 `gray`를 넣습니다.

전체 반복문은 다음과 같습니다.

```cpp
for (int y = tile.yBegin; y < tile.yEnd; ++y) {
    auto* pixel = image.rowPtr(y) + tile.xBegin * ImageBuffer::CHANNELS;
    for (int x = tile.xBegin; x < tile.xEnd; ++x, pixel += ImageBuffer::CHANNELS) {
        const auto gray = luminance(pixel);
        pixel[0] = pixel[1] = pixel[2] = gray;
    }
}
```

바깥 반복문은 행을 선택합니다. 각 행에서 `rowPtr(y)`로 행 시작점을 찾고, `xBegin × 3`만큼 이동해 타일의 첫 픽셀을 찾습니다. 안쪽 반복문은 오른쪽으로 이동하면서 각 픽셀을 바꿉니다. `pixel += 3`은 다음 픽셀의 B 채널로 이동하는 것입니다.

`xEnd`, `yEnd`는 **포함하지 않는 끝 좌표**입니다. 범위가 `[0, 128)`이면 좌표 0~127의 픽셀 128개를 처리합니다.

## 6. 이진화는 무엇이 다른가?

[ThresholdFilter.cpp](ImageProcessor/ThresholdFilter.cpp)의 반복문은 Grayscale과 같은 방식으로 픽셀을 방문합니다. 계산 결과를 저장하는 부분이 다릅니다.

```cpp
const auto value = static_cast<std::uint8_t>(
    luminance(pixel) >= m_threshold ? 255 : 0);
pixel[0] = pixel[1] = pixel[2] = value;
```

`조건 ? A : B`는 조건이 참이면 A, 거짓이면 B를 선택하는 표현입니다. 이 코드에서는 밝기가 임계값 **이상**이면 255, 미만이면 0입니다.

| 밝기 Y | 임계값 | 비교 | 저장되는 BGR |
| --- | ---: | --- | --- |
| 76 | 128 | 76 ≥ 128은 거짓 | `(0, 0, 0)` |
| 127 | 128 | 127 ≥ 128은 거짓 | `(0, 0, 0)` |
| 128 | 128 | 128 ≥ 128은 참 | `(255, 255, 255)` |
| 200 | 128 | 200 ≥ 128은 참 | `(255, 255, 255)` |

임계값을 올리면 흰색이 되는 조건이 더 까다로워지므로 흰 픽셀 수가 줄거나 그대로입니다. 임계값 0이면 모두 흰색이고, 255이면 Y가 255인 픽셀만 흰색입니다.

이진화 필터는 원본 픽셀에서 밝기를 직접 계산합니다. 먼저 Grayscale 필터를 실행해야 하는 것은 아닙니다. 또한 결과의 색이 두 가지여도 파일은 기존과 같은 **24비트 BMP**로 저장됩니다.

### 여기까지 이해했는지 확인하기

종이에 초록 픽셀 `(B=0, G=255, R=0)`을 적고 직접 계산해 보세요.

1. Grayscale의 Y는 얼마인가?
2. 임계값 128이면 검정인가, 흰색인가?
3. 임계값 150이면 검정인가, 흰색인가?

정답은 Y=149, 임계값 128에서는 흰색, 150에서는 검정입니다.

## 7. 추상 클래스는 왜 필요한가?

### 7-1. 두 필터가 함께 사용하는 작업을 분리한다

두 필터 모두 이미지를 타일로 나누고, 작업자를 배정하고, 완료를 기다려야 합니다. 이 코드를 필터마다 복사하면 병렬 처리의 버그를 고칠 때 여러 곳을 수정해야 합니다.

그래서 공통 실행은 `FilterBase`에 두고, 다른 부분인 픽셀 변환만 파생 클래스가 구현합니다.

```cpp
class FilterBase {
public:
    virtual ~FilterBase() = default;
    unsigned int apply(ImageBuffer& image, unsigned int threadCount = 0) const;

protected:
    // Tile 선언 생략
    virtual void applyTile(ImageBuffer& image, const Tile& tile) const = 0;
};
```

끝의 `= 0`은 **순수 가상 함수**를 나타냅니다. `applyTile()`의 실제 픽셀 연산을 파생 클래스에 맡깁니다. 이런 함수를 가진 `FilterBase`는 추상 클래스이므로 `FilterBase filter;`처럼 직접 객체를 만들 수 없습니다.

여기서 `apply()`는 가상 함수가 아닙니다. 모든 필터가 사용하는 공통 실행 함수이고, 그 안에서 가상 함수인 `applyTile()`을 호출합니다. 이 구조를 템플릿 메서드 패턴이라고 부르기도 합니다. 이름보다 공통 순서와 개별 연산을 분리했다는 점을 이해하면 됩니다.

### 7-2. 파생 클래스가 실제 연산을 제공한다

```cpp
class GrayscaleFilter final : public FilterBase {
protected:
    void applyTile(ImageBuffer& image, const Tile& tile) const override;
};
```

`: public FilterBase`는 `FilterBase`를 상속한다는 뜻입니다. `override`는 기반 클래스의 가상 함수를 올바르게 재정의했는지 컴파일러가 확인하게 합니다. `final`은 이 클래스에서 다시 상속하지 않도록 합니다. `protected` 멤버는 클래스와 파생 클래스 안에서 사용할 수 있으며, `main()`에서는 직접 호출할 수 없습니다.

실제 호출 관계는 다음과 같습니다.

```text
main의 filter->apply(...)
    ↓
FilterBase::apply(...)
    ↓ 실제 객체 종류에 따라 선택
GrayscaleFilter::applyTile(...) 또는 ThresholdFilter::applyTile(...)
```

공통 기반 타입을 통해 실제 객체에 맞는 함수를 실행하는 것을 **다형성**이라고 합니다.

### 7-3. `unique_ptr`와 가상 소멸자

```cpp
std::unique_ptr<ip::FilterBase> createFilter(const ip::ProgramOptions& options)
```

이 함수의 반환값은 필터 객체 하나를 소유하는 스마트 포인터입니다. `std::make_unique<ip::GrayscaleFilter>()`로 객체를 만들고, 소유자인 `unique_ptr`가 사라지면 객체도 자동으로 해제합니다. 직접 `delete`를 호출할 필요가 없습니다.

`virtual ~FilterBase() = default;`는 기반 클래스 포인터로 삭제해도 실제 파생 클래스의 소멸 과정이 실행되도록 합니다. 다형적으로 소유하는 이 구조에서 필요한 선언입니다.

객체의 수명에 자원의 정리를 묶는 방식을 **RAII**라고 합니다. 이 프로젝트에서는 `vector`가 픽셀 메모리를, `unique_ptr`가 필터를, 파일 스트림이 파일을 관리합니다. 뒤에서 볼 `ThreadJoiner`도 같은 원리로 스레드 정리를 보장합니다.

### 7-4. 참조와 `const`를 읽는 방법

```cpp
void applyTile(ImageBuffer& image, const Tile& tile) const;
```

| 표현 | 이 코드에서의 의미 |
| --- | --- |
| `ImageBuffer& image` | 호출자가 가진 이미지의 참조를 받는다. 복사본을 만들지 않고 그 이미지를 수정한다. |
| `const Tile& tile` | 타일을 복사하지 않고 참조하되, 이 참조를 통해 타일을 수정하지 않는다. |
| 마지막의 `const` | 이 멤버 함수가 필터 객체의 일반 멤버 변수를 수정하지 않는다. |

마지막 `const`가 있다고 `image`까지 읽기 전용이 되는 것은 아닙니다. 필터 객체와 전달받은 이미지 객체는 서로 다릅니다.

이 구조는 픽셀 연산을 확장할 때 공통 실행 코드를 재사용하기 좋습니다. 다만 현재 `createFilter()`의 생성 분기는 새 필터를 추가할 때 수정해야 합니다. 또한 이웃 픽셀을 읽는 필터라면 입력 버퍼 분리 같은 동시 접근 설계를 추가로 검토해야 합니다. 현재 과제에서는 선택한 두 필터만 사용합니다.

## 8. 멀티스레드는 실제로 무엇을 나눠서 하는가?

### 8-1. 타일은 이미지의 직사각형 구역이다

타일 한 개의 최대 크기는 128×128입니다. 너비 259, 높이 257인 이미지는 가로 3개, 세로 3개로 나뉩니다.

```text
                 x=0~127     x=128~255    x=256~258
y=0~127           타일 0        타일 1        타일 2
y=128~255         타일 3        타일 4        타일 5
y=256             타일 6        타일 7        타일 8
```

오른쪽 타일의 너비는 3이고, 맨 아래 타일의 높이는 1입니다. `std::min()`으로 끝 좌표를 이미지 경계까지 제한하므로 작은 나머지 영역도 처리됩니다.

```cpp
const int columns = (image.width() - 1) / TILE_SIZE + 1;
const int rows = (image.height() - 1) / TILE_SIZE + 1;
```

양의 크기에서 필요한 타일 수를 올림해 구하는 식입니다. 너비 259라면 `(259 - 1) / 128 + 1 = 3`입니다.

### 8-2. 작업자마다 다른 타일 번호를 준다

```cpp
for (std::size_t index = worker; index < tileCount; index += threadCount)
```

타일 9개, 작업자 4명이라면 다음처럼 배정됩니다.

| 작업자 번호 | 담당 타일 |
| --- | --- |
| 0 | 0, 4, 8 |
| 1 | 1, 5 |
| 2 | 2, 6 |
| 3 | 3, 7 |

타일 번호를 좌표로 되돌리는 식도 확인해 보세요.

```cpp
const int x = static_cast<int>(index % columns) * TILE_SIZE;
const int y = static_cast<int>(index / columns) * TILE_SIZE;
```

`%`는 나머지 연산입니다. 가로 타일이 3개일 때 번호 5는 `5 % 3 = 2`번째 열, `5 / 3 = 1`번째 행에 있으므로 시작 좌표가 `(256, 128)`입니다. 행과 열 번호는 0부터 셉니다.

### 8-3. `--threads 4`는 새 스레드를 3개 만든다

```cpp
for (unsigned int worker = 1; worker < threadCount; ++worker) {
    threads.emplace_back(runWorker, worker);
}
runWorker(0);
```

1~3번 작업은 새 `std::thread`에서 시작합니다. 0번은 현재 `apply()`를 호출한 스레드, 이 프로그램에서는 메인 스레드가 맡습니다. 총 작업 스레드 수가 4가 됩니다. 실제로 어느 CPU 코어에서 언제 실행할지는 운영체제가 정합니다.

`--threads 1`은 새 스레드 없이 바로 처리합니다. 작업자가 타일보다 많으면 타일 수로 줄이며, 옵션을 생략하면 하드웨어 동시 실행 수를 참고해 선택합니다. `hardware_concurrency()`의 반환값이 0이면 1로 보정하고 최대 256으로 제한합니다.

### 8-4. 람다는 이름을 붙여 보관하는 작은 함수다

```cpp
const auto processTiles = [&](unsigned int worker) {
    // worker가 맡은 타일들을 처리한다.
};
```

`[&]`는 함수 밖에서 사용 중인 이미지·타일 수 등의 변수를 참조로 사용한다는 뜻입니다. `processTiles(0)`처럼 호출합니다. 이때 참조 대상이 먼저 사라지면 위험하므로, `apply()`가 끝나기 전에 모든 스레드가 종료되도록 해야 합니다.

### 8-5. 왜 픽셀마다 잠금을 걸지 않을까?

잠금은 여러 스레드가 같은 데이터를 동시에 수정할 때 접근 순서를 맞추는 수단입니다. 현재 두 필터는 서로 겹치지 않는 타일의 바이트만 바꾸고, 이웃 픽셀을 읽지 않습니다. 처리 중 벡터 크기와 이미지 크기도 바꾸지 않습니다.

그래서 이 구현은 픽셀 쓰기에 `mutex`를 사용하지 않습니다. **스레드를 사용하면 자동으로 안전해지는 것이 아니라, 겹치는 메모리 접근을 피하도록 나눴기 때문에 안전한 것입니다.**

### 8-6. `join()`과 작업 예외 처리

`join()`은 해당 스레드의 실행이 끝날 때까지 기다립니다. 여러 스레드가 이미 시작된 뒤 차례로 `join()`한다고 해서 작업 자체가 순차 실행으로 바뀌는 것은 아닙니다.

현재 코드는 중괄호로 만든 범위 안에 `ThreadJoiner`를 둡니다.

```cpp
{
    ThreadJoiner joiner(threads);
    // 스레드들을 시작하고 현재 스레드도 작업한다.
}
// 이 위치에서는 시작한 스레드들이 모두 join된 상태다.
```

정상적으로 이 범위를 나가거나, 스레드 생성 도중 예외가 발생해서 나가더라도 `joiner`의 소멸자가 실행됩니다. 이미 생성한 스레드들을 모두 기다립니다. `reserve()`는 벡터 저장 공간을 미리 확보할 뿐 스레드를 시작하는 함수는 아닙니다.

작업 스레드에서 던진 예외는 메인 스레드의 `try`로 자동 전달되지 않습니다. 스레드 함수 밖으로 예외가 빠져나가면 프로그램이 종료될 수 있습니다. 그래서 작업 안에서 예외를 잡아 자신의 슬롯에 저장합니다.

```cpp
catch (...) {
    errors[worker] = std::current_exception();
}
```

모두 `join()`한 다음 `std::rethrow_exception()`으로 호출 측에서 다시 던집니다. 각 작업은 별도의 `errors[worker]`를 쓰며, 호출 측은 작업 종료 후에 읽습니다. 재전달된 오류는 필요에 따라 `FilterError`로 바뀌어 `main()`에서 처리됩니다.

작은 이미지에서는 스레드를 만드는 시간이 계산 시간보다 클 수 있습니다. 스레드 수를 늘렸다고 항상 빨라지는 것은 아니며, 속도는 Release 빌드에서 비교해야 합니다.

## 9. BMP 파일과 `ImageBuffer`는 무엇이 다른가?

필터는 메모리의 픽셀을 처리하지만, BMP 파일에는 그림을 해석하기 위한 정보도 들어 있습니다.

```text
BMP 파일:       파일 헤더 + 이미지 정보 헤더 + 픽셀 행과 패딩
ImageBuffer:    너비, 높이 + 패딩 없는 BGR 픽셀 벡터
```

### 9-1. 헤더는 바이트들을 읽는 방법을 알려 준다

[BmpParser.cpp](ImageProcessor/BmpParser.cpp)에서 다음 필드를 찾아보세요.

| 필드 | 의미 |
| --- | --- |
| `bfType` | BMP 표시인 `BM`인지 확인 |
| `bfSize` | 헤더에 기록된 전체 파일 크기 |
| `bfOffBits` | 픽셀 데이터가 시작되는 파일 위치 |
| `biSize` | 이미지 정보 헤더의 크기 |
| `biWidth`, `biHeight` | 너비와 높이, 행의 저장 방향 |
| `biPlanes` | 현재 지원 형식에서는 1이어야 하는 평면 수 |
| `biBitCount` | 픽셀당 비트 수, 현재는 24만 지원 |
| `biCompression` | 압축 방식, 현재는 0인 무압축만 지원 |

이 프로그램이 저장하는 헤더는 14바이트와 40바이트로 총 54바이트입니다. 입력을 읽을 때는 헤더가 더 클 수 있으므로 픽셀 위치를 54로 고정하지 않고 `bfOffBits`를 사용합니다.

헤더 구조체 주변의 `#pragma pack(push, 1)`은 컴파일러가 멤버 사이에 넣는 정렬용 빈 공간을 조절합니다. `static_assert`로 구조체 크기가 14와 40인지 컴파일할 때 확인합니다. 구조체 바이트를 직접 읽는 방식은 과제의 Windows 대상 환경을 전제로 합니다. 다른 바이트 순서의 시스템까지 그대로 지원한다고 볼 수는 없습니다.

### 9-2. BMP의 한 행은 4바이트 단위로 맞춘다

너비 3인 이미지의 실제 픽셀은 한 행에 `3 × 3 = 9`바이트입니다. BMP 파일에서는 4의 배수인 12바이트에 맞추기 위해 빈 바이트 3개를 붙입니다. 이를 패딩이라고 합니다.

```text
파일의 한 행:    [B G R][B G R][B G R][패딩 3바이트] = 12바이트
메모리의 한 행:  [B G R][B G R][B G R]               =  9바이트
```

읽을 때는 파일에서 12바이트를 읽고 실제 픽셀 9바이트만 복사합니다. 저장할 때는 실제 픽셀 뒤에 0으로 채운 패딩을 붙입니다. `std::memcpy()`는 바이트를 복사하는 표준 함수이며, 픽셀의 색상을 계산하는 기능은 없습니다.

### 9-3. 파일에는 맨 아래 행이 먼저 나올 수 있다

BMP의 높이가 양수이면 bottom-up이어서 파일의 첫 픽셀 행이 그림의 맨 아래 행입니다. 음수이면 top-down입니다. `ImageBuffer`는 항상 맨 위 행부터 저장하도록 통일합니다.

```cpp
const int dstY = isTopDown ? row : (height - 1 - row);
```

높이 2의 bottom-up BMP에서 파일의 0번 행은 메모리의 1번 행으로 복사합니다. 덕분에 필터는 파일의 저장 방향을 몰라도 같은 코드로 처리할 수 있습니다.

### 9-4. 크기를 먼저 검사하는 이유

파일에 기록된 크기는 잘못되어 있을 수 있습니다. 픽셀 데이터를 읽기 전에 헤더와 실제 파일 길이를 확인하고, `ImageBuffer`는 양의 크기와 1억 픽셀 제한을 검사합니다.

```cpp
if (static_cast<std::size_t>(width) > MAX_PIXEL_COUNT / static_cast<std::size_t>(height))
```

`width × height`를 먼저 계산하면 32비트 환경에서는 정수 범위를 넘어 엉뚱한 값이 될 수 있습니다. 이미 양수임을 확인한 높이로 나누어 비교하면 곱셈하기 전에 너무 큰 크기를 거부할 수 있습니다. 이런 범위 초과를 오버플로라고 합니다.

## 10. 잘못된 입력과 로그는 어떻게 처리할까?

### 10-1. 숫자 변환 성공과 전체 입력의 유효성은 다르다

[CommandLineParser.cpp](ImageProcessor/CommandLineParser.cpp)의 `parseNumber()`는 `std::from_chars()`로 문자열을 정수로 변환합니다. 다음 세 가지를 모두 확인합니다.

1. 변환 중 오류가 없었는가?
2. 문자열의 마지막까지 읽었는가?
3. 정수가 허용 범위 안에 있는가?

`128abc`는 앞의 128만 숫자로 읽을 수 있지만 전체가 올바른 정수 입력은 아닙니다. `result.ptr`가 문자열 끝인지 확인하는 이유입니다. 옵션 중복은 `std::set`에 이름을 보관해서 검사합니다. `-f`를 `--filter`로 먼저 바꾸므로 서로 다른 표기로 같은 옵션을 반복해도 감지합니다.

### 10-2. 예외는 실패를 호출한 곳에 전달한다

```cpp
throw FilterError("Unknown filter: " + options.filterName);
```

`throw`는 현재 정상 흐름을 중단하고 오류를 전달합니다. 대응하는 `catch`를 찾는 동안 범위를 벗어나는 지역 객체의 소멸자가 실행됩니다. 이 과정에서도 자원이 정리되게 하는 것이 RAII의 중요한 역할입니다.

```cpp
catch (const ip::FilterError& error) {
    record.message = error.what();
    exitCode = 3;
}
```

`what()`으로 오류 메시지를 읽습니다. `const ...&`는 예외 객체를 복사하지 않고 읽는 방식입니다. 여기서 `exitCode = 3`을 저장한 뒤 공통 로그 처리까지 진행합니다.

| 종료 코드 | 상황 |
| --- | --- |
| 0 | 처리 성공 또는 도움말 |
| 1 | 로그 실패 등 기타 오류 |
| 2 | BMP 열기·형식 검사·저장 실패 |
| 3 | 알 수 없는 필터 또는 필터 적용 실패 |
| 4 | 필수 옵션 누락, 잘못된 숫자 등 인자 오류 |

필터 적용 주변에 작은 `try/catch`가 한 번 더 있는 이유도 보세요. 필터가 실패해도 걸린 시간을 저장하고 `throw;`로 같은 예외를 다시 전달하기 위해서입니다.

### 10-3. 로그는 처리 상황을 나중에 확인하기 위한 기록이다

`Logger::open()`은 로그 경로가 입력·출력 경로와 같은지 먼저 검사합니다. 로그를 BMP에 추가하거나, BMP 저장으로 로그를 덮어쓰는 일을 막기 위해서입니다. 파일은 `std::ios::app`으로 열어 기존 기록 뒤에 추가합니다.

`RunLog`에는 성공 여부, 전체 시간, 필터 시간, 실제 스레드 수, 메시지를 모읍니다. 모든 작업이 끝난 뒤 메인 스레드에서 한 줄을 기록하므로 이 프로그램 내부에서는 여러 작업 스레드가 로그를 동시에 쓰지 않습니다.

```text
status=success ... filter="threshold" threshold=128 ... actual_threads=4 filter_ms=... elapsed_ms=... message="Completed"
```

위 줄은 형식을 보여 주기 위해 일부를 생략한 예시입니다. `filter_ms`는 스레드 생성과 join을 포함한 필터 적용 시간이고, `elapsed_ms`는 파싱부터 저장 또는 오류 처리까지의 시간입니다. 마지막 로그 쓰기 시간은 제외합니다.

시간 측정에는 `std::chrono::steady_clock`을 사용합니다. 경과 시간을 재는 시계이므로 컴퓨터의 표시 시간이 바뀌는 것에 영향을 받지 않습니다. `std::chrono::duration<double, std::milli>`는 시간 차이를 소수점이 있는 밀리초로 표현합니다.

로그 관련 동작은 다음처럼 구분하면 됩니다.

| 실패한 시점 | 결과 |
| --- | --- |
| CLI 파싱 실패 | 콘솔에 오류를 출력한다. 로그 경로가 확정되지 않아 파일을 열지 않는다. |
| 로그 열기 실패 | 콘솔에 오류를 출력하고 이미지 처리를 시작하지 않는다. |
| 로그를 연 뒤 필터 생성·BMP 읽기·필터 적용·저장 실패 | 가능한 경우 실패 로그를 기록한다. |
| BMP 저장 후 로그 쓰기 실패 | 콘솔에 오류를 출력하고 실패 코드로 종료한다. 이미 저장한 BMP는 존재할 수 있다. |

필터 적용이 정상 반환하기 전에 실패하면 `actual_threads`는 초기값 0입니다. 실패 전에 일부 작업이 시작되었을 수 있으므로 이것을 “스레드를 전혀 만들지 않았다”는 증거로 읽으면 안 됩니다.

`flush()`는 스트림에 쌓인 출력을 내보내도록 요청합니다. 로그는 flush 후, BMP는 close 후에도 스트림 상태를 검사해 뒤늦게 드러나는 쓰기 실패를 확인합니다.

## 11. 직접 실행하고 디버깅하며 배우기

### 실습 1. 메인 함수에서 이미지가 생기는 순간 확인하기

Visual Studio에서 `ImageProcessor.sln`을 열고 `Debug`, `x64`를 선택한 뒤 빌드합니다. 디버깅에서는 최적화된 Release보다 Debug가 변수와 실행 순서를 관찰하기 쉽습니다.

프로젝트 속성의 디버깅 설정에서 작업 디렉터리를 `$(SolutionDir)`로 두고, 명령 인수에는 다음을 넣습니다. 여기에는 실행 파일 이름을 넣지 않습니다.

```text
--input .\Resource\1_astronaut.bmp --output .\Resource\study_debug.bmp --filter grayscale --threads 1 --log .\study.log
```

이 문서의 예제 출력 파일은 재실행하면 덮어씁니다. 입력 파일과 출력 파일은 예제처럼 다른 이름으로 둡니다.

`main.cpp`에서 아래 세 위치에 중단점을 걸어 보세요. 기본 키 설정에서는 F9가 중단점 설정/해제, F5가 실행/계속, F10이 한 줄 실행, F11이 함수 안으로 들어가기입니다.

| 멈출 위치 | 확인할 내용 |
| --- | --- |
| `options = ...parse(...)` 실행 직후 | `options.filterName`이 `grayscale`, `options.threadCount`가 1인가? |
| `loadFromFile()` 실행 직후 | `image.width()`와 `image.height()`가 512인가? 픽셀 데이터는 786432바이트인가? |
| `filter->apply(...)` 실행 직후 | `record.actualThreads`가 1인가? |

중단점은 표시된 줄을 **실행하기 전**에 멈춥니다. 반환값을 보고 싶다면 해당 줄에서 F10으로 실행한 뒤 확인하세요.

### 실습 2. 픽셀 하나가 회색이 되는 순간 확인하기

[GrayscaleFilter.cpp](ImageProcessor/GrayscaleFilter.cpp)에서 다음 줄에 중단점을 겁니다.

```cpp
const auto gray = luminance(pixel);
```

조사식 또는 지역 변수 창에서 `x`, `y`, `tile`을 확인하고 다음 값을 읽습니다.

```cpp
static_cast<int>(pixel[0]) // B
static_cast<int>(pixel[1]) // G
static_cast<int>(pixel[2]) // R
```

직접 가중합을 계산한 뒤 F10으로 진행해서 `gray`와 비교합니다. 다음 대입 줄까지 실행하면 세 채널이 같은 값이 됩니다. 이 두 줄을 이해하면 Grayscale의 핵심 연산을 이해한 것입니다.

반복문 안의 중단점은 계속 다시 걸리므로 픽셀 하나를 확인한 뒤 해제하세요. 이 단계에서는 `--threads 1`을 유지합니다.

### 실습 3. 이진화 임계값 바꾸기

명령 인수를 다음처럼 바꿔 실행합니다.

```text
--input .\Resource\4_text_page.bmp --output .\Resource\study_threshold.bmp --filter threshold --threshold 128 --threads 1 --log .\study.log
```

128을 64, 192로 바꿔 각각 다른 출력 이름으로 저장해 보세요. 실행 전에 “어느 결과에 흰 픽셀이 더 많을까?”를 예측한 뒤 이미지를 비교합니다. 소스 코드를 수정하지 않고 파라미터만 바꾸는 실습입니다.

### 실습 4. 스레드 수가 결과를 바꾸지 않는지 확인하기

동일한 입력과 필터에 대해 `--threads 1`, `--threads 4`로 각각 `study_one.bmp`, `study_four.bmp`를 만듭니다. 나머지 인자는 같아야 합니다. 프로젝트 루트의 PowerShell에서 파일 해시를 비교합니다.

```powershell
Get-FileHash .\Resource\study_one.bmp
Get-FileHash .\Resource\study_four.bmp
```

해시는 파일 내용을 바탕으로 계산한 값입니다. SHA-256 값이 같으면 실무적으로 파일 내용이 같은지 확인하는 데 사용할 수 있습니다. 현재 구현에서는 두 결과가 같아야 합니다. 병렬 처리는 계산을 나눠 실행할 뿐 필터의 수식을 바꾸지 않습니다.

속도를 비교할 때는 중단점을 사용하지 않는 Release 빌드로 실행하고 로그의 `filter_ms`를 봅니다. `elapsed_ms`에는 파일 읽기·쓰기 등의 시간도 들어 있습니다.

### 실습 5. 일부러 실패시키고 경로 따라가기

실습 3의 인수에서 한 번에 한 항목만 바꾸세요.

| 바꿀 내용 | 예상 결과 |
| --- | --- |
| `--threshold 256` | `ArgumentError`, 종료 코드 4. 필터 생성 전에 실패. |
| `--threshold 128abc` | `ArgumentError`, 종료 코드 4. 숫자 뒤 문자도 거부. |
| 입력 경로를 존재하지 않는 `missing.bmp`로 변경 | `BmpParseError`, 종료 코드 2. 로그를 정상적으로 열었다면 실패 로그 기록. |
| `--log`를 입력 BMP와 같은 경로로 변경 | 로그 경로 충돌, 종료 코드 1. 이미지 처리를 시작하지 않음. |

실행 파일을 PowerShell에서 직접 실행했다면 바로 다음 명령으로 종료 코드를 확인할 수 있습니다.

```powershell
$LASTEXITCODE
```

`main.cpp`의 각 `catch`에 중단점을 걸고 예상한 곳에 도착하는지 확인하면 예외 처리를 따라가기 쉽습니다.

## 12. 테스트 코드에서 무엇을 배울 수 있을까?

[ImageProcessorTests.cpp](Tests/ImageProcessorTests.cpp)는 작은 입력과 기대 결과를 만들어 구현을 확인합니다. 아래 함수부터 읽어 보세요.

| 함수 | 검증하는 내용 | 이 검증이 필요한 이유 |
| --- | --- | --- |
| `testPixels()` | 빨강·초록·파랑 등의 예상 밝기, 이진화 경계 | 두 실행 결과가 같아도 둘 다 계산이 틀릴 수 있으므로 정답과 비교한다. |
| `testParallel()` | 단일/병렬 일치, 작은 크기, 타일 경계, 작업 예외 | 작업 분배와 예외 전달이 올바른지 확인한다. |
| `testBmp()` | 직접 만든 BMP의 방향·패딩·잘못된 헤더 | 같은 파서로 쓰고 읽는 검사만으로 놓칠 수 있는 형식 오류를 확인한다. |
| `benchmark()` | 1스레드와 4스레드의 처리 시간 | 속도는 측정하고, 환경에 따라 달라지는 수치를 정답 조건으로 삼지 않는다. |

`CoverageFilter`는 테스트용 파생 클래스입니다. 방문한 픽셀 값을 1씩 올려서 모든 픽셀이 정확히 한 번 처리되었는지 검사합니다. 같은 픽셀을 두 번 회색으로 바꾸어도 결과가 같을 수 있기 때문에, Grayscale 출력 비교만으로는 중복 처리를 발견하기 어렵습니다.

`ThrowingFilter`는 일부러 예외를 던지는 테스트용 클래스입니다. CLI에 등록된 이미지 처리 기능은 아닙니다. 작업 스레드의 실패가 프로세스를 갑자기 종료시키지 않고 호출 측으로 돌아오는지 확인합니다.

전체 테스트는 프로젝트 루트에서 실행합니다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tests\run-tests.ps1
```

스크립트는 앱과 테스트를 빌드하고, C++ 검증과 실제 CLI 실행 검사를 수행합니다. 테스트 파일은 `build/tests/` 아래에 생성합니다. 출력의 `PASS`는 해당 검증을 통과했다는 뜻이고, `BENCH`는 시간 측정값입니다.

## 13. 읽다가 막힐 때 보는 C++ 표현 사전

| 표현 | 이 프로젝트에서 읽는 방법 |
| --- | --- |
| `namespace ip` / `ip::ImageBuffer` | 이름이 겹치지 않도록 `ip`라는 공간에 묶는다. `::`로 그 안의 이름을 가리킨다. |
| 이름 없는 `namespace { ... }` | 그 소스 파일 내부에서 사용할 함수·상수 등의 이름을 둔다. |
| `#include "파일.h"` | 필요한 선언을 현재 소스에서 사용할 수 있게 포함한다. |
| `#pragma once` | 같은 헤더를 한 번역 단위 안에서 중복 포함하지 않도록 한다. |
| `static constexpr int CHANNELS = 3` | 모든 객체가 공유하는 컴파일 시점 상수다. 픽셀당 채널 수를 한곳에서 정한다. |
| `static` 멤버 함수 | 객체를 만들지 않고 `BmpParser::loadFromFile()`처럼 호출할 수 있다. |
| `std::size_t` | 메모리 크기와 인덱스 등에 사용하는 부호 없는 정수 자료형이다. |
| `static_cast<자료형>(값)` | 변환하려는 자료형을 명시한다. 범위 검사를 자동으로 해 주지는 않는다. |
| `reinterpret_cast<char*>(주소)` | 파일 스트림에 메모리를 바이트로 읽어 넣을 주소를 전달할 때 사용한다. BMP 검증이나 바이트 순서 변환을 대신하지 않는다. |
| `auto* pixel` | 우변에서 자료형을 추론한 포인터 변수다. 픽셀 바이트의 주소를 가리킨다. |
| `const std::uint8_t* bgr` | 이 포인터를 통해 가리키는 채널 값을 수정하지 않는다. |
| `= default` | 해당 생성자·대입 연산자·소멸자의 기본 구현을 컴파일러에 요청한다. |
| `= delete` | 해당 함수의 사용을 금지한다. `ThreadJoiner`는 복사를 막아 정리 책임이 복제되지 않게 한다. |
| `noexcept` | 함수 밖으로 예외를 내보내지 않는다는 선언이다. 예외가 빠져나가면 종료되므로 단순히 오류를 무시한다는 뜻이 아니다. |
| `inline` | 이 문맥에서는 헤더의 같은 함수 정의가 여러 번역 단위에 포함될 수 있게 한다. 호출 지점에 코드를 펼치는 최적화를 보장하지 않는다. |
| `throw;` | 현재 처리 중인 예외를 그대로 다시 던진다. |
| `catch (...)` | 예외의 구체적인 자료형과 관계없이 잡는다. |

## 14. 설명할 수 있는지 마지막으로 점검하기

다음 질문에 소스를 보며 자기 말로 답해 보세요. 막히는 질문이 있으면 표시한 장으로 돌아가면 됩니다.

| 질문 | 답에 들어가야 할 핵심 | 다시 볼 곳 |
| --- | --- | --- |
| 이미지 한 픽셀은 메모리에서 어떻게 표현되는가? | BGR 세 채널, 각 1바이트, 좌표를 1차원 위치로 계산 | 4장 |
| Grayscale에서 초록이 왜 149가 되는가? | 가중합과 정수 나눗셈의 버림 | 5장 |
| 임계값과 밝기가 같으면 어떻게 되는가? | `>=` 비교이므로 흰색 | 6장 |
| `apply()`와 `applyTile()`의 책임은 무엇인가? | 공통 실행과 개별 픽셀 연산 | 7장 |
| 왜 `ImageBuffer&`로 받는가? | 큰 이미지 복사를 피하고 원본 픽셀을 수정 | 7장 |
| 왜 가상 소멸자가 있는가? | 기반 타입으로 소유해도 파생 객체를 올바르게 해제 | 7장 |
| 스레드 4개를 요청하면 무엇이 생성되는가? | 타일이 충분하면 새 스레드 3개와 호출 스레드 1개가 작업 | 8장 |
| 작업 스레드 예외는 어떻게 돌아오는가? | 작업 안에서 보관, 모두 join, 호출 측에서 재전달 | 8장 |
| 파일 행 크기와 메모리 행 크기는 왜 다를 수 있는가? | BMP의 4바이트 행 패딩 | 9장 |
| 숫자 파싱에서 문자열 끝까지 확인하는 이유는? | `128abc` 같은 부분 변환을 거부 | 10장 |
| 성공 로그를 남기지 못했는데 BMP는 있을 수 있는가? | BMP 저장 후 로그 쓰기가 실패할 수 있음 | 10장 |
| 단일/병렬 결과가 같으면 알고리즘 정답까지 보장되는가? | 둘 다 틀릴 수 있어 알려진 픽셀 정답 검사가 따로 필요 | 12장 |

마지막 연습으로 `main.cpp`를 열고 실행 흐름을 소리 내어 설명해 보세요. “이 줄은 어디에서 데이터를 받고, 무엇을 바꾸며, 실패하면 어디로 가는가?”를 각 단계에 적용하면 함수 이름을 외우는 데서 실제 동작을 이해하는 단계로 넘어갈 수 있습니다.
