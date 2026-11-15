# Sound and voice

OpenAL supplies playback and microphone capture. Speex encodes mono voice at 8000 Hz with 160 samples per frame. Files in `src/client/audio/` cover device setup, codecs, capture, speaker selection, datagram sockets and voice controls.

## Records

| Record | Fields | Use |
| --- | --- | --- |
| `AudioSound`, 0x18 bytes | `object`, `kind`, `keep_alive`, `source` | One source in the packed sound list. A vehicle uses separate engine and tire sources. |
| `AudioEncoder`, 0x2c bytes | `state`, `bits`, `preprocess` | One encoder with noise reduction and automatic gain control. |
| `AudioDecoder`, 0x28 bytes | `state`, `bits` | One decoder for each of eight incoming voices. |
| `VoiceRing`, 0x9210 bytes | `samples`, `head`, `size`, `mode`, `data`, `silence_ticks`, `voice_off` | Sixty-four outgoing frames for one player. Each payload reserves 256 bytes. |
| `AudioChannel`, 0x2924c bytes | `active`, `human`, `item`, `mode`, `pos`, `samples`, `head`, `tail`, `source` | One incoming voice with its playback queue and spatial position. |
| `VoiceSlot`, 0x20 bytes | `active`, `player`, `human`, `item`, `distance` | One of eight speakers in a connection. |
| `AudioCamera`, 0x44 bytes | `player`, `pos`, `rot`, `projection`, `vel` | The camera state used for spatial sound and voice icons. |

The socket address, handle slots and enable flags use `BsdSocket`. Its address stores an IPv4 family, a port in network byte order and an address of zero.

## Devices and codecs

`setupaudio` opens the default playback device, creates a context and makes it current. A failed device or context ends setup. Capture uses the default capture device with an 8000-sample buffer. A capture failure still enables playback.

`shutdownaudio` closes capture when its pointer is nonzero. It gets the current context and its device, clears the current context and closes both. It disables audio but retains the pointer values.

`enablespeex` selects narrowband mode, quality 8, noise reduction and automatic gain control with a level of 16000. It creates eight decoder states. `disablespeex` releases the states and their bit buffers. `preprocessspeex` changes one signed sample frame in place. `encodespeex` converts its 160 samples to floats, resets the bit buffer and stores up to 200 encoded bytes.

## Microphone input

`getaudioinput` clears all 64 encoded sizes and sets `voice_off` when voice is disabled. A missing capture device leaves the voice ring unchanged. Otherwise it reads at most sixteen complete frames per call.

Each sample is multiplied by the input gain, truncated to an integer and limited to -32767 through 32767. The gain product has no intermediate float store. Preprocessing follows the threshold test. Automatic detection holds a voice open for sixteen quiet frames. Push to talk holds it open for twenty-four frames after release. The silence counter stops at 64.

A quiet frame temporarily contains zeros during encoding. Its samples are saved and restored, its encoded size becomes zero and its voice mode remains stored. The ring head wraps at 64. Modes 0, 1 and 2 select whisper, talk and yell.

## Speaker selection

`getnextvoice` returns the first inactive slot. With a full list, its first comparison reads `Connection.voice_key`. Later comparisons read the distance of the current candidate. With finite distances, the first candidate prevents later replacements because its distance equals the comparison limit.

`addvoice` stores the speaker, human, item and distance only when selection returns an index. A rejected speaker leaves every slot unchanged. With a full list, a NaN input distance or a NaN candidate distance prevents replacement.

## Spatial sound

`soundsimulation` places each active voice at its human head, radio item or a point behind the camera. The listener receives position, orientation and velocity. Velocity uses sixty simulation ticks per second. The master volume uses a bounded fade and then squares the gain.

Up to eight vehicles supply engine and tire sounds. A full vehicle list can replace the nearest selected vehicle farther than the new one. Sources follow the vehicle axes and velocity. Engine pitch and volume use engine speed. Tire volume sums the first four wheel slips. The listener inside a vehicle halves vehicle volume.

Stopped sources without `keep_alive` leave the packed sound list. `deletesound` moves the last sound into the removed slot and stores the stopped source handle in the free tail slot.

Each voice ring keeps a target delay of eight frames. A distance outside four through twenty-four frames resets its tail. Each processed OpenAL buffer receives four decoded frames. Active sources restart when necessary. Whisper, talk and yell use reference distances of 2, 5 and 10. Radio voices use a reference distance of 0.5.

Float stores retain the fade, gain, engine pitch, tire sums and vector results. Each tire sum retains its nested evaluation order.

Vector length rounds its squared sum and square root result to float. Vehicle distances return a float before sound selection compares them.

## Sound buffers and sources

`src/client/audio/sound_create.c` makes the OpenAL objects and starts sounds. `src/client/audio/wav.c` loads sound files.

- `sound_buffers` has one record for each sound number (256 records of 12 bytes): the OpenAL buffer, a loaded flag and a reference distance. `setupsounds` makes the 256 buffers, four buffers for each of the 8 voice channels, and one source for each channel. A channel source that OpenAL cannot make gets the id -1.
- `setupsounds` then makes sound sources one at a time. It stops at the first OpenAL error, or after 33 sources. `audio_source_limit` counts the sources after the first one.
- `loadwav(sound, path, reference)` loads a WAV file of the folder `sound`. Only a mono file loads. A file of 8 bit samples uses the 8 bit format. Every other file uses the 16 bit format. The record keeps the reference distance and its loaded flag becomes 1. The folder changes back only when it changed.
- `createsoundobject` starts a sound on the next free source and returns its index. It returns -1 when audio is off, when the buffer has no sound, or when the live list is full. A call with an object and a kind that already plays returns the index of that sound. A call with object -1 refuses a ninth sound of the same buffer. The gain goes from 0 to 1 and the pitch from 0 to 2. The velocity is the given vector times 60, or zero.
- `createsound` is the same with no object and no looping. The sound must start within 512 units of the listener, and a ninth sound of the same buffer is refused. It does not change the kind of the record.
- The list of live sounds is packed: a new sound goes at the end, and `deletesound` moves the last sound into the freed place.

## Sockets and controls

`enablesocketserver` creates a UDP socket and binds the configured port. It enables the slot only after a successful bind. `enablesocketclient` creates an unbound UDP socket. Both request nonblocking I/O. A failed nonblocking request still leaves the slot enabled.

`frame_drawvoiceicons` measures the previous playback frame, projects the speaker direction and places an icon on a 1024 by 640 canvas. Its size depends on distance. Rear icons use a darker color. The vertical coordinate stays between 24 and 616.

`voiceoptionsmenu` shows the microphone waveform, threshold, voice switch and talk binding. It accepts key transitions and mouse buttons. The last waveform endpoint reads the low half of the ring head after the sample array. It draws the cursor, swaps the window and updates audio until the menu returns or the window closes.

## Functions

| File | Functions |
| --- | --- |
| `setup.c` | `setupaudio`, `shutdownaudio` |
| `speex.c` | `enablespeex`, `disablespeex`, `preprocessspeex`, `encodespeex` |
| `voice.c` | `getaudioinput`, `getnextvoice`, `addvoice` |
| `sockets.c` | `enablesocketserver`, `enablesocketclient` |
| `sound.c` | `soundsimulation`, `deletesound` |
| `voice_ui.c` | `frame_drawvoiceicons`, `voiceoptionsmenu` |
