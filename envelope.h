// envelopes.h
// Segment is a class of points on a line connecting a beginning and ending voltage
// Envelope is a class of several 'Segment' objects
#ifndef ENVELOPE_H
#define ENVELOPE_H

#include "mbed.h"
#include <vector>

class Envelope;
class Adsr;
typedef void (*segfunctype)(void);
#define NUMBERENVS 16
typedef Envelope* ENVPtr;
extern ENVPtr *ENVS;

class Segment // Segment is a class of points on a line connecting a beginning and ending voltage
{
private:
    Envelope *m_envelope;
    segfunctype m_function;
    int16_t m_hold, m_holdcnt;
    int32_t m_begin, m_end;
    uint16_t m_point, m_lastpoint;
    uint32_t m_change, m_cnt, m_intervals;
    bool m_change_up;
public:
    Segment(Envelope *envelope, segfunctype function=NULL);
    void Start(void);
    void Dump(void);
    void Calc(int32_t begin, int32_t end, int16_t intervals, int16_t hold=1);
    void Set(bool set=true);
    bool Next(bool set=true);
};

typedef void (*begfunctype)(void);
typedef void (*endfunctype)(void);

class Envelope // Envelope is a class of several 'Segment' objects
{
protected:
    LTC2668 *m_dac;
    bool m_repeat;
    begfunctype m_begfunction;
    endfunctype m_endfunction;
    std::vector<Segment> m_segments; // list of line segments
    uint32_t m_repeats;
    bool m_attack, m_funcalled;
public:
    Envelope(int8_t m_num, LTC2668 *dac=NULL, bool repeat=false, bool printit=false, begfunctype begfunction=NULL, endfunctype endfunction=NULL);
    bool m_printit, m_stopflag;
    uint32_t m_segcnt;
    LTC2668 *GetDAC(void);
    void Info(void);
    void Add(int32_t begin,
             int32_t end,
             int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction);
    void Add(int8_t begoctave, int8_t beghalfstep,
             int8_t endoctave, int8_t endhalfstep,
             int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction);
    void Replace(int32_t begin,
             int32_t end,
             int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction); // replaces a line segment with new values
    void Replace(int8_t begoctave, int8_t beghalfstep,
             int8_t endoctave, int8_t endhalfstep,
             int32_t end,
             int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction); // replaces a line segment with new values
    void Start(void);
    bool Finished(void);
    void Stop(void);
    void Restart(void);
    bool Next(void); // step through all the line segments, return true when finished
    void Dump(void);
};

#define NUMBERADSRS 32
typedef Adsr* ADSRPtr;
extern ADSRPtr *ADSRS;

class Adsr : public Envelope
{
public:
    Adsr(int8_t num,
            LTC2668 *dac=NULL,
            bool repeat=false,
            bool printit=false,
            begfunctype begfunction=NULL,
            endfunctype endfunction=NULL);
    void Start(void);
    bool Finished(void);
    void Restart(void);
    bool Next(void); // step through all the line segments, return true when finished
    void Release(bool wait=false);
};

Envelope envtst(int8_t num, LTC2668 *dac, bool repeat, bool printit);

#endif
