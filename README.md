# TailKana

面向中文用户的 macOS 日文输入法，基于 Fcitx5 macOS 和 Mozc，支持末尾音节纠错、简体中文释义、拼音辅助及本地 AI 上下文推荐。

## 下载与安装

从 [Releases](https://github.com/Nvidddd/TailKana/releases/latest) 下载 **TailKana-0.4.3-arm64.pkg**。目前提供 Apple Silicon 版本，要求 **macOS 15 或更高版本**。

1. 打开安装包，完成系统安装。
2. 打开「应用程序」里的 **TailKana**，点击 **启用并切换**。
3. 在备忘录等应用切换到 TailKana 输入。如果输入源尚未出现，注销并重新登录。

更新后可从右上角输入法菜单选择「重新启动输入法」。系统只安装一个应用入口；后台输入法组件位于 `/Library/Input Methods/TailKanaInput.app`。

应用目前使用临时签名，安装包未经过 Apple 开发者签名和公证，macOS 可能要求在「系统设置 → 隐私与安全性」中确认打开。

## 功能

- **实时假名和汉字候选**：预编辑直接显示在输入处；紧凑面板默认三格，汉字附带假名。文字变长时自动缩小字号，放不下时展开为一行一条，变短后恢复。
- **末尾十字轮盘**：只替换最后一个音节。例如 `ti` 可改为 `tyo`；通过鼠标或右 Shift＋方向键操作。
- **纠错与模糊拼音**：提供键盘误按纠错，以及 `xi→shi`、`qi→chi`、`ci→tsu` 等中文拼音习惯辅助。各组规则可在设置中独立开启；候选数字下的 E、B 分别表示纠错、模糊拼音。
- **简体中文辅助**：离线日中词典显示当前候选释义；可将右侧轮盘切换为释义区。智能中文开启后，同一串字母也按拼音检索，第三格显示中文候选，标记 C。
- **离线 AI 上下文推荐**：可选 Zenz v3 small 量化模型，按前文推荐日语转换，独立显示 AI 推荐，不改变数字候选顺序。支持调整最长预测长度、参考上下文长度。推理在本地运行，无需 API 密钥；开启后模型常驻，关闭后退出。
- **自定义外观**：跟随系统／浅色／深色、原生透明毛玻璃开关、自定义背景／文字／高亮色和底色浓度；设置窗口与候选面板联动。
- **输入偏好**：默认日文／英文、全角／半角／自动标点、平／片假名、360–1000px 框宽、面板缩放及空格／Enter 对调。

释义是词条释义，不是整句翻译。中文描述找日语的语义检索仍在独立试验目录 `experimental/ai`，没有加入系统输入法。

## 常用操作

| 操作 | 默认行为 |
| --- | --- |
| 空格 | 开始变换，继续按键循环候选 |
| Enter | 确定当前输入 |
| 1–3／鼠标点击 | 提交对应候选 |
| Tab／面板加号 | 展开或收起候选列表 |
| Esc | 先收起列表，再取消预编辑 |
| Shift＋Tab | 日文／英文切换 |
| 右 Shift＋方向键 | 修改末尾音节 |
| 按住左 Shift | 临时切换平／片假名（可设置） |
| Option＋Enter | 提交当前 AI 推荐 |

面板齿轮、输入菜单「TailKana 设置…」和启动器「打开设置」均进入同一设置窗口。设置自动保存。AI 与输入文字无需联网；主动点击释义来源时会打开对应词典网页。

## 0.4.3 性能修复

移除每次重放拼写时生成、随后又被丢弃的中间预测，最终输入仍正常产生候选。本机开启纠错、中文候选和释义的逐键测试：17–32 个罗马字的处理中位数 **251→28ms**，33–46 个 **746→42ms**，最慢一键 **1020→76ms**。这些是原生按键处理耗时，不包含宿主应用、面板绘制或 AI 评分，实际性能随机器及词句变化。

## 从源码构建

当前构建目标为 Apple Silicon，使用 Xcode、CMake、Ninja、extra-cmake-modules、gettext、librsvg、Node.js、Python 3、zstd。上游构建环境要求见 [fcitx5-macos](https://github.com/fcitx/fcitx5-macos)；当前版本使用 Xcode 26.6 构建。

```sh
./scripts/bootstrap.sh
./scripts/build.sh
python3 scripts/package.py
python3 scripts/installer.py
python3 scripts/verify-installer.py
```

`bootstrap.sh` 获取固定版本的上游代码、词典、Zenz 模型和 azooKey/llama.cpp；正式版本不需要 Python 推理环境、E5 模型或浏览器服务。模型版本和校验值见 `experimental/ai/model-lock.json`，上游版本见 `upstream-lock.json`。生成的模型、词库、构建目录和安装包不存入 Git。

面板测试：`./scripts/test-panel.sh`。构建脚本运行六组原生回归；更多记录见 [TESTING.md](TESTING.md)。试打需先构建、打包，然后运行 `scripts/Preview-TailKana.command`；它不会注册系统输入源。校验安装包后会清理开发应用副本，重新试打前需再次构建、打包。

核心实现位于 `src/`，候选面板位于 `panel/`，构建与打包逻辑位于 `scripts/`。开发应用只在 `.noindex` 目录生成；不要把临时 `.app` 复制到其他目录，以免产生重复应用入口。

## 移除与个人数据

先切回其他输入法，在系统设置 → 键盘 → 输入源中移除 TailKana，再移除 `/Applications/TailKana.app` 和 `/Library/Input Methods/TailKanaInput.app`，注销并重新登录。

设置、个人学习数据位于 `~/Library/Application Support/TailKana`，移除程序不会主动删除这些数据。

## 许可证

TailKana 使用 **GPL-3.0-or-later**。Fcitx5、Mozc、LibIME、azooKey、词典及模型各自的许可证和来源见 [THIRD_PARTY.md](THIRD_PARTY.md)。Zenz 量化模型使用 CC-BY-SA-4.0，日中释义来自中文维基词典，构建和安装包保留相关声明。
