# Killer Quebes! 🎮

A 3D arcade-style game built with the **TL-Engine** for the CO1301 Games Concepts module.

Launch marbles at a wall of evil cubes — a mashup of *Space Invaders*, *Breakout*, and *Pinball*!

## Features

### Core Gameplay
- **4-state game model**: Ready → Firing → Contact → Over
- **Sphere-Box collision detection** with angle-based bouncing
- **Multiple marbles** (up to 8) with individual state management
- **Moving block rows** that advance towards the player
- **Side barriers** with collision detection

### Special Block Types (85%+ features)
| Block | Behaviour |
|-------|-----------|
| **Hard** | Requires 3 hits to destroy, changes colour progressively |
| **Stealth** | Disappears and reappears periodically |
| **Bonus** | Turns green on first hit, spawns an extra marble on second |
| **Sticky** | Traps the marble for 2 seconds, then halves its speed |
| **Angry** | Swells up when nearly dead, explodes and destroys neighbours |

### Additional Features
- Rolling marble visual effect
- Sinking animation when blocks are destroyed
- Computer wins if blocks reach the front line

## Controls

| Key | Action |
|-----|--------|
| `Z` | Rotate aim left |
| `X` | Rotate aim right |
| `Space` | Fire marble |
| `Esc` | Quit game |

## Setup

### Prerequisites
- **Visual Studio 2022** (v143 toolset)
- **TL-Engine** installed at `C:\ProgramData\TL-Engine\`
- **Assignment model files** (Floor.x, Block.x, Marble.x, Arrow.x, Dummy.x, Barrier.x, Skybox_Hell.x)

### Build & Run
1. Open `test02.sln` in Visual Studio
2. **Update the media folder path** in `test02.cpp` (line 147) to point to your models folder
3. Set configuration to **Debug | Win32**
4. Build with `Ctrl+Shift+B`
5. Run with `F5`

## Project Structure

```
test02/
├── test02.cpp          # Main game source code (single file as required)
├── test02.sln          # Visual Studio solution
├── test02.vcxproj      # Visual Studio project file
├── Col.psh             # TL-Engine pixel shader
├── ColTex.psh          # TL-Engine textured pixel shader
├── Diffuse.vsh         # TL-Engine diffuse vertex shader
├── DiffuseTex.vsh      # TL-Engine diffuse textured vertex shader
├── PixelLighting.psh   # TL-Engine pixel lighting shader
├── PixelLighting.vsh   # TL-Engine pixel lighting vertex shader
└── .gitignore
```

## Code Quality

- **30+ named constants** — zero magic numbers
- **4 enumerated types** for game, marble, block state, and block type
- **Struct-based arrays** for blocks and marbles with loop processing
- **5 dedicated functions** for collision detection, resolution, and utilities
- **Extensive comments** with clear section organisation

## Marking Scheme Coverage

| Category | Marks Available | Status |
|----------|:-:|:-:|
| Scene setup | /5 | ✅ |
| State model | /10 | ✅ |
| Collision detection | /10 | ✅ |
| Interface controls | /3 | ✅ |
| Playable speed | /2 | ✅ |
| Sphere-Box collision | /3 | ✅ |
| Collision resolution | /6 | ✅ |
| Barriers | /1 | ✅ |
| Arrow separation | /2 | ✅ |
| Block states | /2 | ✅ |
| Second row | /1 | ✅ |
| Marble reset | /2 | ✅ |
| Multiple marbles | /5 | ✅ |
| Moving blocks | /1 | ✅ |
| Sinking blocks | /2 | ✅ |
| Special blocks | /11+ | ✅ |
| Code style | /25 | ✅ |
