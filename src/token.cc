#include "token.h"
#include "glog/logging.h"
#include <string.h>

namespace mio {

const TokenMetadata kTokenMetadata[] {
#define TokenMetadata_GLOBAL_DEFINE(token, proi, txt) \
    { .code = TOKEN_##token, .name = #token, .op_priority = proi, .text = txt, },
    DEFINE_TOKENS(TokenMetadata_GLOBAL_DEFINE)
#undef TokenMetadata_GLOBAL_DEFINE
    { // End of TokenMetadata
        .code = TOKEN_ERROR,
        .name = nullptr,
        .op_priority = 0,
        .text = nullptr
    },
};

int TokenOpPriority(Token token, bool *ok) {
    auto metadata = &kTokenMetadata[static_cast<int>(token)];
    // TODO:
    return metadata->op_priority;
}

std::string TokenNameWithText(Token token) {
    auto metadata = &kTokenMetadata[static_cast<int>(token)];

    if (metadata->text[0] == '\0') {
        return std::string(metadata->name);
    }

    char buf[128];
    snprintf(buf, arraysize(buf), "%s `%s\'", metadata->name, metadata->text);
    return std::string(buf);
}

TokenObject::TokenObject()
    : token_code_(TOKEN_ERROR)
    , position_(-1)
    , len_(-1) {
    memset(&data_, 0, sizeof(data_));
}

TokenObject::~TokenObject() {

}

} // namespace mio
