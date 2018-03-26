C++ code for the ST-micro NUCLEO-F767ZI board connected to the redbicycle.org
NDAC6416 daughter board. The board includes:

    1.  4 16-channel 16-bit LTC2668 DAC chips connected to SPI channels 5, 1, 4, and 6
    2.  2 8-channel 16-bit LTC1859 ADC chips connected to SPI channels 2 and 3
    3.  Signal conditioners for frequency counters on TIM2 channels 1-4 and TIM5 channels 1-4.

The software uses the 'mbed-os' platform and drivers.  Software includes:

    1.  LTC1859 class to drive two Linear Technology 8-channel 16-bit ADC chips on SPI channels
    2 and 3.  Eight objects are created one for each ADC.
    2.  LTC2668 class to drive four Linear Technology 16-channel 16-bit DAC chips on SPI channels
    5, 1, 4, and 6.  64 objects are created one for each DAC.
    3.  FreqTimer class to drive the STM32F767 32-bit timers 2 and 5.
    4.  FreqChannel class to drive the timer capture channels on timers 2 and 5.
    5.  VCO class uses the FreqChannel and LTC2668 objects to tune the VCOs connected to the LTC2668
    16-bit DACs.  The class also translates octave/semitones to voltage.
    6.  The Segment class is used by the Envelope class to generate voltage control waveforms.  The
    ADSR class is a subclass of the Envelope class and generates the classic Attack, Decay, Sustain,
    and Release waveform.
    7.  the FuncTimer class uses the STM32F767 16-bit timers to call Envelope functions in real-time.

Installation Procedure

It is required to install mbed-cli on your development machine.  Drivers, RTOS and platform
dependencies are in the 'mbed-os' folder.  Here are the steps:

    1. Must have mbed-cli and Python 2.7.11 installed, see https://github.com/ARMmbed/mbed-cli.  Get the latest version using "pip install mbed-cli".  See the mbed-cli link for installing GCC_ARM.
    2. Create a the ndac6416 project:  "git clone https://github.com/mikeredbike/ndac6416 ndac6416".
    3. Go to the ndac6416 folder and run "git clone -b latest https://github.com/ARMmbed/mbed-os".
    4. Run "copychanges".
    5. Add memory-status: "mbed add https://github.com/nuket/mbed-memory-status"
    6. To compile to release "makerls".
    7. To compile to debug "makedbg".

Notes:

    A.  "copychanges" modifies the following files:

        1.  File .\mbed-os\targets\TARGET_STM\TARGET_STM32F7\TARGET_STM32F767xI\device\hal_tick.h
        normally uses uses TIM5.  It has been changed to TIM7 so we can use TIM5 which is a 32-bit timer.      
        2. File.\mbed-os\.....\TARGET_STM32F767xI\TARGET_NUCLEO_F767ZI\PeripheralPins.c must be
        modified to get correct SPI and GPIO pin-mapping.
