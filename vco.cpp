// vco.cpp ... VCO class which uses the FreqTimer results to adjust the LTC2668 DACs connected
// Voltage Controlled Oscillators.
#include "mbed.h"
#include "math.h"
#include "main.h"
#include "freq.h"
#include "vco.h"
#include "dac.h"

// 1 Volt per octave.  12*34=408 digital input counts per volt.
// 4080=10 volts.  For 12-bit DAC with 4095 (0xfff) steps. Highest value is 10.036 volts.
// Oct tritone  V   Hz       Vout  Midi#
// 0   0        -5  13.75    0     9
// 1   2        -4  27.5     408   21
// 2   4        -3  55       816   33
// 3   6        -2  110      1224  45
// 4   8        -1  220      1632  57
// 5   10       0   440      2040  69
// 6   12       +1  880      2448  81
// 7   14       +2  1760     2856  93
// 8   16       +3  3520     3264  105
// 9   18       +4  7040     3672
// 10  20       +5  14080    3672

VCOPtr *VCOS = new VCOPtr[NUMBERVCOS]();

// This is the method use by VCO's m_thread.  The method must be static so is defined here instead of in the Object.
void VCOAdj0(VCO *vco)
{
    int32_t semaphoreid;
    while (1)
    {
        // The m_vcoadjsem semaphore will release this wait otherwise adjust every 5 seconds.
        // vco->Getvcoadjsem()->wait(osWaitForever);
        semaphoreid = vco->Getvcoadjsem()->wait(5000);
        // if timeout (5 secs expired) set adjusted to false to start adustment
        //if(semaphoreid == 0) vco->Getdac()->Reset();
        if (semaphoreid == 0)
            vco->Getdac()->Clradjusted();
        vco->Getdac()->Adj(vco->GetFreqChannel()->GetWidth());
    }
}

VCO::VCO(FreqChannel *freqchannel, LTC2668 *dac, int8_t vconum, float lowfreq, float lowvolts,
         int8_t octaves, int16_t dins_per_volt, int16_t octaves_per_volt, vcoadjtype vcoadjfunc)
{
    m_buf = (char *)malloc(140);
    m_vcoadjfunc = vcoadjfunc;
    m_dac = dac;
    m_dac->SetVCO(this);
    m_freq_channel = freqchannel;
    m_freq_channel->SetDAC(m_dac); // The dac of the freqchannel is only set by the VCO
    m_vconum = vconum;
    m_lowfreq = lowfreq;
    m_lowvolts = lowvolts;
    m_octaves = octaves;
    m_dins_per_volt = dins_per_volt;
    m_octaves_per_volt = octaves_per_volt;
    m_dinmod = 0, m_din = 0;
    m_tritone = 0;
    m_dins_per_halfstep = int16_t((m_octaves_per_volt * m_dins_per_volt) / HALFSTEPS_PER_OCTAVE);
    //m_offsets = new int16_t[m_octaves*2+1]; // Each tritone has a different offset
    m_offsets = (int16_t *)malloc((m_octaves * 2 + 1) * 2); // Each tritone has a different offset
    Clr();
    m_hmul = 0, m_omul = 0, m_freq = 0;
    //m_octavemuls = new int16_t[m_octaves+1]; // multipliers for the halfsteps
    m_octavemuls = (int16_t *)malloc((m_octaves + 1) * 2); // multipliers for the halfsteps
    for (int octave = 0; octave < (m_octaves + 1); octave++)
    {
        m_octavemuls[octave] = round(m_lowfreq * pow(2, octave) * pow(2, OCTAVEPWR));
    };
    Semaphore m_vcoadjsem(0);
    StartAdj();
    VCOS[m_vconum] = this;
}

void VCO::Info(void)
{
    printf("dins_per_volt %d, dins_per_halfstep %d\n\r    [", m_dins_per_volt, m_dins_per_halfstep);
    for (int octave = 0; octave < (m_octaves + 1); octave++)
        printf("%d ", m_octavemuls[octave]);
    printf("]\n\r");
}

void VCO::StartAdj(void)
{
    if (m_vcoadjfunc)
        m_thread.start(callback(m_vcoadjfunc, this));
}

void VCO::StopAdj(void)
{
    m_thread.terminate();
}

Semaphore *VCO::Getvcoadjsem(void)
{
    return (&m_vcoadjsem);
}

char *VCO::Getoffsets(void)
{
    sprintf(m_buf, "[");
    for (int tritone = 0; tritone < (m_octaves * 2 + 1); tritone++)
    {
        sprintf(m_buf, "%s%3d ", m_buf, m_offsets[tritone]);
    }
    sprintf(m_buf, "%s]\n\r", m_buf);
    return (m_buf);
}

LTC2668 *VCO::Getdac(void)
{
    return (m_dac);
}

OctaveXstep VCO::Tritone(int8_t tritone)
{ // returns octave and xstep
    OctaveXstep x;
    int32_t tx = tritone * XSTEPS_PER_TRITONE;
    x.octave = tx / XSTEPS_PER_OCTAVE;
    x.xstep = tx % XSTEPS_PER_OCTAVE;
    return (x);
}

void VCO::MiniDump(void)
{
    uint32_t freq16 = m_freq_channel->SampledFreq16();
    int32_t din = m_dac->Getvoctdin(); // this will only work if the dac has been set using 'Voct'
    uint32_t df = Dintofreq16(din);
    float v = Getvolts(din);
    int8_t o = Getoctave(din);
    int16_t s = Getstep(din);
    OffsetVals ov = m_dac->Getoffsetvals();
    printf("   VCO %d %04x %5d %6.2f %8.2f %9.4f %2d %4d (%5d %6d %8d %8d %8d %8d %8d)\n\r",
           m_vconum,
           (uint16_t)din & 0xffff,
           (uint16_t)din & 0xffff,
           (float)freq16 / 16.0 - (float)df / 16.0,
           (float)df / 16.0,
           v,
           o,
           s,
           ov.newoffset,
           ov.newdin,
           ov.posadj,
           ov.negadj,
           ov.pos,
           ov.neg,
           ov.dadj);
}

void VCO::Dump(void)
{
    printf("Tritones:\n\r");
    for (int tritone = 0; tritone < (m_octaves * 2 + 1); tritone++)
    {
        OctaveXstep x = Tritone(tritone);
        WidthFreq w = Getwidth(x.octave, x.xstep);
        uint32_t dinx = Dinx(x.octave, x.xstep);
        uint32_t freq16 = Getfreq16(x.octave, x.xstep);
        uint32_t dinf = Dinf(freq16);
        int8_t t = Gettritone(x.octave, x.xstep);
        uint16_t f = Getfreq(x.octave, x.xstep);
        int8_t o = Getoctave(dinx);
        WidthFreq dw = Dintowidth(dinx);
        uint32_t df = Dintofreq16(dinx);
        float v = Getvolts(dinx);
        printf("   (%2d %2d) (%2d %2d) %6d (%7d %7d) (%5d %5d %5d) (%7d %7d %7d %7d) (%5d %5d) %6.2f\n\r",
               tritone,
               t,

               x.octave,
               o,
               x.xstep,

               w.width,
               dw.width,

               w.freq,
               dw.freq,
               f,

               w.freq16,
               dw.freq16,
               freq16,
               df,

               dinx,
               dinf, // log2((float)freq16/(m_lowfreq*16.0)) * (float)m_dins_per_volt,

               v);
    };
}

int8_t VCO::Gettritone(int16_t octave, int16_t xstep)
{
    return (octave * 2 + xstep / XSTEPS_PER_TRITONE);
}

void VCO::Clr(void)
{
    m_tuned = false;
    for (int tritone = 0; tritone < (m_octaves * 2 + 1); tritone++)
    {
        m_offsets[tritone] = 0;
    };
}

bool VCO::Gettuned(void)
{
    return (m_tuned);
}

FreqChannel *VCO::GetFreqChannel(void)
{
    return (m_freq_channel);
}

uint32_t VCO::Getfreq16(int16_t octave, int16_t xstep)
{ // returns the frequency in Hz * 16
    //printf("G16: %d %d\n\r", octave, xstep);
    //return(pow(2.0, (float)((float)octave*12.0+((float)xstep/(pow(2.0, XPWR))))/12.0)*m_lowfreq);
    // return 2**((octave*12+(xstep/(2**XPWR)))/12.)*m_lowfreq
    m_omul = m_octavemuls[octave];
    m_hmul = XSTEPMULS[xstep];
    m_freq = m_omul * m_hmul;
    return (m_freq >> (FPWR - 4));
}

uint16_t VCO::Getfreq(int16_t octave, int16_t xstep)
{ // returns the frequency in Hz
    m_omul = m_octavemuls[octave];
    m_hmul = XSTEPMULS[xstep];
    m_freq = m_omul * m_hmul;
    return (m_freq >> (FPWR));
    // throws heap locked:
    // return 2**((octave*12+halfstep)/12.)*m_lowfreq
    // for xstep:
    // return 2**((octave*12+(xstep/(2**XPWR)))/12.)*m_lowfreq
}

WidthFreq VCO::Getwidth(int16_t octave, int16_t xstep)
{ // returns wave width
    WidthFreq w;
    w.freq16 = Getfreq16(octave, xstep);
    w.freq = w.freq16 >> 4;
    w.width = (FREQUENCY << 4) / w.freq16;
    return w;
}

int8_t VCO::Getoctave(uint32_t din)
{ // return the octave which corresponds to the DAC's digital input value
    return (din / m_dins_per_volt);
}

int16_t VCO::Getstep(uint32_t din)
{ // return the xstep which corresponds to the DAC's digital input value;
    m_dinmod = (din % m_dins_per_volt) << XPWR;
    return (m_dinmod / m_dins_per_halfstep);
}

int16_t VCO::Getoffset(int8_t tritone)
{ // returns the offset for the tritone
    return (m_offsets[tritone]);
}

int16_t VCO::Getoffsetsteps(int16_t octave, int16_t xstep)
{
    m_tritone = Gettritone(octave, xstep);
    m_lo = m_offsets[m_tritone];
    m_hi = m_offsets[m_tritone + 1];
    m_trimod = xstep % XSTEPS_PER_TRITONE;
    return (m_lo + ((m_hi - m_lo) * m_trimod) / XSTEPS_PER_TRITONE);
}

void VCO::TritoneOffsetsDump(int8_t tritone)
{
    OctaveXstep x0 = Tritone(tritone);
    printf("%2d %4d to %4d -- ", tritone, m_offsets[tritone], m_offsets[tritone + 1]);
    for (uint16_t xstep = x0.xstep; xstep < (x0.xstep + XSTEPS_PER_TRITONE); xstep += XSTEPS_PER_HALFSTEP)
    {
        printf("%4d ", Getoffsetsteps(x0.octave, xstep));
    }
    printf("\n\r");
}

int16_t VCO::Getdinoffset(uint32_t din)
{ // returns the amount of offset for the DAC's digital input value
    return (Getoffsetsteps(Getoctave(din), Getstep(din)));
}

uint16_t VCO::Dintofreq(uint32_t din)
{
    return (Getfreq(Getoctave(din), Getstep(din)));
}

uint32_t VCO::Dintofreq16(uint32_t din)
{
    return (Getfreq16(Getoctave(din), Getstep(din)));
}

WidthFreq VCO::Dintowidth(uint32_t din)
{ // returns width, tfreq16
    return (Getwidth(Getoctave(din), Getstep(din)));
}

void VCO::Setoffset(int8_t tritone, int16_t offset)
{ // set the offset for the tritone
    m_offsets[tritone] = offset;
}

uint32_t VCO::Dinh(int8_t octave, int8_t halfstep)
{ // return DAC's digital input value
    return (octave * m_dins_per_volt + halfstep * m_dins_per_halfstep);
}

uint32_t VCO::Dinx(int8_t octave, int16_t xstep)
{ // return DAC's digital input value
    return ((uint32_t)octave * (uint32_t)m_dins_per_volt + (((uint32_t)xstep * (uint32_t)m_dins_per_halfstep) >> XPWR));
}

uint32_t VCO::Dinj(int8_t octave, float ratio)
{ // return DAC's digital input value ... a ratio of 3/2 is a fifth above the octave in just intonation
    return (log2((float)pow(2, octave) * ratio) * (float)m_dins_per_volt + .5);
}

uint32_t VCO::Dinf(uint32_t freq16)
{ // return DAC's input value for frequency*16
    return (log2((float)freq16 / (m_lowfreq * 16.0)) * (float)m_dins_per_volt + .5);
}

float VCO::Getvolts(uint32_t din)
{ // return the voltage which corresponds to the DAC's digital input value
    return (m_lowvolts + ((float)din / (float)m_dins_per_volt));
}

OctaveXstep VCO::Freqtooctave(uint32_t freq16)
{ // return octave and xstep for frequency*16
    m_din = Dinf(freq16);
    OctaveXstep x;
    x.octave = Getoctave(m_din);
    x.xstep = Getstep(m_din);
    return (x);
}

void VCO::Tuneup(int8_t tritone, float sleep, bool printit)
{ // tune to the tritone frequency
    OctaveXstep ox = Tritone(tritone);
    int32_t targetdin = Dinx(ox.octave, ox.xstep);
    int32_t offset = 0;
    uint32_t targetfreq16 = Getfreq16(ox.octave, ox.xstep);
    int16_t cnt = 0, matching = 0, i = 0;
    Setoffset(tritone, offset);
    int16_t *offsets = new int16_t[8];
    uint32_t freq16 = 0;
    int32_t freqdin;
    bool firsttime = true;
    if (printit)
        printf("tritone %d, targetdin %6d, targetfreq %7.1f, oct %d,%d\n\r",
               tritone, targetdin, (float)targetfreq16 / 16.0, ox.octave, ox.xstep);
    while (true)
    {
        //if (printit) printf("%d\n\r", cnt);
        if (!m_dac->DinOK(targetdin, offset))
        {
            freq16 = m_freq_channel->SampledFreq16();
            printf("DinOK not OK %d %d %d\n\r", targetdin, offset, freq16);
            break;
        }
        //m_dac->Vout(targetdin+offset, false);     // this sets voltage with offset
        m_dac->Voutprim(targetdin + offset); // this sets voltage with offset
        wait(sleep);                         // allow VCO to settle
        freq16 = m_freq_channel->SampledFreq16();
        if (!freq16)
        {
            printf("Freq16 not OK %d %d %d\n\r", targetdin, offset, freq16);
            break;
        }
        freqdin = (int32_t)Dinf(freq16);
        if (abs(targetdin - freqdin) > 3000)
        {
            printf("   vco %d tdin fdin diff %5d %5d %7.1f %5d %7.1f %5d %d\n\r", m_vconum, abs(targetdin - freqdin), targetdin, (float)targetfreq16 / 16.0, freqdin, (float)freq16 / 16.0, offset, targetdin + offset);
            continue;
        }
        offset += (targetdin - freqdin);
        if (printit)
            printf("Offsets: %d %d %d %d %7.1f %7.1f\n\r", cnt, offset, targetdin, freqdin, (float)targetfreq16 / 16.0, (float)freq16 / 16.0);
        if (firsttime)
        {
            firsttime = false;
            continue;
        }
        offsets[cnt] = offset;
        matching = 0;
        for (i = 0; i < (cnt - 1); i++)
        {
            if (offsets[i] == offset)
            {
                matching += 1;
            }
        }
        if (matching == 3)
            break;
        if (cnt == 7)
            break;
        cnt += 1;
        //if freq == targetfreq: break
    }
    float total = 0.0;
    for (i = 0; i <= cnt; i++)
        total += (float)offsets[i];
    offset = round(total / (float)(cnt + 1));
    Setoffset(tritone, offset);
    if (printit)
        printf("%2d %2d %5d %7.1f %s", tritone, cnt, offset, freq16 / 16.0, Getoffsets());
}

void VCO::Tuneups(float sleep, bool printit)
{ // tune to the tritone frequency
    m_dac->Reset();
    int16_t vout = m_dac->GetVOUT();
    int8_t tritone;
    Clr(); // clears offset and sets m_tuned false
    for (tritone = 4; tritone < 19; tritone++)
    {
        Tuneup(tritone, sleep, printit);
    }
    int16_t offset0, offset1;
    for (tritone = 16; tritone < 20; tritone++)
    {
        offset0 = Getoffset(tritone);
        offset1 = Getoffset(tritone + 1);
        if (offset1 == 0 and offset0 != 0)
            Setoffset(tritone + 1, offset0);
    }
    for (tritone = 4; tritone > 0; tritone--)
    {
        offset0 = Getoffset(tritone);
        offset1 = Getoffset(tritone - 1);
        if (offset1 == 0 and offset0 != 0)
            Setoffset(tritone - 1, offset0);
    }
    m_dac->Vout(vout, true); // setting adjust True will cause the adjustments to occur.
    m_tuned = true;
    if (printit)
        printf("%s\n\r%s\n\r", m_freq_channel->vars(), Getoffsets());
}
