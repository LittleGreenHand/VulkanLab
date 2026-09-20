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
- Windows 10 / Ubuntu 26.04
- CMake 4.4
- C++ 20
- Vulkan 1.4
- OpenCV 5.0
- Python
- Git
- Visual Studio 2026

# 第三方依赖，已包含在仓库的thirdParty中
- GLFW（在Ubuntu中要注意下载相关依赖，一般缺少的是X11和wayland）
- GLM
- ImGui
- tinygltf
- KTX
- Slang
- PhysX-For-VS2026
    - PhysX-For-VS2026是作者基于PhysX官方仓库Fork出来的仓库，区别是为Windows平台添加了VS2026生成预设，官方仓库目前不支持生成VS2026。并且移除了Werror警告，这个警告在新版的Clang中会造成编译错误。

# 项目构建
项目当前依赖了PhysX来实现刚体模拟，因此克隆本仓库后需要先下载submodule，然后再通过CMake生成解决方案
- 1.克隆本仓库后，在仓库根目录打开 Git Bash，执行：
    - git submodule update --init --recursive

- 2.双击执行thirdParty\PhysX\physx目录下的“generate_projects.bat”脚本。

- 3.在弹出的命令行中输入5来生成vc18win64-cpu-only配置（意思是生成VS2026的slnx，也可以根据自己的需求选择其他版本的配置）。
    - 如果生成成功，会在thirdParty\PhysX\physx\compiler\vc18win64-cpu-only目录下生成VS2026的slnx文件，生成的目录取决于命令行中选择的配置。
 
- 4.打开compiler\vc18win64-cpu-only目录下的slnx，选择debug配置，然后生成解决方案。
    - 如果要以release运行本仓库项目的话，则需要生成PhysX的release版本。

- 5.生成PhysX后，就可以使用CMake生成本仓库项目，打开CMake GUI，分别设置源码目录和build目录，然后点击Configure按钮，执行结束后再点击Generate按钮，等待执行结束。
    - 如果PhysX不是使用vc18win64-cpu-only配置生成的slnx，需要在Cmake GUI中通过PHYSX_BIN_DIR变量手动设置PhysX的编译输出目录，默认是${CMAKE_SOURCE_DIR}/thirdParty/PhysX/physx/bin/win.x86_64.vc143.md

- 6.Generate成功后点击Open Project打开VS，将VulkanLab设置为启动项目并编译即可。
    - 编译时会自动将PhysX的DLL文件复制到输出目录。

## Linux 构建与运行

需要安装 CMake、Clang、Python、Vulkan SDK、GLFW 所需的 X11 或 Wayland 开发库，以及可opencv 5.0。
在仓库根目录执行以下终端命令：

```bash
git submodule update --init --recursive
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-linux -j
./build-linux/bin/VulkanLab
```
构建成功后可执行以下命令来运行程序：
```bash
./build-linux/bin/VulkanLab
```
Linux 构建会自动将 PhysX 作为 CPU-only 静态库编译，并将 Slang Shader 编译到 `build-linux/bin/shaders`。

### Shader System
Shader 使用Slang着色器语言编写，生成Shaders项目时会通过python脚本编译生成SPIR-V，相关编译设置已经集成到 CMake 文件中。

### Demo screenshot
<img width="2562" height="1486" alt="3d23e144c1b546877bafffcbe29ad2e4" src="https://github.com/user-attachments/assets/2bda226c-9522-484a-9379-40b49f0704e1" />
<img width="2562" height="1486" alt="3d23e144c1b546877bafffcbe29ad2e4" src="https://github.com/user-attachments/assets/de7200b7-7863-45df-a121-abb6a0f5eba2" />
<img width="2562" height="1486" alt="ee375da9273cde24007740d202b3d283" src="https://github.com/user-attachments/assets/bbbdd5e7-70a4-409b-b0b1-c6f5c0e1cb18" />
