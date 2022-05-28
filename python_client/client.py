import time
from pySerialTransfer import pySerialTransfer as txfer

from enum import Enum

class ResponseId(Enum):
    AVAILABLE_BUFFER = 0

class InkjetController:

    # def waitAnswer(self, id):
    #       while True:
    #         if self.link.available():
    #             self.callback_list()[self.link.idByte]()
    #             if(self.link.idByte == id):
    #                 break
    #         elif self.link.status < 0:
    #             if self.link.status == txfer.CRC_ERROR:
    #                 print('ERROR: CRC_ERROR')
    #             elif self.link.status == txfer.PAYLOAD_ERROR:
    #                 print('ERROR: PAYLOAD_ERROR')
    #             elif self.link.status == txfer.STOP_BYTE_ERROR:
    #                 print('ERROR: STOP_BYTE_ERROR')
    #             else:
    #                 print('ERROR: {}'.format(self.link.status))
    #             break

    def askBufferAvailable(self):
        self.link.send(0, packet_id=1)

    def forceSendOneLine(self, line):
        """sends one line, but without checking
        """
        sendSize = 0
        for elt in line:
            sendSize = self.link.tx_obj(elt, start_pos=sendSize, val_type_override='B')
        self.link.send(sendSize, packet_id=0)
        self.available -= 1

    def sendOneLine(self, line, blocking=True):
        """
        """
        if self.available <= 0:
            return False

        print(f"sending line since available is: {self.available}")
        self.forceSendOneLine(line)
        # if blocking:
        #     self.waitAnswer(ResponseId.AVAILABLE_BUFFER.value)
        return True

    def availableBufferCallback(self):
        self.available = self.link.rx_obj(obj_type='h', start_pos=0)
        print('ATMEGA says there are {} lines available left in buffer'.format(self.available))

    def primeCallback(self):
        pass

    def setSpeedCallback(self):
        pass

    def startTriggerCallback(self):
        pass

    '''
    list of callback functions to be called during tick. The index of the function
    reference within this list must correspond to the packet ID. For instance, if
    you want to call the function hi() when you parse a packet with an ID of 0, you
    would write the callback list with "hi" being in the 0th place of the list:
    '''
    def callback_list(self):
        return [ self.availableBufferCallback, self.primeCallback, self.setSpeedCallback, self.startTriggerCallback ]

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

if __name__ == '__main__':
    try:
        ctl = InkjetController('/dev/ttyACM0')
        time.sleep(2)
        index = 0
        while True:
            time.sleep(0.02)
            line = [index]*38
            ctl.sendOneLine(line, blocking=True)
            ctl.update()

    except KeyboardInterrupt:
        ctl.close()

    except:
        import traceback
        traceback.print_exc()

        ctl.close()