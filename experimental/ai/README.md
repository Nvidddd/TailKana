# TailKana AI 独立试验

已在当前这台 Apple M1 Pro Mac 上准备好资源。双击本目录的 **启动AI试验.command**，浏览器会打开试打页。它会复用仍在运行的试验服务。关闭试验用页面底部的「结束试验」；「释放模型」仅退出模型进程，随后仍可继续查询。两分钟未使用会自动释放模型。

这是验证两项功能效果的独立入口。**日语上下文推荐已在 0.4.x 接入系统输入法；中文语义检索仍只在此试验入口提供。**不安装应用，不注册输入源，不改动当前 TailKana 或配置。系统版和保留的 `outputs/TailKana-0.3.9-arm64.pkg` 继续使用；稳定源码快照位于 `work/stable-0.3.9/source.tar.gz`。

## 两项功能

- **上下文候选**：输入或修改前面的日语文字与当前罗马字，原有 Mozc 后端实时生成最多 12 项完整候选，Zenz v3 small 评分。左右并列显示原顺序与 AI 推荐，点击写入本页试打区。取最后 40 字前文，只评估已有候选，不生成词库外文字。试验 Mozc 使用独立配置，不提交、学习或改动正式词频。
- **中文找日语**：输入中文词语或描述，multilingual E5 small 的 ONNX 量化模型在现有 112,173 个词条中检索，显示最多 24 个不同词及词典已有的读音和简体释义。短中文词额外结合词典精确匹配。长描述可能匹配不准、漏掉想要的词，某些词条缺读音；这不是句子翻译，也不是已验证的高准确率功能。

完全离线推理。仅监听随机端口的 127.0.0.1，服务访问令牌放在本机私有会话文件中；拒绝跨来源的推理请求，不记录输入内容。只有显式运行 setup 时下载公开依赖和模型。普通打字不触发这两项试验。

## 实时模式

上下文候选现在随罗马字或前文变化自动更新，不必点击；页面直接显示“输入→显示”的延迟。每次输入合并 16ms，只保留一份在途请求和最新输入，过期结果不会替换候选。未完成的辅音组合仅显示 Mozc 结果，等待假名完整再运行神经评分。原候选与 AI 推荐仍分别显示。

原生 Zenz 评分器对 12 个候选共享前文 KV，按每组四条批量评分；实际对照中分数误差小于 0.003，测试词首选不变。转换试打重建拼写时关闭中间建议，最后仍通过正常 Space 转换生成完整候选；正式引擎默认参数保持原行为。

“测实时性”按钮用固定样例测试首次加载、100/50ms 间隔输入、长句、删除重输及只改前文，结束后恢复原试打内容。测量从页面 input 事件到两次 requestAnimationFrame 的绘制机会，包含输入合并、本地 HTTP、Mozc、Zenz、DOM；不包含硬件键盘、系统 IMK 或其他宿主应用延迟。测试进行中手动输入会中止测试。

M1 Pro 的最终两轮实测（共 30 条有效热运行展示样本）：中位数 **73ms**，P95 **155ms**。

| 场景 | 停键到显示 |
| --- | --- |
| 正常打字（100ms/字母） | 66–73ms |
| 快速打字（50ms/字母） | 70–76ms |
| 长句 | 95–113ms |
| 长前文＋长句，快速输入 | 155–170ms |
| 删除／重输、回到已计算过的上下文（有缓存） | 35–39ms |
| 模型进程重新加载（已有系统／Metal 缓存） | 889–902ms |

第一轮 40ms 输入合并试验的加载曾为 1.8 秒；真正首次 Metal 编译仍可能更慢。上述数值是本机固定样例的小样本，不能作为其他宿主或设备的延迟保证。实时版日语评分子进程 RSS 约 **202MiB**，Mozc 约 40MiB，服务约 25MiB；批处理用约 80MiB 额外内存换取更低延迟。中文检索继续按需运行，与之前的内存限制相同。

原始数据：`work/ai-experiment/realtime-summary.json`、`realtime-browser-16ms-run1.json`、`realtime-browser-16ms-run2.json`、`realtime-native-parity.json`、`realtime-memory.json`。

## 早期手动模式测量（实时优化前）

固定版本与模型 SHA256 见 `model-lock.json`；完整测试数据见 `work/ai-experiment/benchmark.json`。

- 12 项日语评分的热运行中位数 111 ms；本次冷启动 879 ms。首次 Metal 着色器编译曾用约 14 秒，不能当作总是即时加载。
- 112,173 词中文检索的热运行中位数约 86 ms；本次首次查询含模型加载约 656 ms。
- 两项均加载时，各进程 RSS：服务约 25 MiB、Zenz 125 MiB、Mozc 37 MiB、中文检索 728 MiB，合计约 0.89 GiB。RSS 是本机采样，不等于模型文件大小或系统统一内存压力；这版中文检索仍偏重。
- 释放模型后，三个模型／词库子进程退出，服务 RSS 约 25 MiB；结束试验后服务也退出。

评分例：吃饭的前文 + hashi → 箸，过河的前文 + hashi → 橋，宇宙船が地球に + kikan → 帰還。中文“下雨时用来遮雨的东西”检索前两项为雨具／傘。测试样例不代表整体准确率。

## 重建与测试

清理试验缓存后或换机器时，需要先完成主项目 bootstrap，然后执行：

```sh
experimental/ai/setup.sh
work/ai-experiment/venv/bin/python experimental/ai/test_ai.py
node experimental/ai/test_realtime.cjs
# 服务运行后：
work/ai-experiment/venv/bin/python experimental/ai/test_http.py
work/ai-experiment/venv/bin/python experimental/ai/benchmark.py
```

脚本依赖 uv、CMake、Ninja、编译器及主项目已有工具链。Python 环境和模型位于 `work/ai-experiment/`，没有安装到系统 Python。原生评分器编译使用 azooKey/llama.cpp 固定提交 `88b97a47dc7f5892e2d5a6856fbe9cfe237f9e5c`，支持 Zenz 的专用字符分词器。GGUF 内 EOS 元数据与词表标签不一致，评分器按词表的 `</s>` 解析结束标记。原生进程与 Python 服务隔离，异常或超时只影响试验页。

## 来源与许可

- [Zenz v3 small GGUF](https://huggingface.co/Miwa-Keita/zenz-v3-small-gguf)，Miwa-Keita，CC-BY-SA-4.0；量化原文件未经修改。提示词格式参照 [azooKey 转换器](https://github.com/azooKey/AzooKeyKanaKanjiConverter/tree/d59a28e4c7ca049aef04f29a91eae9677a7753f2)，未复制 Swift 评分代码。
- [multilingual E5 small](https://huggingface.co/intfloat/multilingual-e5-small)，intfloat，MIT；使用 [Xenova ONNX 导出](https://huggingface.co/Xenova/multilingual-e5-small)。按官方 query:/passage: 前缀、掩码平均池化及 L2 归一化；试验截断到 96 tokens。
- [azooKey/llama.cpp](https://github.com/azooKey/llama.cpp/tree/88b97a47dc7f5892e2d5a6856fbe9cfe237f9e5c)，MIT；完整许可证保存在 licenses/llama.cpp-LICENSE。ONNX Runtime、Hugging Face Tokenizers 为 Apache-2.0，NumPy 为 BSD-3-Clause，环境内保留各依赖许可证。
- 检索索引由原简体中文词典派生，条目、释义及向量索引继续按 CC-BY-SA-4.0 提供；署名、词典原始来源、转换说明和许可见 assets/language/Wiktionary-NOTICE.txt 及 glossary-source.json。可按词条名在中文维基词典核对来源。
