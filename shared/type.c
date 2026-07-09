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

#if INTPTR_MAX == INT64_MAX
    // 64-bit pointers
    CometType cometTypePointer = (CometType){.typeKind = COMET_BIG };
#elif INTPTR_MAX == INT32_MAX
    // 32-bit pointers
    CometType cometTypePointer = (CometType){.typeKind = COMET_INT };
#else
    #error "Unsupported pointer size"
#endif

static CometArrayType stringArray = {
    .elem = &cometTypeSmall,
    .isFixedSize = {false},
    .dims = 1
};

CometType cometTypeString = {
    .typeKind = COMET_ARRAY,
    .arrayType = &stringArray
};

bool typesAreEqual(CometType child, CometType parent) {
    if (child.typeKind != parent.typeKind) {
        return false;
    }

    if (child.typeKind == COMET_GENERIC) {
        return strcmp(child.genericParamName, parent.genericParamName) == 0;
    }

    if (child.typeKind == COMET_STRUCT) {
        // is "child" child child of "parent"
        if (child.structType != parent.structType && child.structType->parent != NULL) {
            CometType parentType = {
                .typeKind = COMET_STRUCT,
                .structType = child.structType->parent
            };

            return typesAreEqual(parentType, parent);
        }

        return child.structType == parent.structType;
    }

    if (child.typeKind == COMET_ARRAY) {
        
        if (child.arrayType->dims != parent.arrayType->dims)
            return false;

        for (size_t i = 0; i < child.arrayType->dims; i++) {

            if (!(typesAreEqual(*child.arrayType->elem, *parent.arrayType->elem))) 
                return false;

            if (!parent.arrayType->isFixedSize[i])
                continue;

            if (child.arrayType->fixedSize[i] != parent.arrayType->fixedSize[i]) 
                return false;
            
        }
    }

    return true;
}