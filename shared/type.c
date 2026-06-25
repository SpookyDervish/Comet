#include "../include/type.h"

CometType cometTypeSmall  = (CometType){.typeKind = COMET_SMALL };
CometType cometTypeInt    = (CometType){.typeKind = COMET_INT   };
CometType cometTypeBig    = (CometType){.typeKind = COMET_BIG   };
CometType cometTypeFloat  = (CometType){.typeKind = COMET_FLOAT };
CometType cometTypeDouble = (CometType){.typeKind = COMET_DOUBLE};
CometType cometTypeBool   = (CometType){.typeKind = COMET_BOOL  };
CometType cometTypeVoid   = (CometType){.typeKind = COMET_VOID  };

static CometArrayType stringArray = {
    .elem = &cometTypeSmall,
    .isFixedSize = {false},
    .dims = 1
};

CometType cometTypeString = {
    .typeKind = COMET_ARRAY,
    .arrayType = &stringArray
};