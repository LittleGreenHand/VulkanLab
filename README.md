<img width="2562" height="1486" alt="3d23e144c1b546877bafffcbe29ad2e4" src="https://github.com/user-attachments/assets/5c92ced9-b285-4939-ac2d-d237203e6f3f" /># 

一个基于 Vulkan 的个人实时渲染器项目，用于学习、研究和实践现代实时渲染与物理模拟技术。

项目基于 [Sascha Willems Vulkan Samples](https://github.com/SaschaWillems/Vulkan) 的部分初始化 Vulkan 的 Framework 进行开发，目前已简化重构其大部分逻辑，然后在此基础上实现了自己的渲染管线、PBR 光照、延迟渲染、阴影、后处理、材质、Shader 资源布局以及相关渲染功能。

本项目的主要目标是在一个稳定的 Vulkan 基础渲染框架上，持续研究和实现Realtime Rendering、物理模拟与引擎架构，项目中使用的是Y向上的右手系，XYZ旋转顺序。

## Features
目前已经实现的主要功能包括：
- Hybrid Deferred
- PBR渲染
- IBL
- 金属度-粗糙度工作流的PBR材质
- 各向异性高光
- DOF
- 定向光与点光源
- 定向光CSM阴影与点光源的Omni阴影
- PCF
- glTF 2.0 model loading
- glTF material system
- KTX texture loading
- 刚体物理模拟

# 开发环境

- Windows 10、VS 2026 / Ubuntu 26.04、VS Code、ClangV
 CMake 4.4
- C++ 20
- Vulkan SDK 1.4
- OpenCV 5.0
- Python
- Git

# 第三方依赖，已包含在仓库的thirdParty或者submodule中

- GLFW（在Ubuntu中要注意下载相关依赖，一般缺少的是X11和wayland）
- GLM
- ImGui
- tinygltf
- KTX
- Slang
- ONNX Runtime
- PhysX-For-VS2026
    - PhysX-For-VS2026是作者基于PhysX官方仓库Fork出来的仓库，区别是为Windows平台添加了VS2026生成预设，官方仓库目前不支持生成VS2026。并且移除了Werror警告，这个警告在新版的Clang中会造成编译错误。
- OneTBB
- OpenUSD

# 项目构建

## 1. 编译子模块

在仓库根目录执行：

```bash
git submodule update --init --recursive
```

然后运行**build_dependencies.py**脚本编译子模块，默认编译 OneTBB、OpenUSD 和 PhysX 的 **Release** 版本；默认生成 OpenUSD C++ 核心库和 PhysX CPU 库。

## 2. 生成主项目

### Windows：使用 CMake GUI

1. 将 **Where is the source code** 设置为仓库根目录。
2. 将 **Where to build the binaries** 设置为仓库下的 `build` 目录。
3. 点击 **Configure**，生成器选择 **Visual Studio 18 2026**，平台选择 **x64**。
4. 点击 **Generate**，完成后点击 **Open Project**。
5. 在 Visual Studio 中将 `VulkanLab` 设为启动项目，选择 **Debug** 或 **Release**，生成并运行。

### Linux：使用命令行

在仓库根目录执行：

```bash
cd build
cmake ..
cmake --build . --parallel
```

## 3. 链接Debug依赖

**主项目的编译配置与依赖库版本独立选择。** 默认情况下，主项目 Debug、Release 均链接 Release 依赖。

如需链接 Debug 依赖，先在仓库根目录编译对应 SDK：

```bash
python build_dependencies.py --config Debug
```

然后在 CMake GUI 中将 `VULKANLAB_DEPENDENCY_CONFIG` 改为 **Debug**，重新 Configure、Generate；或在 `build` 目录执行：

```bash
cmake .. -DVULKANLAB_DEPENDENCY_CONFIG=Debug
```

### Shader System
Shader 使用Slang着色器语言编写，生成Shaders项目时会通过python脚本编译生成SPIR-V，相关编译设置已经集成到 CMake 文件中。

### Demo screenshot
<img width="2562" height="1486" alt="3d23e144c1b546877bafffcbe29ad2e4" src="https://github.com/user-attachments/assets/2bda226c-9522-484a-9379-40b49f0704e1" />
<img width="2562" height="1486" alt="3d23e144c1b546877bafffcbe29ad2e4" src="https://github.com/user-attachments/assets/de7200b7-7863-45df-a121-abb6a0f5eba2" />
<img width="2562" height="1486" alt="ee375da9273cde24007740d202b3d283" src="https://github.com/user-attachments/assets/bbbdd5e7-70a4-409b-b0b1-c6f5c0e1cb18" />

## 手部姿态推理

程序启动时启用 `palm_detection_mediapipe_2023feb`，通过 MediaPipe adapter 完成手掌检测和关键点推理，在 `Application::AIInference()` 中依次采集相机图像、执行模型及解析结果。模型列表重新扫描后，手部模型会重新绑定适配器，可在 AI 面板再次启用。

`Yolo26HandAdapter::CreateInput(frame, input, error)` 接收 `CV_8UC3` BGR 图像（支持 ROI），预处理采用 640×640 等比例缩放、114 填充、RGB、Float32 NCHW 和除以 255 归一化。当前 ONNX 模型的输入为 `[1,3,640,640]`，端到端输出为 `[1,300,69]`，每行依次为 `x1,y1,x2,y2,confidence,class` 与 21 组 `x,y,confidence`。解析按置信度过滤，默认阈值为 0.25。

主程序通过 `Application::GetHandPoseResult()` 读取最新结果，包含原图尺寸、各只手的检测框及 21 个关键点。坐标单位为原始图像像素；关键点顺序为手腕，以及拇指、食指、中指、无名指、小指各自从根部到指尖的 4 个点。无检测时 `Hands` 为空；采集或推理失败会清除上一帧结果，可通过 `GetHandPoseError()` 读取错误。结果引用应在主线程中使用，其内容会在下一次推理时更新。

默认打开独立 OpenCV 调试窗口，绘制同帧图像、检测框和置信度至少为 0.5 的关键点及骨架。在 AI 面板停用默认 MediaPipe 手掌模型会关闭调试窗口；重新启用后恢复推理和显示。也可在主线程调用 `CameraDevice::ShowHandPoseDebug(frame, result, error)` 显示外部提供的同帧结果。

适配器通过 `InferenceOutput::HandPoses` 返回结构化结果。每个适配器实例按预处理、推理、后处理的顺序串行调用，保存的 letterbox 参数用于当前帧坐标还原。当前调度为主线程同步推理，渲染帧率会受推理耗时影响。

### MediaPipe 手部姿态 adapter

`MediaPipeHandAdapter` 挂载到 `palm_detection_mediapipe_2023feb` 手掌模型，内部通过 ONNX Runtime 调用同目录树下的 `handpose_estimation_mediapipe_2023feb`。模型管理器也会为 `_int8` 和 `_int8bq` 版本绑定对应的关键点模型。手掌阶段沿用 `AIModel` 选择的后端，关键点阶段固定为 ONNX Runtime；关键点会话在首次预处理时加载，随 adapter 销毁释放。

```cpp
#include "AI/Adapter/MediaPipeHandAdapter.h"
#include "AI/AIModelManager.h"

auto* model = AIModelManager::Get().FindModel("palm_detection_mediapipe_2023feb");
InferenceInput input;
InferenceOutput output;
std::string error;
if (model && model->SetEnabled(true) && MediaPipeHandAdapter::CreateInput(frame, input, error) && model->Run(input, output))
{
    const HandPoseResult& poses = *output.HandPoses;
    // 使用 poses.Hands 中的关键点、世界坐标和左右手概率。
}
```

输入是 `CV_8UC3` BGR 图像，支持非连续 ROI。adapter 完成 192×192 RGB NHWC 预处理、2016 个 anchor 解码、NMS、旋转裁剪及 224×224 关键点推理。输出 `HandPose::Keypoints` 的 X/Y 为原图像素坐标，Z 为相对腕部的像素尺度深度；关键点可位于画面外。模型提供整只手的置信度，该值填入各关键点的 Confidence。`WorldKeypoints` 为逆旋转后的米制坐标，`HasWorldKeypoints` 标记有效性；`RightHandProbability` 是模型原始的右手概率，其左右手语义受输入镜像约定影响。

默认手掌阈值 0.5、手部阈值 0.8、NMS 阈值 0.3，最多输出 2 只手，可通过构造函数或 adapter UI 调整。无手时返回包含图像尺寸的空 `Hands`。输出用于手部姿态，不包含手势类别。相机循环默认运行 MediaPipe，并通过 `CameraDevice::ShowHandPoseDebug` 显示当前帧、手部框、置信度及 21 点骨架。停用模型或推理失败时关闭调试窗口。
