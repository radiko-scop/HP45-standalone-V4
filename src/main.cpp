#include <Arduino.h>
#include <assert.h>
#include <SerialTransfer.h>
#include "DMAPrint.h"
#include "yteclogo.h"
// #include "IntervalTimer.h"

#define CIRCULAR_BUFFER_INT_SAFE // safe interrupts
#include <CircularBuffer.h>

int positionPx = 0;

SerialTransfer myTransfer;

#define ROW_GAP 4050 //the distance in microns between odd and even row in long
#define ROW_GAP_PX 96

#define TRIGGER_PIN 24

typedef struct InkLine {
  uint8_t data[38];
  InkLine()
  {
    memset(data, 0, sizeof(data));
  }
} InkLine;

uint16_t DataBurst[22]; //the printing burst for decoding

// uint32_t px_position = floor((double) microns / 42.333333333333336)

/**
 * @brief InkjetBuffer;
 * It is supposed to work the following way :
 * - Fill it using buffer.push(0);
 * - buffer.shift(); object from it each time you advance one pixel
 *
 */
CircularBuffer<InkLine, 5000> InkJetBuffer;
CircularBuffer<InkLine, 1000> OddInkJetBuffer;

//for DMA
const uint32_t dmaBufferSize = 320; //242 max theoretical, the actual size of the DMA buffer (takes data per 2 bytes, 1 per port)
const uint32_t dmaFrequency = 1050000; //the frequency in hertz the buffer should update at

DMAMEM uint8_t portCMemory[dmaBufferSize]; //stores data for port C DMA
DMAMEM uint8_t portDMemory[dmaBufferSize]; //stores data for port D DMA
uint8_t portCWrite[dmaBufferSize]; //stores data for port C editing
uint8_t portDWrite[dmaBufferSize]; //stores data for port D editing

DMAPrint dmaHP45(dmaBufferSize, portCMemory, portDMemory, portCWrite, portDWrite, dmaFrequency); //init the dma library
uint32_t inkjetLastBurst = 0;
bool printing = false;

#define CODE_BUFFER_AVAILABLE 0

int PixelPeriodMicros = 4233; // microseconds
// IntervalTimer myTimer;

float parseFloat()
{
  float mm;
  uint16_t recSize = 0;
  recSize = myTransfer.rxObj(mm, recSize);
  return mm;
}

void setSpeed()
{
  float PixelSizeMicrons = 42.33333333333334;
  float MicronsPerMicroSecond = parseFloat() / 1000; // *1000 / 1e6
  PixelPeriodMicros = PixelSizeMicrons / MicronsPerMicroSecond;
}

void flushOneByte()
{
 // trick to avoid PAYLOAD ERROR, since a null payload is considered an error
  uint16_t recSize = 0;
  uint8_t unused = 0;
  recSize = myTransfer.rxObj(unused, recSize);
}

void prime()
{
  // Serial1.println("Prime");
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
  // Serial1.println("Starting printing");
  dmaHP45.SetEnable(1); //enable the head
  printing = true;
  positionPx = 0;
  // myTimer.begin(setAvailableSpaceFlag, PixelPeriod);
}

void stopPrinting()
{
  flushOneByte();
  dmaHP45.SetEnable(0);
  InkJetBuffer.clear();
  OddInkJetBuffer.clear();
  printing = false;
  // myTimer.end();
}

void getBufferAvailable()
{
  flushOneByte(); // always one byte of payload, even if unused...
  // Serial1.println("Getting available buffers");
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
    recSize = myTransfer.rxObj(line.data, recSize);
    InkJetBuffer.push(line);

  }else{
    Serial1.println("Buffer full !!!");

    // uint16_t sendSize = 0;
    // sendSize = myTransfer.txObj<int16_t>(-1, sendSize);
    // myTransfer.sendData(sendSize, 0);
  }
}

// supplied as a reference - persistent allocation required
const functionPtr callbackArr[] = { appendToBuffer, getBufferAvailable, prime, startPrinting, stopPrinting, setSpeed };
///////////////////////////////////////////////////////////////////

void onTrigger()
{
  if(!InkJetBuffer.isEmpty()) // || !OddInkJetBuffer.isEmpty())
  {

    dmaHP45.SetEnable(1); //enable the head

    if( !InkJetBuffer.isEmpty())
    {
      InkLine currentBurst = InkJetBuffer.shift();
      OddInkJetBuffer.push(currentBurst);
      dmaHP45.ConvertB8ToBurst(currentBurst.data, DataBurst, true); // set even values
    }else{
      InkLine empty;
      dmaHP45.ConvertB8ToBurst(empty.data, DataBurst, true); // set odd values
    }

    // if( (positionPx >= ROW_GAP_PX) && !OddInkJetBuffer.isEmpty())
    // {
    //   InkLine oddCurrentBurst = OddInkJetBuffer.shift();
    //   dmaHP45.ConvertB8ToBurst(oddCurrentBurst.data, DataBurst, false); // set odd values
    // }else{
      InkLine empty;
      dmaHP45.ConvertB8ToBurst(empty.data, DataBurst, false); // set odd values
    // }
    dmaHP45.SetBurst(DataBurst, 1);
    dmaHP45.Burst(); //burst the printhead
    positionPx++;
  }else{
    Serial1.println("Trying to print but Buffer are empty !!");
  }
}

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

  pinMode(TRIGGER_PIN, INPUT);
  // pinMode(TRIGGER_PIN, OUTPUT);
  // digitalWrite(TRIGGER_PIN, LOW);

  // attachInterrupt(digitalPinToInterrupt(TRIGGER_PIN), onTrigger, RISING);

  // Serial1.println(F("Ready - setup finished"));
  printing = false;
}


uint8_t lastState = HIGH;
void loop()
{
  // noInterrupts();
  myTransfer.tick();
  // interrupts();
  uint8_t state = digitalRead(TRIGGER_PIN);
  if(state != lastState )
  {
    if(printing)
    {
      if((micros() - inkjetLastBurst) > (PixelPeriodMicros+700))
      {
        Serial1.print(micros() - inkjetLastBurst);
        Serial1.print("ms elapsed instead of ");
        Serial1.println(PixelPeriodMicros);
      }
      inkjetLastBurst = micros();
      onTrigger();
      lastState = state;
    }
  }
  // if (printing && ((micros() - inkjetLastBurst) > PixelPeriodMicros)) { //if burst is required again based on time (updated regradless of burst conditions)
  //   inkjetLastBurst = micros();
  //   onTrigger();
  // }
  // else {
  //   // dmaHP45.SetEnable(0); //disable the head
  // }
}
