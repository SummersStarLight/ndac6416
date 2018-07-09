#ifndef FREQ_H
#define FREQ_H

#include "mbed.h"

class FreqChannel;
class LTC2668;
class FreqTimer;

extern FreqTimer *freqtimer0;
extern FreqTimer *freqtimer1;

#define NUMBERFREQS 6
typedef FreqChannel *FREQPtr;
extern FREQPtr *FREQS;

class FreqTimer
{
  private:
    typeof(TIM2) m_timer;
    uint32_t m_enable;
    uint32_t m_prescaler;
    IRQn_Type m_IRQn;
    uint8_t m_tnum;
    uint16_t m_overflow;
    uint8_t m_count_step;
    uint32_t m_count_start;
    uint32_t m_count_stop;
    uint32_t m_width;
    char m_buffer[80];
    FreqChannel *m_channels[4];

  public:
    // Parameterized Constructor
    FreqTimer(typeof(TIM2) ftimer, uint32_t timer_enable, IRQn_Type timer_IRQn, uint32_t irq_func, uint32_t timer_prescaler, uint8_t ftnum);
    void SetChannel(FreqChannel *freqchannel, uint8_t chnum);
    void start_action(bool s1, bool s2, bool s3, bool s4);
    char *print(void);
    uint32_t timercount(void);
    uint16_t overflow(void);
    void irq_ic_timer(void);
};

class FreqChannel
{
  private:
    typeof(GPIOA) m_gpio;
    FreqTimer *m_freq_timer;
    LTC2668 *m_dac;
    uint32_t m_enable;
    uint8_t m_port;
    uint8_t m_chnum;
    uint32_t m_ccrdata;
    volatile uint32_t *m_ccr;
    bool m_count_step, m_count_sampled;
    uint32_t m_count_0, m_count_1, m_width;
    char *m_buffer;
    Semaphore m_freq_sample;
    int32_t m_semaphore_id;

  public:
    // Parameterized Constructor
    FreqChannel(typeof(GPIOA) fgpio, FreqTimer *ftimer, uint32_t fenable, uint8_t fport, uint8_t fchnum, volatile uint32_t *fccr, uint32_t af);
    char *vars(void);
    void irq_freq(void);
    void SetDAC(LTC2668 *dac);
    LTC2668 *GetDAC(void);
    uint32_t GetWidth(void);
    uint32_t GetSampledWidth(void);
    uint32_t Freq16(void);
    uint32_t SampledFreq16(void);
    float Freqf(void);
    float SampledFreqf(void);
};

#endif
