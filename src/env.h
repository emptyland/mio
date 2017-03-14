#ifndef MIO_ENV_H_
#define MIO_ENV_H_

#include "base.h"

namespace mio {

class SimpleFileSystem;

class Env {
public:
    Env() = default;
    virtual ~Env() = default;

    virtual SimpleFileSystem *GetSFS() = 0;

}; // class Env

} // namespace mio

#endif // MIO_ENV_H_
