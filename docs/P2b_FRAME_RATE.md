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

Stage 1, first screen, TIMING OFF, pictures shown per second:

| | ares upstream | ares `ARES_32X_TIMING=console` |
|---|---|---|
| P2a | 60 | 30 |
| P2b | 60 | 60 |

The disc's default name is now HurricanP2b (HurricanP2b-16MB with CART=16).
