// freq.cpp ... Defines classes FreqTimer and FreqChannel four frequency counting on STM32F7 timers
#include "main.h"
#include "freq.h"
#include "vco.h"
#include "dac.h"

FreqTimer::FreqTimer(typeof(TIM2) ftimer, uint32_t timer_enable, IRQn_Type timer_IRQn, uint32_t irq_func, uint32_t timer_prescaler, uint8_t ftnum)
{
    m_timer = ftimer;
    m_enable = timer_enable;
    m_prescaler = timer_prescaler;
    m_IRQn = timer_IRQn;
    m_tnum = ftnum;
    m_channels[0] = NULL;
    m_channels[1] = NULL;
    m_channels[2] = NULL;
    m_channels[3] = NULL;
    // Initialize Timer
    RCC->APB1ENR  |= m_enable;
    // count up + div by 1
    m_timer->CR1     &= (uint16_t)(~(TIM_CR1_DIR | TIM_CR1_CMS));
    // only counter overflow for interrupt
    m_timer->CR1 |= (uint16_t)TIM_CR1_URS;
    m_timer->CR1 |= TIM_COUNTERMODE_UP;
    m_timer->CR1 &= ~TIM_CR1_CKD;
    /* Set the clock division to 1 */
    m_timer->CR1 |= TIM_CLOCKDIVISION_DIV1;
    // Up counter uses from 0 to max(32bit)
    m_timer->ARR      = 0xffffffff;
    m_timer->PSC      = m_prescaler;    // 108Mhz/(prescaler+1)
    m_timer->CCER     = 0;         // Reset all
    // input filter + input select
    m_timer->CCMR1   &= ~(TIM_CCMR1_IC1F | TIM_CCMR1_CC1S | TIM_CCMR1_IC2F | TIM_CCMR1_CC2S);
    m_timer->CCMR2   &= ~(TIM_CCMR2_IC3F | TIM_CCMR2_CC3S | TIM_CCMR2_IC4F | TIM_CCMR2_CC4S);
    m_timer->CCMR1   |= TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0; // filter none
    m_timer->CCMR2   |= TIM_CCMR2_CC3S_0 | TIM_CCMR2_CC4S_0; // filter none
    m_timer->SMCR    = RESET;
    m_timer->SMCR    &= ~TIM_SMCR_SMS;             // Internal clock
    m_timer->CR1     |= (uint16_t)TIM_CR1_CEN;     // Enable the TIM Counter
    // positive edge
    m_timer->CCER    &= (uint16_t)~(TIM_CCER_CC1P | TIM_CCER_CC1NP | TIM_CCER_CC2P | TIM_CCER_CC2NP | TIM_CCER_CC3P | TIM_CCER_CC3NP | TIM_CCER_CC4P | TIM_CCER_CC4NP);
    m_timer->CCER    |= (uint16_t)TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;   // enable capture
    m_timer->SR = 0;           // clear all IC/OC flag
    m_timer->DIER = 0;         // Disable all interrupt
    //m_timer->DIER = TIM_DIER_UIE;  // set only overflow
    m_overflow = 0;
    NVIC_SetVector(m_IRQn, irq_func);
    NVIC_ClearPendingIRQ(m_IRQn);
    NVIC_EnableIRQ(m_IRQn);
}

void FreqTimer::SetChannel(FreqChannel *freqchannel, uint8_t chnum)
{
    m_channels[chnum] = freqchannel;
}

void FreqTimer::start_action(bool s1, bool s2, bool s3, bool s4)
{
    __disable_irq();
     m_timer->DIER = TIM_DIER_UIE;      // set overflow

     if (s1) {
         m_timer->SR &= ~TIM_SR_CC1IF;      // clear IC flag
         m_timer->DIER |= TIM_DIER_CC1IE;   // Enable IC1
     }
     if (s2) {
         m_timer->SR &= ~TIM_SR_CC2IF;      // clear IC flag
         m_timer->DIER |= TIM_DIER_CC2IE;   // Enable IC2
     }
     if (s3) {
         m_timer->SR &= ~TIM_SR_CC3IF;      // clear IC flag
         m_timer->DIER |= TIM_DIER_CC3IE;   // Enable IC3
     }
     if (s4) {
         m_timer->SR &= ~TIM_SR_CC4IF;      // clear IC flag
         m_timer->DIER |= TIM_DIER_CC4IE;   // Enable IC4
     }
    __enable_irq();
}

char *FreqTimer::print()
{
    sprintf(m_buffer, "Timer %d %12lu %lx %lx %lx %lx %lx %d\r\n",
            m_tnum,
            m_timer->CNT,
            (long unsigned int)&m_timer->CNT,
            m_timer->ARR,
            m_timer->PSC,
            m_timer->CR1,
            m_timer->SMCR,
            m_overflow);
    return m_buffer;
}

uint32_t FreqTimer::timercount(void)
{
    return m_timer->CNT;
}

uint16_t FreqTimer::overflow(void)
{
    return m_overflow;
}

void FreqTimer::irq_ic_timer(void)
{
    if (m_timer->SR & TIM_SR_UIF){             // 32bit counter overflow
        m_timer->SR &= ~TIM_SR_UIF;
        ++m_overflow;
    }
    if (m_timer->SR & TIM_SR_CC1IF) {
        m_timer->SR &= ~TIM_SR_CC1IF;  // clear IC flag
        if (m_channels[0] != NULL) {
            m_channels[0]->irq_freq();
        }
    }
    if (m_timer->SR & TIM_SR_CC2IF) {
        m_timer->SR &= ~TIM_SR_CC2IF;  // clear IC flag
        if (m_channels[1] != NULL) {
            m_channels[1]->irq_freq();
        }
    }
    if (m_timer->SR & TIM_SR_CC3IF) {
        m_timer->SR &= ~TIM_SR_CC3IF;  // clear IC flag
        if (m_channels[2] != NULL) {
            m_channels[2]->irq_freq();
        }
    }
    if (m_timer->SR & TIM_SR_CC4IF) {
        m_timer->SR &= ~TIM_SR_CC4IF;  // clear IC flag
        if (m_channels[3] != NULL) {
            m_channels[3]->irq_freq();
        }
    }
    // timer overflow
}

FREQPtr *FREQS = new FREQPtr[NUMBERFREQS];

FreqChannel::FreqChannel(typeof(GPIOA) fgpio,
                         FreqTimer *ftimer,
                         uint32_t fenable,
                         uint8_t fport,
                         uint8_t fchnum,
                         volatile uint32_t *fccr,
                         uint32_t af)
{
    m_buffer = (char *)malloc(140);
    m_gpio = fgpio;
    m_freq_timer = ftimer;
    m_dac = NULL;
    m_enable = fenable;
    m_port = fport;
    m_chnum = fchnum;
    m_ccr = fccr;
    int d = m_port/8;
    int r = m_port % 8;
    RCC->AHB1ENR |= m_enable;
    m_gpio->AFR[d] &= ~(0xf<< (4*r));
    m_gpio->AFR[d] |=  (af  << (4*r));
    m_gpio->MODER  &= ~(3  << (2*m_port)); // AF
    m_gpio->MODER  |=  (2  << (2*m_port)); // alternate function mode
    m_gpio->PUPDR  &= ~(3  << (2*m_port)); // PU
    m_gpio->PUPDR  |=  (1  << (2*m_port)); //  Pull-up mode
    m_freq_timer->SetChannel(this, m_chnum % 4);
    m_count_0 = 0, m_count_1 = 0, m_count_step = false;
    m_width = 0;
    m_dac = NULL;
    Semaphore m_freq_sample(0);
    int32_t m_semaphore_id;
    FREQS[m_chnum] = this;
}

void FreqChannel::SetDAC(LTC2668 *dac) {
    m_dac = dac;
}

LTC2668 *FreqChannel::GetDAC(void) {
    return(m_dac);
}

uint32_t FreqChannel::GetWidth(void)
{
    return m_width;
}

uint32_t FreqChannel::GetSampledWidth(void)
{
    m_semaphore_id = m_freq_sample.wait(500);
    if (m_semaphore_id == 0 || m_semaphore_id == -1) {
        return(0);
    } else {
        return(m_width);
    }
}

uint32_t FreqChannel::SampledFreq16(void)
{
    if (uint32_t width = GetSampledWidth()) {
        return((FREQUENCY<<4)/width);
    } else {
        return(0);
    }
}

uint32_t FreqChannel::Freq16(void)
{
    if (m_width) {
        return((FREQUENCY<<4)/m_width);
    } else {
        return(0);
    }
}

float FreqChannel::Freqf(void)
{
    if (m_width) {
        return((float)FREQUENCY/(float)m_width);
    } else {
        return(0.0);
    }
}

float FreqChannel::SampledFreqf(void)
{
    if (uint32_t width = GetSampledWidth()) {
        return((float)FREQUENCY/(float)width);
    } else {
        return(0.0);
    }
}

char *FreqChannel::vars()
{
    sprintf(m_buffer, "%d %d %12lu %12lu %8.2f %12lu",
            m_chnum,
            m_count_step,
            m_count_0,
            m_count_1,
            Freqf(),
            m_freq_timer->timercount());
    return m_buffer;
}

void FreqChannel::irq_freq(void)
{
    m_ccrdata = *m_ccr; // m_freq_timer->CCR1;
    if (m_count_step) {             // start measuring
        m_count_1 = m_ccrdata;
        m_count_step = false;
        if (m_count_1 > m_count_0) { // if not > it means the counter overflowed so don't set width ... get it next time.
           m_width = m_count_1 - m_count_0;
           m_freq_sample.release();
           // release the adjust semaphore only if not adjusted
           if (!m_dac->Getoffsetvals().adjusted) m_dac->GetVCO()->Getvcoadjsem()->release();
        }
    } else {      // stop
        //m_freq_timer->DIER &= ~TIM_DIER_CC1IE;  // disable IC1 interrupt
        m_count_0 = m_ccrdata;
        m_count_step = true;
        if (m_count_0 > m_count_1) { // if not > it means the counter overflowed so don't set width ... get it next time.
           m_width = m_count_0 - m_count_1;
           m_freq_sample.release();
           // release the adjust semaphore only if not adjusted
           if (!m_dac->Getoffsetvals().adjusted) m_dac->GetVCO()->Getvcoadjsem()->release();
        }
    }
}


