#ifndef FUNCTIMER_H
#define FUNCTIMER_H

#include "mbed.h"
#include <vector>
#include "envelope.h"

class FuncTimer;

extern FuncTimer *functimer0;
extern FuncTimer *functimer1;

class FuncTimer
{
  private:
    typeof(TIM2) m_timer;
    uint32_t m_enable;
    uint32_t m_prescaler;
    int32_t m_auto_reload;
    IRQn_Type m_IRQn;
    uint8_t m_tnum;
    uint16_t m_overflow, m_s1, m_s2, m_s3, m_s4, m_envcnt, m_adsrcnt, m_daccnt;
    char m_buffer[80];
    std::vector<Envelope *> m_envelopes; // list of Envelopes
    std::vector<Adsr *> m_adsrs;         // list of Adsrs
    std::vector<LTC2668 *> m_dacs;       // list of DACs
  public:
    // Parameterized Constructor
    FuncTimer(typeof(TIM2) ftimer, uint32_t timer_enable,
              IRQn_Type timer_IRQn, uint32_t irq_func,
              uint32_t timer_prescaler, uint16_t auto_reload, uint8_t ftnum);
    void Clear(void);
    void Add(Envelope *env);
    void Add(Adsr *adsr);
    void Add(LTC2668 *dac);
    void SetReload(int32_t auto_reload);
    void IncReload(int32_t inc);
    void Start(void);
    void Stop(void);
    char *print(void);
    uint32_t timercount(void);
    uint16_t overflow(void);
    void irq_ic_timer(void);
};

#endif
