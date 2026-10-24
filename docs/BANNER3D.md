# 3D spinning HOME Menu banner

The `.cia` can carry a real 3D banner: the logo is an extruded relief model that turns on the spot while the title is selected in the HOME Menu.
It is **experimental and off by default**: the first test on a console did not work (details to be filled in). Tick **"3D spinning banner"** in the patcher
(or pass `-Banner3d` on the command line) to try it; the default is the flat logo image.

## What is in the box
- `assets/banner3d/golden_sun_relief.glb`, `logo_texture` data inside it: the static relief model from the asset kit (stone plaque, medallion, sunburst, five lettering groups; 2,668 triangles).
- `assets/banner3d/golden_sun_spin.glb`: the same model with a turntable animation added by `tools/banner3d/make_spin_glb.py` (all meshes skinned to one bone; 360 degrees about the vertical axis in 6 s, looping).
- `assets/banner3d.cgfx`: the banner model (CGFX, 232 KB, limit 512 KB) converted from the spinning glb with [pycgfx](https://github.com/skyfloogle/pycgfx) at revision `1f78850086f3a77c41e07162e842f97a5bf3c18a`. It contains the animation `Spin`.
- The patcher passes it to `bannertool makebanner -ci banner3d.cgfx -a silence.wav`; `makerom` puts the resulting `banner.bnr` into the .cia.

## Rebuilding it
```
python tools/banner3d/make_spin_glb.py assets/banner3d/golden_sun_relief.glb assets/banner3d/golden_sun_spin.glb 6
python path/to/pycgfx/main.py assets/banner3d/golden_sun_spin.glb assets/banner3d.cgfx     # needs: pip install gltflib pillow numpy
```
(pycgfx is not included in this repository.) The third argument of the first command is the length of one turn in seconds.

## Known unknowns
- pycgfx plays all animations at once and supports skinning, which is why the spin is a one-bone skin; how the HOME Menu loops and times it has to be checked on a console.
- Size, lighting and the camera framing have not been compared with the HOME Menu's banner camera (`banner-camera.gltf` in the pycgfx repository).
- The model is derived from fan art of the Golden Sun logo (see THIRD_PARTY_NOTICES.md).
