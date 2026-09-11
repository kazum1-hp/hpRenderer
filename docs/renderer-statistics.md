# Renderer Statistics

ImGui 的 `Renderer Statistics` 窗口展示真实渲染提交统计与异步 GPU 计时。

| 数据 | 口径 |
| --- | --- |
| Frame Time | 上一应用帧起点到当前起点的 wall time，包含交换缓冲、VSync 等待和事件轮询。首帧显示 Pending。 |
| CPU Frame Time | 上一帧从 update 前到 ImGui 提交结束的 wall time；不含交换缓冲及事件轮询，包含驱动调用可能产生的等待。不是纯 CPU 执行时间。 |
| CPU Renderer | 当前 `Renderer::render` 的 wall time，包括资源准备、各 Pass 提交与 profiler 开销，不含 resize、场景提取和 ImGui。 |
| GPU Frame Time | 最近完成的 `Renderer::render` GPU timestamp 区间；包含资源准备，不含 ImGui 和 presentation。区间可能包含 GPU 等待 CPU 提交的空隙，不能等同于纯 GPU 忙碌时间。 |
| Draw Calls | 当前 renderer 帧实际发出的 draw 调用数，含阴影、环境预计算、后处理与调试绘制，不含 ImGui。一次 instanced draw 计为一次。 |
| Triangles | draw 输入的三角形数，乘以实例数；重复 Pass 重复计数。屏幕四边形计 2；点光源阴影 geometry shader 的六面扩增不乘 6。不是最终可见三角形或 rasterized primitives。 |
| Rendered Objects | 主视图 opaque / transparent Pass 中至少提交一次非空绘制的场景对象数，跨 mesh / Pass 去重；包含地面，不含阴影重复、天空盒、灯光标记、全屏效果。跳过的空模型和奇异变换不计入；屏幕外或被遮挡但仍提交的对象会计入。 |

上方 CPU/counter 数据与下方 GPU sample 不强行混用帧号。GPU 表格显示 sample 的帧龄，其中时间、Draws、Triangles 都来自同一帧。关闭功能后，旧 sample 的行可能保留到下一份 GPU 结果就绪。数据为原始单帧值，没有平滑或伪造零值。

## 给新功能增加测量

在 `Renderer::render` 生命周期内，包含头文件并使用命名 scope：

```cpp
#include "hpr/renderer/opengl/RenderProfiler.h"
using namespace Rendering;

BeginGPUQuery("Shadow Pass");
// submit shadow commands
EndGPUQuery();

// 推荐：离开作用域（包括异常）时自动结束，支持嵌套。
{
    ScopedGPUQuery query("My New Effect");
    // submit commands
}
```

相同名称的多个 scope 分别显示，嵌套 scope 的时间和计数包含子 scope，不能把父子行相加。现有 Frame、Environment Prepare、Scene Prepare、Shadow、Forward / Geometry + Lighting、Light Markers、Skybox、Transparent、GBuffer Debug、Bloom、Tone Mapping 已接入。可选功能未执行时不创建对应 scope。

通过 `Mesh::draw()` / `drawInstanced()` 提交会自动记数。如果新代码直接调用 OpenGL draw，需要紧邻调用增加 `RecordDraw(inputTriangleCount)`。新建主视图对象的绘制路径时，用 `ScopedRenderedObject(stableIdentity)` 包住绘制；identity 在帧内必须稳定且唯一。仅创建 scope 不会增加对象数，非空 draw 才会计入。

`renderer.statistics()` 可供 UI、自动化测试或后续导出使用。Profiler 位于 OpenGL 层，核心渲染器不依赖 ImGui。独立 Pass 测试或其他 renderer 之外的调用默认不记录；可用 `RenderProfiler` + `ScopedRenderFrame` 单独测量。所有操作在拥有当前 GL context 的渲染线程调用，同线程不能重叠两个 profiling frame。

## 查询生命周期与开销

使用 OpenGL 3.3 timestamp pairs，支持嵌套；4 个 frame slot，每个最多 128 个 scope（含 Frame）。查询对象按 slot 首次使用分配并重复使用。下一帧轮询最外层结束 timestamp 的 `GL_QUERY_RESULT_AVAILABLE`，只读取已完成的 sample，不调用 `glFinish`、busy-wait，也不为了数据等待 GPU。

全部 slot 仍在飞行时跳过该帧 GPU 采样，CPU/counter 继续记录；scope 超限时丢弃整个 GPU sample。面板显示累计 skipped samples。驱动不提供 timestamp counter 时显示 unavailable，保留 CPU/counter。`Renderer::shutdown()` 在 context 销毁前删除全部 query，重复 shutdown 与重新初始化均受支持。

timestamp 和 draw bookkeeping 本身仍有开销，因此细粒度 scope 应围绕 Pass 或功能块，而不是每个三角形。

## 后续最有价值的扩展

1. 帧时间历史、P50 / P95 / P99、峰值及 hitch 次数：发现平均值掩盖的卡顿。
2. CPU 分阶段计时：场景提取、模型准备、剔除、排序、draw submission、ImGui、present wait，定位 CPU 瓶颈。
3. submitted / culled objects 与 triangles：区分视锥剔除、遮挡剔除和实际提交，评价剔除收益。
4. 纹理、几何、render target 的分配字节与每帧上传量：评价显存和带宽成本；分配量估算需与驱动实际 resident VRAM 区分。
5. shader / material / texture / framebuffer 切换次数、instance 数和 batch 数：评价排序与 instancing。
6. 各灯光阴影耗时、shadow draw 数、分辨率和更新次数：评价阴影缓存、级联阴影等功能。
7. 带设置与分辨率的 CSV / JSON 采样导出：在相同相机、场景与预热条件下做功能 A/B 比较。

GPU invocation、overdraw 和 cache/bandwidth 指标需要额外查询或厂商 profiler，不能从当前 draw / triangle 数可靠推算。
