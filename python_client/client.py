import time
from pySerialTransfer import pySerialTransfer as txfer

from enum import Enum

from setuptools import Command

class ResponseId(Enum):
    AVAILABLE_BUFFER = 0

class CommandId(Enum):
    APPEND = 0
    GET_AVAILABLE_BUFFER = 1
    PRIME = 2
    START_PRINT = 3
    STOP_PRINT = 4


class InkjetController:

    def waitFor(self, id):
        while True:
            if self.link.available():
                self.callback_list()[self.link.idByte]()
                if(self.link.idByte == id):
                    break
            elif self.link.status < 0:
                if self.link.status == txfer.CRC_ERROR:
                    print('ERROR: CRC_ERROR')
                elif self.link.status == txfer.PAYLOAD_ERROR:
                    print('ERROR: PAYLOAD_ERROR')
                elif self.link.status == txfer.STOP_BYTE_ERROR:
                    print('ERROR: STOP_BYTE_ERROR')
                else:
                    print('ERROR: {}'.format(self.link.status))
                break

    def sendEmptyCommand(self, code):
        sendSize = self.link.tx_obj(0, start_pos=0, val_type_override='B')
        if not self.link.send(sendSize, packet_id=code):
            raise Exception("Couldn't send message")

    def startPrint(self):
        self.sendEmptyCommand(CommandId.START_PRINT.value)

    def stopPrint(self):
        self.sendEmptyCommand(CommandId.STOP_PRINT.value)

    def askBufferAvailable(self):
       self.sendEmptyCommand(CommandId.GET_AVAILABLE_BUFFER.value)

    def prime(self):
        self.sendEmptyCommand(CommandId.PRIME.value)

    def forceSendOneLine(self, line):
        """sends one line, but without checking
        """
        sendSize = 0
        for elt in line:
            sendSize = self.link.tx_obj(elt, start_pos=sendSize, val_type_override='H')
        self.link.send(sendSize, packet_id=CommandId.APPEND.value)
        print("sent line")
        self.available -= 1

    def sendOneLine(self, line, blocking=True):
        """
        """
        if self.available <= 0:
            print("0 available !")
            return False

        print(f"sending line since available is: {self.available}")
        self.forceSendOneLine(line)
        # if blocking:
        #     self.waitAnswer(ResponseId.AVAILABLE_BUFFER.value)
        return True

    def availableBufferCallback(self):
        self.available = self.link.rx_obj(obj_type='h', start_pos=0)
        print('ATMEGA says there are {} lines available left in buffer'.format(self.available))


    '''
    list of callback functions to be called during tick. The index of the function
    reference within this list must correspond to the packet ID. For instance, if
    you want to call the function hi() when you parse a packet with an ID of 0, you
    would write the callback list with "hi" being in the 0th place of the list:
    '''
    def callback_list(self):
        return [ self.availableBufferCallback ]

    def update(self):
        self.link.tick()

    def close(self):
        self.link.close()

    def __init__(self, port):
        self.link = txfer.SerialTransfer(port)
        self.link.set_callbacks(self.callback_list())
        if not self.link.open():
            raise Exception("Failed to open port")
        self.available = 0
        self.askBufferAvailable()
        self.waitFor(ResponseId.AVAILABLE_BUFFER.value)

if __name__ == '__main__':
    try:
        ctl = InkjetController('/dev/ttyACM0')
        time.sleep(2)
        index = 0
        for _ in range(5):
            print('prime!')
            ctl.prime()
            time.sleep(1)

        ctl.startPrint()
        while True:
            for i in range(255):
                time.sleep(0.02)
                line = [index]*22
                print(f"sending: {index}")
                ctl.sendOneLine(line, blocking=True)
                ctl.update()
                index += 1

    except KeyboardInterrupt:
        line = [0]*22
        ctl.sendOneLine(line, blocking=True)
        ctl.update()
        ctl.stopPrint()
        ctl.close()

    except:
        import traceback
        traceback.print_exc()

        ctl.close()