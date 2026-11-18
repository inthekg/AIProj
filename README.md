## 인코딩 관련 설정
- **`.editorconfig`** — Visual Studio가 텍스트 파일을 UTF-8(BOM 없음)·CRLF로 저장하게 했다. 셸 스크립트와 Git 훅만 LF.
  -> VS가 한글 주석 파일을 CP949로 저장하면, 소스를 UTF-8로 컴파일하는 언리얼 빌드에서 한글이 깨지기 때문.
- **`.gitattributes`** — 저장소 안에서는 줄바꿈을 LF로 통일하고 체크아웃할 때 CRLF로 바꾸게 했다. `.sh`·훅은 LF, `.bat`은 CRLF, `.uasset`·`.umap`은 바이너리로 지정.
  -> 여러 도구(AI·VS·스크립트)가 줄바꿈을 제각각 쓰면 diff가 오염되고 스크립트가 오동작하기 때문.
- **`.githooks/pre-commit`** — 커밋하는 `.h`·`.cpp`·`.cs`·`.md`·`.csv`·`.bat` 파일이 UTF-8이 아니면 커밋을 거부한다.
  -> Git과 GitHub는 인코딩을 검사하지 않으므로, 어느 도구에서 만든 파일이든 커밋 직전 한 곳에서 걸러 내기 위해.
