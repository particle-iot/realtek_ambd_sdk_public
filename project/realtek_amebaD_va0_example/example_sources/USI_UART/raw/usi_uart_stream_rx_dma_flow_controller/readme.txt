Example Description

This example describes how to use USI UART to communicate with PC.

Required Components:
    USBtoTTL adapter

Connect to PC
 - Connect Ground: connect to GND pin via USBtoTTL adapter
 - Use USI UART
	GPIOB_21 as UART0_RX connect to TX of USBtoTTL adapter
	GPIOB_20 as UART0_TX connect to RX of USBtoTTL adapter

Open Super terminal or teraterm and 
set baud rate to 38400, 1 stopbit, no parity, no flow contorl.

This example shows:
User input will be received by demo board, and demo board will loopback the received character with timeout.

Note that buffer length should be integral multiple of 32 bytes, and buffer address should be 32 bytes aligned when using DMA. 
