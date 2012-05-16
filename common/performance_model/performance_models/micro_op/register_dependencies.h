#ifndef __REGISTER_DEPENDENCIES_H
#define __REGISTER_DEPENDENCIES_H

#include "fixed_types.h"

#include <xed-reg-enum.h>

class DynamicMicroOp;

class RegisterDependencies {
private:
    // Array containing the sequence number of the producers for each of the registers.
  uint64_t producers[XED_REG_LAST];
public:
  RegisterDependencies();

  void setDependencies(DynamicMicroOp& microOp, uint64_t lowestValidSequenceNumber);

  void clear();
};

#endif /* __REGISTER_DEPENDENCIES_H */
