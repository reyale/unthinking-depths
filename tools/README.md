# tools/

Developer utilities for Unthinking Depths.

## gen_example_replay

Generates a demo replay file for manual testing of the visualizer.

```bash
./build/tools/gen_example_replay [output_path]
```

Defaults to `demo.ud` if no path is given. Use a `.ud.zst` extension to
write a zstd-compressed replay instead:

```bash
./build/tools/gen_example_replay demo.ud.zst
```

The generated match is two idle bots on a 20×20 map with some terrain, running
for 100 ticks. Feed the output to `ud_viz` to inspect it:

```bash
./build/viz/ud_viz demo.ud
```
