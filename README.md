node.gl
=======

`node.gl` is a [GoPro][gopro] OpenGL engine for building and rendering
graph-based scenes. It is designed to run on both desktop (Linux, OSX, Windows)
and mobile (Android, iOS).

![node.gl logo](/doc/nodegl.png)

**Warning:** note that `node.gl` is still highly experimental. This means the ABI
and API can change at any time.

![tests Linux](https://github.com/gopro/gopro-lib-node.gl/workflows/tests%20Linux/badge.svg)
![tests Mac](https://github.com/gopro/gopro-lib-node.gl/workflows/tests%20Mac/badge.svg)
![tests MinGW](https://github.com/gopro/gopro-lib-node.gl/workflows/tests%20MinGW/badge.svg)
![tests MSVC](https://github.com/gopro/gopro-lib-node.gl/workflows/tests%20MSVC/badge.svg)
[![coverage](https://codecov.io/gh/gopro/gopro-lib-node.gl/branch/master/graph/badge.svg)](https://codecov.io/gh/gopro/gopro-lib-node.gl)

[gopro]: https://gopro.com/


## 📜 License

`node.gl` is licensed under the Apache License, Version 2.0. Read the
[LICENSE][license] and [NOTICE][notice] files for details.

[license]: /LICENSE
[notice]: /NOTICE

## 👤📚 Users documentation

### 🛠 Tutorial

- [Starter tutorial][tuto-start]

### 💡 How-to guides

- [Installation][howto-install]
- [Using the C API][howto-c-api]

### ⚙️ Discussions and explanations

- [node.gl standalone build environment][expl-ngl-env]
- [Shaders][expl-shaders]
- [Media (video) time remapping][expl-time-remap]
- [Graphic configuration (blending, inheritance, ...)][expl-graphicconfig]
- [How the Noise node works][expl-noise]

### 🗜 Reference documentation

- [libnodegl][ref-libnodegl]
- [pynodegl][ref-pynodegl]
- [pynodegl-utils][ref-pynodegl-utils]
- [ngl-tools][ref-ngl-tools]
- [Eval][ref-eval]


## 👷📚 Developers documentation

### 🛠 Tutorial

- [Writing a new node][howto-write-new-node]

### 💡 How-to guides

- [Working with tests][howto-tests]

### ⚙️ Discussions and explanations

- [How the Python binding is created][expl-pynodegl]
- [What happens in a draw call?][expl-draw-call]
- [Technical choices][expl-techchoices]
- [The maths behind the YCbCr to RGBA color matrix][expl-colormatrix]
- [Tests categories][expl-tests-categories]

### 🗜 Reference documentation

- [Developer guidelines][proj-developers]
- [Project architecture and organization][proj-archi]


[proj-archi]:            /doc/project/architecture.md
[proj-developers]:       /doc/project/developers.md
[tuto-start]:            /doc/tuto/start.md
[howto-install]:         /doc/howto/installation.md
[howto-c-api]:           /doc/howto/c-api.md
[howto-tests]:           /doc/howto/tests.md
[howto-write-new-node]:  /doc/howto/write-new-node.md
[expl-pynodegl]:         /doc/expl/pynodegl.md
[expl-draw-call]:        /doc/expl/draw-call.md
[expl-shaders]:          /doc/expl/shaders.md
[expl-techchoices]:      /doc/expl/techchoices.md
[expl-time-remap]:       /doc/expl/media-time-remapping.md
[expl-colormatrix]:      /doc/expl/colormatrix.md
[expl-graphicconfig]:    /doc/expl/graphicconfig.md
[expl-ngl-env]:          /doc/expl/ngl-env.md
[expl-noise]:            /doc/expl/noise.md
[expl-tests-categories]: /doc/expl/tests-categories.md
[ref-libnodegl]:         /libnodegl/doc/libnodegl.md
[ref-pynodegl]:          /doc/ref/pynodegl.md
[ref-pynodegl-utils]:    /doc/ref/pynodegl-utils.md
[ref-ngl-tools]:         /doc/ref/ngl-tools.md
[ref-eval]:              /doc/ref/eval.md
