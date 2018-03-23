// The NDAC6416 has two LTC1859 ADC chips providing 16-channels of 16-bit ADC.
#include "freq.h"
#include "vco.h"
#include "dac.h"
#include "adc.h"

ADCPtr *ADCS = new ADCPtr[NUMBERADCS]();

LTC1859::LTC1859(int8_t adcnum,
                 typeof(SPI) *spi,
                 typeof(DigitalOut) *spinss,
                 typeof(DigitalIn) *busy,
                 voltspan span)
{
    m_adcnum = adcnum & 0xf; // only 16 ADCs (2x8) per board
    m_spi = spi;
    m_spinss = spinss;
    m_busy = busy;
    m_vin = 0;
    Setspan(span);
    ADCS[m_adcnum] = this;
}

void LTC1859::Setspan(voltspan span) {
    m_span = span;
    bool uni=true, gain=false;
    if (m_span.low == 0 && m_span.high == 5) {
       uni=true, gain=false;
    } else if(m_span.low == 0 && m_span.high == 10) {
       uni=true, gain=true;
    } else if(m_span.low == -5 && m_span.high == 5) {
       uni=false, gain=false;
    } else if(m_span.low == -10 && m_span.high == 10) {
       uni=false, gain=true;
    }
    m_dataword  = 0x80;                   // sgl/diff, always run in single mode
    m_dataword |= (m_adcnum & 1)?0x40:0;  // odd sign (a0)
    m_dataword |= (m_adcnum & 4)?0x20:0;  // select 1 (a2)
    m_dataword |= (m_adcnum & 2)?0x10:0;  // select 0 (a1)
    m_dataword |= uni?0x08:0;             // uni
    m_dataword |= gain?0x04:0;            // gain
    m_send[0] = m_dataword;
    m_send[1] = 0;
}

uint16_t LTC1859::Vin1(void) {
    m_spinss->write(0);
    m_spi->write(m_send, 2, m_recv, 2);
    m_spinss->write(1);
    m_vin = ((uint8_t)m_recv[0]<<8) + (uint8_t)m_recv[1];
    // sign extend
    if(m_span.low != 0)  m_vin = (m_vin & 0x7fff) - (m_vin & 0x8000);
    return(m_vin);
}

void LTC1859::Vchk(void) {
    int8_t b1, b2, b3, b4, cnt1=100, cnt2=100;
    b3 = m_busy->read();
    while(!m_busy->read() && cnt2--);
    b4 = m_busy->read();
    m_spinss->write(0);
    m_spi->write(m_send, 2, m_recv, 2);
    m_spinss->write(1);
    b1 = m_busy->read();
    while(!m_busy->read() && cnt1--);
    b2 = m_busy->read();
    m_spinss->write(0);
    m_spi->write(m_send, 2, m_recv, 2);
    m_spinss->write(1);
    //b3 = m_busy->read();
    //while(!m_busy->read() && cnt2--);
    //b4 = m_busy->read();
    printf("ADC %2d %02x %02x, %02x %02x (%d %d %d) (%d %d %d)\n\r",
           m_adcnum,
           (uint8_t)m_send[0],
           (uint8_t)m_send[1],
           (uint8_t)m_recv[0],
           (uint8_t)m_recv[1],
           b1, b2, cnt1, b3, b4, cnt2);
}

uint16_t LTC1859::Vin(void) {
    Vin1();
    while(!m_busy->read());
    m_vin = Vin1();
    return(m_vin);
}

bool LTC1859::Busy(void) {
    return(!m_busy->read());

}


