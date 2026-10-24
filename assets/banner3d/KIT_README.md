# Golden Sun 3D banner asset kit

Based on the supplied fanart image recovered from the referenced conversation.

## Deliverables and status

- `golden_sun.glb`: editable, textured 3D relief model, importable into Blender. Front faces +Z, up +Y. Eight mesh parts: stone plaque, medallion disk, sunburst and five connected lettering groups. Connected letters can be separated or edited as mesh vertices. Lettering rises 0.5 units above its base; complete depth is 1.55 units. This is a shallow sculptural logo, not a fully sculpted reverse side.
- `editable_shapes.json`: editable outline points, holes and extrusion depths for each component.
- `build_model.py`: reproducible geometry source; uses the supplied `reference.png`. Requires Python with pillow, numpy, opencv-python-headless, shapely, trimesh, mapbox-earcut and networkx. Run `python build_model.py`. Changing the JSON alone does not rebuild the model; edit the generator or import the outlines into your own modeling workflow.
- `logo_texture.png`: 512 × 256 texture shared by the model parts. It preserves the source artwork's painted gold, cracks and shading. Broad relief and lettering have real geometry; fine bevels and cracks are texture detail. Side and back UVs use projected artwork, so they are less polished than the front.
- `open_in_blender.py`: optional import/save helper. Blender is not installed here, so no `.blend` is claimed to have been generated or tested.
- `interactive_preview.html`: self-contained, offline WebGL model viewer. Open in a browser; drag to rotate, scroll to zoom. This displays the GLB mesh data, not the BCMDL rendering pipeline.
- `model_preview.png` and `rotation_preview.gif`: software-rendered previews of the model geometry. They are not console captures.
- `golden_sun_candidate.bcmdl`: actual CGFX binary emitted by pycgfx from the GLB. **Experimental export, not a verified 3DS HOME Menu banner.** It is not a renamed image or renamed GLB.
- `model_validation.json`: geometry and binary checks performed here. Signature, declared file length, size budget and COMMON model-name presence are structural checks only, not proof of console compatibility.

The final mesh contains 2,668 triangles and 1,344 vertices. The plaque, disk, sunburst and three lettering groups are watertight; two lettering groups retain non-watertight triangulation edges. The preview renders them, but repair these before workflows requiring closed solids, and inspect them in the target CGFX viewer. The GLB is intended for game rendering, not direct 3D printing.

Export used pycgfx revision `1f78850086f3a77c41e07162e842f97a5bf3c18a`. Candidate size: 203,928 bytes. No independent CGFX parser/viewer or hardware validation was performed.

## 3DS integration handoff

The converter is [skyfloogle/pycgfx](https://github.com/skyfloogle/pycgfx). Its documented input is glTF/GLB, and its documented banner CGFX limit is 512 KiB. Reproduce the export from the pycgfx checkout with:

```
python main.py /path/to/golden_sun.glb /path/to/golden_sun_candidate.bcmdl
```

The converter creates a model named `COMMON`. The model has no animation track; rotation in the HTML/GIF is preview behavior. It must not be mistaken for an embedded HOME Menu animation.

Before integrating: inspect the candidate in a compatible CGFX viewer, confirm textures and orientation, and check framing using the converter repository's `banner-camera.gltf`. The model width is approximately 28.4 banner units, centered near the origin. Adjust scale/placement for the target banner camera as needed.

Package the CGFX with your project's banner tool and appropriate banner audio into its banner container, then supply that container through your project's existing CIA build pipeline. The exact command depends on the tool/version and project configuration. No CIA project, banner audio, banner packager or console test session was supplied, so this kit does not include a `.bnr`, modified CIA, or hardware verification. Keep the original project banner until the candidate passes a HOME Menu test on the intended region/system. Check cropping, lighting, depth, title selection transitions and any desired animation there.

This is a fanart-derived asset for the user's project; no Nintendo SDK assets or tools are included.
