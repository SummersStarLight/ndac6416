// envelopes.cpp
// Segment is a class of points on a line connecting a beginning and ending voltage
// Envelope is a class of several 'Segment' objects

#include "mbed.h"
#include <vector>
#include "freq.h"
#include "vco.h"
#include "dac.h"
#include "envelope.h"

Segment::Segment(Envelope *envelope, segfunctype function) {
    m_envelope = envelope, m_function = function;
    m_begin = 0, m_end = 0, m_point = 0, m_hold = 1, m_lastpoint = 0;
    m_intervals = 0;  // number of time-intervals to go from begin to end
    m_change = 0;   // every Interval change point by this amount.  This is <<16 to get accuracy
    m_change_up = true; // if true change is positive otherwise negative
    Start();
}

void Segment::Start(void) {
    m_cnt = 0;       // number of times Next has been called
    m_holdcnt = 0;   // number of times Next has been called Next. m_holdcnt increments from 0 to m_hold
}

void Segment::Dump(void) {
    printf("DAC %d, %d intervals from %d to %d at %s%.2f Hold %d Holdcnt %d cnt %d.\n\r",
       m_envelope->GetDAC()->m_dacnum,
       m_intervals,
       m_begin,
       m_end,
       m_change_up?"+":"-",
       (float)m_change/(float)0x10000,
       m_hold,
       m_holdcnt,
       m_cnt
    );
    while(!Next(false));
    printf("\n\r");
}

void Segment::Calc(int32_t begin, int32_t end, int16_t intervals, int16_t hold) {
    m_begin = begin, m_end = end, m_hold = hold;
    m_intervals = !intervals?1:intervals;  // number of time-intervals to go from begin to end
    Start();
    m_change = (abs(end - begin)<<16) / m_intervals;  // m_change is times 65536 (<<16)
    m_change_up = (end - begin)>=0;
    m_point = begin;
    m_lastpoint = 0;
}

void Segment::Set(bool set) {
    if (set && m_envelope->GetDAC() && m_point != m_lastpoint) {
        m_envelope->GetDAC()->Vout(m_point);
        if (m_function) m_function();
    }
    if (m_envelope->m_printit) printf("%d ", m_point);
    m_lastpoint = m_point;
}

bool Segment::Next(bool set) {
    if (m_change_up) m_point = ((m_cnt * m_change)>>16) + m_begin;  // m_change has been shifted <<16 so must be shifted back
    else m_point = -((m_cnt * m_change)>>16) + m_begin;  // m_change has been shifted <<16 so must be shifted back
    if (m_cnt >= m_intervals) m_point = m_end;
    Set(set=set);
    m_holdcnt++;
    if (m_holdcnt >= m_hold) {
        m_holdcnt = 0;
        m_cnt++;
        if (m_cnt >= m_intervals) {
            m_cnt = 0;
            m_lastpoint = 0;
            return true;
        }
    }
    return false;
}

Envelope::Envelope(LTC2668 *dac, bool repeat, bool printit, begfunctype begfunction, endfunctype endfunction) {
    m_dac = dac;      // points to an object with 'Vout' method
    m_repeat = repeat, m_printit = printit;
    m_begfunction = begfunction, m_endfunction = endfunction;
    std::vector<Segment>(m_segments).swap(m_segments); // clear and shrink vector of list of pointers to a line segment
    // .h:  std::vector<Segment> m_segments; // list of line segment
    Start();
}

LTC2668 *Envelope::GetDAC(void) {
    return m_dac;
}

void Envelope::Start(void) {
    m_segcnt = 0;      // when the last point of a line is reached segcnt is incremented to the next Segment
    m_repeats = 0;     // number of times all line segments have repeated
    m_stopflag = false;// when true stop Next from moving thru segments
    m_attack = false;  // ADSR: if true will run to the second segment, if false will run to the third
    m_funcalled = false;
}

void Envelope::Info(void) {
    printf("%s, Segments: %d, SCnt: %d, RCnt: %d, Stop: %s, Repeat: %s, Finished: %d\n\r",
        m_dac,
        m_segments.size(),
        m_segcnt,
        m_repeats,
        m_stopflag,
        m_repeat,
        Finished()
    );
}

void Envelope::Add(int32_t begin, int32_t end,
                   int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction) {
    Segment seg = Segment(this, segfunction);
    seg.Calc(begin, end, intervals, hold=hold);
    if (segment == -1) {
        m_segments.push_back(seg);
    } else {
        m_segments.insert(m_segments.begin()+segment, seg);
    }
}

void Envelope::Add(int8_t begoctave, int8_t beghalfstep,
                   int8_t endoctave, int8_t endhalfstep,
                   int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction) {
    Segment seg = Segment(this, segfunction);
    // this will throw error if dac doesn't have a VCO
    seg.Calc(m_dac->GetVCO()->Dinh(begoctave, beghalfstep),
             m_dac->GetVCO()->Dinh(endoctave, endhalfstep),
             intervals, hold=hold);
    if (segment == -1) {
        m_segments.push_back(seg);
    } else {
        m_segments.insert(m_segments.begin()+segment, seg);
    }
}

void Envelope::Replace(int32_t begin, int32_t end, int16_t intervals, int16_t hold, int16_t segment, segfunctype segfunction) { // replaces a line segment with new values
    Segment seg = Segment(this, segfunction);
    seg.Calc(begin, end, intervals, hold=hold);
    m_segments[segment] = seg;
    Restart();
}

bool Envelope::Finished(void) {
    return m_segcnt >= m_segments.size();
}

void Envelope::Stop(void) {
    m_stopflag = true;
}

void Envelope::Restart(void) {
    if (m_begfunction) m_begfunction();
    for (unsigned int i = 0; i<m_segments.size(); i++) m_segments[i].Start();
    m_segcnt = 0;
    m_repeats += 1;
    m_stopflag = false;
    m_funcalled = false;
}

bool Envelope::Next(void) { // step through all the line segments, return true when finished
    if (!m_stopflag) {
        if (!Finished() && m_segments[m_segcnt].Next()) m_segcnt += 1;
        if (Finished()) {
            if (m_repeat) {
                Restart();
            } else {
                if (m_endfunction && !m_funcalled && !m_attack) {
                    m_funcalled = true;
                    m_endfunction();
                }
            }
            return true;
        }
    }
    return false;
}

void Envelope::Dump(void) {
    for (unsigned int i = 0; i<m_segments.size(); i++) {
        printf("**** Segment number %d ", i);
        m_segments[i].Dump();
    }
}

Adsr::Adsr(LTC2668 *dac, bool repeat, bool printit, begfunctype begfunction, endfunctype endfunction)
   : Envelope(dac, repeat, printit, begfunction, endfunction) {
}

bool Adsr::Finished(void) {
    if (m_attack) {
       return m_segcnt >= (2<m_segments.size()?2:m_segments.size());
    } else {
       return m_segcnt >= m_segments.size();
    }
}

void Adsr::Start(void) {
    m_segcnt = 0;      // when the last point of a line is reached segcnt is incremented to the next Segment
    m_repeats = 0;     // number of times all line segments have repeated
    m_stopflag = false;// when true stop Next from moving thru segments
    m_attack = true;   // if true will run to the second segment, if false will run to the third
    m_funcalled = false;
}

void Adsr::Restart(void) {
    if (m_begfunction) m_begfunction();
    m_attack = true;   // if true will run to the second segment, if false will run to the third
    for (unsigned int i = 0; i<m_segments.size(); i++) m_segments[i].Start();
    m_segcnt = 0;
    m_repeats += 1;
    m_stopflag = false;
    m_funcalled = false;
}

void Adsr::Release(bool wait) {
    while(wait) {
        wait_us(50);
        if (m_segcnt == 2) break;
    }
    m_attack = false; // if true will run to the second segment, if false will run to the third
}

Envelope envtst(LTC2668 *dac, bool repeat, bool printit) {
   Envelope env = Envelope(dac=dac, repeat=repeat, printit=printit);
   env.Add(3000, 30000, 10);
   env.Add(20000, 2000, 5);
   env.Dump();
   return env;
}

