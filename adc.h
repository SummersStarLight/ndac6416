#ifndef ADC_H
#define ADC_H

#include "mbed.h"

class LTC1859;
#define NUMBERADCS 16
typedef LTC1859* ADCPtr;
extern ADCPtr *ADCS;

// The LTC1859 is a 8-channel 16-bit ADC with programmable span connected through SPI.
class LTC1859
{
private:
    typeof(SPI) *m_spi;
    typeof(DigitalOut) *m_spinss;
    typeof(DigitalIn) *m_busy;
    int8_t m_adcnum;
    voltspan m_span;
    char m_send[2], m_recv[2];
    uint16_t m_vin, m_dataword;
public:
    LTC1859(int8_t adcnum,  typeof(SPI) *spi, typeof(DigitalOut) *spi_nss, typeof(DigitalIn) *busy, voltspan span);
    void Setspan(voltspan span);
    uint16_t Vin1(void);
    void Vchk(void);
    uint16_t Vin(void);
    bool Busy(void);
};


#endif
