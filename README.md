# Vulkan Hardware Raytracer

A hardware-accelerated path tracer for glTF files using Vulkan.

## Installation

```
git clone --recursive https://github.com/pixelcluster/VkRaytracer.git
```

## Usage

glTF files specified on the command line are loaded and rendered.
"Open With" or dragging and dropping a glTF file over the executable works on Windows too.

There are restrictions about which glTF files can be opened.
- All geometries must have tangents.
- Only one set of texture coordinates is supported.
- There must be at least one texture.
- All primitives must have a material.
- All materials must have a base color texture.

Many models from https://github.com/KhronosGroup/glTF-Sample-Models/, such as the Sponza, Damaged Helmet, and Lantern, fulfill these restrictions.

## Screenshot

screenshots/sponza.png