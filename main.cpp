// main.cpp ... Software for an ST-micro NUCLEO-F767ZI board connected to the redbicycle.org
// NDAC6416 board. The board includes:
//      1.  4 16-channel 16-bit LTC2668 DAC chips connected to SPI channels 5, 1, 4, and 6
//      2.  2 8-channel 16-bit LTC1859 ADC chips connected to SPI channels 2 and 3
//      3.  Signal conectioners for frequency counters on TIM2 channels 1-4 and TIM5 channels 1-4.

// To install:
//   1. Must have mbed-cli and Python 2.7.11 installed, see https://github.com/ARMmbed/mbed-cli.  Get the latest version using "pip install mbed-cli".  See
//      the mbed-cli link for installing GCC_ARM.
//   2. Create a the ndac6416 project:  "git clone https://github.com/mikeredbike/ndac6416 ndac6416"
//   3. Go to the ndac6416 folder and run "git clone -b latest https://github.com/ARMmbed/mbed-os"
//   4. Run "copychanges"
//   5. Add memory-status: "mbed add https://github.com/nuket/mbed-memory-status"
//   6. Add MIDI: "mbed add http://os.mbed.com/users/Kojto/code/USBDevice/"
//   7. To compile to release "makerls"
//   8. To compile to debug "makedbg"
// Notes:
// 1.  copychanges changes the following files:
//     The mbed systick normally uses TIM5.  It has been changed to TIM7 so we can use TIM5 which is a 32-bit timer. The change is at:
//       .\mbed-os\targets\TARGET_STM\TARGET_STM32F7\TARGET_STM32F767xI\device\hal_tick.h
//     To get correct pin mapping for SPI and GPIO change this:
//       .\mbed-os\targets\TARGET_STM\TARGET_STM32F7\TARGET_STM32F767xI\TARGET_NUCLEO_F767ZI\PeripheralPins.c
#include "mbed.h"
#include "USBMIDI.h" // uses pa8 thru pa12
#include "rtos.h"
#include "mbed_memory_status.h"

Serial pc(SERIAL_TX, SERIAL_RX); // this is PD8 and PD9
Timer timer;

DigitalOut dac_nclr(PE_7);
DigitalOut myled(LED1);

#include "main.h"
#include "freq.h"
#include "vco.h"
#include "dac.h"
#include "adc.h"
#include "envelope.h"
#include "functimer.h"
#include "waves.h"

// for these to work  copytargetchanges has modified .\mbed-os\targets\TARGET_STM\TARGET_STM32F7\TARGET_STM32F767xI\TARGET_NUCLEO_F767ZI\PeripheralPins.c:

SPI spi1(PD_7, PG_9, PG_11); //SPI(MOSI, MISO, SCLK); -> DAC16-31 ok
DigitalOut spi1_nss(PG_10);

SPI spi2(PB_15, PB_14, PB_13); //SPI(MOSI, MISO, SCLK); -> ADC0-7 ok
DigitalOut spi2_nss(PB_9);

SPI spi3(PC_12, PC_11, PC_10); //SPI(MOSI, MISO, SCLK); -> ADC8-15 ok
DigitalOut spi3_nss(PA_4);

SPI spi4(PE_6, PE_5, PE_2); //SPI(MOSI, MISO, SCLK); -> DAC32-47 ok
DigitalOut spi4_nss(PE_4);

SPI spi5(PF_9, PF_8, PF_7); //SPI(MOSI, MISO, SCLK); -> DAC0-15 ok
DigitalOut spi5_nss(PF_6);

SPI spi6(PG_14, PG_12, PG_13, PG_8); //SPI(MOSI, MISO, SCLK); -> DAC48-63 ok
DigitalOut spi6_nss(PG_8);

voltspan span = {.low = -5, .high = 5};
DigitalIn adc0_nbusy(PG_0, PullUp);

LTC1859 a0(0, &spi2, &spi2_nss, &adc0_nbusy, span);
LTC1859 a1(1, &spi2, &spi2_nss, &adc0_nbusy, span);
LTC1859 a2(2, &spi2, &spi2_nss, &adc0_nbusy, span);
LTC1859 a3(3, &spi2, &spi2_nss, &adc0_nbusy, span);
LTC1859 a4(4, &spi2, &spi2_nss, &adc0_nbusy, span);
LTC1859 a5(5, &spi2, &spi2_nss, &adc0_nbusy, span);
LTC1859 a6(6, &spi2, &spi2_nss, &adc0_nbusy, span);
LTC1859 a7(7, &spi2, &spi2_nss, &adc0_nbusy, span);

DigitalIn adc1_nbusy(PG_1, PullUp);

LTC1859 a8(8, &spi3, &spi3_nss, &adc1_nbusy, span);
LTC1859 a9(9, &spi3, &spi3_nss, &adc1_nbusy, span);
LTC1859 a10(10, &spi3, &spi3_nss, &adc1_nbusy, span);
LTC1859 a11(11, &spi3, &spi3_nss, &adc1_nbusy, span);
LTC1859 a12(12, &spi3, &spi3_nss, &adc1_nbusy, span);
LTC1859 a13(13, &spi3, &spi3_nss, &adc1_nbusy, span);
LTC1859 a14(14, &spi3, &spi3_nss, &adc1_nbusy, span);
LTC1859 a15(15, &spi3, &spi3_nss, &adc1_nbusy, span);

LTC2668 d0(0, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d1(1, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d2(2, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d3(3, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d4(4, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d5(5, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d6(6, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d7(7, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d8(8, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d9(9, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d10(10, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d11(11, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d12(12, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d13(13, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d14(14, &spi5, &spi5_nss, NULL, false, 9);
LTC2668 d15(15, &spi5, &spi5_nss, NULL, false, 9);

LTC2668 d16(16, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d17(17, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d18(18, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d19(19, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d20(20, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d21(21, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d22(22, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d23(23, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d24(24, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d25(25, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d26(26, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d27(27, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d28(28, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d29(29, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d30(30, &spi4, &spi4_nss, NULL, false, 9);
LTC2668 d31(31, &spi4, &spi4_nss, NULL, false, 9);

// You can only attach static functions to NVIC_SetVector. Cannot use non-static member functions.  https://os.mbed.com/questions/69315/NVIC-Set-Vector-in-class/
uint32_t irqcounter = 0;

static void IRQ1()
{
    irqcounter++;
    freqtimer0->irq_ic_timer();
}
FreqTimer t0(TIM2, RCC_APB1ENR_TIM2EN, TIM2_IRQn, (uint32_t)&IRQ1, PRESCALER, 0);
FreqTimer *freqtimer0 = &t0;

// You can only attach static functions to NVIC_SetVector. Cannot use non-static member functions.  https://os.mbed.com/questions/69315/NVIC-Set-Vector-in-class/
static void IRQ2()
{
    irqcounter++;
    freqtimer1->irq_ic_timer();
}
FreqTimer t1(TIM5, RCC_APB1ENR_TIM5EN, TIM5_IRQn, (uint32_t)&IRQ2, PRESCALER, 1);
FreqTimer *freqtimer1 = &t1;

FreqChannel ch0(GPIOA, &t0, RCC_AHB1ENR_GPIOAEN, 15, 0, &TIM2->CCR1, 1);
FreqChannel ch1(GPIOB, &t0, RCC_AHB1ENR_GPIOBEN, 3, 1, &TIM2->CCR2, 1);
FreqChannel ch2(GPIOB, &t0, RCC_AHB1ENR_GPIOBEN, 10, 2, &TIM2->CCR3, 1);
FreqChannel ch3(GPIOB, &t0, RCC_AHB1ENR_GPIOBEN, 11, 3, &TIM2->CCR4, 1);

FreqChannel ch4(GPIOA, &t1, RCC_AHB1ENR_GPIOAEN, 0, 4, &TIM5->CCR1, 2);
FreqChannel ch5(GPIOA, &t1, RCC_AHB1ENR_GPIOAEN, 1, 5, &TIM5->CCR2, 2);
//FreqChannel ch6(GPIOA, &t1, RCC_AHB1ENR_GPIOAEN,  2, 6, &TIM5->CCR3, 2);
//FreqChannel ch7(GPIOA, &t1, RCC_AHB1ENR_GPIOAEN,  3, 7, &TIM5->CCR4, 2);

volatile static uint32_t funccounter0 = 0;

static void FIRQ0()
{
    functimer0->irq_ic_timer();
    funccounter0++;
}

FuncTimer ft0(TIM3, RCC_APB1ENR_TIM3EN, TIM3_IRQn, (uint32_t)&FIRQ0, FREQUENCY / 10000 - 1, 1000, 0);
FuncTimer *functimer0 = &ft0;

volatile static uint32_t funccounter1 = 0;

static void FIRQ1()
{
    functimer1->irq_ic_timer();
    funccounter1++;
}

FuncTimer ft1(TIM7, RCC_APB1ENR_TIM7EN, TIM7_IRQn, (uint32_t)&FIRQ1, FREQUENCY / 40000 - 1, 1000, 0);
FuncTimer *functimer1 = &ft1;

void timerdump(typeof(TIM2) timer)
{
    printf("CR1 %08x CR2 %08x ARR %08x PSC %08x CCER %08x CCMR1 %08x CCMR2 %08x SMCR %08x CR1 %08x CCER %08x SR %08x DIER %08x\r\n",
           timer->CR1, timer->CR2, timer->ARR, timer->PSC, timer->CCER, timer->CCMR1, timer->CCMR2, timer->SMCR, timer->CR1, timer->CCER, timer->SR, timer->DIER);
}

void gpiodump(typeof(GPIOA) gpio)
{
    printf("AFR %08x %08x MODER %08x PUPDR %08x\r\n", gpio->AFR[0], gpio->AFR[1], gpio->MODER, gpio->PUPDR);
}

int getchar(const char *msg = NULL)
{
    if (msg == NULL)
    {
        printf("0 timed dump, 1 Tuneup, 2 Minidump, 3 vco adjust, 4 ADC\n\r");
    }
    else
    {
        printf("%s\n\r", msg);
    }
    //int c = pc.getc();
    //printf("Typed %d\n\r", c);
    return pc.getc();
}

DigitalOut irqled(LED2);

void IRQBlinker(void)
{
    uint32_t irqcounterhold = 0;
    while (1)
    {
        wait(.3);
        if (irqcounterhold != irqcounter)
        {
            irqled = !irqled;
            irqcounterhold = irqcounter;
        }
    }
}

Thread IRQBlinkerThread;

void SetupSPIs(void)
{
    // DAC16-31
    spi1_nss = 1;
    spi1.format(8, 0);
    spi1.frequency(5000000);
    // ADC0-7
    spi2_nss = 1;
    spi2.format(8, 0);
    spi2.frequency(5000000);
    // ADC8-15
    spi3_nss = 1;
    spi3.format(8, 0);
    spi3.frequency(5000000);
    // DAC32-47
    spi4_nss = 1;
    spi4.format(8, 0);
    spi4.frequency(5000000);
    // DAC0-15
    spi5_nss = 1;
    spi5.format(8, 0);
    spi5.frequency(5000000);
    // DAC48-63
    spi6_nss = 1;
    spi6.format(8, 0);
    spi6.frequency(5000000);
}

void Tuneups0(void) { VCOS[0]->Tuneups(.02, false); }
void Tuneups1(void) { VCOS[1]->Tuneups(.02, false); }
void Tuneups2(void) { VCOS[2]->Tuneups(.02, false); }
void Tuneups3(void) { VCOS[3]->Tuneups(.02, false); }
void Tuneups4(void) { VCOS[4]->Tuneups(.02, false); }
void Tuneups5(void) { VCOS[5]->Tuneups(.02, false); }
Envelope env0 = Envelope(0, &d0, true, false);
Envelope env1 = Envelope(1, &d1, true, false);
Envelope env2 = Envelope(2, &d2, true, false);
Envelope env3 = Envelope(3, &d3, true, false);
Envelope env4 = Envelope(4, &d4, true, false);
Envelope env5 = Envelope(5, &d5, true, false);
void Env0SegChange(void) { env0.IncSegChange(); }
void Env1SegChange(void) { env1.IncSegChange(); }
void Env2SegChange(void) { env2.IncSegChange(); }
void Env3SegChange(void) { env3.IncSegChange(); }
void Env4SegChange(void) { env4.IncSegChange(); }
void Env5SegChange(void) { env5.IncSegChange(); }

// VCAs instead of using the Adsr class use the wave tables in the DAC class
//Adsr adsr0 = Adsr(0, &d6, false, false);
//d6.Setwave(&Waves::vca[0], 0);
//Adsr adsr1 = Adsr(1, &d10, false, false);
//d10.Setwave(&Waves::vca[0], 0);
// VCFs
//Adsr adsr2 = Adsr(2, &d7, false, false);
//d7.Setwave(&Waves::vcf[0], 0);
//Adsr adsr3 = Adsr(3, &d11, false, false);
//d11.Setwave(&Waves::vcf[0], 0);

void show_message(MIDIMessage msg)
{
    //int t = (int)msg.type();
    switch (msg.type())
    {
    case MIDIMessage::NoteOnType:
        //printf("NoteOn key:%d, velocity: %d, channel: %d\n\r", msg.key(), msg.velocity(), msg.channel());
        if (msg.channel() < NUMBERVCOS)
        {
            VCOS[msg.channel()]->Getdac()->Vmidi(msg.key());
            if (msg.channel() == 0)
            {
                d6.Start();
                //ADSRS[0]->Restart();
                d7.Start();
                //ADSRS[2]->Restart();
            }
            else if (msg.channel() == 1)
            {
                d10.Start();
                //ADSRS[1]->Restart();
                d11.Start();
                //ADSRS[3]->Restart();
            }
        }
        break;
    case MIDIMessage::NoteOffType:
        //printf("NoteOff key:%d, velocity: %d, channel: %d\n\r", msg.key(), msg.velocity(), msg.channel());
        if (msg.channel() == 0)
        {
            d6.Release();
            //ADSRS[0]->Release();
            d7.Release();
            //ADSRS[2]->Release();
        }
        if (msg.channel() == 1)
        {
            d10.Release();
            //ADSRS[1]->Release();
            d11.Release();
            //ADSRS[3]->Release();
        }
        break;
    case MIDIMessage::ControlChangeType:
        //printf("ControlChange controller: %d, data: %d\n\r", msg.controller(), msg.value());
        break;
    case MIDIMessage::PitchWheelType:
        //printf("PitchWheel channel: %d, pitch: %d\n\r", msg.channel(), msg.pitch());
        break;
    default:
        //printf("Another message %d\n\r", (int)msg.type());
        break;
    }
}

void Triads(int8_t octave)
{
    DACS[0]->Voct(octave, 0);
    DACS[1]->Voct(octave, 4);
    DACS[2]->Voct(octave, 7);
    DACS[3]->Voct(octave + 1, 0);
    DACS[4]->Voct(octave + 1, 4);
    DACS[5]->Voct(octave + 1, 7);
}

void Octaves(void)
{
    DACS[0]->Voct(2, 0);
    DACS[1]->Voct(3, 0);
    DACS[2]->Voct(4, 0);
    DACS[3]->Voct(5, 0);
    DACS[4]->Voct(6, 0);
    DACS[5]->Voct(7, 0);
}

void VCFSet(void)
{
    DACS[7]->Vout(0x4000);  // Channel 1 VCF LP
    DACS[8]->Vout(0xf000);  // Channel 1 VCF Q
    DACS[11]->Vout(0x4000); // Channel 2 VCF LP
    DACS[12]->Vout(0xf000); // Channel 2 VCF Q
}

void ft0Set(void)
{
    ft0.Clear();
    ft0.Add(&env0);
    env0.Clear();
    env0.Add(4, 0, 6, 0, 10, 1, -1, NULL);
    env0.Add(5, 7, 4, 7, 6, 1, -1, NULL);

    env0.Add(3, 0, 7, 0, 13, 1, -1, NULL);
    env0.Add(6, 7, 3, 7, 8, 1, -1, NULL);

    env0.Add(2, 0, 8, 0, 15, 1, -1, NULL);
    env0.Add(7, 7, 2, 7, 10, 1, -1, NULL);

    env0.Add(3, 0, 7, 0, 13, 1, -1, NULL);
    env0.Add(7, 7, 2, 7, 10, 1, -1, NULL);

    env0.Add(4, 0, 6, 0, 10, 1, -1, NULL);
    env0.Add(6, 7, 3, 7, 8, 1, -1, NULL);

    ft0.Add(&env1);
    env1.Clear();
    env1.Add(2, 0, 8, 0, 15, 1, -1, NULL);
    env1.Add(7, 7, 2, 7, 10, 1, -1, NULL);

    env1.Add(3, 0, 7, 0, 13, 1, -1, NULL);
    env1.Add(6, 7, 3, 7, 8, 1, -1, NULL);

    env1.Add(4, 0, 6, 0, 10, 1, -1, NULL);
    env1.Add(5, 7, 4, 7, 6, 1, -1, NULL);

    env1.Add(4, 0, 6, 0, 10, 1, -1, NULL);
    env1.Add(6, 7, 3, 7, 8, 1, -1, NULL);

    env1.Add(3, 0, 7, 0, 13, 1, -1, NULL);
    env1.Add(7, 7, 2, 7, 10, 1, -1, NULL);

    ft0.Add(&env2);
    env2.Clear();
    env2.Add(2, 7, 8, 7, 15, 1, -1, NULL);
    env2.Add(8, 0, 3, 0, 10, 1, -1, NULL);

    env2.Add(3, 7, 7, 7, 13, 1, -1, NULL);
    env2.Add(7, 0, 4, 0, 8, 1, -1, NULL);

    env2.Add(4, 7, 6, 7, 10, 1, -1, NULL);
    env2.Add(6, 0, 5, 0, 6, 1, -1, NULL);

    env2.Add(4, 7, 6, 7, 10, 1, -1, NULL);
    env2.Add(7, 0, 4, 0, 8, 1, -1, NULL);

    env2.Add(3, 7, 7, 7, 13, 1, -1, NULL);
    env2.Add(8, 0, 3, 0, 10, 1, -1, NULL);

    ft0.Add(&env3);
    env3.Clear();
    env3.Add(7, 7, 2, 7, 10, 1, -1, NULL);
    env3.Add(2, 0, 8, 0, 15, 1, -1, NULL);

    env3.Add(6, 7, 3, 7, 8, 1, -1, NULL);
    env3.Add(3, 0, 7, 0, 13, 1, -1, NULL);

    env3.Add(5, 7, 4, 7, 6, 1, -1, NULL);
    env3.Add(4, 0, 6, 0, 10, 1, -1, NULL);

    env3.Add(6, 7, 3, 7, 8, 1, -1, NULL);
    env3.Add(4, 0, 6, 0, 10, 1, -1, NULL);

    env3.Add(7, 7, 2, 7, 10, 1, -1, NULL);
    env3.Add(3, 0, 7, 0, 13, 1, -1, NULL);

    ft0.Add(&env4);
    env4.Clear();
    env4.Add(8, 0, 3, 0, 10, 1, -1, NULL);
    env4.Add(2, 7, 8, 7, 15, 1, -1, NULL);

    env4.Add(7, 0, 4, 0, 8, 1, -1, NULL);
    env4.Add(3, 7, 7, 7, 13, 1, -1, NULL);

    env4.Add(6, 0, 5, 0, 6, 1, -1, NULL);
    env4.Add(4, 7, 6, 7, 10, 1, -1, NULL);

    env4.Add(7, 0, 4, 0, 8, 1, -1, NULL);
    env4.Add(4, 7, 6, 7, 10, 1, -1, NULL);

    env4.Add(8, 0, 3, 0, 10, 1, -1, NULL);
    env4.Add(3, 7, 7, 7, 13, 1, -1, NULL);

    ft0.Add(&env5);
    env5.Clear();
    env5.Add(6, 0, 5, 0, 6, 1, -1, NULL);
    env5.Add(4, 7, 6, 7, 10, 1, -1, NULL);

    env5.Add(7, 0, 4, 0, 8, 1, -1, NULL);
    env5.Add(3, 7, 7, 7, 13, 1, -1, NULL);

    env5.Add(8, 0, 3, 0, 10, 1, -1, NULL);
    env5.Add(2, 7, 8, 7, 15, 1, -1, NULL);

    env5.Add(8, 0, 3, 0, 10, 1, -1, NULL);
    env5.Add(3, 7, 7, 7, 13, 1, -1, NULL);

    env5.Add(7, 0, 4, 0, 8, 1, -1, NULL);
    env5.Add(4, 7, 6, 7, 10, 1, -1, NULL);
}

int main()
{
    int c = 0, cnt = 0, ai;
    uint32_t fcnt;
    pc.baud(115200);
    fclose(stdout);
    stdout = pc;
    fclose(stderr);
    stderr = pc;
    for (int i = 0; i < 20; i++)
        printf("%x ", Waves::vca[i]);
    printf("\n\r");
    c = getchar("check it out");
    //print_all_thread_info();
    //print_heap_and_isr_stack_info();
    printf("\r\n\r\n\r\n\r\n\r\nXSTEPMULS ********************* \r\n");
    printf("%d %d %d %d %d %d %d %d\r\n",
           XSTEPMULS[0], XSTEPMULS[1],
           XSTEPMULS[1000], XSTEPMULS[1001],
           XSTEPMULS[4000], XSTEPMULS[4001],
           XSTEPMULS[6000], XSTEPMULS[6001]);
    printf("init midi");
    USBMIDI midi;
    printf("midi defined\n\r");
    midi.attach(show_message); // call back for messages received

    IRQBlinkerThread.start(callback(IRQBlinker));
    dac_nclr = true; // enable the dacs, this has to come before the DAC spans get set.
    printf("dac_nclr is high\r\n");

    SetupSPIs();
    voltspan span = {.low = -5, .high = 5};
    for (int i = 0; i < NUMBERDACS; i++)
    {
        if (DACS[i])
        {
            DACS[i]->Setspan(span);
            DACS[i]->Vchk(6528 * (i % 8));
        }
    }

    VCO v0(&ch0, &d0, 0, 13.75, -5.0, 10, 6528, 1, &VCOAdj0);
    VCO v1(&ch1, &d1, 1, 13.75, -5.0, 10, 6528, 1, &VCOAdj0);
    VCO v2(&ch2, &d2, 2, 13.75, -5.0, 10, 6528, 1, &VCOAdj0);
    VCO v3(&ch3, &d3, 3, 13.75, -5.0, 10, 6528, 1, &VCOAdj0);
    VCO v4(&ch4, &d4, 4, 13.75, -5.0, 10, 6528, 1, &VCOAdj0);
    VCO v5(&ch5, &d5, 5, 13.75, -5.0, 10, 6528, 1, &VCOAdj0);
    //for (int i=0; i<NUMBERVCOS; i++) if (VCOS[i]) VCOS[i]->StartAdj();

    printf("%s", t0.print());
    printf("%s", t1.print());
    gpiodump(GPIOA);
    gpiodump(GPIOB);
    gpiodump(GPIOC);
    gpiodump(GPIOD);
    gpiodump(GPIOE);
    gpiodump(GPIOF);
    gpiodump(GPIOG);
    timerdump(TIM2);
    timerdump(TIM5);

    c = getchar("start t0");
    //            ch0 1 2 3
    t0.start_action(1, 1, 1, 1);
    d0.Setwave(&Waves::vca[0], 0);
    d0.Dumpwave();
    d0.Nexts();
    d0.Setwave(&Waves::vca[0], 1);
    d0.Dumpwave();
    d0.Nexts();
    d0.Setwave(&Waves::vca[0], 2);
    d0.Dumpwave();
    d0.Nexts();

    c = getchar("start t1");
    //            ch4 5 6 7
    t1.start_action(1, 1, 0, 0);

    c = getchar("envelopes ft0");
    ft0Set();
    //VCA ADSRs
    ft1.Clear();
    ft1.Add(&d6); // this is a VCA ADSR
    d6.Setwave(&Waves::vca[0], 0);
    ft1.Add(&d10); // this is a VCA ADSR
    d10.Setwave(&Waves::vca[0], 0);
    //VCF ADSRs
    ft1.Add(&d7); // this is a VCA ADSR
    d7.Setwave(&Waves::vcf[0], 0);
    ft1.Add(&d11); // this is a VCA ADSR
    d11.Setwave(&Waves::vcf[0], 0);
    printf("\n\rENVS\n\r");
    for (int i = 0; i < NUMBERENVS; i++)
        if (ENVS[i])
            ENVS[i]->Info();
    //printf("\n\rADSRS\n\r");
    //for (int i=0; i<NUMBERADSRS; i++) if (ADSRS[i]) ADSRS[i]->Info();
    //c=getchar("ft0.Start");
    //ft0.Start();
    //printf("ft0 %s\n\r", ft0.print());
    //ft0.Stop();
    c = getchar("set DACs");
    for (int i = 0; i < NUMBERDACS; i++)
    {
        if (DACS[i])
        {
            DACS[i]->Vout((i % 8) * 6528, false);
            printf("(%d %d %d)", (i % 8) * 6528, DACS[i]->m_dacnum, DACS[i]->Getvoutdin());
        }
    }
    c = getchar("\n\rAscending DACs");
    while (1)
    {
        c = getchar(NULL);
        if (c == 0x30)
        {
            ft0.Stop();
            cnt = 0;
            while (cnt < 5)
            {
                timer.start();
                wait(0.1);
                d0.Voct(3, 0 + cnt % 12);
                d1.Voct(3, 4 + cnt % 12);
                d2.Voct(3, 7 + cnt % 12);
                d3.Voct(4, 0 + cnt % 12);
                d4.Voct(4, 4 + cnt % 12);
                d5.Voct(4, 7 + cnt % 12);
                cnt++;
                wait(0.5);
                printf("%8.1f: ovl %4d,%12d %12d,%12d\n\r", (float)timer.read_ms() / 1000.0, t0.overflow(), t0.timercount(), t1.overflow(), t1.timercount());
                for (int i = 0; i < NUMBERVCOS; i++)
                    if (VCOS[i])
                        VCOS[i]->MiniDump();
                myled = !myled;
            }
        }
        if (c == 0x31)
        {
            ft0.Stop();
            //for (int i=0; i<NUMBERVCOS; i++) if (VCOS[i]) VCOS[i]->Tuneups(.02, false);
            Thread thread0;
            thread0.start(callback(Tuneups0));
            Thread thread1;
            thread1.start(callback(Tuneups1));
            Thread thread2;
            thread2.start(callback(Tuneups2));
            Thread thread3;
            thread3.start(callback(Tuneups3));
            Thread thread4;
            thread4.start(callback(Tuneups4));
            Thread thread5;
            thread5.start(callback(Tuneups5));
            //print_all_thread_info();
            printf("thread0 %d\n\r", thread0.join());
            printf("thread1 %d\n\r", thread1.join());
            printf("thread2 %d\n\r", thread2.join());
            printf("thread3 %d\n\r", thread3.join());
            printf("thread4 %d\n\r", thread4.join());
            printf("thread5 %d\n\r", thread5.join());
            for (int i = 0; i < NUMBERVCOS; i++)
            {
                if (VCOS[i])
                {
                    printf("%d Tuned %d\n\r%s", i, VCOS[i]->Gettuned(), VCOS[i]->Getoffsets());
                }
            }
            //for (int tritone=4; tritone < 19; tritone++) v0.TritoneOffsetsDump(tritone);
            Triads(3);
        }
        if (c == 0x32)
        {
            ft0.Stop();
            Triads(3);
            for (int i = 0; i < 2; i++)
            {
                printf("\n\r");
                for (int i = 0; i < NUMBERVCOS; i++)
                    if (VCOS[i])
                        VCOS[i]->MiniDump();
            }
        }
        if (c == 0x33)
        {
            ft0.Stop();
            for (int i = 0; i < NUMBERVCOS; i++)
                if (VCOS[i])
                    VCOS[i]->Getdac()->Wadj(true);
        }
        if (c == 0x34)
        {
            //printf("A0 busy %d\n\r", a0.Busy());
            //a0.Vchk();
            printf("A0 busy %d\n\r", a0.Busy());
            printf("A8 busy %d\n\r", a8.Busy());
            for (int i = 0; i < NUMBERADCS; i++)
                if (ADCS[i])
                    ADCS[i]->Vchk();
        }
        if (c == 0x35)
        {
            ft0.Stop();
            Octaves();
            for (int i = 0; i < NUMBERVCOS; i++)
                if (VCOS[i])
                    VCOS[i]->MiniDump();
        }
        if (c == 0x36)
        {
            env0.Dump();
            printf("\n\r");
            while (!env0.Next())
                wait(.1);
            printf("\n\r");
        }
        if (c == 0x37)
        {
            //rtostimer.stop();
            //rtostimer.start(40);
            //ticks = 0;
            ft0Set();
            ft0.Start();
            while (1)
            {
                c = getchar("Quit or n(-1) or p(+1) or r(repl)");
                if (c == 'q')
                    break;
                switch (c)
                {
                case 'n':
                    for (ai = 0; ai < 6; ai++)
                        ENVS[ai]->IncSegHold(-1);
                    break;
                case 'p':
                    for (ai = 0; ai < 6; ai++)
                        ENVS[ai]->IncSegHold(1);
                    break;
                case 'r':
                    //env0.Replace(4,0, 6,0, 10, 1, 0, NULL);
                    env0.Replace(4, 0, 6, 0, 5, 1, 0, NULL);
                    //env1.Replace(2,0, 8,0, 15, 1, 0, NULL);
                    env1.Replace(2, 0, 8, 0, 10, 1, 0, NULL);
                    break;
                case 's':
                    env0.Replace(4, 0, 6, 0, 10, 1, 0, NULL);
                    //env0.Replace(4,0, 6,0, 5, 1, 0, NULL);
                    env1.Replace(2, 0, 8, 0, 15, 1, 0, NULL);
                    //env1.Replace(2,0, 8,0, 10, 1, 0, NULL);
                    break;
                case 'i':
                    for (ai = 0; ai < 6; ai++)
                        ENVS[ai]->IncSegChange(8388608);
                    break;
                case 'd':
                    for (ai = 0; ai < 6; ai++)
                        ENVS[ai]->IncSegChange(-8388608);
                    break;
                case 'l':
                    ft0.IncReload(ai * 10);
                    break;
                case 'k':
                    ft0.IncReload(-ai * 10);
                    break;
                case 'x':
                    env0.SetSegFunc(Env0SegChange);
                    env1.SetSegFunc(Env1SegChange);
                    env2.SetSegFunc(Env2SegChange);
                    env3.SetSegFunc(Env3SegChange);
                    env4.SetSegFunc(Env4SegChange);
                    env5.SetSegFunc(Env5SegChange);
                    break;
                case 'y':
                    env0.SetSegFunc(NULL);
                    env1.SetSegFunc(NULL);
                    env2.SetSegFunc(NULL);
                    env3.SetSegFunc(NULL);
                    env4.SetSegFunc(NULL);
                    env5.SetSegFunc(NULL);
                    ft0Set();
                    break;
                default:
                    for (ai = 0; ai < 6; ai++)
                        ENVS[ai]->Dump();
                    break;
                }
            }
            printf("%d %s", funccounter0, ft0.print());
            funccounter0 = 0;
            wait(1);
            fcnt = funccounter0;
            printf("%d %s", funccounter0, ft0.print());
            printf("funccounter0 %d\n\r", fcnt);
        }
        if (c == 0x38)
        {
            ft0.Stop();
            env1.Dump();
            printf("\n\r");
            while (!env1.Next())
                wait(.05);
            printf("\n\r");
            print_all_thread_info();
            print_heap_and_isr_stack_info();
        }
        if (c == 0x39)
        {
            ft0.Stop();
            for (ai = 0; ai < 6; ai++)
                DACS[ai]->Voct(4, 7); // All VCO set the same
            VCFSet();
            while (1)
            {
                c = getchar("Choose DAC");
                if (c == 'q')
                    break;
                char buffer[50];
                char *hexString = (char *)"0";
                hexString[0] = c;
                int dac;
                sscanf(hexString, "%x", &dac);
                printf("DAC %d\n\r", dac);
                int32_t vout = 0, inc = 0x1000;
                while (1)
                {
                    DACS[dac]->Vout(vout);
                    sprintf(buffer, "%d. 0x%x ... Next, Previous, Quit", dac, vout & 0xffff);
                    c = getchar(buffer);
                    if (c == 'q')
                        break;
                    if (c == 'n')
                        inc = 0x1000;
                    else
                        inc = -0x1000;
                    vout += inc;
                }
            }
        }
        if (c == 'a')
        {
            ft0.Stop();
            Triads(4);
            VCFSet();
            ft1.Start();
            while (1)
            {
                c = getchar("Restart or Quit");
                if (c == 'q')
                    break;
                ai = (c < 0x30 || c > 0x37) ? 0x30 : c - 0x30;
                d6.Setwave(&Waves::vca[0], ai);
                d7.Setwave(&Waves::vcf[0], ai);
                d10.Setwave(&Waves::vca[0], ai);
                d11.Setwave(&Waves::vcf[0], ai);
                d6.Start();
                d7.Start();
                //while(!adsr0.Next()) wait(.01);
                //printf("\n\rm_segcnt %d\n\r", adsr0.m_segcnt);
                c = getchar("Release, Quit or Dump");
                if (c == 'q')
                    break;
                d6.Release();
                d7.Release();
                if (c == 'd')
                {
                    print_all_thread_info();
                    print_heap_and_isr_stack_info();
                }
                //while(!adsr0.Next()) wait(.01);
            }
        }
        ft1.Stop();
    }
}
