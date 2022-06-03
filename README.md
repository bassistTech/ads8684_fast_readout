# ADS8684: A fast precision ADC for the Teensy 4 microcontroller board

Francis Deck

Here's a circuit and firmware for operating an ADS8684 16-bit ADC chip at a 500 kHz sampling rate, on a Teensy 4.0 board. The circuit is easy to build by hand on a breakout board, yet meets the datasheet specs for the ADC chip.

## About the ADS8684 ADC chip

* 4 channels, 16 bits, 500 kHz
* Can be hand soldered
* Built-in buffered inputs eliminated a bunch of analog signal conditioning
* Available!

## About the Teensy 4.0 microcontroller board

* Can be programmed in the Arduino development environment
* Screaming fast: 600 MHz, dual core, built-in floating point
* Available!

# Circuit construction

I haven't designed a printed circuit board for the project yet. Meanwhile it seems pretty attractive to leave it on the breakout board. The only challenge was soldering the TSSOP chip package, and a custom board won't make that any easier. The amount of hand wiring was minimal, and took just a few minutes of effort. And... in the spirit of the times, it makes little sense to spin up for production when the chip might suddenly become unavailable. This is going to be a "life is short" project.

I also have a "let's get coding" attitude. Building the circuit by hand was the quickest route to writing and testing firmware and software. 

## Schematic diagram

## The first prototype

## Software development

Turns out there's already a library!

	https://github.com/siteswapjuggler/ADS8688a
	
It doesn't handle high speed continuous data collection, but contains everything needed to set up the chip. So I use the ADS8688 library to configure all of the operating parameters of the chip, then I transfer control of the chip to an interrupt service routine that interacts directly with the Teensy 4.0 SPI hardware registers. The result is that my code is actually quite small.

## Gearing up for fast data collection

Here's the math: The ADS8688 has a maximum SPI clock speed of 17 MHz, and a maximum sampling rate of 500 kHz. The entire SPI transaction is 32 bits long, which takes 1882 ns, but the entire time available to read the ADC at a sampling rate of 500 kHz is 2000 ns. This leaves precious little time, about 120 ns, for anything to happen in between samples.

The standard SPI transaction is "blocking," meaning that your program waits for the transaction to finish before proceeding. Instead, I've broken up the transaction into separate parts for writing and reading data. This allows me to perform a "read then write" operation, with no waiting. My program can do other things while the SPI hardware performs the transaction.

The transaction is performed inside an interrupt service routine (ISR), controlled by an interval timer, that can run at the maximum sampling rate of 500 kHz. The ISR is broken into two parts:

1. The "read then write" operation of the SPI interface.
2. Storing the conversion result in an array, and performing some basic statistics.

There are still some minor issues. The 17 MHz clock frequency in the SPI settings is not quite enough to finish each transaction before the next interrupt starts, wreaking havoc. (This is noticeable because a grounded input produces seemingly random garbage). My scope isn't quite good enough to tell if it's exactly 17 MHz, but it seems a bit higher, so I raised the frequency in my program to 18 MHz and everything worked fine. In any event, the clock speed is approximate because it's based on a small integer divisor.

## Building a simple API

We need a command "language" for interacting with the board. My preferences are: Human readable, and easily translated by a language like Python. Typing commands into the terminal is useful for debugging, but ultimately, anything elaborate will benefit from scripting. 

My preference *du jour* is JSON format. There are lovely libraries for JSON for Arduino, as well as for most languages on the PC side. It's not as "tight" as you could imagine, but the Teensy is so fast that it's OK to waste a few bytes. Thanks to libraries on both ends of the USB cable, I don't have to write any low level parsing code. And it's really easy to add stuff to the API.

Everything in JSON is a key and a value, such as 

	{"npts": 4096}
	
You can have multiple key-value pairs in one string, such as 

	{"npts": 4096, "fsamp": 200000, "read": 1, "dump": 1}
	
Every key needs a value, so keys that represent commands are given a value that's ignored.

## API command list, 

	{"fsamp": f} sets the sampling frequency in Hz
	
	{"npts": n} sets the number of data points in the array
	
	{"status": 1} prints all of the current settings, and then some
	
	{"read": 1} reads a block of data into a memory buffered
	
	{"print": 1} prints the data in text format, useful for debugging
	
	{"dump": 1} prints a text header, followed by the contents of the array in binary format
	
	{"chans": list} sets the list of input channels that are turned on, such as [0, 1]
	
