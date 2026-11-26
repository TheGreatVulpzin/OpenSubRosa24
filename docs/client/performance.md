# Speed of the client

## How to measure

Run the client with a sampler. It stops the main thread each 10 ms and prints where the thread is.

```powershell
$env:SR_SAMPLE = "1"; $env:SR_SAMPLE_MS = "10"; $env:SR_NATIVE_SAMPLE = "1"; $env:SR_SMOKE_HIDDEN = "1"
python -I instrumentation/client/smoke.py build/hook/opensubrosa24_client_native_smoke.exe play --output C:\svnt\prof
python -I tools/re/profile_samples.py build/hook/opensubrosa24_client_native_smoke.map C:\svnt\prof\opensubrosa24_client_native_smoke\play\run.log 25
```

The tool lists the functions where the thread was (self time) and the functions on the stack (inclusive time). A sample outside the executable is a system library or the graphics driver. A function that is `static` has no name in the map: its time shows under the closest public function before it.

The `play` scenario loads the city, starts a round and fires. About 4 seconds of it is the creation of the level.

## What the profile shows

Profile of the `play` scenario (3800 samples of 10 ms):

| Where | Share |
| --- | ---: |
| Outside the executable (driver, system) | 22% |
| `area_blocksector` (level creation) | 9% |
| `bondsimulation2` (bonds of the physics) | 6% |
| `mark_block` | 4% |
| `bond_partpart` | 3% |
| `serpnoise2d` | 2% |

The creation of the level (`resetgame`, `createlevel`) takes about 11% of the run, 4 seconds at the start.

## What changed

- The client has its own `rand`. The Microsoft C runtime reads the data of the thread in each call to `rand`. The level calls it millions of times. `os_rand` and `os_srand` in `src/platform/os_calls.c` use the same generator (`state * 214013 + 2531011`, bits 16 to 30, start seed 1). A test of 400000 numbers with four seeds found no difference from the runtime. The creation of the level is about 7% faster. The server keeps the runtime `rand`, because the test environment shares its sequence with the original code.

## What is left

- The functions of the table above are shared with the server. A change there must give the same float result bit for bit. The server gate needs its recordings (`build/recordings`) for that.
- A frustum test that calls `projectvector` in the loop of its corners is in `src/client/render/frustum.c`. It gives the same result each time in a frame and is a candidate to compute once.
- A 64 bit build is not planned. It changes the layout of the records.
