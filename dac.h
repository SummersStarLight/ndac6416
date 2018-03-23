#ifndef DAC_H
#define DAC_H

#include "mbed.h"

class VCO;

#define NUMBERDACS 64
typedef LTC2668* DACPtr;
extern DACPtr *DACS;

struct voltspan
{
    int8_t low;
    int8_t high;
};

struct OffsetVals {
    int16_t newoffset;
    int32_t newdin;
    uint32_t posadj;
    uint32_t negadj;
    uint32_t pos;
    uint32_t neg;
    int16_t dadj;
    bool adjusted;
    void reset(void) {
        posadj = 0;
        negadj = 0;
        pos = 0;
        neg = 0;
        dadj = 0;
        adjusted = false;
    }
};

class LTC2668  // Linear Technologies 2668 16 channel 16-bit DAC chip with SPI interface
// The LTC2668 has 16 DACs.  Each of four LTC2668 use a different SPI channel for communication.
{
private:
    VCO *m_vco;
    typeof(SPI) *m_spi;
    typeof(DigitalOut) *m_spinss;
    //GPIO_TypeDef *m_spinss;
    voltspan m_span;
    bool m_printit, m_adjust;
    int8_t m_tshift, m_sendcode, m_csadrs, m_voct_octave, m_voct_halfstep;
    char m_send[3], m_recv[3];
    int16_t m_vout, m_cnt, m_tfreq;
    int32_t m_width, m_twidthshift, m_dwidth;
    int32_t m_din, m_voutdin;
    OffsetVals m_offsetvals;
    WidthFreq m_twidthfreq;
public:
    LTC2668(int8_t dacnum,  typeof(SPI) *spi, typeof(DigitalOut) *spi_nss, VCO *vco, bool printit, int8_t tshift);
    VCO *GetVCO(void);
    int8_t m_dacnum;
    void SetVCO(VCO *vco);
    bool DinOK(int32_t din, int16_t offset);
    bool DinOKoffset(int32_t din);
    void Voutprim(int32_t din); // Send digital input value to the DAC
    void Setspan(voltspan span);
    void Voutall(int32_t din); // Send digital input value to all DACs on chip
    void Vchk(int32_t din); // Send digital input value to the DAC
    void Adj(int32_t width, bool printit=false); // adjust din up or down 1 toward frequency
    void Wadj(bool printit=false); // adjust din up or down 1 toward frequency
    void Vout(int32_t din, bool adjust=false); // Send digital input value to the DAC.
                                               // Default to no adjustment.
    void Voct(int8_t octave, int8_t halfstep, bool clr=false);
    int16_t GetVOUT(void);
    int32_t Getvoutdin(void);
    int32_t Getvoctdin(void);
    OffsetVals Getoffsetvals(void);
    void Reset(void);
    void Clradjusted(void);
};


#endif
