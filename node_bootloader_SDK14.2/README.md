# Bootloader used for Bluetooth Mesh test rig with PCA20036 cards

This bootloader is used on PCA20036 cards with POE using Mesh SDK v3.1.0 and Softdevice v6.1.1. The purpose is to DFU over ethernet.

## How it works:

The ethernet_mesh_node project includes code for handling different ethernet packages. These packages can for example be sent with the program "Packet Sender" loaded with the correct commands file. 

- When a "New firmware all devices" package is received, a flag is set in GPREGRET register, and the device is restarted (reset). The bootloader reads this flag, and DFU is started on all devices connected to the network.

- When a "New firmware using MAC" package is received, the device checks if its MAC address is the same as the MAC address recieced in the package. If this is true a flag is set in GPREGRET register, and the device is restarted (reset). The bootloader reads this flag, and DFU is started on only this device.

- When a "New firmware using button enabled" package is received, a flag is set making it possible to start DFU by pressing BUTTON 0. If the button is pressed, a flag is set in GPREGRET register, and the device is restarted (reset). The bootloader reads this flag, and DFU is started on only this device. 

- When a "New firmware using button disabled" package is received, a flag is cleared, making it not possible to start DFU by pressing BUTTON 0.

The DFU is done over ethernet using TFTP protocoll. There is a timeout on 2 minutes if the bootloader does not get contact with the TFTP server.

If anything goes wrong the device should be restarted (reset) after 2 minutes or less, but if a DFU is started and somehow stopped halfways, the firmware is broken and must be flashed manually using J-Link.

## How to use:

1) Start the TFTP server (tftpd64.exe) in RootFolder/tftp_server
2) Start Packet Sender and load the commands file located in RootFolder/ethernet_commands
3) Build the firmware code in ethernet_mesh_node SES project located in RootFolder/node/ethernet_mesh_node_project/server
4) Send one of the "New firmware" commands from Packet Sender - all the connected cards should now check if they should go into DFU mode