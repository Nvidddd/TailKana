# Upstream components

- Fcitx5 macOS: https://github.com/fcitx/fcitx5-macos (GPL-3.0-or-later)
- Fcitx5 core: https://github.com/fcitx/fcitx5 (LGPL-2.1-or-later)
- Fcitx5 webview: https://github.com/fcitx-contrib/fcitx5-webview (GPL-3.0-or-later)
- Fcitx5 Mozc CMake port: https://github.com/fcitx-contrib/fcitx5-mozc
- Google Mozc: https://github.com/google/mozc (BSD-3-Clause; retain original headers and acknowledgments)
- `src/roman_table.inc` is generated from Mozc's `src/data/preedit/romanji-hiragana.tsv`.

Builds retain upstream copyright files and third-party components. Source and local changes are available alongside the built application. No AI API is used and no cloud prediction is enabled.

Mozc's complete BSD license is retained in `third_party/Mozc-LICENSE`.

- The registration and parent-enable helpers in `third_party/fcitx5-macos-installer/` are copied from [fcitx-contrib/fcitx5-macos-installer](https://github.com/fcitx-contrib/fcitx5-macos-installer), commit `a825d02734dac3a9abde874f9dd5164abe63c65d`, under GPL-3.0. Only the product bundle ID/path are substituted. Original license and provenance are retained there. Squirrel commit `769ae426f33f500266101a88f694aab0d158e680` is a lifecycle/metadata reference, not a runtime dependency.

- LibIME pinyin engine: https://github.com/fcitx/libime, pinned in upstream-lock.json (LGPL-2.1-or-later). KenLM and its double-conversion component retain their license notices under assets/language/ and inside the input-method bundle.
- Chinese pinyin dictionary/model: fixed upstream LibIME data archives (dict-20260907 / lm_sc.arpa-20260629), hashes and runtime files recorded in assets/language/pinyin-source.json. Compiled with upstream tools into pinyin.dict / pinyin.lm; loaded lazily when SmartChinese is enabled.
- Japanese→Chinese definitions: Chinese Wiktionary contributors, extracted by Wiktextract/Kaikki.org. The derived offline index is under CC-BY-SA-4.0. Word-level attribution is available through the candidate panel; source, dump hash, modifications and license are in assets/language/glossary-source.json and Wiktionary-NOTICE.txt. No examples or media are copied into the index.

- OpenCC t2s data and opencc-python-reimplemented 0.1.7 (Apache-2.0): https://github.com/BYVoid/OpenCC and https://github.com/yichen0831/opencc-python. Used only while building to convert Chinese meanings to Simplified Chinese; the installed input method has no OpenCC runtime dependency. The wheel hash is pinned in upstream-lock.json; license and attribution accompany the derived dictionary in assets/language/OpenCC-NOTICE.txt.

- azooKey classic romaji typo correction: [AzooKeyKanaKanjiConverter](https://github.com/azooKey/AzooKeyKanaKanjiConverter), commit `d59a28e4c7ca049aef04f29a91eae9677a7753f2`, MIT, copyright 2023 Miwa / Ensan. The ten roman2KanaPossibleTypo pairs and edit penalty approach from `DictionaryManagement/TypoCorrection.swift` are adapted to bounded C++ search in `src/roman_correction.cpp`. Full original file, MIT notice and provenance are kept in `third_party/azookey-typo` and in packaged TailKana-Licenses/AzooKey-Typo. The full Swift converter and its dictionaries are not bundled. Zenz is packaged separately starting with TailKana 0.4.0, under the model license below.

- Native AI in TailKana 0.4.0 and later, plus the isolated trial: Zenz v3 small GGUF (Miwa-Keita, CC-BY-SA-4.0), multilingual-e5-small (intfloat, MIT; Xenova ONNX export), azooKey/llama.cpp (MIT, commit 88b97a47dc7f5892e2d5a6856fbe9cfe237f9e5c). Model revisions/hashes and all attributions are recorded in experimental/ai/model-lock.json and experimental/ai/README.md. The 0.4.x installer bundles Zenz and the native llama.cpp runtime; it does not bundle E5, Python, or the browser server. The retained 0.3.9 installer contains no neural models.
