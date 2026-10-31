# Radar protocol host test

Runs on your computer, no board needed:

```bash
gcc -std=c11 -Wall -Wextra -Werror -I../include ../radar_protocol.c test_radar_protocol.c -lm -o radar_protocol_test
./radar_protocol_test
```

It builds synthetic TinyFrame packets and checks fragmented and concatenated
input, checksum rejection and recovery, 2D, 3D and HLK target layouts, the
nearest-target pick and the 0x0A08 point cloud.
