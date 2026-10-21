# P2b: frame rate (20 Oct 2026)

Measured in ares (MicroNut99/ares_android, branch `cd32x-bus-jh`), not on the console.
The same changes as OpenJazz R7 (OpenJazzCD32X-REV `docs/R7_FRAME_RATE.md`), in
`port/plat_mars.c`:

- **Busy-waits stay off the bus** (`bus_rest`): the slave's wait for a job, the master's wait for
  the slave, the waits on 32X registers and `comm_pause`. The two SH2s share one bus; a waiting
  SH2 that spins on a cache-through address or a 32X register slows every cart, SDRAM and frame
  buffer access of the other. The slave runs the steps for part of each frame and waited the
  rest of it on SDRAM (in ares 240,000-400,000 reads a picture; now about 40,000).
- **The pad is asked for ahead** (`pad_ask`, `pad_collect`): the request (CMD 48 with a waiting
  sound effect) goes out before the frame pacing wait, the answer is read at the next pad poll.

- **The pad's answer is waited for** before anything else uses COMM8 / COMM0 (`plat_comm_settle`,
  and COMM8 / COMM10 through `comm16_settled`). Without this the backdrop's scroll command
  (`genbg_scroll`, COMM8 = scroll position, COMM0 = 62) landed on the pad's answer: every third
  frame the game read the scroll value as the pad (no buttons), so the player lost a third of
  his steps - the jerky movement seen in emulation with the first P2b.
- **Steps from the time the frame is shown** (`plat_frame_ms`, as OpenJazz J24), not the time the
  loop starts: frames shown 2 vblanks apart got 1, 2 or 3 steps (also in P2a); now always 2.

Stage 1, first screen, TIMING OFF, pictures shown per second:

| | ares upstream | ares `ARES_32X_TIMING=console` |
|---|---|---|
| P2a | 60 | 30 |
| P2b | 60 | 60 |

The disc's default name is now HurricanP2b (HurricanP2b-16MB with CART=16).

## Loading (P2b, measured in ares)

The same boot-stack and cart changes as OpenJazz R7 (OpenJazzCD32X-REV `docs/R7_FRAME_RATE.md`,
"Loading"):
- **The drive reads on** between 32 KB chunks, with a 64 KB read-ahead buffer while the 68000
  copies: about 143 KB/s for a stage, against 41 KB/s before.
- **The first boot fills the 16 MB cart** with all nine stages and the story (6.8 MB).
  - The splash shows "COPYING TO THE CART - START: SKIP".
  - Later boots copy nothing.

| ares, blank 16 MB cart | P2a | P2b |
|---|---|---|
| first boot to the menu | ~65 s | ~95 s (all stages and the story copied) |
| later boots to the menu | ~31 s | ~30 s |
| stage 1, not in the cart | 14.6 s | 5.3 s |
| any stage after the first boot | - | ~2 s (disc check) |
