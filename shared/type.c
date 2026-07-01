#include "../include/type.h"
#include "../include/struct.h"
#include <string.h>

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

bool typesAreEqual(CometType a, CometType b) {
    if (a.typeKind != b.typeKind) {
        return false;
    }

    if (a.typeKind == COMET_GENERIC) {
        return strcmp(a.genericParamName, b.genericParamName) == 0;
    }

    if (a.typeKind == COMET_STRUCT) {
        if (a.structType->parent != NULL) {
            CometType parentType = {
                .typeKind = COMET_STRUCT,
                .structType = a.structType->parent
            };

            return typesAreEqual(parentType, b);
        }

        return a.structType == b.structType;
    }

    if (a.typeKind == COMET_ARRAY) {
        
        if (a.arrayType->dims != b.arrayType->dims)
            return false;

        for (size_t i = 0; i < a.arrayType->dims; i++) {

            if (!(typesAreEqual(*a.arrayType->elem, *b.arrayType->elem))) 
                return false;

            if (a.arrayType->isFixedSize[i] != b.arrayType->isFixedSize[i]) 
                return false;

            if (!a.arrayType->isFixedSize[i])
                return true;

            if (a.arrayType->fixedSize[i] != b.arrayType->fixedSize[i]) 
                return false;
            
        }
    }

    return true;
}