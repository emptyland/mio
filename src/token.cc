#include <string.h>
#include "token.h"

namespace mio {

TokenObject::TokenObject()
    : token_code_(TOKEN_ERROR)
    , position_(-1)
    , len_(-1) {
    memset(&data_, 0, sizeof(data_));
}

TokenObject::~TokenObject() {

}

} // namespace mio
