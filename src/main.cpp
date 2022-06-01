#include <Arduino.h>
#include <assert.h>
#include <SerialTransfer.h>
#include "DMAPrint.h"
#include "IntervalTimer.h"

#define CIRCULAR_BUFFER_INT_SAFE // safe interrupts
#include <CircularBuffer.h>

SerialTransfer myTransfer;

typedef struct {
  uint16_t data[22];
} InkLine;

// uint32_t px_position = floor((double) microns / 42.333333333333336)

/**
 * @brief InkjetBuffer;
 * It is supposed to work the following way :
 * - Fill it using buffer.push(0);
 * - buffer.shift(); object from it each time you advance one pixel
 *
 */
CircularBuffer<InkLine, 100> InkJetBuffer;


//for DMA
const uint32_t dmaBufferSize = 320; //242 max theoretical, the actual size of the DMA buffer (takes data per 2 bytes, 1 per port)
const uint32_t dmaFrequency = 1050000; //the frequency in hertz the buffer should update at

DMAMEM uint8_t portCMemory[dmaBufferSize]; //stores data for port C DMA
DMAMEM uint8_t portDMemory[dmaBufferSize]; //stores data for port D DMA
uint8_t portCWrite[dmaBufferSize]; //stores data for port C editing
uint8_t portDWrite[dmaBufferSize]; //stores data for port D editing

DMAPrint dmaHP45(dmaBufferSize, portCMemory, portDMemory, portCWrite, portDWrite, dmaFrequency); //init the dma library

#define CODE_BUFFER_AVAILABLE 0

int SpeedPx = 2362; // pixel/s, ie ~10mm/s
int PixelPeriod = 423; // microseconds
IntervalTimer myTimer;

void flushOneByte()
{
 // trick to avoid PAYLOAD ERROR, since a null payload is considered an error
  uint16_t recSize = 0;
  uint8_t unused = 0;
  recSize = myTransfer.rxObj(unused, recSize);

}

void prime()
{
  Serial1.println("Prime");
  flushOneByte(); // always one byte of payload, even if unused...
  dmaHP45.SetEnable(1); //temporarily enable head
  dmaHP45.Prime(100);
  dmaHP45.SetEnable(0); //temporarily enable head
}

volatile bool sendAvailableSpace = false;

void setAvailableSpaceFlag(void)
{
    sendAvailableSpace = true;
}

void startPrinting()
{
  flushOneByte(); // always one byte of payload, even if unused...
  Serial1.println("Starting printing");
  myTimer.begin(setAvailableSpaceFlag, PixelPeriod);
}

void stopPrinting()
{
  flushOneByte();
  myTimer.end();
}

void getBufferAvailable()
{
  flushOneByte(); // always one byte of payload, even if unused...
  Serial1.println("Getting available buffers");
  uint16_t sendSize = 0;
  sendSize = myTransfer.txObj<int16_t>(InkJetBuffer.available(), sendSize);
  myTransfer.sendData(sendSize, CODE_BUFFER_AVAILABLE);
}

void appendToBuffer()
{
  if (InkJetBuffer.available() > 0)
  {
    InkLine line;
    memset(&line, 0, sizeof(line));
    uint16_t recSize = 0;
    Serial1.println("Going to receive");
    recSize = myTransfer.rxObj(line.data, recSize);
    Serial1.print("Size Received: ");
    Serial1.print(recSize);
    Serial1.print(" value: ");
    Serial1.println(line.data[0]);

    // for(int i=0; i< 38; i++)
    // {

    //     // uint16_t sendSize = 0;
    //     // sendSize = myTransfer.txObj<int16_t>(-2, sendSize);
    //     // myTransfer.sendData(sendSize, 0);

        // Serial1.print("Received: ");
        // Serial1.print(i);
        // Serial1.print("value: ");
        // Serial1.println(line.data[i]);


    // }
    InkJetBuffer.push(line);

  }else{
    // uint16_t sendSize = 0;
    // sendSize = myTransfer.txObj<int16_t>(-1, sendSize);
    // myTransfer.sendData(sendSize, 0);
  }
}

void doSendAvailableSpace(void)
{
  if(!InkJetBuffer.isEmpty())
  {
    InkLine currentBurst = InkJetBuffer.shift();
    dmaHP45.SetEnable(1); //enable the head
    dmaHP45.SetBurst(currentBurst.data, 1);
    dmaHP45.Burst(); //burst the printhead
    Serial1.println("Buuurst !");
  }
  //buffer.burst values if any, zeros else.
  uint16_t sendSize = 0;
  sendSize = myTransfer.txObj<int16_t>(InkJetBuffer.available(), sendSize);
  myTransfer.sendData(sendSize, CODE_BUFFER_AVAILABLE);
  sendAvailableSpace = false;
}

// supplied as a reference - persistent allocation required
const functionPtr callbackArr[] = { appendToBuffer, getBufferAvailable, prime, startPrinting, stopPrinting };
///////////////////////////////////////////////////////////////////

void setup()
{
  dmaHP45.begin();

  Serial.begin(115200);
  Serial1.begin(115200);

  ///////////////////////////////////////////////////////////////// Config Parameters
  configST myConfig;
  myConfig.debug        = false;
  myConfig.callbacks    = callbackArr;
  myConfig.callbacksLen = sizeof(callbackArr) / sizeof(functionPtr);
  /////////////////////////////////////////////////////////////////

  myTransfer.begin(Serial, myConfig);

  Serial1.println(F("Ready - setup finished"));
}

void loop()
{
  noInterrupts();
  myTransfer.tick();
  bool sendAvailableSpaceCopy = sendAvailableSpace;
  interrupts();

  if(sendAvailableSpaceCopy)
  {
    doSendAvailableSpace();
  }
}
