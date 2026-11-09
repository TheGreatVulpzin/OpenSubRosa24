# Photoshop images

The PSD loader reads version-one Photoshop images with eight-bit RGB planes. Files: `src/client/image/psd.c` and `psd.h`.

`psd_load` skips color mode data, resources and layer data. It accepts raw planes and PackBits runs. Output pixels use four interleaved channels before an optional channel conversion.

PackBits commands below 128 copy literal bytes. Commands above 128 repeat one byte. Command 128 consumes no output. Run lengths come from the stream. Row length fields are skipped.

For raw data, a plane at an index equal to the declared channel count is read too. For compressed data, plane indices stop before that count. Missing color planes contain zero. A missing alpha plane contains 255. The reported channel count retains the declared value.
