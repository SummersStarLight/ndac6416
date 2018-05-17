// functimer.cpp ... Defines class FuncTimer on STM32F7 timers to cause a function to execute when timer's update event occurs
#include "main.h"
#include <vector>
#include "freq.h"
#include "vco.h"
#include "dac.h"
#include "envelope.h"
#include "functimer.h"

FuncTimer::FuncTimer(typeof(TIM2) ftimer, uint32_t timer_enable,
                     IRQn_Type timer_IRQn, uint32_t irq_func,
                     uint32_t prescaler, uint16_t auto_reload, uint8_t ftnum)
{
    m_timer = ftimer;
    m_enable = timer_enable;
    m_prescaler = prescaler;
    m_IRQn = timer_IRQn;
    m_tnum = ftnum;
    RCC->APB1ENR  |= m_enable;
    // count up + div by 1
    m_timer->CR1 = 0;
    m_timer->CR2 = 0;
    m_timer->CR1     |= TIM_CR1_DIR; // downcount
    m_timer->CR1 |= (uint16_t)TIM_CR1_URS; // only counter overflow generates an interrupt
    m_timer->CCMR1 = 0;
    m_timer->CCMR2 = 0;
    set_reload(auto_reload);
    m_timer->PSC      = m_prescaler; // prescaler = (108Mhz/(desired frequency)) -1)
    m_timer->EGR      = TIM_EGR_UG;  // generate an update event to reload the prescaler value immediately
    m_timer->CCER     = 0;           // Reset all
    m_timer->SMCR    = RESET;
    m_timer->SMCR    &= ~TIM_SMCR_SMS;             // Internal clock
    m_timer->CR1     |= (uint16_t)TIM_CR1_CEN;     // Enable the TIM Counter
    m_timer->SR = 0;           // clear all IC/OC flag
    m_timer->DIER = 0;         // Disable all interrupt
    //m_timer->DIER = TIM_DIER_UIE;  // set only overflow
    m_overflow = 0;
    m_s1 = 0, m_s2 = 0, m_s3 = 0, m_s4 = 0;
    m_envcnt = 0, m_adsrcnt = 0;
    std::vector<Envelope *>(m_envelopes).swap(m_envelopes); // clear and shrink vector
    std::vector<Adsr *>(m_adsrs).swap(m_adsrs); // clear and shrink vector
    NVIC_SetVector(m_IRQn, irq_func);
    NVIC_ClearPendingIRQ(m_IRQn);
    NVIC_EnableIRQ(m_IRQn);
}

void FuncTimer::Add(Envelope *env)
{
    m_envelopes.push_back(env);
}

void FuncTimer::Add(Adsr *adsr)
{
    m_adsrs.push_back(adsr);
}

void FuncTimer::set_reload(uint32_t auto_reload)
{
    m_auto_reload = auto_reload;
    m_timer->ARR = m_auto_reload;
}

void FuncTimer::Start()
{
    __disable_irq();
    m_timer->SR = 0;
    m_timer->DIER = TIM_DIER_UIE;      // set overflow
    __enable_irq();
}

void FuncTimer::Stop()
{
    __disable_irq();
    m_timer->SR = 0;
    m_timer->DIER = 0;
    __enable_irq();
}

char *FuncTimer::print()
{
    sprintf(m_buffer, "Timer %d %12lu %lx %lx %lx %lx %lx %lx %lx %d %d %d %d %d\r\n",
            m_tnum,
            m_timer->CNT,
            (long unsigned int)&m_timer->CNT,
            m_timer->ARR,
            m_timer->PSC,
            m_timer->CR1,
            m_timer->SMCR,
            m_timer->SR,
            m_timer->DIER,
            m_overflow, m_s1, m_s2, m_s3, m_s4);
    return m_buffer;
}

uint32_t FuncTimer::timercount(void)
{
    return m_timer->CNT;
}

uint16_t FuncTimer::overflow(void)
{
    return m_overflow;
}

void FuncTimer::irq_ic_timer(void)
{
    if (m_timer->SR & TIM_SR_UIF){             // counter overflow
        m_timer->SR &= ~TIM_SR_UIF;
        ++m_overflow;
        for (m_envcnt = 0; m_envcnt<m_envelopes.size(); m_envcnt++) {
            if (!m_envelopes[m_envcnt]->m_stopflag) m_envelopes[m_envcnt]->Next();
        }
        for (m_adsrcnt = 0; m_adsrcnt<m_adsrs.size(); m_adsrcnt++) {
            if (!m_adsrs[m_adsrcnt]->m_stopflag) m_adsrs[m_adsrcnt]->Next();
        }
    }
}

