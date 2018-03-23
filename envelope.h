// envelopes.h
// Segment is a class of points on a line connecting a beginning and ending voltage
// Envelope is a class of several 'Segment' objects
#ifndef ENVELOPE_H
#define ENVELOPE_H

#include "mbed.h"
#include <vector>

class Envelope;
typedef void (*segfunctype)(void);

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
private:
    LTC2668 *m_dac;
    bool m_repeat;
    begfunctype m_begfunction;
    endfunctype m_endfunction;
    std::vector<Segment> m_segments; // list of line segments
    uint32_t m_repeats, m_segcnt;
    bool m_stopflag, m_attack, m_funcalled;
public:
    Envelope(LTC2668 *dac=NULL, bool repeat=false, bool printit=false, begfunctype begfunction=NULL, endfunctype endfunction=NULL);
    bool m_printit;
    LTC2668 *GetDAC(void);
    void Start(void);
    void Info(void);
    void Add(int32_t begin,
             int32_t end,
             int16_t intervals, int16_t hold=1, int16_t segment=-1, segfunctype segfunction=NULL);
    void Add(int8_t begoctave, int8_t beghalfstep,
             int8_t endoctave, int8_t endhalfstep,
             int16_t intervals, int16_t hold=1, int16_t segment=-1, segfunctype segfunction=NULL);
    void Replace(int32_t begin,
             int32_t end,
             int16_t intervals, int16_t hold=1, int16_t segment=0, segfunctype segfunction=NULL); // replaces a line segment with new values
    bool Finished(void);
    void Stop(void);
    void Restart(void);
    bool Next(void); // step through all the line segments, return true when finished
    void Dump(void);
};

class Adsr : public Envelope
{
private:
    LTC2668 *m_dac;
    bool m_repeat;
    begfunctype m_begfunction;
    endfunctype m_endfunction;
    std::vector<Segment> m_segments; // list of line segments
    uint32_t m_repeats, m_segcnt;
    bool m_stopflag, m_attack, m_funcalled;
public:
    Adsr(LTC2668 *dac=NULL,
              bool repeat=false,
              bool printit=false,
              begfunctype begfunction=NULL,
              endfunctype endfunction=NULL);
    void Start(void);
    bool Finished(void);
    void Restart(void);
    void Release(bool wait=false);
};

Envelope envtst(LTC2668 *dac=NULL, bool repeat=false, bool printit=false);

#endif
