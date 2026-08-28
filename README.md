# LumaComp — 多段压缩 + 谐波亮化插件（VST2 / VST3）

一个面向 **REAPER** 及任何 VST 宿主的高质量动态处理插件，对标 **FabFilter Pro-C / Pro-MB** 的
核心功能设计：**宽频/三段可切换压缩、逐段侧链高低频避让、任意频段独立检测压缩、谐波饱和染色提升高频亮感**，
并内置频谱分析、逐段增益衰减（GR）表与电平表。

- 插件格式：**VST3**（`LumaComp.vst3`）与 **VST2**（`LumaComp.dll`），均为 Windows x64
- 默认 **零延迟**（Lookahead 可开 0–10 ms，开启后宿主自动做延迟补偿 PDC）
- 音频线程零堆分配、零锁、零异常 —— 内存占用小且稳定，REAPER 兼容性优先

---

## 一、快速安装（REAPER）

1. 把 **整个 `LumaComp.vst3` 文件夹** 复制到：
   - `C:\Program Files\Common Files\VST3\`（系统级，所有用户可见）
   - 或 `C:\Users\<你的用户名>\AppData\Roaming\REAPER\UserPlugins\VST3\`（仅当前用户，推荐）
2. 把 **`LumaComp.dll`**（VST2）复制到 REAPER 的 VST 插件目录，例如：
   - `C:\Program Files\VSTPlugins\` 或你自定义的 VST 扫描路径
3. 打开 REAPER：**选项 → 首选项 → 插件 → 重新扫描**（或 动作列表 → `重新扫描插件`）。
   - 若 VST2 未出现，确认 REAPER 的 VST 插件路径设置包含上述目录，然后重新扫描。
4. 在 FX 浏览器搜索 **LumaComp**，加载即可。

> 提示：VST2 与 VST3 可同时安装；REAPER 默认优先显示 VST3。两者参数完全一致，可随时切换。

---

## 二、功能与界面

界面分为四区（深色图形化风格）：

```
┌──────────────────────────────────────────────────────────────┐
│ LumaComp                   [WIDE|SPLIT]          [BYPASS]     │
├──────────────────────────────────────────────────────────────┤
│ 频谱分析面板：输入(灰) / 输出(青) 频谱、三段色块、交叉点虚线   │
├─────────────┬───────────────┬───────────────────────────────┤
│ LOW 段       │ MID 段         │ HIGH 段                       │
│ THRESH RATIO │  (同左)        │  (同左)                       │
│ ATTACK REL   │                │                               │
│ MAKEUP 等    │                │                               │
│ [GR 横向刻度表]│ [GR 刻度表]   │ [GR 刻度表]                   │
├─────────────┴───────────────┴───────────────────────────────┤
│ SATURATION:  [SOFT][TUBE][BRIGHT]  DRIVE TONE-F TONE-G SATMIX │
│ OUTPUT:      INPUT MIX LOOKAHEAD OUTPUT                       │
└──────────────────────────────────────────────────────────────┘
```

### 1. 压缩模块 —— 双模式（对标 Pro-C / Pro-MB）

- **WIDE（宽频）模式**：单条压缩链路处理全频段信号（类似 Pro-C），使用中间一列（MID）的旋钮。
- **SPLIT（分段）模式**：Linkwitz-Riley 4 阶分频，交叉频率 X-Over 1 / X-Over 2 可调，
  低 / 中 / 高三段各自独立压缩（类似 Pro-MB）。三段在频谱面板中以不同颜色显示。

每一段（或宽频压缩）提供：
| 旋钮 | 含义 | 范围 |
|---|---|---|
| THRESH | 阈值 | -60 … 0 dB |
| RATIO | 压缩比 | 1:1 … 20:1 |
| ATTACK | 启动时间 | 0.1 … 300 ms |
| RELEASE | 释放时间 | 10 … 3000 ms |
| MAKEUP | 补偿增益 | 0 … +24 dB |
| SC HP | 侧链高通（低频避让） | 20 … 1000 Hz |
| SC LP | 侧链低通（高频避让） | 500 … 20000 Hz |
| SOLO / MUTE / BYP | 独听 / 静音 / 该段旁通 | — |

- **高低频避让**：SC HP / SC LP 是加在**检测通路**上的滤波器——例如把 SC HP 调到 200 Hz，
  压缩器就不会被低频触发（鼓的冲击不会压掉人声亮度）；把 SC LP 拉低则让压缩只关注低频能量。
- **分段侧链压缩**：三段各有独立的检测器与滤波器，即“任意频段分段侧链压缩”：
  想让中频段只按 1k–8k 的能量压缩，把该段 SC HP=1000、SC LP=8000 即可。
- 压缩器采用 **软拐点（6 dB knee）+ RMS 检测 + 指数包络**，听感平滑；GR 表实时显示各段压了多少。

### 2. 谐波染色模块（提升高频亮感）

| 旋钮/按钮 | 含义 | 范围 |
|---|---|---|
| DRIVE | 饱和驱动量 | 0 … 24 dB |
| **SOFT / TUBE / BRIGHT** | 三种染色类型，独立按钮单选 | — |
| TONE FREQ | 高频亮化架频点 | 1k … 20 kHz |
| TONE GAIN | 高频架增益（空气感/亮感） | -12 … +12 dB |
| SAT MIX | 干湿混合 | 0 … 100% |

- **SOFT**：纯 tanh 软饱和，干净温和。
- **TUBE**：非对称曲线产生偶次谐波（温暖、电子管质感），内置直流滤除。
- **BRIGHT**：三次谐波增量（更亮、更有“空气”的泛音）。
- 饱和后接 **Tone 高频架**（高架滤波），专门用来提亮高频；SAT MIX 做并行混合，
  DRIVE=0 时表现为纯高频 EQ，可当作“空气增强器”使用。

### 3. 全局控制

- INPUT / OUTPUT：输入/输出增益（±24 dB）
- MIX：整条链路的干湿比例（并行压缩）
- LOOKAHEAD：0–10 ms 前视（开启后插件报告延迟，宿主自动补偿；默认 0 = 零延迟）
- BYPASS：全局旁通（跳过全部处理，绝对干声）

### 4. 图形显示

- **频谱分析**：输入（灰色）与输出（青色）实时频谱；SPLIT 模式下三段以
  橙色 / 青色 / 紫色色块显示，交叉频率处有白色虚线标记。
- **GR 横向刻度表**：每段面板底部一条横向增益衰减表，0 dB 在左、24 dB 在右，
  带刻度线与数值标签；压缩量越大填充越长，颜色由青 → 橙 → 红渐变，并带阻尼平滑。

---

## 三、构建方法（从源码重新编译）

```powershell
# 依赖（已随项目提供于 third_party/）：
#   JUCE 6.1.6、VST2 SDK 头文件、VST3 SDK 3.7.8、CMake 3.31、MinGW-w64 GCC 16.2
# 构建命令：
$env:PATH = "E:\AI\VST\third_party\mingw\mingw64\bin;" + $env:PATH
& "E:\AI\VST\third_party\cmake-3.31.12-windows-x86_64\bin\cmake.exe" `
    -S E:\AI\VST\LumaComp -B E:\AI\VST\LumaComp\build `
    -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ `
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY `
    -DCMAKE_MAKE_PROGRAM="E:\AI\VST\third_party\mingw\mingw64\bin\mingw32-make.exe" `
    "-DCMAKE_CXX_FLAGS=-include utility -include cstdint -include algorithm -include memory -include limits -include cstring -include type_traits -include functional -include iterator -include cstdlib -include cstdio -include vector -include map -include string"
Push-Location E:\AI\VST\LumaComp\build
mingw32-make -j8 LumaComp_VST3 LumaComp_VST
Pop-Location
# 产物：
#   build/LumaComp_artefacts/Release/VST3/LumaComp.vst3
#   build/LumaComp_artefacts/Release/VST/libLumaComp.dll
```

> 说明：`CMAKE_CXX_FLAGS` 里的 `-include <标准头>` 是为 GCC 16 编译老版 JUCE 6.1.6 所做的兼容处理；
> 若换用 GCC 14 等更老工具链可去掉。JUCE 的 `VST2_SDK` 头文件为兼容层（供 VST2 目标使用）。

---

## 四、REAPER 兼容性与内存优化说明

- **零分配实时安全**：`processBlock` 中无 `new`/`malloc`、无锁、无异常，所有缓冲在
  `prepareToPlay` 阶段预分配，采样率变化时安全重建滤波器。
- **内存占用**：固定大小状态 + 两个 2048 点 FFT 分析缓冲（UI 线程计算），内存占用约几 MB。
- **延迟**：LOOKAHEAD=0 时零延迟；开启后 `setLatencySamples` 上报，REAPER 自动 PDC。
- **声道**：单声道 / 立体声自动适配；立体声检测采用 L/R 联动（mono-link）。
- **Denormal 防护**：使用 `ScopedNoDenormals`，避免低电平下 CPU 飙升。
- **旁通**：支持插件自带 BYPASS 与宿主旁通，均为绝对干声直通。
- **工程/预设**：参数随 REAPER 工程自动保存（VST3 状态持久化）。

---

## 五、许可说明

- 本插件源码位于 `Source/`，可自由修改用于个人用途。
- 使用到的第三方组件：
  - **JUCE 6.1.6**（GPLv3 / 商业授权）— 仅用作插件框架，见 `third_party/JUCE-6.1.6/LICENSE.md`
  - **VST3 SDK 3.7.8**（Steinberg 3-Clause BSD）— 见 `third_party/vst3sdk/.../LICENSE.txt`
  - **VST2 SDK 头文件** — 兼容层（aeffect.h / aeffectx.h / vstfxstore.h）
- 若用于商业发布，请注意遵守以上组件的许可条款。
