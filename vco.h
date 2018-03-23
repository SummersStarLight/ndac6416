#ifndef VCO_H
#define VCO_H

#include "mbed.h"

class FreqChannel;
class LTC2668;

#define _M_LN2  0.693147180559945309417 // Natural log of 2
#define log2(x) (log(x)/_M_LN2)

#define XPWR 9
#define HALFSTEPS_PER_OCTAVE 12
const int32_t XSTEPS_PER_OCTAVE = HALFSTEPS_PER_OCTAVE<<XPWR; // 6144
#define HALFSTEPS_PER_TRITONE 6
const int32_t XSTEPS_PER_TRITONE = HALFSTEPS_PER_TRITONE<<XPWR;
const int32_t XSTEPS_PER_HALFSTEP = XSTEPS_PER_TRITONE/HALFSTEPS_PER_TRITONE;
#define HALFSTEPPWR 15
#define OCTAVEPWR 2
#define FPWR HALFSTEPPWR+OCTAVEPWR

static uint16_t* generate_xsteps() {            // static here is "internal linkage"
   uint16_t * p = new uint16_t[XSTEPS_PER_OCTAVE];
   for (uint32_t i=0; i < XSTEPS_PER_OCTAVE; ++i) {
      p[i] = uint16_t(round(pow((float)2, ((float)i/(float)XSTEPS_PER_OCTAVE))*pow(2, HALFSTEPPWR)));
   };
   return p;
}
static uint16_t *XSTEPMULS = generate_xsteps();

struct OctaveXstep
{
    uint16_t octave;
    uint16_t xstep;
};
struct WidthFreq
{
    int32_t width;
    uint32_t freq;
    uint32_t freq16;
};

class VCO;
typedef void (*vcoadjtype)( VCO *vco);

#define NUMBERVCOS 6
typedef VCO* VCOPtr;
extern VCOPtr *VCOS;

class VCO
{
private:
    FreqChannel *m_freq_channel;
    LTC2668 *m_dac;
    vcoadjtype m_vcoadjfunc;
    float m_lowfreq, m_lowvolts;
    bool m_tuned;
    int8_t m_octaves, m_vconum, m_tritone;
    int16_t *m_offsets;    // Each tritone has a different offset
    int16_t *m_octavemuls; // 55, 110, 220 ...
    uint16_t m_hmul, m_omul;
    int16_t m_dins_per_volt, m_octaves_per_volt, m_dins_per_halfstep;
    int32_t m_dinmod, m_xstep, m_hi, m_lo, m_trimod;
    uint32_t m_freq, m_din;
    char *m_buf;
    Thread m_thread;  //
    Semaphore m_vcoadjsem;
public:
    VCO(FreqChannel *freqchannel, LTC2668 *dac, int8_t vconum, float lowfreq, float lowvolts,
        int8_t octaves, int16_t dins_per_volt, int16_t octave_per_volt, vcoadjtype vcoadjfunc);
    bool Gettuned(void);
    FreqChannel *GetFreqChannel(void);
    void Info(void);
    void Dump(void);
    void MiniDump(void);
    char *Getoffsets(void);
    OctaveXstep Tritone(int8_t tritone); // returns octave and xstep
    int8_t Gettritone(int16_t octave, int16_t xstep);
    void Clr(void);
    uint32_t Getfreq16(int16_t octave, int16_t xstep); // returns the frequency in Hz * 16
    uint16_t Getfreq(int16_t octave, int16_t xstep); // returns the frequency in Hz
    WidthFreq Getwidth(int16_t octave, int16_t xstep); // returns wave width and freq
    int8_t Getoctave(uint32_t din); // return the octave which corresponds to the DAC's digital input value
    int16_t Getstep(uint32_t din);  // return the xstep which corresponds to the DAC's digital input value
    int16_t Getoffset(int8_t tritone); // returns the offset for the tritone
    int16_t Getoffsetsteps(int16_t octave, int16_t xstep); // reuturns offset steps
    int16_t Getdinoffset(uint32_t din);  // returns the amount of offset for the DAC's digital input value
    uint16_t Dintofreq(uint32_t din);
    uint32_t Dintofreq16(uint32_t din);
    WidthFreq Dintowidth(uint32_t din); // returns width, tfreq16
    void Setoffset(int8_t tritone, int16_t offset); // set the offset for the tritone
    uint32_t Dinh(int8_t octave, int8_t halfstep); // return DAC's digital input value
    uint32_t Dinx(int8_t octave, int16_t xstep); // return DAC's digital input value
    uint32_t Dinj(int8_t octave, float ratio); // return DAC's digital input value ... a ratio of 3/2 is a fifth above the octave in just intonation
    uint32_t Dinf(uint32_t freq16);       // return DAC's input value for a frequency
    float Getvolts(uint32_t din);         // return the voltage which corresponds to the DAC's digital input value
    OctaveXstep Freqtooctave(uint32_t freq16);    // return octave and xstep for a frequency
    void Tuneup(int8_t tritone, float sleep, bool printit); // tune to the tritone frequency
    void Tuneups(float sleep, bool printit); // tune to the tritone frequency
    void TritoneOffsetsDump(int8_t tritone);
    Semaphore *Getvcoadjsem(void);
    LTC2668 *Getdac(void);
    void StartAdj(void);
    void StopAdj(void);
};


void VCOAdj0(VCO *vco);


#endif
