# Indoor positioning platform firmware

This repository contains firmware for indoor positioning platform based on PCA20036 nodes and PCA10056 DKs as tags and time sync controller. 
Fimrware for the following parts is available
- Nodes
  - Bootloader for DFU using TFTP: [node_bootloader_SDK14.2](https://github.com/jtguggedal/positioning_firmware/tree/master/node_bootloader_SDK14.2)
  - Application: [node](https://github.com/jtguggedal/positioning_firmware/tree/master/node)
- [Tags](https://github.com/jtguggedal/positioning_firmware/tree/master/tag)
- [External time sync](https://github.com/jtguggedal/positioning_firmware/tree/master/time_syncer)

The folders contain source files, including SEGGER Embedded Studio project files (v3.30 tested), along with compiled hex files that can be flashed directly to PCA20036 (nodes) and PCA10056 (tags and time sync signal controller).