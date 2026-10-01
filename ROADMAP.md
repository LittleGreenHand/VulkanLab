# VulkanLab 开发规划

> 更新日期：2026-10-01。此文档用于记录 VulkanLab 的后续开发目标与验收标准，具体实施顺序可随当前功能进度调整。

## 当前阶段：手部识别与物理交互

当前主线继续推进相机采集、手部姿态识别、三维手部映射和 PhysX 场景交互，完成基本的动态交互验证后，进入下一阶段。

## 下一阶段：Vulkan 硬件光线追踪（Ray Tracing）

**阶段目标**：在现有 Vulkan 渲染器中新增独立的硬件光追模式，利用现有 glTF 场景、PBR 材质及 PhysX 变换，实现从基础光追到完整 Path Tracing 的可扩展架构。保留 Rasterization / Hybrid Deferred 作为现有渲染路径，以便对比、调试及逐步引入 Hybrid Ray Tracing。

### 第一阶段：RT 基础设施与场景求交（预计 1–2 周）

- [ ] 检测 Vulkan RT 支持，启用 `VK_KHR_acceleration_structure`、`VK_KHR_ray_tracing_pipeline` 等所需扩展及 Features。
- [ ] 扩展 GPU Vertex/Index Buffer 的 Usage 和 Device Address 支持。
- [ ] 基于现有 Model / Primitive 几何数据构建 BLAS，并根据 Node Transform 构建 TLAS。
- [ ] 封装加速结构、RT Pipeline、Shader Binding Table（SBT）和 Storage Image。
- [ ] 使用 Slang 实现 RayGen / ClosestHit / Miss，完成 glTF 场景的求交和法线可视化。
- [ ] 建立 RT Output → 现有 PostProcess → 显示的完整路径。

**验收**：可以切换至 RT 模式，对现有 Cube 或 Sponza 等静态场景进行正确的硬件光追，并通过 Validation Layer 检查资源及同步问题。

### 第二阶段：PBR 材质、光照与反射（预计 1–2 周）

- [ ] 建立 Instance / Geometry / Primitive / Material 的 GPU 索引映射。
- [ ] 接入材质参数、纹理访问、UV、法线和切线数据。
- [ ] 适配光追阶段的法线贴图与显式纹理 LOD 采样。
- [ ] 实现直接光照、光追阴影及一次镜面反射。
- [ ] 实现环境光/天空盒未命中处理。

**验收**：glTF 场景可显示基础 PBR 材质、光追阴影和一次反射。

**优先达成的短期目标**：完成 glTF 硬件光追三角形求交、材质读取、一次反射及 PhysX 刚体位姿更新的支持。整体目标粗估为 40–80 小时有效开发时间，需结合实现进度重新校准。

### 第三阶段：完整 Path Tracing（预计 2–4 周）

- [ ] 实现适用于随机采样的 PBR BSDF（含 BRDF 评估、采样与 PDF）。
- [ ] 实现多次反弹、漫反射间接光照、镜面反射和基础透射。
- [ ] 实现光源重要性采样、下一事件估计（NEE）和多重重要性采样（MIS）。
- [ ] 实现俄罗斯轮盘终止和采样种子管理。
- [ ] 实现静态场景的多帧累积，并在相机、场景或材质变化后重置历史。

**验收**：静态场景中的间接照明、反射能够随采样次数增加逐步收敛。

### 第四阶段：动态场景、降噪与优化（预计 3–6 周）

- [ ] 利用 PhysX / Node Transform 更新 TLAS Instance；为实例增加、删除和场景变化提供重建机制。
- [ ] 研究形变几何体的 BLAS 更新/重建；蒙皮动画按需求逐步接入。
- [ ] 接入历史帧、法线、深度和 Motion Vector 的时域重投影。
- [ ] 实现基础时空降噪，进一步评估 SVGF / NVIDIA NRD 等方法。
- [ ] 检查 AS Build/Trace 同步、缓冲区生命周期、多帧资源及窗口 Resize。
- [ ] 分析 RT 时间开销、AS 构建成本、采样数、显存和最终画质。
- [ ] 根据需要，将 RT 阴影/反射/GI 接入现有 Hybrid Deferred 流程。

**验收**：PhysX 运动物体能够正确参与光追，在相机与场景变化时保持可用的实时交互显示，并具备可复现的性能测量结果。

## 建议代码组织

```text
src/Render/RayTracing/
    RayTracingContext.h/.cpp
    AccelerationStructure.h/.cpp
    RayTracingScene.h/.cpp
    RayTracingPipeline.h/.cpp
    ShaderBindingTable.h/.cpp
    PathTracingRenderer.h/.cpp
shaders/RayTracing/
    RT_Common.slang
    RT_Binding.slang
    RayGen.slang
    ClosestHit.slang
    Miss.slang
```

建议与现有 `VulkanRenderer`、`MeshManager`、`VulkanglTFModel`、材质系统以及后处理模块复用底层数据和资源，在光追模式下独立组织 Pipeline 与场景加速结构。目录及类名是规划草案，实际以实现需要为准。

## 参考实现

- [NVIDIA Vulkan Ray Tracing Tutorial KHR](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR)
- [NVIDIA vk_mini_path_tracer](https://github.com/nvpro-samples/vk_mini_path_tracer)
- [NVIDIA Real-Time Denoisers](https://github.com/NVIDIA-RTX/NRD)
