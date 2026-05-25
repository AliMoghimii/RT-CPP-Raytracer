# Sparse Radiance Cascades in Vulkan
## A Dissertation Study Guide

> **Compiled for**: Jose Javier Serrano Solis  
> **Programme**: MSc Computer Science for Games Programming, University of Hull  
> **Academic Year**: 2025–2026  
> **Subject**: Hardware-Agnostic Real-Time Global Illumination via Radiance Cascades in Vulkan  
> **Document Type**: Self-contained reference document, intended for both human study and AI-assisted development sessions (Claude Code, etc.)

---

## How to Use This Document

This is a **reference and study guide**, not a step-by-step implementation tutorial. The companion document (`showcase_refactoring_guide.md`) contains the practical refactoring steps for the GameRepublic Student Showcase. This document gives you the theoretical foundation and design space you need to defend a dissertation on the topic.

It is structured so that:

1. **Human readers** can read it linearly to learn the topic from first principles.
2. **AI code assistants** (Claude Code, GitHub Copilot Chat, etc.) can be given this document as context to answer detailed implementation questions without re-deriving theory from scratch.
3. **Examiners** reading your dissertation can be pointed to specific sections for clarification.

When in doubt, the **External References** section at the end lists the primary sources. This document is a synthesis, not a replacement for the original papers.

---

## Table of Contents

### Part I — Foundations
1. Global Illumination: The Problem
2. The Rendering Equation
3. Direct vs Indirect Light, Diffuse vs Specular
4. Stochastic vs Deterministic Approaches
5. Why Noise Is the Enemy of Real-Time GI

### Part II — Ray Tracing & Acceleration Structures
6. Ray Tracing Fundamentals
7. Ray-Primitive Intersection Math
8. Software Ray Tracing vs Hardware Ray Tracing
9. Vulkan's Ray Tracing Options
10. Bounding Volume Hierarchies (BVH)
11. Median Split vs Surface Area Heuristic (SAH)
12. Dynamic Scenes: Refit vs Rebuild

### Part III — Radiance Cascades Theory
13. The Penumbra Hypothesis
14. Probes, Radiance Intervals, and the Merging Operator
15. Cascade Hierarchy: Spatial and Angular Scaling
16. The Branching Factor α
17. Memory Scaling Properties
18. Directional Encoding: Octahedral Maps, Cubemaps, SH
19. Direction-First vs Probe-First Layout

### Part IV — From 2D to 3D
20. 2D Flatland: The Cleanest Formulation
21. Bilinear Fix and the Ringing Artifact
22. The 3D Memory Problem
23. The 3D Variants Landscape
24. Why Sparse Radiance Cascades

### Part V — Sparse Radiance Cascades
25. The Core Insight: Screen-Bound Probe Allocation
26. GPU Hash Map Storage
27. Per-Pixel Cascade Level Selection
28. Probe Lifecycle: Allocation, Insertion, Eviction
29. The Cascade Merging Pass (Top-Down)
30. Depth-Aware Interpolation: Bilateral and "Bilinear 3D"
31. The Final Gather Pass
32. Pre-Averaging Optimization
33. Min-Max Probes (Advanced Stabilization)

### Part VI — Holographic Radiance Cascades (Stretch)
34. Why HRC Exists: Small-Penumbra Failure of Standard RC
35. The Anisotropic Probe Layout
36. The Acceleration Structure for Ray Combination
37. 3D HRC Limitations (the N⁴ Memory Wall)

### Part VII — Vulkan Implementation
38. Pipeline Architecture for Sparse RC
39. Required Vulkan Features and Extensions
40. Compute Pipelines and Descriptor Indexing
41. Buffer Layout for Cascades
42. GPU Hash Map: Implementation Strategies
43. Synchronization Between Passes
44. Building a Clean Vulkan Renderer From Zero
45. Modular Class Architecture
46. The ImGui Debug Interface

### Part VIII — Future Adaptability
47. Strategy Pattern for GI Modules
48. DDGI as a Drop-In Alternative
49. SSGI as a Comparison Baseline
50. The Ablation Study Framework

### Part IX — Dissertation Methodology
51. Test Scenes: Indoor, Outdoor, Dynamic
52. Reference Path Tracer for Ground Truth
53. Metrics: RMSE, Frame Time, VRAM
54. Benchmarking Methodology
55. Common Pitfalls and How to Defend Against Them

### Part X — References
56. Primary Papers
57. Implementations Worth Studying
58. Community Resources

---

# Part I — Foundations

## 1. Global Illumination: The Problem

**Global illumination** (GI) is the simulation of how light interacts with a scene through multiple bounces. The "direct" lighting from a lamp is easy: trace one ray from each surface point to the lamp, check for occlusion. The "indirect" part is hard: a wall lit by the lamp also re-emits light onto other surfaces; those surfaces re-emit again; the process is recursive and high-dimensional.

In offline rendering (cinema, architectural visualization), this is solved by **path tracing**: shoot many rays per pixel, follow them through many bounces, average the results. Given enough rays, the image converges to the physically correct answer. The downside is convergence speed — a "production" frame may take minutes or hours per image.

In real-time rendering (games, interactive applications), we have ~16 milliseconds per frame. We cannot afford convergence. Every realtime GI technique is a compromise: which physical phenomena do we model accurately, and which do we approximate or skip entirely?

The state of real-time GI as of 2026 falls into three broad families:

- **Screen-space techniques** (SSAO, SSGI, SSR) — fast, but can only see what's on-screen. Off-screen indirect light is missed.
- **Probe-based techniques** (DDGI, Lumen surface caches, Radiance Cascades) — store the radiance field at sample points in space, interpolate at shading time. Trade some accuracy for noise-free, scene-aware results.
- **Path-traced techniques** (ReSTIR, hardware ray-traced GI in Unreal/Frostbite) — accurate but require denoising, which introduces temporal artifacts.

**Radiance Cascades** is a probe-based technique, but it differs from the rest in that it is *single-shot* (no temporal accumulation needed) and *noiseless* (deterministic). This is the property that makes it interesting for a dissertation contribution.

## 2. The Rendering Equation

Every GI algorithm is an attempt to numerically solve the **rendering equation** (Kajiya, 1986):

$$
L_o(\mathbf{x}, \omega_o) = L_e(\mathbf{x}, \omega_o) + \int_\Omega f_r(\mathbf{x}, \omega_i, \omega_o) \, L_i(\mathbf{x}, \omega_i) \, (\omega_i \cdot \mathbf{n}) \, d\omega_i
$$

Read this in plain English:

- $L_o(\mathbf{x}, \omega_o)$ — the radiance leaving point $\mathbf{x}$ in direction $\omega_o$ (what the camera sees)
- $L_e(\mathbf{x}, \omega_o)$ — what the surface emits on its own (if it's a light source)
- $\int_\Omega \dots d\omega_i$ — an integral over the hemisphere of incoming directions
- $f_r$ — the BRDF (how the surface scatters incoming light into outgoing directions)
- $L_i(\mathbf{x}, \omega_i)$ — the incoming radiance from direction $\omega_i$
- $(\omega_i \cdot \mathbf{n})$ — Lambert's cosine factor (light hitting at a glancing angle counts less)

The hard part is $L_i$: to know what light is incoming from direction $\omega_i$, you'd need to know what's at the other end of that ray, which means... solving the rendering equation again, recursively. This is why offline path tracers exist.

**Radiance Cascades sidesteps this**: instead of computing $L_i(\mathbf{x}, \omega_i)$ on demand for every shading point, it pre-computes the **radiance field** $L(\mathbf{x}, \omega)$ for *every point in space and every direction* at a coarse-but-sufficient resolution, stores it in a hierarchical data structure (the cascades), and then interpolates at shading time. The expensive integral becomes a few texture lookups.

This is why probes are the central concept: a probe is a point sample of $L(\mathbf{x}, \omega)$ for a fixed $\mathbf{x}$, covering all directions.

## 3. Direct vs Indirect Light, Diffuse vs Specular

A common point of confusion: people use "GI" to mean different things. Let's pin down terminology.

**Direct light** = the first bounce. The surface is lit by a light source (sun, lamp) without anything in between.

**Indirect light** = all subsequent bounces. The surface is lit by light that bounced off some other surface first.

**Diffuse reflection** = light scatters uniformly in all hemispherical directions. A matte wall, fabric, paper. The BRDF is roughly constant.

**Specular reflection** = light scatters in (approximately) the mirror direction. A polished metal surface, a wet floor, water.

| Combination | Example | Difficulty for RC |
|---|---|---|
| Direct + Diffuse | Sunlight on a matte wall | Trivial (just trace a shadow ray) |
| Direct + Specular | Sun in a mirror | Trivial (reflect the ray) |
| Indirect + Diffuse | Color bleeding from a red wall | **What RC is for** |
| Indirect + Specular | Reflection of a TV in a glossy table | Hard for RC (limited angular resolution) |

**Radiance Cascades excels at indirect diffuse GI** — color bleeding, ambient occlusion, soft shadows, sky illumination. It is *not* the right tool for sharp glossy reflections; for that you still want screen-space reflections, planar reflections, or hardware path-traced reflections. Your dissertation should be honest about this scope: "we solve indirect diffuse GI noise-free in real time; specular reflections remain handled by a separate technique."

## 4. Stochastic vs Deterministic Approaches

A **stochastic** approach uses random sampling. Path tracing fires rays in randomly chosen directions, averages the results, and converges to the correct answer asymptotically. The downside is **noise**: with few samples, the image looks grainy.

A **deterministic** approach uses fixed sample patterns. Discrete ordinates methods, finite element methods, and Radiance Cascades all fall here. They don't have noise, but they have **bias**: the answer they converge to is slightly off from the "true" rendering equation because of discretization.

For real-time games:

- **Noise** is visually objectionable and temporally unstable (shimmering, ghosting through denoisers).
- **Bias**, if small enough, is unnoticeable — and crucially, it's *consistent frame-to-frame*, so it doesn't shimmer.

RC's deterministic approach is its competitive advantage. PoE2 ships with no denoiser. No temporal accumulation. No "ghosting" on fast camera motion. This is the property worth highlighting in your dissertation introduction.

## 5. Why Noise Is the Enemy of Real-Time GI

Path-traced GI looks beautiful in still images but presents three problems in real-time:

1. **Spatial noise** — each pixel's value depends on which random rays it sampled, producing grain
2. **Temporal noise** — successive frames have different random samples, producing flicker
3. **Denoiser artifacts** — fixing 1 and 2 with a denoiser introduces ghosting, blurring, and lag

Modern path-traced games (Cyberpunk 2077 RT Overdrive, Alan Wake 2 PT) use sophisticated spatiotemporal denoisers (NRD, OptiX denoiser, ReSTIR GI) that mostly work, but at the cost of:

- 4–8 frames of temporal accumulation lag (visible during fast camera motion)
- Loss of high-frequency detail (small shadows, contact lighting)
- Significant ML model overhead in some pipelines

For a hardware-agnostic technique targeting GTX 1080-class hardware, denoiser-based path tracing is simply not feasible. RC sidesteps the entire problem: by being noiseless, there's nothing to denoise.

---

# Part II — Ray Tracing & Acceleration Structures

## 6. Ray Tracing Fundamentals

A **ray** is a half-line in 3D space:

$$
\mathbf{P}(t) = \mathbf{O} + t \cdot \mathbf{D}, \quad t \in [t_{\min}, t_{\max}]
$$

- $\mathbf{O}$ — origin point
- $\mathbf{D}$ — direction vector (typically normalized: $|\mathbf{D}| = 1$)
- $t$ — parameter along the ray (distance from origin if $\mathbf{D}$ is normalized)
- $[t_{\min}, t_{\max}]$ — the valid interval of the ray

The $[t_{\min}, t_{\max}]$ interval is critical for Radiance Cascades. Each cascade level $i$ traces rays clamped to a specific interval $[t_i, t_{i+1}]$. Hits outside this interval are ignored. This is how cascades partition the world into spatial shells.

Ray tracing in graphics asks two questions:

1. **What's the first thing this ray hits?** (closest-hit ray)
2. **Does this ray hit anything at all?** (any-hit / occlusion / shadow ray)

For RC, you'll use both:
- Cascade rays use closest-hit to find the first emissive or reflective surface
- Shadow rays in the final gather pass use any-hit for direct lighting occlusion

## 7. Ray-Primitive Intersection Math

The three intersection tests you'll write yourself (or already have in your teammate's code):

### Ray vs Sphere

A sphere is $|\mathbf{P} - \mathbf{C}|^2 = r^2$. Substitute the ray equation:

$$
|(\mathbf{O} + t\mathbf{D}) - \mathbf{C}|^2 = r^2
$$

Expanding gives a quadratic in $t$:

$$
t^2 + 2 t (\mathbf{D} \cdot (\mathbf{O} - \mathbf{C})) + |\mathbf{O} - \mathbf{C}|^2 - r^2 = 0
$$

Solve with the quadratic formula. The smaller positive root is the entry point.

### Ray vs Triangle (Möller-Trumbore)

This is the algorithm in your teammate's `triangleIntersect()` function. Given triangle vertices $\mathbf{v}_0, \mathbf{v}_1, \mathbf{v}_2$:

```glsl
vec3 edge1 = v1 - v0;
vec3 edge2 = v2 - v0;
vec3 h = cross(ray.direction, edge2);
float a = dot(edge1, h);
if (abs(a) < EPSILON) return -1.0; // parallel
float f = 1.0 / a;
vec3 s = ray.origin - v0;
float u = f * dot(s, h);
if (u < 0.0 || u > 1.0) return -1.0;
vec3 q = cross(s, edge1);
float v = f * dot(ray.direction, q);
if (v < 0.0 || u + v > 1.0) return -1.0;
float t = f * dot(edge2, q);
return t > EPSILON ? t : -1.0;
```

The `u` and `v` you get out are **barycentric coordinates** — useful later for interpolating per-vertex normals, UVs, and other attributes.

### Ray vs AABB (Axis-Aligned Bounding Box)

Used inside BVH traversal to decide whether to descend into a node. Slab method:

```glsl
vec3 t0 = (boxMin - ray.origin) * invDir;
vec3 t1 = (boxMax - ray.origin) * invDir;
vec3 tmin_v = min(t0, t1);
vec3 tmax_v = max(t0, t1);
float tEnter = max(max(tmin_v.x, tmin_v.y), tmin_v.z);
float tExit  = min(min(tmax_v.x, tmax_v.y), tmax_v.z);
// hit if tEnter <= tExit AND tExit >= 0
```

Note `invDir = 1.0 / ray.direction` is precomputed once per ray. This is the same code you have in `intersectAABB()`.

## 8. Software Ray Tracing vs Hardware Ray Tracing

**Software ray tracing** = the BVH and traversal are implemented in user code (a compute shader, for example). This is what your teammate's project does.

**Hardware ray tracing** = the GPU has dedicated silicon for BVH traversal and triangle intersection. In Vulkan, you access it via `VK_KHR_ray_tracing_pipeline` (with ray-gen, miss, hit, intersection shader stages) or `VK_KHR_ray_query` (call `rayQueryEXT` from any shader stage).

| Aspect | Software RT | Hardware RT |
|---|---|---|
| Hardware requirement | Any GPU with compute | RTX 2000+, RDNA2+, Arc |
| Performance | 5–50× slower | Optimal |
| Flexibility | Total control | Constrained by API |
| Code complexity | More shader code | More host-side setup |
| Memory layout | Your choice | Driver-managed AS |

**For your dissertation's hardware-agnostic angle, software RT is actually a strength**. The whole point of choosing RC is "works on GTX 1080." If you require RTX cores, you've defeated your own thesis.

That said, you should mention in your dissertation that hardware RT is a drop-in optimization: replace your BVH traversal with `rayQueryEXT(...)`, build a TLAS/BLAS on the host side, and the rest of the pipeline is unchanged. The cascade algorithm is **agnostic to the ray-tracing backend**, which is one of its selling points.

## 9. Vulkan's Ray Tracing Options

In order of complexity:

### Option A: Compute shader + custom BVH (what your teammate has)

```cpp
// Host
vkCreateComputePipelines(...);
vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

// Shader
layout(std430, binding = 8) readonly buffer BVHBuffer { GPUBVHNode bvhNodes[]; };
// ... custom traversal code
```

**Pros**: Total control. Runs anywhere. No special extensions.  
**Cons**: Slow. You write all the math.

### Option B: Ray Query in a compute shader

```cpp
// Host - device feature
VkPhysicalDeviceRayQueryFeaturesKHR rqFeatures{};
rqFeatures.rayQuery = VK_TRUE;
// Enable VK_KHR_ray_query, VK_KHR_acceleration_structure, VK_KHR_deferred_host_operations

// Shader
#extension GL_EXT_ray_query : require
rayQueryEXT rq;
rayQueryInitializeEXT(rq, topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, origin, tMin, dir, tMax);
while (rayQueryProceedEXT(rq)) {}
if (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
    float t = rayQueryGetIntersectionTEXT(rq, true);
    // ...
}
```

**Pros**: Hardware acceleration. Still inside compute (good for RC). Cleaner code.  
**Cons**: Requires RT-capable hardware. Host-side AS management.

### Option C: Ray Pipeline (separate ray-gen / miss / hit shaders)

```cpp
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, ...);
vkCmdTraceRaysKHR(cmd, &raygenSBT, &missSBT, &hitSBT, &callSBT, width, height, depth);
```

**Pros**: Highest performance for "traditional" path tracing patterns.  
**Cons**: More complex setup (Shader Binding Table). Doesn't compose as cleanly with RC's compute-pass-based merging.

**For RC: stick with Option A for the showcase, plan a path to Option B for future work.** Option C is overkill — RC's structure (alloc → trace → merge → gather) is fundamentally compute-pass-based, and `rayQueryEXT` inside compute gives you HW acceleration without forcing you into the ray pipeline paradigm.

## 10. Bounding Volume Hierarchies (BVH)

A **BVH** is a binary tree where each node holds an axis-aligned bounding box (AABB) and either:

- A list of triangles (a leaf node), or
- Two child nodes (an internal node)

To find what a ray hits, you traverse the tree: at each internal node, test against the AABB; if hit, descend into both children; if not, prune that subtree. The cost is roughly $O(\log N)$ per ray instead of $O(N)$.

Your teammate's `GPUBVHNode` is a standard flat-array layout:

```cpp
struct GPUBVHNode {
    glm::vec3 aabbMin;
    int leftFirst;     // if internal: index of left child; if leaf: first triangle index
    glm::vec3 aabbMax;
    int triCount;      // 0 = internal node, >0 = leaf with this many triangles
};
```

This is 32 bytes per node — cache-friendly and GPU-uploadable as a single SSBO. The "leftFirst + triCount" trick saves a field: a node is internal if `triCount == 0` (then `leftFirst` is the left child index, and the right child is implicitly at `leftFirst + 1`), or a leaf otherwise.

### Traversal Code (GPU side)

```glsl
int stack[64];
int stackPtr = 0;
stack[stackPtr++] = 0; // start at root
vec3 invDir = 1.0 / ray.direction;

while (stackPtr > 0) {
    int nodeIdx = stack[--stackPtr];
    GPUBVHNode node = bvhNodes[nodeIdx];
    
    float distAABB = intersectAABB(ray, invDir, node.aabbMin, node.aabbMax);
    if (distAABB >= closestHit) continue; // already found something closer
    
    if (node.triCount > 0) {
        // Leaf: test all triangles
        for (int i = 0; i < node.triCount; i++) {
            float t = triangleIntersect(ray, triangles[node.leftFirst + i], u, v);
            if (t > 0 && t < closestHit) {
                closestHit = t;
                // record hit
            }
        }
    } else {
        // Internal: push both children
        stack[stackPtr++] = node.leftFirst;
        stack[stackPtr++] = node.leftFirst + 1;
    }
}
```

Stack size of 64 is typically enough for trees up to depth 64, which corresponds to ~10^19 triangles. For real-time scenes you'll never come close.

## 11. Median Split vs Surface Area Heuristic (SAH)

When building the BVH, at each node you must decide:

1. **Which axis to split on?** (X, Y, or Z)
2. **Where on that axis to split?**

### Median Split (your teammate's current approach)

```cpp
// 1. Pick the longest axis
glm::vec3 extent = node.aabbMax - node.aabbMin;
int axis = 0;
if (extent.y > extent.x) axis = 1;
if (extent.z > extent[axis]) axis = 2;

// 2. Split at the centroid of the AABB
float splitPos = node.aabbMin[axis] + extent[axis] * 0.5f;

// 3. Partition triangles by centroid position
```

**Pros**: Fast to build, simple.  
**Cons**: Often produces unbalanced trees with overlapping siblings → slower ray traversal.

### Surface Area Heuristic (SAH)

SAH evaluates the *cost* of a split based on the surface area of the resulting child AABBs:

$$
\text{Cost}(\text{split}) = C_{\text{trav}} + \frac{SA(\text{left})}{SA(\text{parent})} \cdot N_{\text{left}} \cdot C_{\text{isect}} + \frac{SA(\text{right})}{SA(\text{parent})} \cdot N_{\text{right}} \cdot C_{\text{isect}}
$$

Where:
- $C_{\text{trav}}$ = cost of traversing a node (typically 1.0)
- $C_{\text{isect}}$ = cost of testing a triangle (typically 1.0–2.0)
- $SA(\cdot)$ = surface area of an AABB
- $N$ = triangle count

The intuition: a child node with large surface area is more likely to be hit by a random ray, so we want children with small surface area (tight bounds around few triangles).

### Binned SAH (the practical version)

Evaluating SAH at every possible split position is $O(N^2)$ — too slow. **Binned SAH** approximates this:

1. For each candidate axis (X, Y, Z), compute the AABB of triangle *centroids*.
2. Divide that range into K bins (typically 16 or 32).
3. Bucket each triangle into one bin based on its centroid.
4. For each of the K-1 inter-bin boundaries, compute the SAH cost using prefix sums of bin AABBs and counts.
5. Pick the (axis, boundary) with minimum cost across all candidates.

Build complexity: $O(N \log N \cdot K)$ — slightly more than median split's $O(N \log N)$, but the resulting tree traverses 2–5× faster.

**Importantly**: the `GPUBVHNode` data structure doesn't change. SAH only changes where the split happens inside the CPU-side `subdivide()` function. The GPU traversal code is untouched.

**Reference implementations**:
- Jacco Bikker's blog series, "How to Build a BVH" (Parts 1–9)
- PBRT 4th edition, chapter 4
- Wald 2007, "On Fast Construction of SAH-based Bounding Volume Hierarchies"

For a clean implementation, write a `BVHBuilder` class that supports both median-split and binned-SAH strategies, switchable by parameter. That way you can A/B compare for your dissertation's scalability chapter.

## 12. Dynamic Scenes: Refit vs Rebuild

The BVH discussed so far is **static** — built once when the scene loads. For dynamic scenes (animated characters, moving objects), you have three options:

### Option 1: Full rebuild every frame
Simple but expensive. Acceptable for very small scenes or low-frequency updates.

### Option 2: Refit (also called "refresh")
Keep the tree structure, only update the AABBs of each node bottom-up:
- For each leaf, recompute its AABB from current triangle positions
- For each internal node (in reverse-DFS order), set its AABB to the union of its children's AABBs

Refit is fast ($O(N)$) but the *tree quality degrades* over time as objects move. Eventually traversal becomes slow enough that you must rebuild.

### Option 3: Two-level BVH (TLAS + BLAS)
The standard hardware-RT approach, and a great pattern even for software RT:
- Each *object* has its own static BVH (a Bottom-Level Acceleration Structure, BLAS) built in object space.
- A *top-level* BVH (TLAS) is built each frame containing one entry per object, with the object's current world-space transform.
- Ray traversal: hit the TLAS to find which objects to test, transform the ray into each object's local space, then traverse the BLAS.

For your dissertation's "dynamic mixed" test scene, the TLAS+BLAS pattern is what you want. The BLAS for each model is built once at load time (with SAH). The TLAS is small (one entry per object, ~10–100 entries) and rebuilt per frame, which is cheap.

**For the showcase**, you can defer this entirely: ship with static scenes only. Document this limitation in your dissertation and present TLAS+BLAS as future work.

---

# Part III — Radiance Cascades Theory

## 13. The Penumbra Hypothesis

This is the core insight that justifies the entire RC algorithm. Stated formally by Sannikov:

> *Near-field radiance varies with high spatial frequency and low angular frequency. Far-field radiance varies with low spatial frequency and high angular frequency.*

In plainer terms: nearby things change a lot from one point to another in space, but each point sees them at roughly the same angle. Distant things look about the same from any nearby point in space, but you need many angular samples to find their tiny apparent size.

**Why this is true geometrically**: consider a light source of width $w$ and an occluder. The penumbra it casts has:

- **Linear size** at distance $h$ from the occluder: $H(h) \approx \gamma \cdot h$ where $\gamma = 2 \arctan(w / 2d)$
- **Angular subtense** at distance $D$: $\epsilon(D) = 2 \arctan(w / 2D)$

As $D$ grows: $H$ grows linearly (more spatial blur), $\epsilon$ shrinks like $1/D$ (smaller angular target). So:

- Near: $\Delta_s$ (spatial step) must be small to resolve sharp penumbra; $\Delta_\omega$ (angular step) can be large because the source is angularly huge.
- Far: $\Delta_s$ can be large because the radiance is smooth; $\Delta_\omega$ must be small to find the tiny source.

Mathematically, the **penumbra criterion** is:

$$
\begin{cases}
\Delta_s \lesssim F(D) \propto D \\
\Delta_\omega \lesssim G(1/D) \propto 1/D
\end{cases}
$$

Standard RC exploits this by using cascades where:

$$
\Delta_s \propto 2^i, \quad \Delta_\omega \propto \frac{1}{2^{\alpha i}}, \quad t_i \propto 2^{\alpha i}
$$

Each cascade level $i$ trades spatial resolution for angular resolution at exactly the right rate to satisfy the penumbra criterion at every distance.

## 14. Probes, Radiance Intervals, and the Merging Operator

### What is a Probe?

A **probe** is a point sample of the directional radiance field. Conceptually:

$$
\text{Probe}(\mathbf{p}) = \{ L(\mathbf{p}, \hat\omega) \mid \hat\omega \in S^2 \}
$$

In practice, you discretize the sphere $S^2$ into N directions and store N radiance values per probe. The encoding (octahedral map, cubemap, spherical harmonics) is an implementation choice — we'll cover this in Section 18.

### What is a Radiance Interval?

A **radiance interval** is a probe's directional sample, but constrained to a *distance range* $[a, b]$:

$$
\mathcal{R}_{a,b}(\mathbf{p}, \hat\omega) = (L_{b \to a}(\mathbf{p}, \hat\omega), \tau_{b \to a}(\mathbf{p}, \hat\omega))
$$

Two values are stored per direction:

- $L_{b \to a}$ — the radiance accumulated along the segment from $b$ to $a$ (i.e., light entering the probe from the shell $[a, b]$)
- $\tau_{b \to a}$ — the optical depth (or equivalently, the transmittance $T = \exp(-\tau)$) of that segment

This is fundamentally different from a regular probe in that **it only "sees" the shell $[a, b]$ around itself**, not the full infinite ray.

Different cascade levels store different intervals: cascade 0 stores $[0, t_0]$, cascade 1 stores $[t_0, t_1]$, cascade 2 stores $[t_1, t_2]$, and so on, with $t_i$ growing exponentially.

### The Merging Operator

Two **contiguous** radiance intervals can be combined using a formula identical to **premultiplied alpha blending**:

$$
\mathcal{M}(\mathcal{R}_{a,b}, \mathcal{R}_{b,c}) = \big(L_{b \to a} + T_{b \to a} \cdot L_{c \to b}, \; T_{b \to a} \cdot T_{c \to b}\big)
$$

Or in pseudo-code:

```glsl
vec4 merge(vec4 near, vec4 far) {
    // .rgb = radiance, .a = transmittance (1 - alpha)
    return vec4(
        near.rgb + near.a * far.rgb,
        near.a * far.a
    );
}
```

**Why this works**: the radiance from segment $[b, c]$ has to pass through segment $[a, b]$ to reach the probe at $\mathbf{p}$. The fraction that survives is the transmittance $T_{b \to a}$. The total radiance is the near segment's radiance plus the attenuated far segment's radiance.

This is mathematically equivalent to the rendering of premultiplied-alpha images stacked in depth. Porter and Duff (1984) defined the same operation 40 years earlier in a completely different context. The graphics universe has good aesthetic taste.

## 15. Cascade Hierarchy: Spatial and Angular Scaling

A **cascade** is a complete set of probes at a uniform spatial resolution, each storing a radiance interval for a specific distance shell:

- **Cascade 0**: dense probes (high spatial), few directions per probe (low angular), shell $[0, t_0]$
- **Cascade 1**: half-density probes, double the directions, shell $[t_0, t_1]$ with $t_1 = 2^\alpha t_0$
- **Cascade 2**: half-density again, double angular again, shell $[t_1, t_2]$ with $t_2 = 2^{2\alpha} t_0$
- ... and so on until you cover the full scene extent

In 3D, "half-density" means halving the probe count along each axis, so total probe count drops by 8× per cascade level.

The **total radiance** at a point is the merge of all cascade intervals along each direction:

$$
L_{\text{total}}(\mathbf{p}, \hat\omega) = \mathcal{M}(\mathcal{R}_{0,t_0}, \mathcal{M}(\mathcal{R}_{t_0,t_1}, \mathcal{M}(\mathcal{R}_{t_1,t_2}, \ldots)))
$$

In practice this is computed top-down: start with cascade $N$ (farthest), merge into cascade $N-1$, then into $N-2$, and so on until cascade 0 contains the full hemispherical radiance to infinity.

## 16. The Branching Factor α

The exponent $\alpha$ controls how fast both angular resolution and ray length grow per cascade level:

$$
\Delta_\omega \propto \frac{1}{2^{\alpha i}}, \quad t_i \propto 2^{\alpha i}
$$

In 2D, the standard choices are:

- $\alpha = 1$: each cascade has 2× the rays per probe, 2× the spacing, 2× the ray length. Total ray count *halves* each cascade. Total memory across all cascades is $\sim 2M_0$.
- $\alpha = 2$ (PoE2's choice): each cascade has 4× the rays per probe, 2× the spacing, 4× the ray length. Total ray count *stays constant*. Each cascade uses the same amount of memory.

In 3D, with α=2 you get: 4× rays per probe per level, 2× spacing each axis (so 8× fewer probes), and 4× ray length. Memory per cascade: $(4 / 8) \cdot M_{i-1} = M_{i-1} / 2$. Total memory across cascades converges to $\sim 2M_0$ — same as 2D.

**For your dissertation**: report which α you use, justify the choice (likely α=2 for consistency with PoE2 and the literature), and consider an ablation study testing different α values for quality/performance tradeoffs.

## 17. Memory Scaling Properties

The killer property of RC is that *adding more cascades is essentially free*. In 2D:

$$
\sum_{i=0}^\infty M_i = M_0 + M_0/2 + M_0/4 + \ldots = 2 M_0
$$

Adding cascade 1 costs as much as 50% of cascade 0. Adding cascades 2 through ∞ costs another 50% combined. After 4 or 5 cascades, additional levels cost almost nothing.

In 3D **dense** grids, the same convergence holds across cascades — but cascade 0 itself is enormous because it's effectively a voxelization of the entire scene. For a $1024^3$ cascade 0 grid with 16 directions per probe at 8 bytes per direction, you need $1024^3 \cdot 16 \cdot 8 = 128$ GB. Even at $256^3$ with the same parameters, you'd need 2 GB just for cascade 0. This is the wall that **sparse storage** breaks through.

In **sparse** 3D RC, the probe count is bounded by what the screen can see, not by scene volume:

$$
N_{\text{probes}} \approx W \cdot H \cdot C
$$

where $W \times H$ is screen resolution and $C$ is the number of cascade levels. For 1080p with 6 cascades, that's roughly 12 million probe slots maximum. At 16 directions × 8 bytes = 128 bytes per probe, that's 1.5 GB — already tight, but workable with conservative cascade counts (4–5) or lower base resolution.

The art of sparse RC is keeping this bound tight by ensuring probes that aren't currently contributing get evicted.

## 18. Directional Encoding: Octahedral Maps, Cubemaps, SH

Each probe stores N directional samples. How you encode them matters for memory layout, interpolation behavior, and shader performance.

### Cubemaps

The classic approach: 6 square faces of a cube. Hardware sampling support is excellent. Downside: faces have different sampling density per solid angle, and storing per-direction radiance + transmittance in a cubemap is awkward.

### Spherical Harmonics

Project the directional function onto orthonormal basis functions on the sphere. Order-2 SH = 9 coefficients. Compact, smooth, supports analytic dot product with cosine BRDF.

**Downsides for RC**:
- SH cannot represent sharp angular features (an emissive thin source becomes a blurry blob)
- Negative SH coefficients can produce non-physical results
- Mixing SH with the merge operator (which is alpha blending) is non-trivial

Not recommended for RC's interval merging.

### Octahedral Maps

Map the unit sphere to a 2D square via the octahedron projection (Cigolle et al., 2014):

```glsl
// Encode: vec3 dir -> vec2 uv
vec2 octEncode(vec3 dir) {
    dir /= (abs(dir.x) + abs(dir.y) + abs(dir.z));
    vec2 uv = (dir.z >= 0) ? dir.xy : (vec2(1) - abs(dir.yx)) * sign(dir.xy);
    return uv * 0.5 + 0.5;
}

// Decode: vec2 uv -> vec3 dir
vec3 octDecode(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec3 dir = vec3(uv.xy, 1.0 - abs(uv.x) - abs(uv.y));
    if (dir.z < 0) dir.xy = (1.0 - abs(dir.yx)) * sign(dir.xy);
    return normalize(dir);
}
```

**Properties**:
- Area-preserving (each pixel covers roughly the same solid angle)
- Continuous at the equator (good for interpolation)
- Stored as a flat 2D texture (excellent for GPU bilinear sampling)
- Trivially scales with cascade level: cascade 0 might use 4×4, cascade 1 uses 8×8, cascade 2 uses 16×16, etc.

**Octahedral maps are the standard for Radiance Cascades.** Use them.

### Per-Direction Storage Format

For each direction:
- RGB radiance (3 floats)
- 1 transmittance (1 float)
- = 16 bytes per direction at full FP32
- = 8 bytes per direction at FP16 (half precision)

FP16 is a significant memory win for cascade probes and the precision is usually adequate. Use it unless you observe banding artifacts.

## 19. Direction-First vs Probe-First Layout

When you store a cascade as a 2D texture, you can lay out the probes' octahedral maps in two ways:

### Probe-First Layout

Each octahedral map sits as a contiguous square block. Probes are tiled across the texture:

```
[Probe 0 oct]   [Probe 1 oct]   [Probe 2 oct] ...
[Probe 4 oct]   [Probe 5 oct]   [Probe 6 oct] ...
```

**Pros**: Conceptually simple. Easy to debug visually (each probe is one block).  
**Cons**: When you want to interpolate the same direction across 4 probes (for cascade merging), those 4 samples are far apart in memory.

### Direction-First Layout

Each *direction* gets its own grid of all the probes:

```
[Probe grid for direction 0]  [Probe grid for direction 1] ...
[Probe grid for direction 4]  [Probe grid for direction 5] ...
```

**Pros**: Hardware bilinear interpolation now works directly — you sample the "direction 0" block at the probe's screen UV and get the bilinearly-interpolated value across 4 adjacent probes for free.  
**Cons**: More complex addressing.

**For Sparse RC: direction-first is strongly preferred** because cascade merging needs to interpolate one direction across 4 spatial neighbors, and free hardware bilinear is a big win.

---

# Part IV — From 2D to 3D

## 20. 2D Flatland: The Cleanest Formulation

Before you implement 3D RC, you should understand 2D RC inside-out. Many of the implementations on GitHub and Shadertoy are 2D-only, and they're the ideal learning vehicle.

In 2D "flatland":

- Probes are points on a 2D plane
- "Directions" are angles around a circle, not points on a sphere
- A probe with N directions stores N rays at angles $\theta_k = 2\pi k / N$
- Cascade scaling: each level doubles N and halves probe density along each axis

Memory: cascade 0 has $P_0$ probes with $W_0$ rays each. Cascade 1 has $P_0/4$ probes with $2 W_0$ rays each (assuming α=1) or $4 W_0$ rays each (α=2). Each cascade in α=2 stores the same number of values; the total across all cascades converges to $\sim 2$× cascade 0.

The 2D rendering loop is satisfyingly simple:
1. For each cascade level (highest to lowest): trace rays per probe, write into the cascade texture
2. Merge each cascade with the previous (interpolating directions and probes)
3. The final cascade 0 holds the angle-averaged radiance, which you use for shading

Three implementations worth studying:

- **Jason Today's tutorial** (jason.today/rc) — walks through the entire concept with interactive WebGPU demos
- **Yaazarai's GMShaders article** (mini.gmshaders.com/p/radiance-cascades) — clear shader-level breakdown
- **Mathis on Shadertoy** (Radiance Cascades 3D, view ID X3XfRM) — first UV-space implementation

Build a 2D version in your repo as a warmup. Even 100 lines of compute shader will teach you the core mechanics.

## 21. Bilinear Fix and the Ringing Artifact

The most visible artifact in basic RC is **ringing**: visible rings around small bright sources. This happens because:

When merging cascade $i$ with cascade $i+1$, you interpolate bilinearly from the 4 nearest probes of cascade $i+1$. But each of those 4 probes saw the light source from a *different angle* (parallax), so they have *different occlusion patterns* for that direction. Naive interpolation can fail to occlude one contribution while occluding the others, producing a spurious extra source — visible as a faint ring.

### Important: Two Separate Techniques Share the Word "Bilinear"

Before going further, clarify which technique you mean — they have very different costs:

**Technique A — Bilinear angular filter** (always enabled, cheap): when sampling a single parent probe's octahedral map for direction $d$, do a 4-tap bilinear filter across the 4 nearest directional texels instead of nearest-neighbour. This smooths angular transitions at the coarse 4×4 octahedral resolution used by cascade 0 and is essentially free. The `sampleParentProbe` function in `cascade_merge.comp` implements this already.

**Technique B — The full bilinear fix** (parallax correction, expensive): the technique described in the rest of this section. It addresses ringing caused by the fact that nearby parent probes occlude the same light source differently from their different spatial positions. It requires re-tracing or pre-storing multiple near-intervals per probe. Do not confuse it with the angular filter above.

### The Full Bilinear Fix

Instead of merging the cascade-$i$ interval with the interpolated cascade-$i+1$ value, trace **separate cascade-$i$ rays** toward each surrounding parent probe's starting position, merge each one independently with its corresponding cascade-$i+1$ ray, then average the results with trilinear weights.

In 2D (4 surrounding parent probes):

```glsl
vec4 merged = vec4(0);
for (int p = 0; p < 4; p++) {
    vec3 parentStart = getProbePosition(i+1, parentIndex[p]) + parentRay[p].start;
    vec4 myRay = trace(myPos, parentStart - myPos);
    vec4 parentRay = sampleCascade(i+1, parentIndex[p], directionIndex);
    merged += bilinearWeight[p] * merge(myRay, parentRay);
}
```

In **3D**, there are **8 surrounding parent probes** (trilinear corners), so the loop runs 8 times — and each of those 8 near-intervals must be either re-traced or pre-stored. The canonical cost in 3D is therefore **8× the cascade_trace storage and dispatch budget**, not the 4× figure quoted for the 2D case.

**Cost**: 8× cascade_trace storage and dispatch (3D); 4× in 2D.  
**Benefit**: Eliminates ringing; also fixes "light leaking" when occluders are smaller than the probe spacing.

### Implementation Strategy

In a pre-traced system (where `cascade_trace.comp` already ran), the full bilinear fix requires `cascade_trace` to store 8 near-intervals per (probe, direction) — one aimed toward each parent probe's spatial corner. This is an architectural extension to the trace pass, not just a merge-pass change.

**For the showcase**: the bilinear angular filter (Technique A) is always active. Skip the full bilinear fix — if ringing appears in your scenes, document it as a known limitation of single-interval tracing. Bounded indoor scenes with few small lights are unlikely to exhibit strong ringing.

**For the dissertation**: implement the full bilinear fix as a compile-time option (`#define BILINEAR_FIX`), extend `cascade_trace` to store 8 near-intervals per probe, and measure FPS impact and RMSE reduction against the reference path tracer. This is a standalone, measurable contribution.

## 22. The 3D Memory Problem

In 3D **dense** grids, the memory math is harsh. For a scene volume of $V \times V \times V$ with cascade 0 probe spacing $\Delta_p$:

- Probes per cascade 0: $(V / \Delta_p)^3$
- With $V = 100\text{m}$ and $\Delta_p = 0.5\text{m}$: $200^3 = 8$M probes
- Each probe at 16 directions × 8 bytes FP16: 128 bytes
- Total cascade 0: 1 GB

That's *just* cascade 0, and it's already eating most of your VRAM budget. Higher cascades have lower probe density so they're not the problem, but cascade 0 alone is enough to kill the technique on consumer hardware.

Memory analysis from the original paper: even though all cascades combined only cost $\sim 2 \times$ cascade 0, "storing a cascade 0 is practically equivalent to voxelizing the entire scene -- this is often a 'dealbreaker' for large-scale scenes."

This is the wall that motivates every 3D variant. They all share one observation: **you don't actually need probes everywhere — you only need probes that contribute to the current view**.

## 23. The 3D Variants Landscape

Five distinct approaches to 3D RC have emerged in the community as of 2026:

### Variant A: Screen-Space Probes, Screen-Space Rays (PoE2)
- Probes live on the screen at fixed pixel spacing
- Rays traced through the depth buffer
- **Pros**: Fastest. PoE2 ships this at ~3ms on a GTX 1050.
- **Cons**: No off-screen indirect light. Ghosting on camera motion. Top-down games suit this; first-person games don't.

### Variant B: SPWI — Screen-Space Probes, World-Space Intervals
- Probes still on the screen, but rays traced through world space (via BVH or SDF)
- **Pros**: Captures off-screen light. The approach in the Chalmers thesis you read.
- **Cons**: Probes still attached to screen pixels, so they re-allocate every frame → temporal instability. Memory still scales with screen × cascades.

### Variant C: Dense 3D World-Space Grid
- Probes in a uniform 3D grid throughout the scene
- **Pros**: Maximum stability. Probes don't move when the camera moves.
- **Cons**: Cubic memory. Impractical for large scenes.

### Variant D: Surfel RC
- Probes placed on surfaces (one per "surfel")
- **Pros**: Memory scales with surface area, not volume
- **Cons**: Surfel spawning/recycling is complex. Coverage gaps. Temporal management is hard.

### Variant E: UV-Space RC
- Probes placed in mesh UV space
- **Pros**: Memory follows surface; very stable under camera motion
- **Cons**: Requires good UV unwrapping. Cannot probe inside volumes (only on surfaces).

### Variant F: Sparse 3D RC (Sannikov, Nov 2025) — **Our Target**
- World-space probes, but stored in a GPU hash map keyed by world position + cascade level
- Per-pixel cascade level selection based on screen-projected area
- **Pros**: Near-constant memory regardless of scene size. True 3D. Captures off-screen light. No upscaling needed.
- **Cons**: New (no public paper yet). Hash map adds complexity. Probes still tied to view-visible regions.

## 24. Why Sparse Radiance Cascades

For a dissertation on **hardware-agnostic, real-time, 3D GI**, Sparse RC is the most defensible choice because:

1. **It's genuinely 3D** (unlike SPWI which collapses to screen probes)
2. **Memory is bounded** (unlike dense 3D)
3. **It captures off-screen indirect light** (unlike PoE2's screen-space approach)
4. **It's the newest variant from the algorithm's inventor** (Sannikov, Nov 2025), positioning your work at the frontier
5. **It works with any ray-tracing backend** (software BVH, hardware RT, SDF ray-march) — perfect for the "hardware-agnostic" angle

The contribution your dissertation can claim: **the first documented Vulkan implementation of Sparse 3D Radiance Cascades with quantitative benchmarks against DDGI and SSGI**.

---

# Part V — Sparse Radiance Cascades

## 25. The Core Insight: Screen-Bound Probe Allocation

Sannikov's central observation for Sparse 3D RC, stated in his own words from the Nov 2025 demo:

> "World-space resolution of radiance interval tiles is chosen per pixel, such that the area of each tile is near-constant when projected on the screen, hence the total number of tiles in memory is near-constant as well."

Translating this:

- A "tile" = a probe at a particular cascade level
- Each visible pixel needs *one* probe at *some* cascade level to be sampled at that pixel's depth
- The cascade level is chosen so that the probe's world-space size projects to ~1 pixel on screen at that depth
- Pixels close to the camera need cascade 0 probes (small in world space, dense)
- Pixels far from the camera need higher-cascade probes (large in world space, sparse)

The result: total probe count is approximately $W \times H \times C$ (screen pixels × cascade count), not $V^3$ (scene volume cubed).

Crucially, **probes are still in world space** — they don't move when the camera pans across a static scene. A point in the world that's been probed once will keep being found in the hash map until it falls out of view long enough to be evicted.

### The Per-Pixel Cascade Level Decision

For a pixel with world position $\mathbf{p}$ and distance $d$ from the camera, the cascade level $L$ is chosen such that the probe spacing at level $L$ (which is $\Delta_p \cdot 2^L$ in world space) projects to approximately 1 screen pixel:

$$
L = \log_2\left(\frac{d \cdot \text{pixelSize}}{\Delta_p \cdot \text{focalLength}}\right)
$$

Where $\text{pixelSize} = \tan(\text{FOV}/2) / (H/2)$ in screen-relative terms. Clamp $L$ to $[0, C-1]$ where $C$ is the maximum cascade count.

This formula is computed in a compute shader pass that runs once per visible pixel, producing a "desired cascade level" per pixel.

## 26. GPU Hash Map Storage

The hash map keys probes by `(world_position_quantized, cascade_level)` and stores their octahedral radiance maps as values.

### Quantization

Quantize the world position to the probe spacing at each cascade level:

```glsl
ivec3 probeKey(vec3 worldPos, int cascadeLevel) {
    float spacing = baseSpacing * pow(2.0, float(cascadeLevel));
    return ivec3(floor(worldPos / spacing));
}
```

Two pixels that map to the same cell at the same cascade level share the same probe. This is the entire point.

### Hashing

A good 3D hash for integer coordinates:

```glsl
uint hash3(ivec3 key, int level) {
    uint h = uint(key.x) * 73856093u;
    h ^= uint(key.y) * 19349663u;
    h ^= uint(key.z) * 83492791u;
    h ^= uint(level) * 2654435761u;
    return h;
}
```

These constants are from Teschner et al. 2003 ("Optimized Spatial Hashing for Collision Detection of Deformable Objects"). They produce well-distributed hashes for 3D integer coordinates.

### Storage Layout

The hash map is two GPU buffers:

```cpp
struct ProbeSlot {
    ivec3 key;
    int level;
    uint generation;  // for eviction
    uint probeIndex;  // index into ProbeDataBuffer
};

// Open-addressed hash table, fixed capacity
StructuredBuffer<ProbeSlot> probeIndex[CAPACITY];

// Actual radiance data, separately indexed
StructuredBuffer<vec4> probeData[CAPACITY * OCT_RES * OCT_RES];
```

For 64K probes with 8×8 octahedral maps at 16 bytes/texel: 64K × 64 × 16 = 64 MB. Very tractable.

### Insertion (Linear Probing with Atomic CAS)

```glsl
bool insertProbe(ivec3 key, int level, out uint slotIndex) {
    uint h = hash3(key, level) % CAPACITY;
    for (int probe = 0; probe < MAX_PROBE_STEPS; probe++) {
        uint idx = (h + probe) % CAPACITY;
        
        // Try to claim empty slot
        uint expected = INVALID_KEY;
        uint result = atomicCompSwap(probeIndex[idx].keyHash, expected, hash3(key, level));
        
        if (result == expected || result == hash3(key, level)) {
            // We claimed it (or it was already ours)
            probeIndex[idx].key = key;
            probeIndex[idx].level = level;
            probeIndex[idx].generation = currentFrame;
            slotIndex = idx;
            return true;
        }
    }
    return false; // hash table full
}
```

Notes on this implementation:
- `atomicCompSwap` (GLSL: `atomicCompSwap()`) is mandatory for thread safety
- `MAX_PROBE_STEPS` is typically 8–32; beyond that, the table is too full
- We store both the keyHash *and* the full key — the hash lets us do the atomic CAS, the key disambiguates collisions
- The `generation` field tracks the last frame this probe was used, enabling LRU eviction

### Lookup

```glsl
bool lookupProbe(ivec3 key, int level, out uint slotIndex) {
    uint h = hash3(key, level) % CAPACITY;
    for (int probe = 0; probe < MAX_PROBE_STEPS; probe++) {
        uint idx = (h + probe) % CAPACITY;
        if (probeIndex[idx].keyHash == hash3(key, level) &&
            probeIndex[idx].key == key &&
            probeIndex[idx].level == level) {
            slotIndex = idx;
            return true;
        }
        if (probeIndex[idx].keyHash == INVALID_KEY) return false; // empty slot ends search
    }
    return false;
}
```

## 27. Per-Pixel Cascade Level Selection

The "allocation pass" is a compute shader that runs across visible pixels (typically downsampled — say at 1/2 or 1/4 resolution, since cascade allocations don't need pixel precision):

```glsl
void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy) * STRIDE;
    if (pixel.x >= screenSize.x || pixel.y >= screenSize.y) return;
    
    // Read G-buffer
    float depth = texelFetch(gDepth, pixel, 0).r;
    if (depth == 0.0) return; // sky pixel
    vec3 worldPos = reconstructWorldPos(pixel, depth);
    
    // Compute cascade level for this pixel
    float d = length(worldPos - cameraPos);
    float spacingPx = baseSpacing * focalLength / d;  // probe spacing in pixels at this depth
    float pixelsPerScreen = 1.0;
    float ratio = pixelsPerScreen / spacingPx;
    int level = clamp(int(log2(ratio)), 0, MAX_CASCADE_LEVELS - 1);
    
    // Insert this probe into the hash map
    ivec3 key = probeKey(worldPos, level);
    uint slotIndex;
    insertProbe(key, level, slotIndex);
    
    // Optionally: also insert the parent probes (one level up, 8 of them in 3D)
    // This pre-allocates the merge targets for the trace pass
    ivec3 parentKey = key / 2;
    for (int dx = 0; dx <= 1; dx++)
        for (int dy = 0; dy <= 1; dy++)
            for (int dz = 0; dz <= 1; dz++) {
                if (level + 1 < MAX_CASCADE_LEVELS)
                    insertProbe(parentKey + ivec3(dx, dy, dz), level + 1, slotIndex);
            }
}
```

After this pass, the hash map contains entries for every probe that needs to be traced this frame, at every cascade level needed.

## 28. Probe Lifecycle: Allocation, Insertion, Eviction

### Allocation
Every frame, the allocation pass touches probes for currently-visible pixels, marking them with the current frame's generation number.

### Eviction
Probes not touched for $N$ frames are considered stale and can be overwritten. Eviction happens implicitly: when a new probe needs to be inserted and the linear probing chain finds a stale slot (where `generation < currentFrame - N`), it overwrites it.

```glsl
// In insertProbe, before atomicCompSwap:
if (probeIndex[idx].generation < currentFrame - STALE_THRESHOLD) {
    // This slot is stale; we can claim it
    atomicCompSwap(probeIndex[idx].keyHash, expected, hash3(key, level));
    // ... reset the probe data
}
```

### Hash Table Sizing
Capacity should be ~2× the expected probe count to keep probing chains short. For 1080p with 5 cascades and conservative spacing, expected probe count is ~500K; allocate 1M slots. This is a tunable parameter; expose it in your ImGui debug interface.

### Sannikov's Note on Cache Misses
From his demo description: "tiles are not only cached by their world-space position, but they're also cached by their desired resolution to keep the computational load constant, so if a tile of the needed size is missing, it will render as missing."

This is a feature, not a bug. The graceful degradation under capacity pressure is what makes the technique robust. When the hash table fills up, probes that haven't been needed yet simply aren't computed, and surfaces depending on them render with cascade-1 fallback (one level coarser than ideal).

## 29. The Cascade Merging Pass (Top-Down)

After all probes have been traced, you merge them top-down (highest cascade to lowest):

```
for level = MAX_LEVEL down to 0:
    for each probe P at cascade level in hash map:
        for each direction D in P.octahedralMap:
            // Find the 8 parent probes at level+1 surrounding P
            ivec3 parentBase = P.key / 2;
            vec4 parentInterval = vec4(0);
            for (each of 8 parents):
                ivec3 pkey = parentBase + offset;
                if (lookupProbe(pkey, level+1, slot)):
                    vec2 parentDirUV = octEncode(D);  // same direction
                    vec4 sampled = sampleOctahedralBilinear(slot, parentDirUV);
                    parentInterval += trilinearWeight * sampled;
            
            P.octahedralMap[D] = merge(P.octahedralMap[D], parentInterval);
```

The result: cascade 0 probes contain the *full hemispherical radiance to infinity*, properly merged across all cascades.

### Why Top-Down?

The radiance from far away has to pass *through* the near segments to reach the probe. So you start with the farthest radiance (highest cascade), then attenuate it through each closer segment as you descend. The merge formula is non-commutative — you cannot merge bottom-up.

## 30. Depth-Aware Interpolation: Bilateral and "Bilinear 3D"

Probes at different depths around a depth discontinuity (an edge or silhouette) should not blend together — that would cause light to leak across geometry. Two approaches:

### Bilateral Filtering

Compute trilinear weights normally, then attenuate them by depth difference:

```glsl
float weights[8] = computeTrilinearWeights(probePos);
float currentDepth = ...;
for (int p = 0; p < 8; p++) {
    float depthDiff = abs(probeDepth[p] - currentDepth);
    weights[p] *= 1.0 / (1.0 + depthDiff * DEPTH_SCALE);
}
normalize(weights);
```

This works adequately for moderate depth discontinuities but introduces visible striations at steep angles (the Chalmers thesis Section 4.5.1 documents this).

### Sannikov's "Bilinear 3D" (Shadertoy 4XXSWS)

A more rigorous approach: perform the trilinear interpolation directly in 3D world space, iteratively refining the interpolation ratios so the target point projects correctly onto the interpolation lines.

```glsl
vec3 getBilinear3DRatio(vec3 srcPoints[8], vec3 dstPoint, vec3 initialRatio, int iterations) {
    vec3 ratio = initialRatio;
    for (int i = 0; i < iterations; i++) {
        // Refine X
        vec3 lineStart = mix(mix(srcPoints[0], srcPoints[4], ratio.z),
                             mix(srcPoints[2], srcPoints[6], ratio.z), ratio.y);
        vec3 lineEnd   = mix(mix(srcPoints[1], srcPoints[5], ratio.z),
                             mix(srcPoints[3], srcPoints[7], ratio.z), ratio.y);
        ratio.x = projectPointOntoLine(lineStart, lineEnd, dstPoint);
        // Refine Y similarly
        // Refine Z similarly
    }
    return ratio;
}
```

The Chalmers thesis Section 4.5.2 has a working implementation. For your dissertation, implement both bilateral and bilinear-3D, compare them quantitatively. The visual difference is striking when probes straddle steep surfaces (the floor-meets-wall case is the textbook example).

### Min-Max Probes (Section 33)

A third approach is to store *two probes per cell* — one at the minimum-depth surface, one at the maximum — and interpolate trilinearly between them. This is more memory but gives the cleanest results at hard depth edges. See Section 33.

## 31. The Final Gather Pass

Once cascade 0 holds the full radiance field, you compute the per-pixel indirect lighting by sampling cascade 0 at each visible pixel and integrating over the hemisphere weighted by the cosine BRDF:

```glsl
vec3 finalGather(vec3 worldPos, vec3 normal, vec3 albedo) {
    // Find the 8 nearest cascade-0 probes
    int level = 0;
    ivec3 baseKey = probeKey(worldPos, level);
    
    vec3 totalIrradiance = vec3(0);
    float totalWeight = 0;
    
    for (int p = 0; p < 8; p++) {
        ivec3 pkey = baseKey + offsets[p];
        uint slot;
        if (!lookupProbe(pkey, level, slot)) continue;
        
        vec3 probePos = pkey * baseSpacing + halfSpacing;
        float spatialWeight = trilinearWeight(probePos, worldPos, level);
        
        // Integrate over the hemisphere
        vec3 probeIrradiance = vec3(0);
        for (int d = 0; d < OCT_RES * OCT_RES; d++) {
            vec2 uv = (vec2(d % OCT_RES, d / OCT_RES) + 0.5) / float(OCT_RES);
            vec3 dir = octDecode(uv);
            float cosTheta = max(0.0, dot(dir, normal));
            vec3 radiance = sampleProbe(slot, uv).rgb;
            probeIrradiance += radiance * cosTheta;
        }
        probeIrradiance *= 2.0 * PI / float(OCT_RES * OCT_RES);
        
        totalIrradiance += spatialWeight * probeIrradiance;
        totalWeight += spatialWeight;
    }
    
    return albedo * (totalIrradiance / max(totalWeight, 0.0001));
}
```

This is the diffuse BRDF integration. The factor $2\pi$ accounts for the hemisphere solid angle.

For mirror-like surfaces (high reflection), you'd query a single direction (the reflected view ray) instead of integrating — but RC has limited angular resolution at cascade 0, so sharp specular reflections will be soft. For this reason, most RC implementations only use it for the diffuse term and handle specular with a separate technique (planar/SSR/ray-traced reflections).

## 32. Pre-Averaging Optimization

A simple memory/quality trick from the Chalmers thesis (Section 5.1):

**Idea**: Trace 4 probes per neighborhood at each cascade level, then average them into 1 stored probe.

**Benefit**: You get the angular-domain quality of a 2× denser probe grid without the memory cost.

**Implementation**: Use GPU subgroup operations (`subgroupShuffle` in GLSL, `simdShuffle` in Metal) to share probe trace results between adjacent threads in the same warp, then average and store one result per 4-thread group.

```glsl
// Each lane in the subgroup is a different probe in a 2x2 group
vec4 myResult = traceProbe(myWorldPos, myDirection);

// Pull results from neighbors via subgroup ops
vec4 r0 = subgroupShuffle(myResult, baseLane + 0);
vec4 r1 = subgroupShuffle(myResult, baseLane + 1);
vec4 r2 = subgroupShuffle(myResult, baseLane + 2);
vec4 r3 = subgroupShuffle(myResult, baseLane + 3);
vec4 avg = (r0 + r1 + r2 + r3) * 0.25;

// Only lane 0 writes the result
if (localProbeIndex == 0) writeProbe(avg);
```

For Sparse RC this is harder to apply directly because probes aren't in a regular grid — they're hash-keyed. But the idea still applies: when multiple pixels in a small area all want the same cascade-0 probe at the same world location, only trace it once.

## 33. Min-Max Probes (Advanced Stabilization)

A relatively recent idea from the community (documented in the Chalmers thesis Section 7.1 as future work) for fixing depth-edge artifacts:

**Idea**: For each probe slot, store *two* probes — one at the minimum depth and one at the maximum depth within the probe's footprint. When sampling, trilinearly interpolate between min and max based on the query point's depth.

**Implementation requires a "min-max depth buffer"**:

```glsl
// Build hierarchical min-max depth pyramid
void buildMinMaxLevel(uvec2 dstCoord) {
    uvec2 srcCoord = dstCoord * 2;
    vec2 d0 = texelFetch(prevLevel, srcCoord + uvec2(0,0), 0).rg;
    vec2 d1 = texelFetch(prevLevel, srcCoord + uvec2(1,0), 0).rg;
    vec2 d2 = texelFetch(prevLevel, srcCoord + uvec2(0,1), 0).rg;
    vec2 d3 = texelFetch(prevLevel, srcCoord + uvec2(1,1), 0).rg;
    vec2 result = vec2(
        min(min(d0.r, d1.r), min(d2.r, d3.r)),  // min channel
        max(max(d0.g, d1.g), max(d2.g, d3.g))   // max channel
    );
    imageStore(thisLevel, dstCoord, vec4(result, 0, 0));
}
```

At each cascade level, you sample the mip level whose resolution matches the cascade's probe grid. The min/max values tell you whether the probe's footprint contains a depth discontinuity. If min and max are close, use one probe; if they differ significantly, use two.

**Performance impact**: doubles the ray-tracing workload. To compensate, you can split the rays — half go to min probes, half to max probes — keeping total ray count constant.

**For the dissertation**: implement this as a quality option, present it in the "depth-edge artifact reduction" section, with before/after screenshots.

---

# Part VI — Holographic Radiance Cascades (Stretch Goal)

## 34. Why HRC Exists: Small-Penumbra Failure of Standard RC

Standard RC has a known failure mode documented in Freeman et al. (2025): when a *small* light source is *far* from the camera *and* occluded by something close to the camera, the penumbra cannot be resolved.

Concretely: let the base probe spacing be $A$, the base ray length be $B$. Consider a light of size $Y$ at distance $X \cdot B$, occluded by something at distance $X B / 2$.

- The penumbra at the viewer's position has width $\approx Y$
- The cascade that resolves the occluder is at level $\log_4(X)$, with probe spacing $\sqrt{X} \cdot A$
- If $\sqrt{X} \cdot A > Y$, the penumbra cannot be interpolated — artifacts appear

This is the "ringing around small distant lights" failure mode that's visible in some RC scenes.

## 35. The Anisotropic Probe Layout

HRC's fix: instead of halving probe spacing in *both* axes per cascade level, halve it only in the *direction parallel to the probe's facing*. Probes maintain high spatial resolution *perpendicular* to the angles they're gathering from.

In 2D, this means:

- The world is split into 4 quadrants by angle (each spanning π/2)
- For the +X quadrant, probes at cascade $n$ are at positions $(x \cdot 2^n, y)$ — sparse in X, dense in Y
- The 4 quadrants are processed independently and summed at the end

The geometric justification: a penumbra that varies fast in *both* directions must come from a *nearby* source, which lower cascades already handle. A penumbra from a distant source can only vary fast in one direction (perpendicular to its line of sight).

## 36. The Acceleration Structure for Ray Combination

HRC adds a second hierarchy on top of RC: an acceleration structure $T_n$ that approximates long rays by combining short ones.

For each cascade level $n$, $T_n(\mathbf{p}, k)$ stores the radiance and transmittance of a ray from $\mathbf{p}$ in direction $k$. Higher levels are built by *combining* two lower-level rays:

$$
T_{n+1}(\mathbf{p}, 2k) = \mathcal{M}(T_n(\mathbf{p}, k), T_n(\mathbf{p} + v_n(k), k))
$$

This eliminates the need to trace long rays through a BVH or SDF — the acceleration structure replaces conventional ray tracing entirely. Performance becomes independent of scene complexity (no BVH cost), enabling efficient rendering of detailed volumetrics.

## 37. 3D HRC Limitations (the N⁴ Memory Wall)

The HRC paper notes that 3D HRC has "poor scaling, taking up $N^4$ memory for an $N \times N \times N$ scene." Figure 15 in the paper shows an experimental 128³ voxel 3D HRC implementation — feasible but tight.

**For your dissertation**: HRC is best positioned as **future work** or a **stretch goal in the conclusion**. Mention it as a known improvement that resolves a specific failure mode (small distant penumbras), reference Freeman et al. 2025, and outline how it would integrate. Don't promise to implement 3D HRC for the dissertation submission unless you have abundant time after the core Sparse RC implementation is working and benchmarked.

---

# Part VII — Vulkan Implementation

## 38. Pipeline Architecture for Sparse RC

The complete Sparse RC frame consists of these compute passes, in order:

```
[1] Primary Visibility Pass
    Input:  Scene geometry (TLAS or software BVH)
    Output: G-buffer (depth, world-pos, normal, material-ID, emissive)
    Type:   Compute (or fragment, if you prefer raster primary visibility)

[2] Probe Allocation Pass
    Input:  G-buffer depth, camera params
    Output: Hash map populated with probe entries for this frame
    Type:   Compute

[3] Probe Ray Tracing Pass (one dispatch per cascade level)
    Input:  Hash map, scene BVH/AS, emissive materials
    Output: Radiance + transmittance per direction per probe
    Type:   Compute, dispatched 6× (once per cascade level)

[4] Cascade Merge Pass (one dispatch per cascade level, top-down)
    Input:  Hash map, cascade-i+1 probes
    Output: Cascade-i probes containing merged radiance
    Type:   Compute, dispatched 5× (cascade-N-1 down to cascade-0)

[5] Final Gather Pass
    Input:  G-buffer, cascade-0 probes, direct lights
    Output: HDR color buffer with full lighting
    Type:   Compute

[6] Tonemap + Composite Pass
    Input:  HDR color buffer
    Output: Swapchain image
    Type:   Compute (or fragment if simpler for swapchain)
```

Total compute passes per frame: ~13 (1 + 1 + 6 + 5 + 1 + 1). Each dispatch is small and well-defined; this is exactly the kind of workload modern GPUs eat for breakfast.

## 39. Required Vulkan Features and Extensions

### Minimum (software RT path)

```cpp
// API version
appInfo.apiVersion = VK_API_VERSION_1_3;  // 1.2 minimum, but 1.3 is cleaner

// Device extensions
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,  // for bindless probe arrays
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,  // for debug printf
};

// Features
VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
VkPhysicalDeviceVulkan12Features v12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
v12.descriptorIndexing = VK_TRUE;
v12.runtimeDescriptorArray = VK_TRUE;
v12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
v12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
v12.descriptorBindingPartiallyBound = VK_TRUE;
v12.bufferDeviceAddress = VK_TRUE;  // for hash map pointer chasing

VkPhysicalDeviceVulkan13Features v13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
v13.synchronization2 = VK_TRUE;  // cleaner pipeline barriers
v13.dynamicRendering = VK_TRUE;  // avoid the VkRenderPass dance

v12.pNext = &v13;
features2.pNext = &v12;
vkGetPhysicalDeviceFeatures2(physDev, &features2);

// Subgroup features (for pre-averaging)
VkPhysicalDeviceSubgroupProperties subgroupProps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
// Need: VK_SUBGROUP_FEATURE_SHUFFLE_BIT, VK_SUBGROUP_FEATURE_ARITHMETIC_BIT
```

### Optional (hardware RT path, future upgrade)

```cpp
deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
deviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{...};
asFeatures.accelerationStructure = VK_TRUE;

VkPhysicalDeviceRayQueryFeaturesKHR rqFeatures{...};
rqFeatures.rayQuery = VK_TRUE;
```

Plug these into the `pNext` chain. The shaders then enable `#extension GL_EXT_ray_query : require`.

## 40. Compute Pipelines and Descriptor Indexing

### Per-Pass Pipelines

Each compute pass is its own `VkPipeline` with its own shader. They share descriptor set layouts where possible.

```cpp
class ComputePass {
public:
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSet descriptorSet;
    
    void dispatch(VkCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z);
};

class RenderGraph {
    ComputePass primaryVisibility;
    ComputePass probeAllocation;
    ComputePass probeTrace[MAX_CASCADES];
    ComputePass cascadeMerge[MAX_CASCADES - 1];
    ComputePass finalGather;
    ComputePass tonemap;
};
```

### Bindless Probe Arrays

Cascade probes are stored in arrays of buffers, accessed via descriptor indexing:

```glsl
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 5) readonly buffer ProbeBuffers {
    vec4 data[];  // octahedral map values
} probes[MAX_CASCADES];

// In shader:
int level = ...;
vec4 sample = probes[nonuniformEXT(level)].data[probeIndex * OCT_RES * OCT_RES + dirIdx];
```

The `nonuniformEXT()` qualifier tells the driver the index varies per thread.

## 41. Buffer Layout for Cascades

### Hash Map Buffers

```cpp
// Slot metadata (key + generation)
struct ProbeSlotMeta {
    glm::ivec3 key;
    int32_t level;
    uint32_t keyHash;      // for atomic CAS
    uint32_t generation;   // last-touched frame
    uint32_t pad[2];       // align to 32 bytes
};

// Storage:
VkBuffer probeSlotBuffer;       // CAPACITY * sizeof(ProbeSlotMeta)
VkBuffer probeDataBuffer;       // CAPACITY * OCT_RES * OCT_RES * sizeof(vec4)
```

For 64K probes, 8×8 octahedral, FP32 RGBA: 64K × 32 + 64K × 64 × 16 = 2 MB + 64 MB = 66 MB total for the cascade storage. Comfortable.

Use FP16 (`VK_FORMAT_R16G16B16A16_SFLOAT`) for `probeDataBuffer` to halve this: 33 MB.

### G-Buffer

```cpp
VkImage gPosition;      // RGBA32F: world position + depth in alpha
VkImage gNormal;        // RGBA16F: normal + roughness
VkImage gMaterial;      // RGBA8: albedo (RGB) + metallic (A)
VkImage gEmissive;      // RGBA16F: emissive
```

At 1080p, this is ~50 MB. Tight but acceptable on a GTX 1080 (8 GB).

### HDR Color Buffer

```cpp
VkImage hdrColor;       // RGBA16F: HDR lit color before tonemapping
```

~16 MB at 1080p.

## 42. GPU Hash Map: Implementation Strategies

### Strategy A: Open Addressing with Linear Probing (recommended)

- Simple to implement
- Good cache locality
- Acceptable performance for our load factors (<0.5)
- Used by NVIDIA's spatial hashing in their reservoir resampling papers

### Strategy B: Cuckoo Hashing

- Two hash functions; each key has two possible slots
- Better worst-case lookup time
- More complex insertion logic

### Strategy C: Direct Address with Multi-Level Quantization

- Bypass the hash entirely
- Use a sparse voxel structure (VDB-like, see OpenVDB / NanoVDB)
- More complex to update on GPU

**For the dissertation: implement Strategy A.** Document it well. Mention B and C as alternatives in the discussion section.

### Concurrency Considerations

Multiple threads may try to insert the same probe in the same frame. The CAS-based insertion handles this correctly:

1. Thread A computes `key = (x, y, z), level = 2`
2. Thread B computes the same key
3. Both compute the same `keyHash`
4. Both attempt `atomicCompSwap` on the same slot
5. One wins; the other observes the existing value matches its target → both proceed correctly

The slight overhead is acceptable. Don't overthink this.

## 43. Synchronization Between Passes

Each pass writes to buffers/images that subsequent passes read. You need pipeline barriers between them.

### Synchronization2 (Vulkan 1.3)

```cpp
VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
dep.memoryBarrierCount = 1;
dep.pMemoryBarriers = &barrier;
vkCmdPipelineBarrier2(cmd, &dep);
```

Insert this barrier between every cascade pass. Easy and correct.

### Image Layout Transitions

Between the final gather pass and the swapchain present:

```cpp
// Transition swapchain image from UNDEFINED to GENERAL for compute write
VkImageMemoryBarrier2 imgBarrier{...};
imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
imgBarrier.image = swapchainImages[imageIndex];
// ... etc
```

## 44. Building a Clean Vulkan Renderer From Zero

If you decide to build a fresh Vulkan engine for the dissertation (separate from your teammate's showcase code), here is the structure I'd recommend. This is *not* about refactoring the existing code — it's about what a "from-zero" implementation would look like.

### Project Structure

```
src/
├── core/
│   ├── VulkanContext.{hpp,cpp}      // instance, device, queues, allocator
│   ├── Swapchain.{hpp,cpp}          // swapchain + image acquisition
│   ├── CommandPool.{hpp,cpp}        // per-frame command buffers
│   ├── DescriptorAllocator.{hpp,cpp}// bindless descriptor management
│   └── MemoryAllocator.{hpp,cpp}    // VMA wrapper
├── resources/
│   ├── Buffer.{hpp,cpp}             // VkBuffer + VMA allocation
│   ├── Image.{hpp,cpp}              // VkImage + view + sampler bundles
│   ├── ShaderModule.{hpp,cpp}       // SPIR-V loader
│   └── Pipeline.{hpp,cpp}           // VkPipeline + layout
├── scene/
│   ├── Scene.{hpp,cpp}              // scene graph
│   ├── Mesh.{hpp,cpp}               // vertex/index buffers + BLAS
│   ├── Material.{hpp,cpp}           // material params + texture refs
│   ├── BVHBuilder.{hpp,cpp}         // SAH BVH construction
│   └── ModelLoader.{hpp,cpp}        // OBJ/GLTF loading
├── gi/
│   ├── GIStrategy.{hpp}             // abstract base for GI techniques
│   ├── SparseRC/                    // your main contribution
│   │   ├── SparseRCRenderer.{hpp,cpp}
│   │   ├── ProbeAllocator.{hpp,cpp}
│   │   ├── ProbeTracer.{hpp,cpp}
│   │   ├── CascadeMerger.{hpp,cpp}
│   │   ├── FinalGather.{hpp,cpp}
│   │   └── HashMap.{hpp,cpp}
│   ├── DDGI/                        // baseline 1
│   │   └── DDGIRenderer.{hpp,cpp}
│   ├── SSGI/                        // baseline 2
│   │   └── SSGIRenderer.{hpp,cpp}
│   └── ReferencePathTracer.{hpp,cpp}// ground truth
├── shaders/
│   ├── common/                      // shared GLSL headers
│   ├── visibility/
│   ├── rc/
│   ├── ddgi/
│   ├── ssgi/
│   └── path_tracer/
├── debug/
│   ├── ImGuiLayer.{hpp,cpp}         // ImGui integration
│   ├── ProfilerOverlay.{hpp,cpp}    // per-pass GPU timings
│   └── ProbeVisualizer.{hpp,cpp}    // visualize probe placement
└── main.cpp
```

### Key Design Principles

1. **RAII everywhere**: every Vulkan object lives in a wrapper class whose destructor calls the appropriate `vkDestroy*`. No naked handles in app code.

2. **VMA (Vulkan Memory Allocator)**: use the official allocator. Hand-rolling memory management for a dissertation is wasted effort. Get it from GPUOpen-LibrariesAndSDKs.

3. **One frame in flight, simple synchronization**: don't overengineer triple-buffering for a dissertation prototype. One frame in flight, present after every render, simpler is faster to debug.

4. **Hot-reload shaders**: spend 1 day implementing shader hot-reload. You'll spend 100 days iterating on shaders. Pays back 100×.

5. **Push constants for per-pass uniforms**: don't fight with descriptor sets for small uniform data. Push constants are zero-overhead for ≤128 bytes.

6. **Bindless descriptor table for textures and buffers**: single descriptor set with arrays of every texture and every buffer in the scene. Index by integer in shaders. Modern Vulkan idiom; vastly simpler than the per-frame descriptor dance.

## 45. Modular Class Architecture

The single most important design decision for your dissertation is the **GI Strategy** interface:

```cpp
// gi/GIStrategy.hpp
class GIStrategy {
public:
    virtual ~GIStrategy() = default;
    
    // Called once on scene load
    virtual void initialize(const Scene& scene, const VulkanContext& ctx) = 0;
    
    // Called every frame; returns timing info for benchmarking
    virtual GIFrameStats render(VkCommandBuffer cmd, 
                                const GBuffer& gbuffer,
                                const Camera& cam,
                                Image& outIndirect) = 0;
    
    // Configuration via ImGui
    virtual void drawDebugUI() = 0;
    
    virtual const char* name() const = 0;
};

struct GIFrameStats {
    float totalMs;
    std::map<std::string, float> passMs;  // per-pass breakdown
    size_t vramBytes;
    int rayCount;
};
```

Then concrete implementations:

```cpp
class SparseRCRenderer : public GIStrategy { ... };
class DDGIRenderer : public GIStrategy { ... };
class SSGIRenderer : public GIStrategy { ... };
class ReferencePathTracer : public GIStrategy { ... };
class NoGI : public GIStrategy { ... }; // direct lighting only
```

The main render loop:

```cpp
GIStrategy* currentGI = &sparseRC;  // switchable via ImGui

void renderFrame() {
    primaryVisibility.render(cmd, scene, camera, gbuffer);
    Image indirectLight;
    GIFrameStats stats = currentGI->render(cmd, gbuffer, camera, indirectLight);
    finalComposite.render(cmd, gbuffer, indirectLight, hdrOut);
    tonemap.render(cmd, hdrOut, swapchain);
    
    profilerOverlay.recordStats(currentGI->name(), stats);
}
```

This gives you a clean ablation framework: change the GI module, everything else stays identical, benchmarks are directly comparable. **This is your dissertation's experimental backbone.**

## 46. The ImGui Debug Interface

Essential debug panels:

```cpp
void ImGuiDebug::draw() {
    if (ImGui::Begin("GI Renderer")) {
        // 1. Strategy selector
        const char* strategies[] = {"None", "Sparse RC", "DDGI", "SSGI", "Path Tracer (Ref)"};
        ImGui::Combo("GI Strategy", &selectedStrategy, strategies, IM_ARRAYSIZE(strategies));
        
        // 2. Per-strategy settings
        currentGI->drawDebugUI();
        
        // 3. Performance overlay
        ImGui::Separator();
        ImGui::Text("Frame: %.2f ms", lastStats.totalMs);
        for (auto& [name, time] : lastStats.passMs) {
            ImGui::Text("  %s: %.2f ms", name.c_str(), time);
        }
        ImGui::Text("VRAM: %.1f MB", lastStats.vramBytes / (1024.0 * 1024.0));
        
        // 4. Cascade visualization
        ImGui::Separator();
        ImGui::Checkbox("Visualize Probes", &visualizeProbes);
        ImGui::SliderInt("Show Cascade", &showCascadeLevel, 0, MAX_CASCADES - 1);
        ImGui::Checkbox("Show only Probes touched this frame", &showActiveProbes);
        
        // 5. RC-specific tunables (when SparseRC is selected)
        if (auto* rc = dynamic_cast<SparseRCRenderer*>(currentGI)) {
            ImGui::SliderInt("Max Cascade Levels", &rc->maxCascades, 1, 8);
            ImGui::SliderFloat("Base Probe Spacing", &rc->baseSpacing, 0.1f, 2.0f);
            ImGui::SliderInt("Octahedral Resolution", &rc->octRes, 4, 16);
            ImGui::SliderInt("Branching Factor α", &rc->alpha, 1, 3);
            ImGui::Checkbox("Bilinear Fix", &rc->useBilinearFix);
            ImGui::Checkbox("Bilinear 3D Upscaling", &rc->useBilinear3D);
            ImGui::Checkbox("Min-Max Probes", &rc->useMinMaxProbes);
            ImGui::Text("Probes in hash map: %d / %d", rc->probeCount, rc->probeCapacity);
        }
        
        // 6. Compare with reference
        ImGui::Separator();
        if (ImGui::Button("Capture Reference (Path Trace)")) {
            captureReferenceFrame();
        }
        ImGui::Text("RMSE vs reference: %.5f", lastRMSE);
    }
    ImGui::End();
}
```

This single panel gives you everything you need for a defended dissertation: live parameter tuning, performance breakdown, quality metrics, and visualization of internal state.

---

# Part VIII — Future Adaptability

## 47. Strategy Pattern for GI Modules

By implementing the GIStrategy interface (Section 45), you decouple "the renderer" from "which GI technique is in use." This has multiple benefits for the dissertation:

- **Direct comparison**: swap from RC to DDGI, capture stats, swap back. Same scene, same camera, same hardware → directly comparable numbers.
- **Ablation studies**: implement "Sparse RC without bilinear-3D" as a separate strategy, compare against "Sparse RC with bilinear-3D" to quantify the upscaler's value.
- **Reference truth**: include a reference path tracer that runs offline (1000s of samples per pixel). Capture frames in this mode for ground-truth comparison.
- **Defensibility**: examiners can ask "did you really compare against DDGI?" You point to the strategy interface and show the comparison frames.

## 48. DDGI as a Drop-In Alternative

DDGI (Majercik et al. 2019, JCGT) is the standard "probe-based GI" baseline. Implementation outline:

- Probes placed in a uniform 3D grid at a fixed spacing
- Each probe stores irradiance (not radiance) in an octahedral map
- Each probe also stores depth (distance to nearest surface) for occlusion
- Rays are traced from probes each frame, results temporally accumulated
- Sampling: at each shaded pixel, blend the 8 nearest probes weighted by visibility (using the stored depth)

Key differences from Sparse RC:
- DDGI is **temporally accumulated** (single-frame DDGI is noisy)
- DDGI has a **dense grid** (memory scales cubically)
- DDGI handles only **diffuse irradiance**, not specular

Implementation cost: ~2 weeks if you're already familiar with the architecture. The original paper has clear pseudocode; Majercik's NVIDIA RTX SDK has an MIT-licensed reference implementation.

For your dissertation, even a minimal DDGI implementation is enough to claim "we compared against the state-of-the-art probe-based baseline." The performance comparison will likely favor Sparse RC on dynamic scenes (where DDGI's temporal accumulation hurts) and DDGI on static scenes (where temporal accumulation helps).

## 49. SSGI as a Comparison Baseline

SSGI (Screen-Space GI) is the cheapest baseline:

- Sample N rays per pixel through the depth buffer using stochastic raymarching
- Apply temporal accumulation to denoise
- Limit ray length (typically 4–8 pixels) for performance

Implementation: ~3–5 days. The original SSGI paper (Ritschel et al., 2009) plus various GDC implementations are well-documented.

**Why include SSGI**: it's the technique that ships in most games today (Crysis 3, Unity HDRP default GI, Unreal Engine 4 SSGI). Comparing Sparse RC against SSGI shows:
- The quality gain from world-space tracing (no missing off-screen indirect light)
- The performance cost (SSGI is typically <1 ms; Sparse RC will be 5–20 ms)

This honest comparison is exactly what your dissertation needs.

## 50. The Ablation Study Framework

Plan your ablation studies up front. For Sparse RC, the natural ablation axes are:

| Variable | Values | Hypothesis |
|---|---|---|
| Cascade count | 3, 4, 5, 6, 7 | More cascades = better far-field, diminishing returns |
| Branching factor α | 1, 2, 3 | α=2 best memory/quality balance |
| Octahedral resolution | 4×4, 8×8, 16×16 | Higher = sharper indirect, more VRAM |
| Depth-aware upscaling | None / Bilateral / Bilinear-3D | Bilinear-3D best on slanted surfaces |
| Bilinear fix | Off / On | Eliminates ringing, costs 8× trace storage (3D) |
| Min-max probes | Off / On | Cleaner depth edges, doubles probes |
| BVH builder | Median / SAH | SAH 2-5× faster traversal in complex scenes |

Each ablation produces a row in your dissertation's results table. Quality column = RMSE vs reference path tracer. Performance column = ms/frame at 1080p. VRAM column = MB.

This is *the dissertation*. The novel contribution isn't "we implemented Sparse RC" (Sannikov already showed it works). It's "we implemented Sparse RC in Vulkan with these specific design choices and these specific tradeoffs."

---

# Part IX — Dissertation Methodology

## 51. Test Scenes: Indoor, Outdoor, Dynamic

Your proposal mentions three test scenes. Here's what each should stress:

### Scene 1: Indoor (Cornell Box variant or Sponza interior)
- Multiple colored emissive surfaces
- Closed environment (no sky)
- Hard shadows from direct lighting
- Strong color bleeding (this is what GI is FOR)
- **Stresses**: short-range cascades, accurate near-field penumbras

### Scene 2: Outdoor (Sponza exterior, or open scene with sky)
- Sky as the primary light source (large area light)
- Mix of bright sky and shadowed regions under arches
- Distant geometry
- **Stresses**: far cascades, sky integration, occlusion-aware ambient

### Scene 3: Dynamic Mixed
- Some animated/moving geometry
- Moving lights or time-of-day changes
- Both indoor and outdoor elements
- **Stresses**: probe stability under change, hash map churn

For the showcase, you can ship with just Scenes 1 and 2 (both static). Scene 3 is dissertation-only.

### Scene Sources

- **Sponza** (Marko Dabrovic, 2002) — the industry-standard test scene. Several variants available (Crytek Sponza, Intel's modernized version with PBR materials). Free, MIT-style license.
- **Cornell Box** — public domain, easy to model in Blender from scratch
- **NVIDIA's ORCA models** (https://developer.nvidia.com/orca) — Pica Pica, Emerald Square, Bistro Exterior

Match your scenes to your hardware budget. A GTX 1080 can comfortably handle ~500K-1M triangles with software RT.

## 52. Reference Path Tracer for Ground Truth

You need a **reference path tracer** that runs the same scene through pure Monte Carlo path tracing with 1000+ samples per pixel. This is your "ground truth" for RMSE calculations.

Build it as another `GIStrategy` implementation (Section 47). It can run offline (non-realtime); the output is a single HDR image per scene that you save as ground truth, then load for RMSE comparison during real-time rendering.

```cpp
class ReferencePathTracer : public GIStrategy {
public:
    void initialize(...) override;
    GIFrameStats render(...) override {
        // Iterate N times, accumulating samples
        for (int sample = 0; sample < TARGET_SAMPLES; sample++) {
            // standard path tracing dispatch
            ...
        }
        // Save to disk
        saveEXR("reference_scene1.exr", hdrAccumulator);
        return stats;
    }
};
```

Use this:
1. Once per scene, run the reference until convergence (typically a few seconds of compute on a modern GPU)
2. Save the HDR result
3. Load it as a texture when benchmarking
4. RMSE = sqrt(mean((current - reference)²)) computed pixel-wise

## 53. Metrics: RMSE, Frame Time, VRAM

### RMSE (Root Mean Squared Error)

Compute on the linear HDR values (before tonemapping):

```cpp
float computeRMSE(const Image& current, const Image& reference) {
    double sumSquaredError = 0;
    int pixelCount = current.width * current.height;
    for (int i = 0; i < pixelCount; i++) {
        vec3 diff = current.data[i].rgb - reference.data[i].rgb;
        sumSquaredError += dot(diff, diff);
    }
    return sqrt(sumSquaredError / (pixelCount * 3));
}
```

You may want to also report:
- **PSNR** = 20 * log10(max_value / RMSE)
- **SSIM** = structural similarity (perceptually more meaningful than RMSE)
- **FLIP** (NVIDIA's perceptual error metric, https://research.nvidia.com/publication/2020-07_FLIP)

For your dissertation, RMSE is the standard. Mention the others in a "future metrics" footnote.

### Frame Time

Use Vulkan timestamp queries for **per-pass** GPU timing, not just total frame time:

```cpp
vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, queryPool, startQueryIdx);
// ... render pass ...
vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, queryPool, endQueryIdx);

// After frame submit:
uint64_t timestamps[NUM_QUERIES];
vkGetQueryPoolResults(device, queryPool, 0, NUM_QUERIES, sizeof(timestamps), 
                     timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

float passDurationMs = (timestamps[endIdx] - timestamps[startIdx]) 
                      * timestampPeriodNs / 1e6f;
```

Report results as: total ms, per-pass ms, on a specific GPU at a specific resolution. Always specify hardware in your tables.

### VRAM

Track allocations through VMA:

```cpp
VmaStatistics stats{};
vmaCalculateStatistics(allocator, &stats);
float vramMB = stats.statistics.allocationBytes / (1024.0f * 1024.0f);
```

Break down by category in your dissertation (G-buffer, cascade probes, hash map, scene geometry, etc.).

## 54. Benchmarking Methodology

Discipline for credible benchmarks:

1. **Fix the seed**: any randomness (jittered camera, stochastic rays) uses a fixed seed for reproducibility.
2. **Warm-up frames**: discard the first 30 frames after scene load (driver caches, TLAS builds, etc.).
3. **Multiple runs**: run each configuration 5 times, report median and standard deviation.
4. **Fixed camera path**: animate the camera along a recorded path; benchmark the same path for every configuration.
5. **Lock the GPU clock**: on Windows, use Nvidia's `nvidia-smi --lock-gpu-clocks` to prevent thermal throttling from skewing results.
6. **Single configuration per frame**: don't switch GI strategies mid-frame for comparison. Switch, wait 30 frames, then record.
7. **Report power consumption** (optional but impressive): GPUs in different modes have different power draws; an RC implementation that uses 80% the GPU of DDGI is meaningful.

Your benchmark script (Python likely) should:
- Launch the renderer in benchmark mode (no ImGui, no input)
- Iterate through (scene × GI strategy × parameter combo)
- For each combo, capture metrics over 300 frames
- Output a CSV with all results
- Generate plots automatically (matplotlib)

Plan to spend 1–2 weeks on the benchmark infrastructure. It pays off enormously when you need to re-run experiments after fixing a bug.

## 55. Common Pitfalls and How to Defend Against Them

When your examiners challenge your results, these are the most likely vectors:

### "Your reference path tracer might be wrong"
**Defense**: validate against an off-the-shelf reference. Render the same scene in Cycles (Blender), Mitsuba 3, or PBRT-v4. Show the visual match to your reference. Reference path tracing on simple scenes converges to a unique answer; the bar to clear is fixed.

### "Your DDGI implementation might be slow because you implemented it badly"
**Defense**: cite Majercik's reference implementation. If you can, use the NVIDIA RTX-GI SDK directly (it has a Vulkan port) for the DDGI baseline. Then it's "DDGI as NVIDIA ships it" — not your implementation.

### "Your performance numbers are GPU-specific"
**Defense**: report on multiple GPUs. At minimum, the proposal's GTX 1080 + a more modern card (e.g. RTX 3060 or RTX 4070). Plot scaling characteristics.

### "Sparse RC's hash map causes visible artifacts you're not measuring"
**Defense**: add temporal flicker measurements. Record a frame buffer for 60 consecutive frames with a static camera, compute the temporal variance of each pixel, report the average. RC should have very low temporal variance because it's deterministic (modulo hash map probe shuffles).

### "You skipped HRC"
**Defense**: explicitly state that HRC is future work, cite Freeman et al. 2025, justify by complexity and time budget. Examiners respect honest scope-setting.

### "Your test scenes don't represent real games"
**Defense**: include Bistro Exterior or modern Sponza in your benchmarks. These are industry-standard. Cite ORCA.

---

# Part X — References

## 56. Primary Papers

### Foundational
- **Kajiya, J. T. (1986)**. *The Rendering Equation*. SIGGRAPH '86. The mathematical foundation. Required reading even if you don't cite it directly — every GI paper builds on this.

- **Porter, T., Duff, T. (1984)**. *Compositing digital images*. SIGGRAPH '84. The premultiplied-alpha algebra that underpins RC's merge operator.

### Radiance Cascades
- **Sannikov, A. (2023)**. *Radiance Cascades: A Novel Approach to Calculating Global Illumination*. Preprint. https://github.com/Raikiri/RadianceCascadesPaper. **The original paper.** Read this carefully. The 2D theory is fully developed; the 3D section is more sketch than treatment.

- **Osborne, C. M. J., Sannikov, A. (2024)**. *Radiance Cascades: A Novel High-Resolution Formal Solution for Multidimensional Non-LTE Radiative Transfer*. arXiv:2408.14425. The astrophysics application. The math in Sections 2.2–2.5 is the cleanest formal treatment of the algorithm available.

- **Freeman, R., Sannikov, A., Margel, A. (2025)**. *Holographic Radiance Cascades for 2D Global Illumination*. arXiv:2505.02041. The HRC extension; defines the anisotropic probe layout. Section 3 has the cleanest merge formula derivation in the literature.

- **Sannikov, A. (Nov 2025)**. *Sparse 3D Radiance Cascades demo*. YouTube: https://www.youtube.com/watch?v=TGLAxW0xVDU. Sparse RC announcement. Watch the description text carefully — it has the algorithmic claims you need to cite.

### Comparison Baselines
- **Majercik, Z., Guertin, J.-P., Nowrouzezahrai, D., McGuire, M. (2019)**. *Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields*. JCGT 8(2). The DDGI paper.

- **Ritschel, T., Grosch, T., Seidel, H.-P. (2009)**. *Approximating dynamic global illumination in image space*. I3D '09. The SSGI paper.

- **Bitterli, B., et al. (2020)**. *Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting*. SIGGRAPH '20. ReSTIR — for context on the state-of-the-art in stochastic GI.

### Acceleration Structures
- **Wald, I. (2007)**. *On fast construction of SAH-based bounding volume hierarchies*. IEEE RT'07. The binned SAH paper.

- **Teschner, M., et al. (2003)**. *Optimized Spatial Hashing for Collision Detection of Deformable Objects*. VMV '03. The hash function constants used throughout this document.

- **Cigolle, Z. H., et al. (2014)**. *A Survey of Efficient Representations for Independent Unit Vectors*. JCGT 3(2). The octahedral encoding reference.

### Vulkan Reference
- **Khronos Group**. Vulkan Specification 1.3. https://registry.khronos.org/vulkan/. Reference, not reading material.

- **Sellers, G., Kessenich, J.** *Vulkan Programming Guide*. Addison-Wesley. The canonical book.

## 57. Implementations Worth Studying

- **JTLee98/RadianceCascadesVK3D** (https://github.com/JTLee98/RadianceCascadesVK3D) — Vulkan 3D RC, vkguide-based. Architecturally clean even if incomplete.

- **CodyJasonBennett/three-rc** (GitHub) — three.js + BVH ray tracing in 3D. The Bilinear-3D upscaler implementation is worth porting.

- **Path of Exile 2 GDC 2024 talk** (Sannikov) — production implementation details for 2D screen-space RC. Find the slides via GDC Vault.

- **Mathis on Shadertoy** (Radiance Cascades 3D, X3XfRM) — first published 3D RC variant, UV-space.

- **fad on Shadertoy** — various 2D RC implementations with the bilinear fix.

- **The Chalmers thesis** (Gavras, 2025) — uploaded in your project. SPWI implementation; sections 4.5.2 and 7.1 are the most useful for your work.

- **Jason Today's tutorial** (https://jason.today/rc) — interactive WebGPU walkthrough of 2D RC.

## 58. Community Resources

- **Graphics Programming Discord** (https://discord.gg/graphicsprogramming) — the `#radiance-cascades` channel. Sannikov, Freeman, and the wider community discuss implementation details here. Active as of 2026.

- **radiance.wiki** (https://radiance.wiki) — community wiki, especially the "Variants" page and the "Talks" page for video resources.

- **/r/GraphicsProgramming** subreddit — periodic RC threads worth searching.

---

## Closing Notes

This document is a synthesis as of mid-2026. The field is moving quickly — Sparse RC was announced in November 2025, HRC in May 2025, the original RC paper in 2023. By the time you defend, there will be newer variants, newer papers, newer implementations. The best safeguard is the GIStrategy interface (Section 45): if a better technique appears, you implement it as another strategy, benchmark it, and add a paragraph to your conclusion.

The novelty in your dissertation isn't going to be inventing a new algorithm — it's going to be the **first rigorous Vulkan benchmark of Sparse Radiance Cascades against DDGI and SSGI on game-realistic scenes**. That's a real, defensible contribution. Don't lose sight of it.

Good luck.

