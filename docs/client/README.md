# Client systems

One page for each system of `src/client/`.

## Images and fonts

The image loaders supply texture pixels and font atlases. Each format uses a separate source file and header in `src/client/image/`.

- [PNG](png.md): chunks, row filters, palettes and transparency.
- [zlib](zlib.md): DEFLATE blocks and Huffman codes.
- [JPEG](jpeg.md): baseline frames, component blocks and color conversion.
- [BMP](bmp.md): palettes and packed color masks.
- [PSD](psd.md): planar RGB channels and PackBits runs.
- [HDR](hdr.md): Radiance RGBE samples.
- [Fonts](fonts.md): bitmap atlases, widths and coordinates.
- [Byte access](io.md): memory readers and file fields.

## Textures, models and landscape

- [textures.md](textures.md): the textures that the game makes at the start, and the noise functions.
- [models.md](models.md): the model in work, the model builders, the sphere, the hair and the human.
- [landscape.md](landscape.md): the landscape mesh.

## Sound and voice

Sound and voice use `src/client/audio/`.

- [Sound and voice](audio.md): devices, spatial sources, microphone frames and voice controls.

## Debug, timers and Steam

- [runtime.md](runtime.md): the debug display, the profiler timers, the Steam interface and the JPEG restart.
- [start.md](start.md): the identifiers of models, textures and sounds, and the start of the client.
- [simulation.md](simulation.md): the tick of the client, the keys of the display and the traffic lights.
- [network.md](network.md): the input packet that the client sends, and the smoothing of humans, objects and items.
