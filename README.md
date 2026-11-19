## 인코딩 관련 설정
- **`.editorconfig`** — Visual Studio가 텍스트 파일을 UTF-8(BOM 없음)·CRLF로 저장하게 했다. 셸 스크립트와 Git 훅만 LF.
- **`.gitattributes`** — 저장소 안에서는 줄바꿈을 LF로 통일하고 체크아웃할 때 CRLF로 바꾸게 했다. `.sh`·훅은 LF, `.bat`은 CRLF, `.uasset`·`.umap`은 Git LFS로 저장(줄바꿈 변환 제외함).
- **`.githooks/pre-commit`** — 커밋하는 `.h`·`.cpp`·`.cs`·`.md`·`.csv`·`.bat` 파일이 UTF-8이 아니면 커밋을 거부한다.
