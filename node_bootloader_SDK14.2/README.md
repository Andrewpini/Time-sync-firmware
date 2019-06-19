# Bootloader used for Bluetooth Mesh test rig with PCA20036 cards

This bootloader is used on PCA20036 cards with POE using Mesh SDK v3.1.0 and Softdevice v6.1.1. The purpose is to DFU over ethernet.

## How it works:

The ethernet_mesh_node project includes code for handling different ethernet packages. These packages can for example be sent with the program "Packet Sender" loaded with the correct commands file. 

- When a "New firmware all devices" package is received, a flag is set in GPREGRET register, and the device is restarted (reset). The bootloader reads this flag, and when it is set, DFU is started.

- When a "New firmware using button" package is received, a flag is set in GPREGRET register, and the device is restarted (reset). The bootloader reads this flag, and when it is set, the device goes in to a spin lock waiting for a button press (BUTTON 0). If the button is pressed, DFU is started. If nothing happens in 2 minutes the timeout timer resets the device and the "old" firmware is loaded.

The DFU is done over ethernet using TFTP protocoll. There is a timeout on 2 minutes if the bootloader does not get contact with the TFTP server.

If anything goes wrong the device should be restarted (reset) after 2 minutes or less, but if a DFU is started and somehow stopped halfways, the firmware is broken and must be flashed manually using J-Link.

## How to use:

1) Start the TFTP server (tftpd64.exe) in RootFolder/tftp_server
2) Start Packet Sender and load the commands file located in RootFolder/ethernet_commands
3) Build the firmware code in ethernet_mesh_node SES project located in RootFolder/node/ethernet_mesh_node_project/server
4) Send the "new firmware" command from Packet Sender - the DFU should now start on every connected card