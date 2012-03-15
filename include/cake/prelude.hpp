#include "prelude/common.hpp"
#include "prelude/pairwise.hpp"
#include "prelude/style.hpp"

/* HACK: we *ensure* that sys/types.h has been included
 * (because it defines some stuff we will need, and if it gets
 * included implicitly later, we won't be able to protect ourselves
 * in the way that we are doing here)... */
#include <sys/types.h>
/* ... then we undo the damage some implementations do to
 * the namespace. Yes, NetBSD. */

#ifdef int8_t
#undef int8_t
#endif
#ifdef uint8_t
#undef uint8_t
#endif
#ifdef int16_t
#undef int16_t
#endif
#ifdef uint16_t
#undef uint16_t
#endif
#ifdef int32_t
#undef int32_t
#endif
#ifdef uint32_t
#undef uint32_t
#endif
#ifdef int64_t
#undef int64_t
#endif
#ifdef uint64_t
#undef uint64_t
#endif
#ifdef in_addr_t
#undef in_addr_t
#endif
#ifdef in_port_t
#undef in_port_t
#endif

#ifdef fsblkcnt_t
#undef fsblkcnt_t
#endif
#ifdef fsfilcnt_t
#undef fsfilcnt_t
#endif
#ifdef caddr_t
#undef caddr_t
#endif
#ifdef gid_t
#undef gid_t
#endif
#ifdef mode_t
#undef mode_t
#endif
#ifdef off_t
#undef off_t
#endif
#ifdef pid_t
#undef pid_t
#endif
#ifdef uid_t
#undef uid_t
#endif
#ifdef intptr_t
#undef intptr_t
#endif
#ifdef uintptr_t
#undef uintptr_t
#endif
