#include <Arduino.h>
#include <assert.h>
#include <SerialTransfer.h>

#define CIRCULAR_BUFFER_INT_SAFE // safe interrupts
#include <CircularBuffer.h>

#define TIMER_INTERRUPT_DEBUG         0
#define _TIMERINTERRUPT_LOGLEVEL_     0

#define USE_TIMER_1     true

// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
#include "TimerInterrupt.h"

#define TIMER1_INTERVAL_MS    1000

SerialTransfer myTransfer;

typedef struct {
  uint8_t data[38];
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

#define CODE_BUFFER_AVAILABLE 0

void getBufferAvailable()
{
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
    Serial1.print("Size Received: ");
    Serial1.print(recSize);
    Serial1.print(" value: ");
    Serial1.println(line.data[0]);

    // for(int i=0; i< 38; i++)
    // {

    //     // uint16_t sendSize = 0;
    //     // sendSize = myTransfer.txObj<int16_t>(-2, sendSize);
    //     // myTransfer.sendData(sendSize, 0);

    //     Serial1.print("Received: ");
    //     Serial1.print(i);
    //     Serial1.print("value: ");
    //     Serial1.println(line.data[i]);

    // }
    InkJetBuffer.push(line);

  }else{
    // uint16_t sendSize = 0;
    // sendSize = myTransfer.txObj<int16_t>(-1, sendSize);
    // myTransfer.sendData(sendSize, 0);
  }
}

void TimerHandler1(void)
{
  InkJetBuffer.shift();
  Serial1.println("Shifted one line");
  uint16_t sendSize = 0;
  sendSize = myTransfer.txObj<int16_t>(InkJetBuffer.available(), sendSize);
  myTransfer.sendData(sendSize, CODE_BUFFER_AVAILABLE);
}


// supplied as a reference - persistent allocation required
const functionPtr callbackArr[] = { appendToBuffer, getBufferAvailable };
///////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200);

  ///////////////////////////////////////////////////////////////// Config Parameters
  configST myConfig;
  myConfig.debug        = false;
  myConfig.callbacks    = callbackArr;
  myConfig.callbacksLen = sizeof(callbackArr) / sizeof(functionPtr);
  /////////////////////////////////////////////////////////////////

  myTransfer.begin(Serial, myConfig);

  ITimer1.init();

  // Using ATmega328 used in UNO => 16MHz CPU clock ,
  // For 16-bit timer 1, 3, 4 and 5, set frequency from 0.2385 to some KHz
  // For 8-bit timer 2 (prescaler up to 1024, set frequency from 61.5Hz to some KHz

  if (ITimer1.attachInterruptInterval(TIMER1_INTERVAL_MS, TimerHandler1))
  {
    Serial.print(F("Starting  ITimer1 OK, millis() = ")); Serial.println(millis());
  }
  else
    Serial.println(F("Can't set ITimer1. Select another freq. or timer"));
}


void loop()
{
  myTransfer.tick();
}
