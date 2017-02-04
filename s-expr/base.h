#ifndef MIO_S_EXPR_BASE_H_
#define MIO_S_EXPR_BASE_H_

#include "glog/logging.h"

namespace mio {

#define DISALLOW_IMPLICT_CONSTRUCTORS(clazz_name)   \
    clazz_name (const clazz_name &) = delete;      \
    clazz_name (clazz_name &&) = delete;           \
    void operator = (const clazz_name &) = delete;

#define NOREACHED()  LOG(FATAL) << "no reached! "
#define DNOREACHED() DLOG(FATAL) << "no reached! "

} // namespace mio


#endif // MIO_S_EXPR_BASE_H_


