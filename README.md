# About

Personal monorepo, mostly graphics things.
Vulkan 1.3 bindless renderer and things implemented with it.

Following platforms are supported:
- Linux (X11 / Wayland)
- Windows 10/11

# Build
This project uses bazel as build system.

You can build any target specified in `BUILD.bazel` files

Example: `bazel build src/core:types`

To run demos: `bazel run demos/{demo}:app`

# Gallery
## Atmosphere
![Atmosphere1](https://github.com/SergeiBorzov/fly/blob/master/gallery/atmosphere_1.png)
![Atmosphere2](https://github.com/SergeiBorzov/fly/blob/master/gallery/atmosphere_2.png)
![Atmosphere3](https://github.com/SergeiBorzov/fly/blob/master/gallery/atmosphere_3.png)

## Gaussian splatting
![GaussianSplatting](https://github.com/SergeiBorzov/fly/blob/master/gallery/gaussian_splatting.png)

## Ocean rendering
![Ocean](https://github.com/SergeiBorzov/fly/blob/master/gallery/ocean.gif)

# Sources of wisdom

## General
- https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/
- https://dev.to/gasim/implementing-bindless-design-in-vulkan-34no
- https://www.reedbeta.com/blog/understanding-bcn-texture-compression-formats/

## Atmosphere
- https://sebh.github.io/publications/egsr2020.pdf
- https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf
- https://inria.hal.science/inria-00288758/en
- https://developer.download.nvidia.com/presentations/2007/D3DTutorial_Crytek.pdf
- https://iquilezles.org/articles/fbm/
- https://iquilezles.org/articles/gradientnoise/

## Ocean
- https://people.computing.clemson.edu/~jtessen/reports/papers_files/coursenotes2004.pdf
- https://www.cg.tuwien.ac.at/research/publications/2018/GAMPER-2018-OSG/GAMPER-2018-OSG-thesis.pdf
- https://gdcvault.com/play/1025819/Advanced-Graphics-Techniques-Tutorial-Wakes

## Gaussian splatting
- https://gpuopen.com/download/Introduction_to_GPU_Radix_Sort.pdf
- https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/