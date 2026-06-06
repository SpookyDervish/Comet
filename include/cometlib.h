#ifndef COMETLIB_H
#define COMETLIB_H

#include "comet_operand.h"
#include <stdint.h>
#include <stdbool.h>

#define on_import void onImport()

CometOperand cometValue(CometValueTypeKind valueType, ...);

#endif