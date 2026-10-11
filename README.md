<img width="2562" height="1486" alt="3d23e144c1b546877bafffcbe29ad2e4" src="https://github.com/user-attachments/assets/5c92ced9-b285-4939-ac2d-d237203e6f3f" /># 

# 基于 Vulkan 的个人实时渲染器，用于学习、研究和实践现代实时渲染与物理模拟技术

本项目的主要目标是在一个稳定的 Vulkan 基础渲染框架上，持续研究和实现实时渲染、物理模拟与引擎架构。
项目中使用的是Y向上的右手系，XYZ旋转顺序。

## Features

目前已经实现的Features包括：

- Hybrid Deferred
- PBR渲染
- 金属度-粗糙度工作流的PBR材质
- 各向异性高光
- DOF
- 定向光与点光源（包括定向光CSM阴影与点光源的Omni阴影）
- PCF 软阴影
- GLTF Assets Loading
- KTX Texture Loading
- 刚体物理模拟
- 手势识别与物理交互
- 实例化渲染与物理模拟
- OpenUSD

# 开发环境

- Windows 10、VS 2026 / Ubuntu 26.04、VS Code、Clang
- CMake 4.4
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
cmake -S . -B build
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
