Terrain Amplification Pipeline (openFrameworks, CPU)
===================================================

Overview
--------
This project implements a simplified, CPU-only, multi-scale terrain amplification pipeline inspired by Schott et al. A base 256×256 heightfield is generated procedurally with a hardness mask, then upscaled through four scales to 2048×2048. At each scale the loop is: fluvial erosion → thermal erosion → light deposition → blend → (except final scale) bicubic upsample ×2. A peak-restoration step is applied after the finest scale to protect ridges, and a simplified breaching pass carves outlets instead of globally pit-filling basins.

Key Controls
------------
- `E` — Toggle wireframe rendering on/off.
- `G` — Cycle vertex coloring overlays: Height → Log-Drainage → Slope.
- `H` — Cycle hardness preset (Noise → Radial center hard → Radial edge hard → Uniform); regenerates hardness and resets to the base view.
- `R` — Reset terrain/hardness using the current seeds (rebuild the same 256×256 base, clear multi-scale).
- `N` — Pick new random seeds and regenerate terrain/hardness (256×256), clear multi-scale.
- `F` — Single fluvial erosion step (stream power with hardness).
- `T` — Single thermal erosion step (talus-based creep with hardness).
- `P` — Single deposition pass (uses the coarse-scale deposition params from the multi-scale config).
- `U` — Upsample the current heightfield by 2× (keeps world extents).
- `M` — Run the 4-scale multi-scale pipeline (256 → 512 → 1024 → 2048; fluvial + thermal + (reduced/none on fine scales) deposition + blend per scale; peak restoration at the end).
- `B` — View Base terrain (current regenerated base).
- `V` — View Multi-Scale result (after pressing `M`; warns if unavailable).
- `W/S/A/D` — Camera dolly/truck (navigate the scene).

HUD Elements
------------
- Controls list — Quick reference for the key bindings above.
- Seed — Procedural seed for terrain generation.
- Hardness seed — Procedural seed for the hardness preset.
- Grid — Current heightfield resolution.
- World — Fixed world footprint in scene units.
- Fluvial line — Shows the stream-power formula (`dh -= Kf*A^p*S^q * hardness`) with the current `Kf`, `p`, `q` from the first scale.
- View — Indicates whether you are viewing Base or Multi-Scale.
- Mesh vertices — Vertex count of the current mesh.
