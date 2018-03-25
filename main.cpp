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
//   6. To compile to release "makerls"
//   7. To compile to debug "makedbg"
// Notes:
// 1.  copychanges changes the following files:
//     The mbed systick normally uses TIM5.  It has been changed to TIM7 so we can use TIM5 which is a 32-bit timer. The change is at:
//       .\mbed-os\targets\TARGET_STM\TARGET_STM32F7\TARGET_STM32F767xI\device\hal_tick.h
//     To get correct pin mapping for SPI and GPIO change this:
//       .\mbed-os\targets\TARGET_STM\TARGET_STM32F7\TARGET_STM32F767xI\TARGET_NUCLEO_F767ZI\PeripheralPins.c
#include "mbed.h"
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

// for these to work  copytargetchanges has modified .\mbed-os\targets\TARGET_STM\TARGET_STM32F7\TARGET_STM32F767xI\TARGET_NUCLEO_F767ZI\PeripheralPins.c:

SPI spi1(PD_7,  PG_9,  PG_11); //SPI(MOSI, MISO, SCLK); -> DAC16-31 ok
DigitalOut spi1_nss(PG_10);

SPI spi2(PB_15, PB_14,  PB_13); //SPI(MOSI, MISO, SCLK); -> ADC0-7 ok
DigitalOut spi2_nss(PB_9);

SPI spi3(PC_12, PC_11, PC_10); //SPI(MOSI, MISO, SCLK); -> ADC8-15 ok
DigitalOut spi3_nss(PA_4);

SPI spi4(PE_6,  PE_5,  PE_2);  //SPI(MOSI, MISO, SCLK); -> DAC32-47 ok
DigitalOut spi4_nss(PE_4);

SPI spi5(PF_9,  PF_8,  PF_7);  //SPI(MOSI, MISO, SCLK); -> DAC0-15 ok
DigitalOut spi5_nss(PF_6);

SPI spi6(PG_14, PG_12, PG_13, PG_8); //SPI(MOSI, MISO, SCLK); -> DAC48-63 ok
DigitalOut spi6_nss(PG_8);

voltspan span = { .low=-5, .high=5};
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

LTC2668 d0(0, &spi5, &spi5_nss, NULL, true, 9);
LTC2668 d1(1, &spi5, &spi5_nss, NULL, true, 9);
LTC2668 d2(2, &spi5, &spi5_nss, NULL, true, 9);
LTC2668 d3(3, &spi5, &spi5_nss, NULL, true, 9);
LTC2668 d4(4, &spi5, &spi5_nss, NULL, true, 9);
LTC2668 d5(5, &spi5, &spi5_nss, NULL, true, 9);

// You can only attach static functions to NVIC_SetVector. Cannot use non-static member functions.  https://os.mbed.com/questions/69315/NVIC-Set-Vector-in-class/
uint32_t irqcounter = 0;

static void IRQ1() {
    irqcounter++;
    freqtimer0->irq_ic_timer();
}
FreqTimer t0(TIM2, RCC_APB1ENR_TIM2EN, TIM2_IRQn, (uint32_t)&IRQ1, PRESCALER, 0);
FreqTimer *freqtimer0 = &t0;

// You can only attach static functions to NVIC_SetVector. Cannot use non-static member functions.  https://os.mbed.com/questions/69315/NVIC-Set-Vector-in-class/
static void IRQ2() {
    irqcounter++;
    freqtimer1->irq_ic_timer();
}
FreqTimer t1(TIM5, RCC_APB1ENR_TIM5EN, TIM5_IRQn, (uint32_t)&IRQ2, PRESCALER, 1);
FreqTimer *freqtimer1 = &t1;

FreqChannel ch0(GPIOA, &t0, RCC_AHB1ENR_GPIOAEN, 15, 0, &TIM2->CCR1, 1);
FreqChannel ch1(GPIOB, &t0, RCC_AHB1ENR_GPIOBEN,  3, 1, &TIM2->CCR2, 1);
FreqChannel ch2(GPIOB, &t0, RCC_AHB1ENR_GPIOBEN, 10, 2, &TIM2->CCR3, 1);
FreqChannel ch3(GPIOB, &t0, RCC_AHB1ENR_GPIOBEN, 11, 3, &TIM2->CCR4, 1);

FreqChannel ch4(GPIOA, &t1, RCC_AHB1ENR_GPIOAEN,  0, 4, &TIM5->CCR1, 2);
FreqChannel ch5(GPIOA, &t1, RCC_AHB1ENR_GPIOAEN,  1, 5, &TIM5->CCR2, 2);
//FreqChannel ch6(GPIOA, &t1, RCC_AHB1ENR_GPIOAEN,  2, 6, &TIM5->CCR3, 2);
//FreqChannel ch7(GPIOA, &t1, RCC_AHB1ENR_GPIOAEN,  3, 7, &TIM5->CCR4, 2);

volatile static uint32_t funccounter = 0;

static void FIRQ1() {
    functimer0->irq_ic_timer();
    funccounter++;
}

FuncTimer ft0(TIM3, RCC_APB1ENR_TIM3EN, TIM3_IRQn, (uint32_t)&FIRQ1, FREQUENCY/10000 - 1, 1000, 0);
FuncTimer *functimer0 = &ft0;


void timerdump(typeof(TIM2) timer)
{
    printf("CR1 %08x CR2 %08x ARR %08x PSC %08x CCER %08x CCMR1 %08x CCMR2 %08x SMCR %08x CR1 %08x CCER %08x SR %08x DIER %08x\r\n",
       timer->CR1, timer->CR2, timer->ARR, timer->PSC, timer->CCER, timer->CCMR1, timer->CCMR2, timer->SMCR, timer->CR1, timer->CCER, timer->SR, timer->DIER);
}

void gpiodump(typeof(GPIOA) gpio)
{
    printf("AFR %08x %08x MODER %08x PUPDR %08x\r\n", gpio->AFR[0], gpio->AFR[1], gpio->MODER, gpio->PUPDR);
}

int getchar(const char *msg = NULL) {
    if (msg == NULL) {
        printf("0 timed dump, 1 Tuneup, 2 Minidump, 3 vco adjust, 4 ADC\n\r");
    } else {
        printf("%s\n\r", msg);
    }
    int c = pc.getc();
    printf("Typed %d\n\r", c);
    return c;
}

DigitalOut irqled(LED2);

void IRQBlinker(void) {
    uint32_t irqcounterhold = 0;
    while (1) {
        wait(.3);
        if (irqcounterhold != irqcounter) {
            irqled = !irqled;
            irqcounterhold = irqcounter;
        }
    }
}

Thread IRQBlinkerThread;

void SetupSPIs(void) {
    // DAC16-31
    spi1_nss = 1;
    spi1.format(8,0);
    spi1.frequency(5000000);
    // ADC0-7
    spi2_nss = 1;
    spi2.format(8,0);
    spi2.frequency(5000000);
    // ADC8-15
    spi3_nss = 1;
    spi3.format(8,0);
    spi3.frequency(5000000);
    // DAC32-47
    spi4_nss = 1;
    spi4.format(8,0);
    spi4.frequency(5000000);
    // DAC0-15
    spi5_nss = 1;
    spi5.format(8,0);
    spi5.frequency(5000000);
    // DAC48-63
    spi6_nss = 1;
    spi6.format(8,0);
    spi6.frequency(5000000);
}

void Tuneups0(void) { VCOS[0]->Tuneups(.02, false); }
void Tuneups1(void) { VCOS[1]->Tuneups(.02, false); }
void Tuneups2(void) { VCOS[2]->Tuneups(.02, false); }
void Tuneups3(void) { VCOS[3]->Tuneups(.02, false); }
void Tuneups4(void) { VCOS[4]->Tuneups(.02, false); }
void Tuneups5(void) { VCOS[5]->Tuneups(.02, false); }
Envelope env0 = envtst(&d0, true, true);
Envelope env1 = Envelope(&d1, true, false);
Envelope env2 = Envelope(&d2, true, false);
Envelope env3 = Envelope(&d3, true, false);
Envelope env4 = Envelope(&d4, true, false);

int main() {
    int c = 0, cnt = 0;
    uint32_t fcnt;
    pc.baud(115200);
    fclose(stdout);
    stdout = pc;
    fclose(stderr);
    stderr = pc;

    c = getchar("check it out");
    //print_all_thread_info();
    //print_heap_and_isr_stack_info();
    printf("\r\n\r\n\r\n\r\n\r\nXSTEPMULS ********************* \r\n");
    printf("%d %d %d %d %d %d %d %d\r\n",
        XSTEPMULS[0], XSTEPMULS[1],
        XSTEPMULS[1000], XSTEPMULS[1001],
        XSTEPMULS[4000], XSTEPMULS[4001],
        XSTEPMULS[6000], XSTEPMULS[6001]
    );

    IRQBlinkerThread.start(callback(IRQBlinker));
    dac_nclr = true;  // enable the dacs, this has to come before the DAC spans get set.
    printf("dac_nclr is high\r\n");

    SetupSPIs();
    voltspan span = { .low=-5, .high=5};
    for (int i=0; i<NUMBERDACS; i++) {
        if (DACS[i])  {
            DACS[i]->Setspan(span);
            DACS[i]->Vchk(6528*(i+2));
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
    c=getchar("start t0");
    //            ch0 1 2 3
    t0.start_action(1,1,1,1);
    c=getchar("start t1");
    //            ch4 5 6 7
    t1.start_action(1,1,0,0);
    c=getchar("envelopes ft0");
    ft0.Add(&env1);
    env1.Add(2,0, 8,0, 15, 1, -1, NULL);
    env1.Add(7,7, 2,7, 10, 1, -1, NULL);

    env1.Add(3,0, 7,0, 13, 1, -1, NULL);
    env1.Add(6,7, 3,7, 8 , 1, -1, NULL);

    env1.Add(4,0, 6,0, 10, 1, -1, NULL);
    env1.Add(5,7, 4,7, 6, 1, -1, NULL);

    env1.Add(4,0, 6,0, 10, 1, -1, NULL);
    env1.Add(6,7, 3,7, 8 , 1, -1, NULL);

    env1.Add(3,0, 7,0, 13, 1, -1, NULL);
    env1.Add(7,7, 2,7, 10, 1, -1, NULL);


    ft0.Add(&env2);
    env2.Add(2,7, 8,7, 15, 1, -1, NULL);
    env2.Add(8,0, 3,0, 10, 1, -1, NULL);

    env2.Add(3,7, 7,7, 13, 1, -1, NULL);
    env2.Add(7,0, 4,0, 8 , 1, -1, NULL);

    env2.Add(4,7, 6,7, 10, 1, -1, NULL);
    env2.Add(6,0, 5,0, 6, 1, -1, NULL);

    env2.Add(4,7, 6,7, 10, 1, -1, NULL);
    env2.Add(7,0, 4,0, 8 , 1, -1, NULL);

    env2.Add(3,7, 7,7, 13, 1, -1, NULL);
    env2.Add(8,0, 3,0, 10, 1, -1, NULL);

    ft0.Add(&env3);
    env3.Add(7,7, 2,7, 10, 1, -1, NULL);
    env3.Add(2,0, 8,0, 15, 1, -1, NULL);

    env3.Add(6,7, 3,7, 8 , 1, -1, NULL);
    env3.Add(3,0, 7,0, 13, 1, -1, NULL);

    env3.Add(5,7, 4,7, 6, 1, -1, NULL);
    env3.Add(4,0, 6,0, 10, 1, -1, NULL);

    env3.Add(6,7, 3,7, 8 , 1, -1, NULL);
    env3.Add(4,0, 6,0, 10, 1, -1, NULL);

    env3.Add(7,7, 2,7, 10, 1, -1, NULL);
    env3.Add(3,0, 7,0, 13, 1, -1, NULL);

    ft0.Add(&env4);
    env4.Add(8,0, 3,0, 10, 1, -1, NULL);
    env4.Add(2,7, 8,7, 15, 1, -1, NULL);

    env4.Add(7,0, 4,0, 8 , 1, -1, NULL);
    env4.Add(3,7, 7,7, 13, 1, -1, NULL);

    env4.Add(6,0, 5,0, 6, 1, -1, NULL);
    env4.Add(4,7, 6,7, 10, 1, -1, NULL);

    env4.Add(7,0, 4,0, 8 , 1, -1, NULL);
    env4.Add(4,7, 6,7, 10, 1, -1, NULL);

    env4.Add(8,0, 3,0, 10, 1, -1, NULL);
    env4.Add(3,7, 7,7, 13, 1, -1, NULL);

    c=getchar("ft0.Start");
    //ft0.Start();
    //printf("ft0 %s\n\r", ft0.print());
    //ft0.Stop();
    c=getchar("set VCOs");
    for (int i=0; i<NUMBERDACS; i++) if (DACS[i]) DACS[i]->Voct(i+2, 0);
    c=getchar("VCOs set to octaves");
    while (1) {
       c=getchar(NULL);
       if (c==0x30) {
           ft0.Stop();
           cnt=0;
           while(cnt<5) {
               timer.start();
               wait(0.1);
               d0.Voct(3, 0 + cnt%12);
               d1.Voct(3, 4 + cnt%12);
               d2.Voct(3, 7 + cnt%12);
               d3.Voct(4, 0 + cnt%12);
               d4.Voct(4, 4 + cnt%12);
               d5.Voct(4, 7 + cnt%12);
               cnt++;
               wait(0.5);
               printf("%8.1f: ovl %4d,%12d %12d,%12d\n\r", (float)timer.read_ms()/1000.0, t0.overflow(), t0.timercount(), t1.overflow(), t1.timercount());
               for (int i=0; i<NUMBERVCOS; i++) if (VCOS[i]) VCOS[i]->MiniDump();
               myled = !myled;
           }
       }
       if (c==0x31) {
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
           for (int i=0; i<NUMBERVCOS; i++) {
               if (VCOS[i]) {
                   printf("%d Tuned %d\n\r%s", i, VCOS[i]->Gettuned(), VCOS[i]->Getoffsets());
               }
           }
           //for (int tritone=4; tritone < 19; tritone++) v0.TritoneOffsetsDump(tritone);
           d0.Voct(3, 0);
           d1.Voct(3, 4);
           d2.Voct(3, 7);
           d3.Voct(4, 0);
           d4.Voct(4, 4);
           d5.Voct(4, 7);
       }
       if (c==0x32) {
           ft0.Stop();
           d0.Voct(3, 0);
           d1.Voct(3, 4);
           d2.Voct(3, 7);
           d3.Voct(4, 0);
           d4.Voct(4, 4);
           d5.Voct(4, 7);
           for (int i=0; i<2; i++) {
               printf("\n\r");
               for (int i=0; i<NUMBERVCOS; i++) if (VCOS[i]) VCOS[i]->MiniDump();
           }
       }
       if (c==0x33) {
           ft0.Stop();
           for (int i=0; i<NUMBERDACS; i++) if (DACS[i]) DACS[i]->Wadj(true);
       }
       if (c==0x34) {
           //printf("A0 busy %d\n\r", a0.Busy());
           //a0.Vchk();
           printf("A0 busy %d\n\r", a0.Busy());
           printf("A8 busy %d\n\r", a8.Busy());
           for (int i=0; i<NUMBERADCS; i++) if (ADCS[i]) ADCS[i]->Vchk();
       }
       if (c==0x35) {
           ft0.Stop();
           d0.Voct(2, 0);
           d1.Voct(3, 0);
           d2.Voct(4, 0);
           d3.Voct(5, 0);
           d4.Voct(6, 0);
           d5.Voct(7, 0);
           for (int i=0; i<NUMBERVCOS; i++) if (VCOS[i]) VCOS[i]->MiniDump();
       }
       if (c==0x36) {
           env0.Dump();
           printf("\n\r");
           while(!env0.Next()) wait(.1);
           printf("\n\r");
       }
       if (c==0x37) {
          //rtostimer.stop();
          //rtostimer.start(40);
          //ticks = 0;
          ft0.Start();
          printf("%d %s", funccounter, ft0.print());
          funccounter = 0;
          wait(1);
          fcnt = funccounter;
          printf("%d %s", funccounter, ft0.print());
          printf("funccounter %d\n\r", fcnt);
       }
       if (c==0x38) {
           ft0.Stop();
           env1.Dump();
           printf("\n\r");
           while(!env1.Next()) wait(.05);
           printf("\n\r");
       }
       if (c==0x39) {
           print_all_thread_info();
           print_heap_and_isr_stack_info();
       }
   }
}
