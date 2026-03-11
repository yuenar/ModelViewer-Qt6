# Shaders Directory

This directory contains GLSL (OpenGL Shading Language) shader files used by the renderer for various visual effects and rendering modes.

## Shader Types

### Vertex Shaders (.vert)
- **phong.vert**: Phong shading vertex processing
- **wireframe.vert**: Wireframe rendering
- **normals.vert**: Normal visualization
- **background.vert**: Background/skybox rendering
- **axis.vert**: Coordinate axis display
- And more specialized vertex shaders

### Fragment Shaders (.frag)
- **phong.frag**: Phong lighting calculations
- **wireframe.frag**: Wireframe fragment processing
- **normals.frag**: Normal color visualization
- **background.frag**: Background/skybox coloring
- **brdf.frag**: Bidirectional Reflectance Distribution Function
- **selection.frag**: Selection highlighting
- And many effect-specific fragment shaders

### Geometry Shaders (.geom)
- **normals.geom**: Normal line generation
- **face_normal.geom**: Face normal visualization
- **axis.geom**: Axis line generation
- **twoside_per_fragment.geom**: Two-sided lighting geometry
- **subdiv.geom**: Geometry subdivision

## Specialized Shaders

### Lighting and Materials
- **phong.vert/frag**: Classic Phong lighting model
- **brdf.vert/frag**: Physically-based rendering
- **twoside_per_fragment.vert/frag/geom**: Advanced two-sided lighting

### Visualization Modes
- **normals.vert/frag/geom**: Normal vector visualization
- **wireframe.vert/frag**: Wireframe display
- **selection.vert/frag**: Object selection highlighting

### Effects and Utilities
- **skybox.vert/frag**: Skybox rendering
- **shadow_mapping_depth.vert/frag**: Shadow mapping
- **text.vert/frag**: Text rendering
- **debug_quad.vert/frag**: Debug rendering

### Advanced Rendering
- **irradiance_convolution.frag**: Environment map processing
- **prefilter.frag**: Reflection probe filtering
- **clipping_plane.vert/frag**: Geometry clipping

## Compilation

Shaders are automatically compiled using Qt's shader tools (qsb) during the build process. The CMake configuration handles:
- Precompilation to QSB format
- Multi-target compilation (GLSL, HLSL, MSL)
- Optimization for better performance

## Usage

Shaders are loaded and managed by the `RhiRenderer` class. Each rendering mode selects appropriate shader pairs for the desired visual effect.
