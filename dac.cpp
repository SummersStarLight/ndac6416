// dac.cpp ... Contains the LTC2668 class for controlling the 16-channel 16-bit LTC2668 DAC chip
#include "mbed.h"
#include "freq.h"
#include "vco.h"
#include "dac.h"

DACPtr *DACS = new DACPtr[NUMBERDACS]();

LTC2668::LTC2668(int8_t dacnum,
                 typeof(SPI) *spi,
                 typeof(DigitalOut) *spinss,
                 VCO *vco,
                 bool printit,
                 int8_t tshift
                 )
{
    m_dacnum = dacnum & 0x3f; // only 64 DACs
    m_spi = spi, m_spinss = spinss, m_printit = printit;
    m_vco = vco; // m_vco is NULL DAC is not connected to VCO
    if (m_vco != NULL) {
        m_vco->GetFreqChannel()->SetDAC(this); // so the freqchannel object knows which LTC2668 object to use
    }
    m_cnt = 0;
    m_sendcode = 0x30 | (m_dacnum & 0x0f);  // command is write code to dac and update dac
    m_send[0] = m_sendcode;
    m_csadrs = m_dacnum >> 4;
    m_tshift = tshift;
    m_vout= 0, m_cnt = 0, m_din = 0, m_voutdin = 0, m_voct_octave = 0, m_voct_halfstep = 0;
    WidthFreq m_twidthfreq;
    m_twidthfreq.freq = 1;
    m_twidthfreq.freq16 = 16;
    m_twidthfreq.width = 0;
    m_dwidth = 0, m_width = 0, m_twidthshift = 0;
    DACS[m_dacnum] = this;
    Reset();
    m_adjust = true;
}

void LTC2668::Clradjusted(void) {
    m_offsetvals.adjusted = false;
}

void LTC2668::Reset(void) {
    m_offsetvals.reset();
}

OffsetVals LTC2668::Getoffsetvals(void) {
    return(m_offsetvals);
}

int32_t LTC2668::Getvoctdin(void) {
    return(m_vco->Dinh(m_voct_octave, m_voct_halfstep));
}

int32_t LTC2668::Getvoutdin(void) {
    return(m_voutdin);
}

int16_t LTC2668::GetVOUT(void) {
    return(m_vout);
}

void LTC2668::SetVCO(VCO *vco) {
    m_vco = vco;
}

VCO *LTC2668::GetVCO(void) {
    return(m_vco);
}

bool LTC2668::DinOK(int32_t din, int16_t offset)
{
    if(m_vco) {
        m_offsetvals.newoffset = offset;
        m_offsetvals.newdin = din + offset;
        //printf("DinOK %d %d\n\r", (m_offsetvals.newdin >= 0) && (m_offsetvals.newdin <=0xffff), m_offsetvals.newdin);
        return((m_offsetvals.newdin >= 0) && (m_offsetvals.newdin <=0xffff));
    } else {
        return(false);
    }
}

bool LTC2668::DinOKoffset(int32_t din)
{
    if(m_vco) {
        return DinOK(din, m_vco->Getdinoffset(din));
    } else {
        return(false);
    }
}

void LTC2668::Voutprim(int32_t din)  // Send digital input value to the DAC
{
    m_voutdin = din;
    m_spinss->write(0);
    m_send[0] = m_sendcode;
    m_send[1] = din >> 8;
    m_send[2] = din & 0xff;
    //m_spi->transfer(m_send, 3, m_recv, 3, NULL);
    m_spi->write(m_send, 3, m_recv, 3);
    m_spinss->write(1);
}

void LTC2668::Setspan(voltspan span) {
    m_span = span;
    uint8_t code = 4;
    if (m_span.low == 0 && m_span.high == 5) {
       code = 0;
    } else if(m_span.low == 0 && m_span.high == 10) {
       code = 1;
    } else if(m_span.low == -5 && m_span.high == 5) {
       code = 2;
    } else if(m_span.low == -10 && m_span.high == 10) {
       code = 3;
    }
    m_spinss->write(0);
    m_send[0] = 0x60 | (m_dacnum & 0x0f); // command is write code to dac and update dac
    m_send[1] = 0; // don't care
    m_send[2] = code;
    printf("Setspan %02d %d %d %d %02x %02x %02x\n\r",
           m_dacnum, m_span.low, m_span.high, code,
           (uint8_t)m_send[0],
           (uint8_t)m_send[1],
           (uint8_t)m_send[2]
           );
    //m_spi->transfer(m_send, 3, m_recv, 3, NULL);
    m_spi->write(m_send, 3, m_recv, 3);
    m_spinss->write(1);
}

void LTC2668::Voutall(int32_t din) { // Send digital input value to all DACs on chip
    m_spinss->write(0);
    m_send[0] = 0xa0;
    m_send[1] = din >> 8;
    m_send[2] = din & 0xff;
    //m_spi->transfer(m_send, 3, m_recv, 3, NULL);
    m_spi->write(m_send, 3, m_recv, 3);
    m_spinss->write(1);
}

void LTC2668::Vchk(int32_t din) // Send digital input value to the DAC
{
    Voutprim(din);
    printf("LTC2668 %02d: %04x (%02x %02x %02x -> %02x %02x %02x)\n\r",
           m_dacnum,
           (uint16_t)(din&0xffff),
           (uint8_t)m_send[0],
           (uint8_t)m_send[1],
           (uint8_t)m_send[2],
           (uint8_t)m_recv[0],
           (uint8_t)m_recv[1],
           (uint8_t)m_recv[2]);
}


void LTC2668::Adj(int32_t width, bool printit) { // adjust din up or down 1 toward frequency
    if (!m_vco->Gettuned() || !m_adjust) return;
    m_width = width;
    if (width > m_twidthfreq.width) {
        m_dwidth = width - m_twidthfreq.width;
    } else {
        m_dwidth = m_twidthfreq.width - width;
    }
    if (printit) {
       printf("%d. dwidth %s%-5d    width %6d twidth %6d posadj %-8d negadj %-8d shift %-4d din %5d %d\n\r",
           m_dacnum,
           width > m_twidthfreq.width?"+":"-",
           m_dwidth,
           //(float)FREQUENCY/(float)m_dwidth,
           width,
           //(float)FREQUENCY/(float)width,
           m_twidthfreq.width,
           m_offsetvals.posadj,
           m_offsetvals.negadj,
           m_twidthshift,
           m_din,
           m_dwidth < m_twidthshift);
    }
    if (m_dwidth < m_twidthshift) {
        m_offsetvals.adjusted = true;
        return;
    }
    // m_offsetvals.dadj = m_dwidth>5?5:m_dwidth;
    m_offsetvals.dadj = m_twidthshift>>5;
    if (width > m_twidthfreq.width) {
         m_offsetvals.posadj +=1;
         m_offsetvals.pos += m_offsetvals.dadj;
         m_din += m_offsetvals.dadj;
    } else {
         m_offsetvals.negadj +=1;
         m_offsetvals.neg += m_offsetvals.dadj;
         m_din -= m_offsetvals.dadj;
    }
    Voutprim(m_din);
    if (printit) {
       printf("    dadj %d pos %d neg %d dadj %d din %d\n\r",
           m_offsetvals.dadj,
           m_offsetvals.pos,
           m_offsetvals.neg,
           m_offsetvals.dadj,
           m_din);
    }
    //  m_vco->GetFreqChannel()->Stopadj();
}

void LTC2668::Wadj(bool printit) { // adjust din up or down 1 toward frequency
    // Used by 'Start' in FreqChannel to continuously adjust VCO frequency.
    Adj(m_vco->GetFreqChannel()->GetWidth(), printit);
}

void LTC2668::Vout(int32_t din, bool adjust) { // Send digital input value to the DAC with offset
    m_adjust = adjust; // default value for adjust is false ... ISRs in Envelope by default
                       // when calling Vout cannot do VCO adjustments.
    m_cnt +=1;
    m_vout = din & 0xffff;
    m_din = m_vout;
    if (m_vco && m_vco->Gettuned()) {
        if (DinOKoffset(din)) {
            m_din = m_offsetvals.newdin;
        }
        m_twidthfreq = m_vco->Dintowidth(m_vout);
        m_twidthshift = m_twidthfreq.width>>m_tshift;
        m_offsetvals.reset(); // set adjusted to false
        //m_vco->GetFreqChannel()->Startadj();
    }
    Voutprim(m_din);
}

void LTC2668::Voct(int8_t octave, int8_t halfstep, bool clr) {
    if (m_vco) {
        if (clr) m_vco->Clr();
        m_voct_octave = octave, m_voct_halfstep = halfstep;
        Vout(m_vco->Dinh(octave, halfstep), true); // Adjustments can occur here because Voct is not called in ISRs
    }
}

void LTC2668::Vmidi(int8_t midinumber, bool clr) {
    if (m_vco) {
        if (clr) m_vco->Clr();
        midinumber = midinumber<9?9:midinumber;
        midinumber -= 9;
        m_voct_octave = midinumber/12, m_voct_halfstep = midinumber%12;
        Vout(m_vco->Dinh(m_voct_octave, m_voct_halfstep), false); // No Adjustments can occur here because Vmidi is called in ShowMessages
    }
}



