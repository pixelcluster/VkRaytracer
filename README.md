# Vulkan Hardware Raytracer

A hardware-accelerated path tracer for glTF files using Vulkan.

## Screenshot

![](/screenshots/sponza.png)

## Installation

```
git clone --recursive https://github.com/pixelcluster/VkRaytracer.git
```

## Usage

glTF files specified on the command line are loaded and rendered.
"Open With" or dragging and dropping a glTF file over the executable works on Windows too.

There are restrictions about which glTF files can be opened.
- All primitives using normal maps must have tangents.
- Only one set of texture coordinates is supported.
- All primitives must have a material.
- Vertex colors are unsupported.

Many models from https://github.com/KhronosGroup/glTF-Sample-Models/, such as the Sponza, Damaged Helmet (if tangents are added), and Lantern, fulfill these restrictions.
