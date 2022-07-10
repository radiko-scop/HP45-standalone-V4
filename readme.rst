HP45-standalone-V4 (OpenPrinter version)
==========================================

About
------

Controls a HP 45 cardridge.

It is separated in two parts:

- A teensy3.5 embeded firmware, compiled using platformIO (see below).
- A python client to control the cardridge, found in the python_client folder.

Compile Firmware
---------------------

Open platformio.ini using [PlatformIO ide of your choice](https://platformio.org/install/ide)

It should compile and upload as in [tutorials](https://docs.platformio.org/en/latest/tutorials/index.html)

Print something
-----------------

- Get the motion firmware [here](https://github.com/benjaminforest/OpenPrinterMotion).
- Ensure motion firmware is flashed on motion board
- From your python client folder : `ln -s <path to your motion firmware folder>/OpenPrinterMotion/python_client/SerialMotionCtl.py`, or copy SerialMotionCtl.py into your python_client folder.
- (optional, recommended) `python -m venv .venv`
- (optional, recommended) `source .venv/bin/activate` sous linux ou `.venv/Scripts/Activate` sous windows
- `pip install -r requirements.txt`
- `python client.py /dev/ttyACM0 /dev/ttyUSB0 100` with :
    - `/dev/ttyACM0` the actual serial port of the teensy (cardridge)
    - `/dev/ttyUSB0` the actual serial port of the motion
    - `100` the target motion speed in mm/s