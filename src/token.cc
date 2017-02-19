#include <string.h>
#include "token.h"

namespace mio {

const char *const kTokenName2Text[] = {
#define TokenName_GLOBAL_DEFINE(name, proi, txt) txt,
    DEFINE_TOKENS(TokenName_GLOBAL_DEFINE)
#undef  TokenName_GLOBAL_DEFINE
};

TokenObject::TokenObject()
    : token_code_(TOKEN_ERROR)
    , position_(-1)
    , len_(-1) {
    memset(&data_, 0, sizeof(data_));
}

TokenObject::~TokenObject() {

}

} // namespace mio
