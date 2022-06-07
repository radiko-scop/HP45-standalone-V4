import time
from pySerialTransfer import pySerialTransfer as txfer
from ImageConverter2 import ImageSlicer
from OPSerialGRBL import GRBL
from unittest.mock import Mock

from enum import Enum
class ResponseId(Enum):
    AVAILABLE_BUFFER = 0

class CommandId(Enum):
    APPEND = 0
    GET_AVAILABLE_BUFFER = 1
    PRIME = 2
    START_PRINT = 3
    STOP_PRINT = 4


class InkjetController:

    def connectMotion(self, port):
        self.motion = GRBL()
        self.motion.Connect(port)

    def openImage(self, path):
         self.imageSlicer = ImageSlicer(path)

    def _printSweep(self, sweep):
        index = 0
        self.array += "{\n"
        for line in sweep:
            self.array += f"{line},\n"
            success = False
            while not success:
                success = self.sendOneLine(line)
                if not success:
                    print("fail ...")
                    time.sleep(0.02)
                    self.askBufferAvailable()
                    self.waitFor(ResponseId.AVAILABLE_BUFFER.value)
                else:
                    print(f"sent line {index}")
            index += 1
        self.array += "},\n"

    def print(self):
        self.pixel_to_pos_multiplier = 25.4 / self.imageSlicer.dpi()
        self.motion.setZeros()
        sweep_index = 0
        for sweep in self.imageSlicer.imageSweeps(packed=True):
            print(f"Processing sweep {sweep_index}, lines in sweep : {len(sweep)}")
            x = self.pixel_to_pos_multiplier*len(sweep) + 10
            y = -self.pixel_to_pos_multiplier*sweep_index*self.imageSlicer.dpi()/2
            self.motion.asyncMove(0, y, self.print_velocity)
            self.motion.waitMotionEnd()
            self.motion.asyncMove(x, y, self.print_velocity)
            self.startPrint()
            self._printSweep(sweep)
            self.motion.waitMotionEnd()
            self.stopPrint()
            self.motion.asyncMove(0, y, self.print_velocity*4)
            self.motion.waitMotionEnd()
            sweep_index += 1
            sweep.dump(f"ytec{sweep_index}.bin")

        self.motion.asyncMove(0, y, self.print_velocity*4)
        self.motion.waitMotionEnd()
        self.motion.asyncMove(0, 0, self.print_velocity*4)
        self.motion.waitMotionEnd()

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
        self.array = ""

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
        for elt in line[::-1]:
            sendSize = self.link.tx_obj(elt, start_pos=sendSize, val_type_override='B')
        self.link.send(sendSize, packet_id=CommandId.APPEND.value)
        self.available -= 1

    def sendOneLine(self, line):
        if self.available <= 0:
            # print("0 available !")
            return False

        # print(f"sending line since available is: {self.available}")
        self.forceSendOneLine(line)
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
        # self.link.set_callbacks(self.callback_list())
        if not self.link.open():
            raise Exception("Failed to open port")
        self.available = 0
        self.askBufferAvailable()
        self.waitFor(ResponseId.AVAILABLE_BUFFER.value)
        self.motion = Mock()
        self.print_velocity = 10

if __name__ == '__main__':
    try:
        import sys
        ctl = InkjetController(sys.argv[1])
        ctl.connectMotion(sys.argv[2])
        time.sleep(2)
        index = 0
        # for _ in range(5):
        #     print('prime!')
        #     ctl.prime()
        #     time.sleep(1)
        #ctl.openImage('test600dpi.png')
        ctl.openImage('ytec_logo_icon.png')
        print("image opened")
        ctl.print()

        # ctl.startPrint()
        # while True:
        #     for i in range(255):
        #         time.sleep(0.02)
        #         line = [index]*22
        #         print(f"sending: {index}")
        #         ctl.sendOneLine(line)
        #         ctl.update()
        #         index += 1

    except KeyboardInterrupt:
        line = [0]*38
        ctl.sendOneLine(line)
        ctl.update()
        ctl.stopPrint()
        ctl.close()

    except:
        import traceback
        traceback.print_exc()

        ctl.close()