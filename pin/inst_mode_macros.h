#ifndef __INST_MODE_MACROS_H
#define __INST_MODE_MACROS_H

#include "pin.H"

#define INSTR_IF_DETAILED_CONDITION         (Sim()->getInstrumentationMode() == InstMode::DETAILED)
#define INSTR_IF_NOT_DETAILED_CONDITION     (Sim()->getInstrumentationMode() != InstMode::DETAILED)
#define INSTR_IF_DETAILED_OR_FULL_CONDITION (Sim()->getInstrumentationMode() == InstMode::DETAILED || Sim()->getConfig()->getSimulationMode() == Config::FULL)
#define INSTR_IF_CACHEONLY_CONDITION        (Sim()->getInstrumentationMode() == InstMode::CACHE_ONLY)
#define INSTR_IF_FASTFORWARD_CONDITION      (Sim()->getInstrumentationMode() == InstMode::FAST_FORWARD)
#define INSTR_IF_NOT_FASTFORWARD_CONDITION  (Sim()->getInstrumentationMode() != InstMode::FAST_FORWARD)

#define __INSTRUMENT(predicated, condition, trace, ins, point, func, ...)      \
   if (condition##_CONDITION)                                        \
      INS_Insert##predicated##Call(ins, point, func, __VA_ARGS__);   \

#define INSTRUMENT(...)            __INSTRUMENT(, __VA_ARGS__)
#define INSTRUMENT_PREDICATED(...) __INSTRUMENT(Predicated, __VA_ARGS__)

#endif // __INST_MODE_MACROS_H
