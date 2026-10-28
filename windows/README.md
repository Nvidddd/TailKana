# TailKana Windows 开发版

Windows 版使用 Mozc 的原生 TSF 输入法框架、客户端／转换服务、COM 注册和 MSI 安装流程，接入 TailKana 的可移植会话层和不抢焦点的 Win32 候选窗。上游版本及 TailKana 专用输入源、语言配置、升级 GUID 固定在 `upstream.json`，后续更新继续沿用。

目标为 Windows 10 1809 或更高版本、Windows 11 x64；MSI 同时包含 32 位应用兼容用的 TSF 模块。Windows ARM64 尚未构建验证。

## 首版范围

- 罗马字转实时假名，空格进入汉字变换并循环候选，Enter 确定。
- 默认三个短候选，汉字附带假名，数字键或鼠标提交对应项。候选变长时缩字，仍放不下时自动展开成逐行列表，缩短后恢复；Tab 手动展开保留。
- 右 Shift＋方向键或鼠标十字轮盘修改最后音节；`ti→tyo` 后重新生成轮盘。
- Shift＋Tab 在日文与英文间切换，左 Shift 暂时切换平／片假名；按钮可切换罗马字／假名显示和空格／Enter 对调。
- 复用 macOS 的有界键盘纠错和模糊拼音规则，以 E／B 标记；每键最多四次纠错查询、两条新增结果。
- 墨绿浅色／深色／跟随系统、框宽和基本选项在候选窗齿轮菜单调整，保存在当前用户的 `HKCU\Software\TailKana\TailKana\Panel`。
- 普通续打按增量发送一键；重建或纠错时仅为最终输入生成预测，跳过中间前缀。客户端会话复用，避免反复创建纠错会话。

当前是开发移植，**不能把共享核心测试通过当成 Windows 系统输入已经验证**。Windows DLL 编译、MSI 安装／卸载、TSF 注册及真实应用输入仍须逐项验收，进度以 GitHub Actions 的实际结果为准。

简体中文释义／中文第三候选、Zenz AI、完整设置窗口、自定义颜色与 Windows 原生毛玻璃尚未移植。本版不宣称与 macOS 功能完全一致。

## 在 Windows 构建

使用 Visual Studio 2022（C++ x64／x86、Windows 11 SDK、ATL）、Python 3.12+、CMake、Bazelisk、.NET。工具链要求随固定 Mozc 版本，参考上游的 Windows 构建说明。所有操作从项目根目录执行：

```powershell
./windows/scripts/build.ps1
./windows/scripts/verify.ps1
```

构建脚本会获取固定版本 Mozc、应用可重复的集成修改、执行可移植核心和原生窗口测试，随后构建 TSF DLL、Qt 配置工具和 MSI。`verify.ps1` 读取 MSI，检查名称、版本及固定升级身份。

只在可丢弃的 Windows 测试机运行安装验证：

```powershell
./windows/scripts/verify.ps1 -InstallTest
```

此选项安装 MSI，查询 TSF 语言配置并加载 COM 输入法模块，最后卸载。它不代替记事本、浏览器、Office 等应用中的逐键输入与鼠标交互验证。

构建产物位于 `outputs/windows/TailKana-0.5.0-alpha.1-windows-x64.msi`，SHA256 在同目录。GitHub Actions 成功后上传 Windows 安装器 artifact；它尚未经过 Windows 代码签名。

## 安装与试用

安装开发 MSI 后，在 Windows 的语言／键盘设置中选择日语的 TailKana 输入源（Win＋空格可切换）。有些已打开的程序会继续加载旧输入模块，需要关闭重开；系统要求时注销后重新登录。输入源是否出现以及普通程序输入效果，须在真实 Windows 上确认。

卸载从「设置 → 应用 → 已安装的应用 → TailKana」进行。用户学习数据在 `%LOCALAPPDATA%\TailKana`；卸载程序不主动删除个人数据。

## 核心测试与目录

不依赖 Windows SDK 的部分也能在 macOS／Linux 验证：

```sh
cmake -S windows -B work/windows/core-build -DCMAKE_BUILD_TYPE=Debug
cmake --build work/windows/core-build
ctest --test-dir work/windows/core-build --output-on-failure
```

`core/` 管理输入、候选映射、模式、展开与末尾修改；共享的罗马字及纠错实现继续来自 `src/`。`native/` 实现 Win32 窗口和 Mozc TSF 适配；`scripts/prepare.py` 仅修改独立 Windows 工作副本，固定版本不匹配时拒绝打补丁。

许可证为 GPL-3.0-or-later；沿用并保留 Mozc BSD 和 azooKey MIT 声明。安装包的上游 credits 追加 TailKana 许可证、完整源代码地址与规则来源。
