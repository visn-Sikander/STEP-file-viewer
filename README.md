# Vison CAD Platform

A professional 3D CAD modeling engine built with **Open Cascade Technology (OCCT)**, providing powerful geometry creation, manipulation, and visualization capabilities through both a headless JSON API and an interactive 3D viewer.

## 🎯 Overview

Vison CAD Platform is a C++ application designed for:
- **Headless geometric operations** via JSON commands
- **Interactive 3D visualization** of CAD models
- **STEP file import/export** for industry-standard CAD data
- **Boolean operations** (cut, union, etc.) on 3D shapes
- **Professional rendering** with advanced lighting and materials

The application operates in two modes:
1. **Headless Engine Mode** (default) - JSON-based command processing via stdin/stdout
2. **Debug Viewer Mode** - Interactive 3D visualization for testing

## ✨ Features

### Core Geometry Operations
- **Primitive Shape Creation**
  - Box/Cube generation with configurable dimensions
  - Cylinder generation with radius and height
  - Sphere generation with radius
  
- **Boolean Operations**
  - Cut operation for subtractive geometry
  - Extensible architecture for union/intersection operations
  
- **CAD Import/Export**
  - STEP file import for loading industry-standard CAD models
  - Full topological analysis (faces, edges, vertices)
  - Bounding box calculation

### 3D Viewer
- **Professional Rendering**
  - 8x MSAA anti-aliasing
  - Realistic lighting with multiple light sources
  - Advanced material properties (shininess, refraction)
  - Gradient background with professional color scheme

- **Interactive Controls**
  - Left-click + drag: Rotate view
  - Middle-click + drag: Pan view
  - Right-click + drag: Zoom (precise point-based)
  - Mouse wheel: Zoom in/out
  - Keyboard shortcuts for standard views and display modes

- **Keyboard Shortcuts**
  - `F` - Fit all / Reset view
  - `T/B/R/L/K/G` - Top, Bottom, Right, Left, Back, Front views
  - `I` - Isometric view
  - `W` - Wireframe mode
  - `S` - Shaded mode
  - `E` - Shaded with edges
  - `F5` - Screenshot (saves as `vison_screenshot.png`)
  - `F11` - Force redraw
  - `ESC` - Exit viewer

- **Real-time HUD**
  - Displays shape topology (face/edge/vertex count)
  - Trihedron (coordinate axes) in corner

## 🛠 Technologies Used

### Core Libraries
- **Open Cascade Technology (OCCT) 7.9.3** - 3D geometry kernel and modeling engine
- **RapidJSON 1.10** - High-performance JSON parsing and serialization
- **OpenGL** - Hardware-accelerated 3D rendering
- **Windows API** - Native window management and event handling

### Build System
- **CMake 3.16+** - Cross-platform build configuration

## 📋 Requirements

### System Requirements
- **Windows** (tested on Windows 10/11 with x64 architecture)
- **C++17 compatible compiler** (MSVC 2015 or later recommended)
- **OpenGL 4.0+** capable GPU

### Dependencies
- **OCCT** - Must be installed with `CASROOT` environment variable set
  - Download from: https://dev.opencascade.org/
  - Set environment: `CASROOT=C:\path\to\OCCT`
  
- **RapidJSON** - Header-only library (included in this project)

## 🔨 Building

### Prerequisites
1. Install OCCT 7.9.3 for Windows (VC14, x64)
2. Set `CASROOT` environment variable to your OCCT installation path
3. Install CMake 3.16 or later

### Build Steps

```powershell
# Navigate to project root
cd C:\AiCadPlatform

# Create build directory
mkdir build -Force
cd build

# Generate build files
cmake .. -G "Visual Studio 17 2022" -A x64

# Build the project
cmake --build . --config Release

# Executable will be at: build\Release\AiCadPlatform.exe
```

## 🚀 Usage

### Headless Engine Mode (Default)

Run the executable and send JSON commands via stdin:

```powershell
.\AiCadPlatform.exe
```

**Example Commands:**

**Create a box:**
```json
{"operation": "make_box", "id": "box1", "width": 100, "depth": 150, "height": 200}
```

**Create a cylinder:**
```json
{"operation": "make_cylinder", "id": "cyl1", "radius": 25, "height": 100}
```

**Create a sphere:**
```json
{"operation": "make_sphere", "id": "sphere1", "radius": 50}
```

**Boolean Cut:**
```json
{"operation": "boolean_cut", "id": "result", "target": "box1", "tool": "cyl1"}
```

**Import STEP file:**
```json
{"operation": "import_step", "id": "imported_model", "file": "path/to/model.STEP"}
```

**View geometry (launches interactive viewer):**
```json
{"operation": "view", "id": "box1"}
```

### Response Format

All operations return JSON responses:

**Success:**
```json
{
  "status": "ok",
  "id": "box1",
  "metadata": {
    "faces": 6,
    "edges": 12,
    "vertices": 8
  },
  "bbox": {
    "xmin": 0,
    "ymin": 0,
    "zmin": 0,
    "xmax": 100,
    "ymax": 150,
    "zmax": 200
  }
}
```

**Error:**
```json
{
  "status": "error",
  "message": "Descriptive error message"
}
```

### Debug Viewer Mode

To enable interactive viewer mode for testing:

1. Edit `main.cpp` and uncomment line 1:
   ```cpp
   #define USE_VIEWER
   ```

2. Rebuild the project

3. Run the executable - it will display a debug box in the interactive 3D viewer

## 📊 Project Structure

```
AiCadPlatform/
├── CMakeLists.txt          # Build configuration
├── main.cpp                # Complete application (3000+ lines)
│   ├── Window procedure and event handling
│   ├── 3D viewer and rendering pipeline
│   ├── JSON command processor
│   └── Geometry operations
├── build/                  # Build artifacts (generated)
│   ├── AiCadPlatform.exe  # Compiled executable
│   └── Release DLLs       # Runtime dependencies
└── README.md               # This file
```

## 🎨 Rendering Features

### Lighting Setup
- **Ambient Light** - Soft overall illumination (30% gray)
- **Key Light** - Main directional light for shape definition
- **Fill Light** - Reduces harsh shadows with complementary direction
- **Headlight** - Always follows camera position
- **Rim Light** - Highlights edges and contours

### Material Properties
- **Default Material** - Polished aluminum
- **Shininess** - 0.65 (professional finish)
- **Refraction Index** - 1.5
- **Color** - Neutral blue-gray (0.70, 0.75, 0.82)

### Visual Output
- **Color Scheme** - Professional gradient background (dark blue-gray to lighter)
- **Edge Display** - Thin black lines for crisp detail
- **Anti-aliasing** - 8x MSAA for smooth rendering

## 🔄 API Summary

| Operation | Parameters | Returns |
|-----------|-----------|---------|
| `make_box` | width, depth, height | id, metadata, bbox |
| `make_cylinder` | radius, height | id, metadata, bbox |
| `make_sphere` | radius | id, metadata, bbox |
| `boolean_cut` | target, tool | id, metadata, bbox |
| `import_step` | file (path) | id, metadata, bbox |
| `view` | id | Launches interactive viewer |

## 📁 Geometry Persistence

Shapes are stored in a global registry (`g_shapeMap`) during the engine session. Each shape is identified by a unique ID string and can be:
- Referenced in subsequent operations
- Used as targets or tools for boolean operations
- Displayed in the interactive viewer
- Analyzed for topological properties

## 🖥 System Architecture

```
┌─────────────────────────────────┐
│   JSON Input (stdin)             │
└──────────────┬──────────────────┘
               │
┌──────────────▼──────────────────┐
│  JSON Parser (RapidJSON)        │
└──────────────┬──────────────────┘
               │
┌──────────────▼──────────────────┐
│  Geometry Engine (OCCT)         │
│  - Shape Creation               │
│  - Boolean Operations           │
│  - STEP I/O                     │
└──────────────┬──────────────────┘
               │
       ┌───────┴────────┐
       │                │
   ┌───▼────────┐   ┌──▼────────────┐
   │ JSON Output│   │ 3D Viewer     │
   │  (stdout)  │   │ - OpenGL      │
   │            │   │ - Interactive │
   └────────────┘   └───────────────┘
```

## 🐛 Known Limitations

- Windows-only (uses WinAPI directly)
- Single-threaded JSON processing
- In-memory shape storage (cleared on exit)
- No persistent database backend

## 🔮 Future Enhancements

- [ ] Multi-threaded JSON processing pipeline
- [ ] Additional boolean operations (union, intersection, fillet)
- [ ] Advanced constraint system
- [ ] Parametric feature tree
- [ ] Cross-platform support (Linux, macOS)
- [ ] Plugin architecture for custom operations
- [ ] Web-based UI frontend

## 📝 License

[Specify your license here - e.g., MIT, GPL, proprietary]

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## 📧 Contact & Support

For questions and support, please open an issue in this repository.

---

**Built with Open Cascade Technology (OCCT)**  
*Professional CAD Modeling Engine*
