/*
This is code for reading ADAS8684 ADC chip at sampling rates up to the rated
maximum of 500 kHz, on a Teensy 4 microcontroller board. It's based on the ADS8688 code 
at this site:

  https://github.com/siteswapjuggler/ADS8688a

You need to install this library.

A word about the SPI clock. The datasheet for the ADS8684 says the maximum clock speed is 17 MHz, and
the maximum sampling rate is 500 kHz. Each reading requires a 32 bit data transfer, so the SPI transaction
takes 1882 ns. There seems to be about a 120 ns delay between loading data into the SPI transmit FIFO
and when the bus transaction begins, leaving not enough time to get everything done before 2000 ns later
when the next reading starts.

I've also measured the SPI clock frequency on a scope, and have decided that one of two practical
compromises is possible: 1) Limit the sampling rate to about 450 kHz, still quite useful. 2) Increase
the SPI clock speed to 19 MHz, which seems pretty darn close to an actual rate of 17 MHz on my scope.

*/

#ifndef ARDUINO_TEENSY40
#warning This code is only tested on Teensy 4.0
#endif

#include <ADS8688.h>
#include "spi_regs.h"
#define RST_PIN 9 // reset pin
#define CS_PIN 10 // chip select pin
#define NUM_CHANS 4 // number of channels (using ADS8684 here)
#define SCOPE_PIN 6 // used for program timing using a scope
#define SPI_CLOCK 18000000 // needed to run a bit higher than 17 MHz


ADS8688 bank = ADS8688(CS_PIN);
IntervalTimer adcTimer;

#define maxpts 32768 // max data points during "read" operation

const uint32_t rangeConsts[] = {0, 1, 2, 5, 6};

// ISR is a state machine. These are the states:

typedef enum {
  adcIdle, // typically used for Done with running
  adcStart, // start, ignore the first few readings
  adcRun // running, collecting the data array
} adcStates;

struct {
  volatile adcStates adcState = adcIdle; // machine state within ISR
  volatile int npts = 0; // number of data points collected
  volatile int lastpt = 0; // last point to collect
  volatile uint16_t adcData[maxpts]; // data buffer for ADC results
  volatile uint32_t mans[9]; // list of manual channel selections, terminated with 0
  volatile int nextman = 0; // manual selection for the next ISR cycle
  volatile double adcSum = 0; // sum for computing average
  volatile double adcSum2 = 0; // sum of squares for computing standard deviation
} adsGlobals;

uint8_t chanReg(int ch) {
  uint8_t reg;
  switch (ch) {
      case 0:  reg = MAN_Ch_0;break;
      case 1:  reg = MAN_Ch_1;break;
      case 2:  reg = MAN_Ch_2;break;
      case 3:  reg = MAN_Ch_3;break;
      case 4:  reg = MAN_Ch_4;break;
      case 5:  reg = MAN_Ch_5;break;
      case 6:  reg = MAN_Ch_6;break;
      case 7:  reg = MAN_Ch_7;break;
      case 8:  reg = MAN_AUX; break;
      default: reg = MAN_Ch_0;break;
  }
  return reg;
}

void adcISR(){
  // Interrupt service routine for fast ADC data collection
  
  // First, manage the actual ADC reading
  
  //digitalWriteFast(SCOPE_PIN, HIGH); // used for monitoring conversion timing on a scope
  digitalWriteFast(CS_PIN, HIGH); // end previous conversion
  uint16_t result = SPI_REGS.RDR;
  digitalWriteFast(CS_PIN, LOW); // start next conversion
  SPI_REGS.TDR = adsGlobals.mans[adsGlobals.nextman];  
  //digitalWrite(SCOPE_PIN, LOW); storing the result. Meanwhile,
  // we can take care of the state machine
  
  if (adsGlobals.adcState == adcStart) { // padding and synchronization of array
    adsGlobals.nextman = 0;
    adsGlobals.npts = -2; // first two readings of run state are already in queue
    adsGlobals.adcState = adcRun;
  }
  else if (adsGlobals.adcState == adcRun) { // Filling the data array with readings
    if (adsGlobals.npts >= 0) {
      adsGlobals.adcData[adsGlobals.npts] = result;
      adsGlobals.adcSum += result;
      adsGlobals.adcSum2 += result*result;
    }
    adsGlobals.npts++;
    if (adsGlobals.npts >= adsGlobals.lastpt) {
      adsGlobals.adcState = adcIdle;
    }
    adsGlobals.nextman++;
    if (adsGlobals.mans[adsGlobals.nextman] == 0) adsGlobals.nextman = 0;
  }
}

void readArray(int len, float fs){
  SPI.beginTransaction(SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE1));
  uint32_t old = SPI_REGS.TCR;
  SPI_REGS.TCR = (SPI_REGS.TCR & 0xfffff000) | LPSPI_TCR_FRAMESZ(31); // switch to 32 bit mode
  adsGlobals.npts = 0;
  adsGlobals.lastpt = len;
  adsGlobals.adcSum = 0;
  adsGlobals.adcSum2 = 0;
  adsGlobals.nextman = 0;
  adsGlobals.adcState = adcIdle;
  adcTimer.begin(adcISR, 1e6/fs);
  adcTimer.priority(0);
  adsGlobals.adcState = adcStart;
  while (adsGlobals.adcState != adcIdle);
  adcTimer.end();
  while ((SPI_REGS.RSR & LPSPI_RSR_RXEMPTY))
  SPI_REGS.TCR = old;
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}
