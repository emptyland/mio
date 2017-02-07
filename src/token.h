#ifndef MIO_TOKEN_H_
#define MIO_TOKEN_H_


#include "token-inl.h"
#include "base.h"
#include <string>

namespace mio {

#define TokenObject_PRIMITIVE_DATA_PROPERTY(name, type, size, stuffix) \
    TokenObject_PRIMITIVE_DATA_SETTER(name)                            \
    TokenObject_PRIMITIVE_DATA_GETTER(name)

#define TokenObject_PRIMITIVE_DATA_SETTER(name) \
    inline void set_##name##_data(mio_##name##_t data) { \
        data_.as_##name = data;                          \
    }
#define TokenObject_PRIMITIVE_DATA_GETTER(name) \
    inline mio_##name##_t name##_data() const { \
        return data_.as_##name; \
    }

#define TokenObject_PRIMITIVE_DATA_UNION(name, type, size, stuffix) \
    mio_##name##_t as_##name;

class TokenObject {
public:
    TokenObject();
    ~TokenObject();

    DEF_PROP_RW(Token, token_code);
    DEF_PROP_RW(int, position);
    DEF_PROP_RW(int, len);
    DEF_PROP_RMW(std::string, text);

    DEFINE_PRIMITIVE_TYPES(TokenObject_PRIMITIVE_DATA_PROPERTY)

private:
    Token token_code_;
    int   position_;
    int   len_;

    union {
        DEFINE_PRIMITIVE_TYPES(TokenObject_PRIMITIVE_DATA_UNION)
    } data_;
    std::string text_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(TokenObject);
};

#undef TokenObject_PRIMITIVE_DATA_PROPERTY
#undef TokenObject_PRIMITIVE_DATA_SETTER
#undef TokenObject_PRIMITIVE_DATA_GETTER
#undef TokenObject_PRIMITIVE_DATA_UNION

} // namespace mio

#endif // MIO_TOKEN_H_
