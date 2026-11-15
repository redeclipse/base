# Movie Recording

The in-engine movie recorder (`src/engine/movie.cpp`): the formats it writes, the codec libraries it loads, and how it
keeps the picture and sound in step on a fixed timestep.

`movie [name]` records the screen and sound to `<home>/<moviedir>/<name>.mp4` (or `.avi`), or stops the recording in
progress. With no name the file is named by `filetimeformat`. `moviecodec` picks the format:

- **AV1 MP4** (the default) or **VP9 MP4**: fragmented MP4 with Opus sound, encoded by SVT-AV1 or libvpx, at the constant
  quality `moviecrf`. A fragment starts at each key frame (`moviekeyint`), so a file cut short still plays up to its last
  one, and the lengths are filled in at the start when it closes. The video is BT.709 studio range and is marked as that.
- **MJPEG AVI** or **uncompressed I420 AVI**: OpenDML AVI, which can grow past 1GB, with 16 bit PCM sound. It only needs
  libraries every platform already links (SDL_image's JPEG writer and OpenAL Soft).

The compressed codecs aren't linked. `moviecodecs` loads opus, libvpx and SVT-AV1 with `SDL_LoadObject` the first time
they're wanted, using the headers bundled in `src/support` (SVT-AV1 must be version 4, as its configuration's layout
changes between versions). 64 bit Windows ships them in `bin/amd64`; elsewhere they come from the system. If one is
missing, AV1 falls back to VP9 and then to MJPEG AVI, saying so in the console, and without Opus a movie with sound is
recorded as MJPEG AVI. `dbgmovieskip` pretends libraries are missing, to test that.

- **Fixed timestep**: while recording, `updatetimer()` (`src/engine/server.cpp`) advances time by exactly one frame at
  `moviefps` (`recorder::step()`) instead of by the clock. Every rendered frame is one movie frame, so the movie plays back
  smoothly however long each frame takes to render and encode, and anything timed by `lastmillis`, like `sleep`, runs on
  the movie's time. The clock is rebased with `clockreset()` when it stops.
- **Pacing**: with `moviepace` (the default), `limitmovie()` waits until each frame's movie time comes due on the clock,
  so the game runs at normal speed whatever the frame rate, vsync or refresh rate. A frame that runs more than a frame late
  rebases the clock, so the movie falls behind real time rather than rushing to catch up. `moviepace 0` records as fast as
  frames render, for scripted recordings nobody needs to watch, but only alone in a local game: connected to a server or
  with other players it always paces, as running ahead of real time would work as a speed hack.
- **Sound**: starting a recording with `moviesound` reopens the sound device as an OpenAL Soft loopback device
  (`soundloopback`, `src/engine/sound.cpp`) through `resetsound()`, so the sound device itself plays nothing. Each frame renders
  exactly the samples up to the end of that frame with `soundcapture()`, which keeps the music stream filled first. As
  `musicmillis` counts what has been played, anything kept in time with the music stays in time with the movie. Stopping
  reopens the real device. With `moviemonitor`, each frame's sound is also queued to a second OpenAL device, opened the
  same way as the real one (`sounddevice`, then the default), so it can be heard while recording, keeping at most a
  quarter second queued (`soundmonitor()`).
- **Capture**: `swapbuffers()` calls `recorder::capture()` for frames drawn by the main loop, reading the back buffer at
  `renderw`×`renderh`. Frames go through a ring of jobs to `movieencoders` encoder threads, then a writer thread puts them
  in the file in order, each followed by its sound. Capture waits when the ring is full, which only slows recording down.
- Frames are scaled to `moview`×`movieh` when set (box filtered for I420, AV1 and VP9, linear for MJPEG). AV1 and VP9 are
  then fed to their encoder in order on the writer thread, each encoder using its own threads, and the sound is encoded
  to Opus in 20ms packets.

```
// record 10 seconds of whatever is on screen, starting the menu theme from the top with it
soundmusicmenuvol 1; movie test; sleep 10000 [movie]
```
