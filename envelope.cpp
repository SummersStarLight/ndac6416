// envelopes.cpp
// Segment is a class of points on a line connecting a beginning and ending voltage
// Envelope is a class of several 'Segment' objects

#include "mbed.h"
#include <vector>
#include "freq.h"
#include "vco.h"
#include "dac.h"
#include "envelope.h"

Segment::Segment(Envelope *envelope, segfunctype function)
{
    m_envelope = envelope;
    Init(function);
}

void Segment::Init(segfunctype function)
{
    m_function = function;
    m_begin = 0, m_end = 0, m_point = 0, m_hold = 1, m_lastpoint = 0;
    m_intervals = 0;    // number of time-intervals to go from begin to end
    m_change = 0;       // every Interval change point by this amount.  This is <<16 to get accuracy
    m_change_up = true; // if true change is positive otherwise negative
    Start();
}

void Segment::SetFunc(segfunctype function)
{
    m_function = function;
}

void Segment::ChangeInc(uint32_t inc)
{
    m_change += inc;
}

void Segment::Start(void)
{
    m_cnt = 0;     // number of times Next has been called
    m_holdcnt = 0; // number of times Next has been called Next. m_holdcnt increments from 0 to m_hold
}
void Segment::IncHold(int16_t inc)
{
    m_hold += inc;
}

void Segment::Dump(void)
{
    printf("DAC %2d, %2d ints from %d to %d at %s%.2f Hold %d Hcnt %d cnt %d F%p D%x\n\r",
           m_envelope->GetDAC()->m_dacnum,
           m_intervals,
           m_begin,
           m_end,
           m_change_up ? "+" : "-",
           (float)m_change / (float)0x10000,
           m_hold,
           m_holdcnt,
           m_cnt,
           m_function,
           m_point);
    while (!Next(false))
        ;
    //printf("\n\r");
}

void Segment::Calc(int32_t begin, int32_t end, int16_t intervals, int16_t hold)
{
    m_begin = begin, m_end = end, m_hold = hold;
    m_intervals = !intervals ? 1 : intervals; // number of time-intervals to go from begin to end
    Start();
    m_change = (abs(end - begin) << 16) / m_intervals; // m_change is times 65536 (<<16)
    m_change_up = (end - begin) >= 0;
    m_point = begin;
    m_lastpoint = 0;
}

void Segment::Set(bool set)
{
    if (set && m_envelope->GetDAC() && m_point != m_lastpoint)
    {
        m_envelope->GetDAC()->Vout(m_point);
        if (m_function)
            m_function();
    }
    if (m_envelope->m_printit)
        printf("(%d %d) ", m_point, m_envelope->m_segcnt);
    m_lastpoint = m_point;
}

bool Segment::Next(bool set)
{
    if (m_change_up)
        m_point = ((m_cnt * m_change) >> 16) + m_begin; // m_change has been shifted <<16 so must be shifted back
    else
        m_point = -((m_cnt * m_change) >> 16) + m_begin; // m_change has been shifted <<16 so must be shifted back
    if (m_cnt >= m_intervals)
        m_point = m_end;
    Set(set = set);
    m_holdcnt++;
    if (m_holdcnt >= m_hold)
    {
        m_holdcnt = 0;
        m_cnt++;
        if (m_cnt >= m_intervals)
        {
            m_cnt = 0;
            m_lastpoint = 0;
            return true;
        }
    }
    return false;
}

ENVPtr *ENVS = new ENVPtr[NUMBERENVS]();

Envelope::Envelope(int8_t num, LTC2668 *dac, bool repeat, bool printit, begfunctype begfunction, endfunctype endfunction)
{
    m_dac = dac; // points to an object with 'Vout' method
    m_num = num, m_repeat = repeat, m_printit = printit;
    m_begfunction = begfunction, m_endfunction = endfunction;
    Clear();
    // .h:  std::vector<Segment> m_segments; // list of line segment
    if (num != -1)
        ENVS[num] = this;
    m_inc = 1;
    Start();
}

LTC2668 *Envelope::GetDAC(void)
{
    return m_dac;
}

void Envelope::SetInc(int16_t inc)
{
    m_inc = inc;
}

void Envelope::Clear(void)
{
    std::vector<Segment>(m_segments).swap(m_segments); // clear and shrink vector of list of pointers to a line segment
}

void Envelope::Start(void)
{
    m_segcnt = 0;       // when the last point of a line is reached segcnt is incremented to the next Segment
    m_repeats = 0;      // number of times all line segments have repeated
    m_stopflag = false; // when true stop Next from moving thru segments
    m_attack = false;   // ADSR: if true will run to the second segment, if false will run to the third
    m_funcalled = false;
}

void Envelope::Info(void)
{
    printf("Env %2d, DAC %2d, Segments: %d, SCnt: %d, RCnt: %d, Stop: %d, Repeat: %d, Finished: %d\n\r",
           m_num,
           m_dac->m_dacnum,
           m_segments.size(),
           m_segcnt,
           m_repeats,
           m_stopflag,
           m_repeat,
           Finished());
}

void Envelope::Add(int32_t begin, int32_t end,
                   int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction)
{
    Segment seg = Segment(this, segfunction);
    seg.Calc(begin, end, intervals, hold = hold);
    if (segment == -1)
    {
        m_segments.push_back(seg);
    }
    else
    {
        m_segments.insert(m_segments.begin() + segment, seg);
    }
}

void Envelope::Add(int8_t begoctave, int8_t beghalfstep,
                   int8_t endoctave, int8_t endhalfstep,
                   int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction)
{
    Segment seg = Segment(this, segfunction);
    // this will throw error if dac doesn't have a VCO
    seg.Calc(m_dac->GetVCO()->Dinh(begoctave, beghalfstep),
             m_dac->GetVCO()->Dinh(endoctave, endhalfstep),
             intervals, hold = hold);
    if (segment == -1)
    {
        m_segments.push_back(seg);
    }
    else
    {
        m_segments.insert(m_segments.begin() + segment, seg);
    }
}

void Envelope::SetSegFunc(segfunctype segfunction)
{
    m_segments[m_segments.size() - 1].SetFunc(segfunction);
}

void Envelope::Replace(int32_t begin, int32_t end,
                       int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction)
{ // replaces a line segment with new values
    //Segment seg = Segment(this, segfunction);
    m_segments[segment].Init(segfunction);
    m_segments[segment].Calc(begin, end, intervals, hold = hold);
    //m_segments[segment] = seg;
    // run Restart(); after replacing all necessary segments
}

void Envelope::Replace(int8_t begoctave, int8_t beghalfstep,
                       int8_t endoctave, int8_t endhalfstep,
                       int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction)
{ // replaces a line segment with new values
    //Segment seg = Segment(this, segfunction);
    m_segments[segment].Init(segfunction);
    m_segments[segment].Calc(m_dac->GetVCO()->Dinh(begoctave, beghalfstep),
                             m_dac->GetVCO()->Dinh(endoctave, endhalfstep),
                             intervals, hold = hold);
    //m_segments[segment] = seg;
    // run Restart(); after replacing all necessary segments
}

bool Envelope::Finished(void)
{
    return m_segcnt >= m_segments.size();
}

void Envelope::Stop(void)
{
    m_stopflag = true;
}

void Envelope::Restart(void)
{
    if (m_begfunction)
        m_begfunction();
    for (m_ui = 0; m_ui < m_segments.size(); m_ui++)
        m_segments[m_ui].Start();
    m_segcnt = 0;
    m_repeats += 1;
    m_stopflag = false;
    m_funcalled = false;
}

bool Envelope::Next(void)
{ // step through all the line segments, return true when finished
    if (!m_stopflag)
    {
        if (!Finished() && m_segments[m_segcnt].Next())
            m_segcnt += 1;
        if (Finished())
        {
            if (m_repeat)
            {
                Restart();
            }
            else
            {
                if (m_endfunction && !m_funcalled && !m_attack)
                {
                    m_funcalled = true;
                    m_endfunction();
                }
            }
            return true;
        }
    }
    return false;
}

void Envelope::IncSegHold(void)
{
    for (m_ui = 0; m_ui < m_segments.size(); m_ui++)
    {
        m_segments[m_ui].IncHold(m_inc);
    }
}

void Envelope::IncSegHold(int16_t inc)
{
    for (m_ui = 0; m_ui < m_segments.size(); m_ui++)
    {
        m_segments[m_ui].IncHold(inc);
    }
}

void Envelope::IncSegChange(uint32_t inc)
{
    for (m_ui = 0; m_ui < m_segments.size(); m_ui++)
    {
        m_segments[m_ui].ChangeInc(inc);
    }
}

void Envelope::Dump(void)
{
    printf("\n\rEnvelope %d\n\r", m_num);
    for (unsigned int i = 0; i < m_segments.size(); i++)
    {
        printf("**** Segment number %d ", i);
        m_segments[i].Dump();
    }
}

ADSRPtr *ADSRS = new ADSRPtr[NUMBERADSRS]();

Adsr::Adsr(int8_t num, LTC2668 *dac, bool repeat, bool printit, begfunctype begfunction, endfunctype endfunction)
    : Envelope(-1, dac, repeat, printit, begfunction, endfunction)
{
    ADSRS[num] = this;
}

bool Adsr::Next(void)
{ // step through all the line segments, return true when finished
    if (!m_stopflag)
    {
        //printf("Next0 %d %d %d\n\r", Finished(), m_segcnt, m_attack);
        if (!Finished() && m_segments[m_segcnt].Next())
            m_segcnt += 1;
        //printf("Next1 %d %d %d\n\r", Finished(), m_segcnt, m_attack);
        if (Finished())
        {
            if (m_repeat)
            {
                Restart();
            }
            else
            {
                if (m_endfunction && !m_funcalled && !m_attack)
                {
                    m_funcalled = true;
                    m_endfunction();
                }
            }
            return true;
        }
    }
    return false;
}

bool Adsr::Finished(void)
{
    if (m_attack)
    {
        //printf("finished %d\n\r", m_segments.size()>2?2:m_segments.size());
        return m_segcnt >= (m_segments.size() > 2 ? 2 : m_segments.size());
    }
    else
    {
        return m_segcnt >= m_segments.size();
    }
}

void Adsr::Start(void)
{
    m_segcnt = 0;       // when the last point of a line is reached segcnt is incremented to the next Segment
    m_repeats = 0;      // number of times all line segments have repeated
    m_stopflag = false; // when true stop Next from moving thru segments
    m_attack = true;    // if true will run to the second segment, if false will run to the third
    m_funcalled = false;
}

void Adsr::Restart(void)
{
    if (m_begfunction)
        m_begfunction();
    m_attack = true; // if true will run to the second segment, if false will run to the third
    for (unsigned int i = 0; i < m_segments.size(); i++)
        m_segments[i].Start();
    m_segcnt = 0;
    m_repeats += 1;
    m_stopflag = false;
    m_funcalled = false;
}

void Adsr::Release(bool wait)
{
    while (wait)
    {
        wait_us(50);
        if (m_segcnt == 2)
            break;
    }
    m_attack = false; // if true will run to the second segment, if false will run to the third
}

Envelope envtst(int8_t num, LTC2668 *dac, bool repeat, bool printit)
{
    Envelope env = Envelope(num, dac, repeat, printit);
    env.Add(3000, 30000, 10, 1, -1, NULL);
    env.Add(20000, 2000, 5, 1, -1, NULL);
    env.Dump();
    return env;
}
