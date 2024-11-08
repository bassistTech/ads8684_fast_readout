/*
Test program for ADS8684 ADC chip running at 500 kHz sampling rate
*/

#include <ADS8688.h>
#include <ArduinoJson.h>
#define MAX_DOC 1000
StaticJsonDocument<MAX_DOC> txDoc;
StaticJsonDocument<MAX_DOC> rxDoc;

#include "ads8684_array.h"

int readJson(){
  /* 
  Read JSON from the serial port, convert to document format
  returns 1 if error, otherwise 0
  */
  if (!Serial.available()) return 1;
  char json[MAX_DOC];
  int i = 0;
  while (1) {
    json[i] = Serial.read();
    if (json[i] == '\n') break;
    if (json[i] == '\r') {
      txDoc.clear();
      txDoc["error"] = "Received \\r character, please use \\n instead from now on";
      serializeJson(txDoc, Serial);
      Serial.write('\n');
      return 1;
    }
    i++;
  }
  json[i] = 0;
  
  DeserializationError err = deserializeJson(rxDoc, json);
  
  if (err) {
    txDoc.clear();
    txDoc["error"] = err.c_str();
    serializeJson(txDoc, Serial);
    Serial.write('\n');
    return 1;
  }
  return 0;
}

void setup() {
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);
  pinMode(SCOPE_PIN, OUTPUT);
  digitalWrite(SCOPE_PIN, LOW);
  digitalWrite(RST_PIN, LOW);
  digitalWrite(RST_PIN, HIGH);
  bank.setGlobalRange(R0);              // set range for all channels
  bank.setChannelPowerDown(0);
  Serial.begin(115200);                 // start serial communication
  delay(500);
  Serial.println("Welcome to ads8684_test");
}

// These are all of the globals used outside of the ISR. The ISR has its own globals

struct {
  float fsamp = 500e3; // sampling rate in Hz
  float npts = 10; // number of data points to read
  int chans[9] = {1, 0, 0, 0, 0, 0, 0, 0, 0};
  int nChans = 1;
  int ranges[9] = {0, 0, 0, 0, 0, 0, 0, 0 ,0};
} globals;

void printStatus(){
  /*
  Prints the entire contents of global state, more or less, feel free to add stuff
  */
  int i;
  txDoc.clear();
  txDoc["fsamp"] = globals.fsamp;
  txDoc["npts"] = globals.npts;
  txDoc["maxpts"] = maxpts;
  double average = adsGlobals.adcSum/globals.npts;
  double stdev = sqrt(adsGlobals.adcSum2/globals.npts - average*average);
  txDoc["average"] = average;
  txDoc["stdev"] = stdev;
  for (i=0; i<globals.nChans; i++) {
    txDoc["chans"][i] = globals.chans[i];
  }
  for (i=0; i<globals.nChans; i++) {
    txDoc["ranges"][i] = globals.ranges[i];
  }
  for (i=0; i<globals.nChans; i++) {
    txDoc["getRanges"][i] = bank.getChannelRange(globals.chans[i]);
  }
  serializeJson(txDoc, Serial);
  Serial.write('\n');
}

void printText(){
  /*
  Prints the contents of the data array to the host, in text format. This is expected
  to be used for debugging. Use binary format for any "real" application.
  */
  int i;
  txDoc.clear();
  for (i=0; i<globals.npts; i++) {
    txDoc["data"][i] = adsGlobals.adcData[i];
  }
  serializeJson(txDoc, Serial);
  Serial.write('\n');
}

void printBinary(){
  /*
  Prints a JSON header, followed by the contents of the data array in raw binary format
  */
  // Transmit an array of float32_t
  int nbytes = sizeof(adsGlobals.adcData[0])*adsGlobals.npts;
  txDoc.clear();
  txDoc["bytes"] = nbytes;
  txDoc["type"] = "uint16";
  serializeJson(txDoc, Serial);
  Serial.write('\n');
  Serial.write((char*)&adsGlobals.adcData, nbytes);
  // Serial.flush();
}

void chansMans(){
  unsigned int i;
  for (i=0; i<9; i++) adsGlobals.mans[i] = 0;
  for (i=0; i<globals.nChans; i++) {
    adsGlobals.mans[i] = chanReg(globals.chans[i]) << 24;
    bank.setChannelRange(globals.chans[i], rangeConsts[globals.ranges[i]]);
  }
}
void chansCmd(){
  unsigned int i;
  globals.nChans = rxDoc["chans"].size();
  for (i=0; i<rxDoc["chans"].size(); i++) {
    globals.chans[i] = rxDoc["chans"][i];
  }
}

void rangesCmd(){
  unsigned int i;
  for (i=0; i<rxDoc["ranges"].size(); i++) {
    globals.ranges[i] = rxDoc["ranges"][i];
  }
}

void readArrayCmd(){
  chansMans();
  readArray(globals.npts, globals.fsamp);
}

void resetCmd(){
  digitalWrite(RST_PIN, LOW);
  digitalWrite(RST_PIN, HIGH);
}

void loop() {
  /*
  The main loop reads and parses JSON commands from the host
  */
  int needPrint = 0; // set to 1 if we need to print status at the end
  if (!readJson()) {
    if (rxDoc.containsKey("fsamp")) {
      globals.fsamp = rxDoc["fsamp"];
      needPrint = 1;
    }
    if (rxDoc.containsKey("npts")) {
      globals.npts = rxDoc["npts"];
      needPrint = 1;
    }
    if (rxDoc.containsKey("status")) {
      needPrint = 1;
    }
    if (rxDoc.containsKey("chans")) {
      chansCmd();
      needPrint = 1;
    }
    if (rxDoc.containsKey("ranges")) {
      rangesCmd();
      needPrint = 1;
    }
    if (rxDoc.containsKey("read")) {
      readArrayCmd();
      needPrint = 1;
    }
    if (rxDoc.containsKey("print")) {
      printText();
      needPrint = 1;
    }
    if (rxDoc.containsKey("dump")) {
      printBinary();
      needPrint = 0;
    }
    if (rxDoc.containsKey("reset")) {
      resetCmd();
      needPrint = 1;
    }
  }
  if (needPrint) {
    printStatus();
  }
}
