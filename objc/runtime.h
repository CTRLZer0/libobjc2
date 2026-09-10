#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif
#include "runtime-types.h"

#ifndef __LIBOBJC_RUNTIME_H_INCLUDED__
#define __LIBOBJC_RUNTIME_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

#include "slot.h"
#include "message.h"
#include "runtime-dispatch.h"
#include "runtime-small-object.h"
#include "runtime-block.h"
#include "runtime-selector.h"
#include "runtime-class.h"
#include "runtime-object.h"
#include "runtime-ivar.h"
#include "runtime-method.h"


#include "runtime-protocol.h"

#include "runtime-property.h"

#include "runtime-association.h"

/**
 * Toggles whether Objective-C objects caught in C++ exception handlers in
 * Objective-C++ mode should follow Objective-C or C++ semantics.  The obvious
 * choice is for them to follow C++ semantics, because people using a C++
 * language construct would intuitively expect them to have C++ semantics,
 * where the catch behaviour depends on the static type of the thrown object,
 * not its run-time type.
 *
 * Apple, therefore, chose the other option.
 *
 * We default to Apple-compatible mode, but can enable the sane behaviour if
 * the user opts in.  Note that doing this when linking against third-party
 * frameworks written in Objective-C++ 2 may cause weird problems if the expect
 * the other behaviour.
 *
 * This currently sets a global value.  In the future, it may be configurable
 * on a per-thread basis.
 */
OBJC_PUBLIC
int objc_set_apple_compatible_objcxx_exceptions(int newValue) OBJC_NONPORTABLE;

/** 
 * This function is inserted by the compiler when a mutation is detected during
 * a foreach iteration. It is exported as a weak symbol to enable GNUstep or
 * some other framework to replace it trivially.
 */
OBJC_PUBLIC
void __attribute__((weak)) objc_enumerationMutation(id obj);

/**
 * Ensure that `+initialize` has been sent to the class of the argument (or the
 * argument, if it is a class).  This will not call `+initialize` if it has
 * been called already, either via an explicit call to this function or by
 * being sent some other message.
 */
OBJC_PUBLIC
void objc_send_initialize(id object) OBJC_NONPORTABLE;

#define _C_ID       '@'
#define _C_CLASS    '#'
#define _C_SEL      ':'
#define _C_BOOL     'B'

#define _C_CHR      'c'
#define _C_UCHR     'C'
#define _C_SHT      's'
#define _C_USHT     'S'
#define _C_INT      'i'
#define _C_UINT     'I'
#define _C_LNG      'l'
#define _C_ULNG     'L'
#define _C_LNG_LNG  'q'
#define _C_ULNG_LNG 'Q'

#define _C_FLT      'f'
#define _C_DBL      'd'

#define _C_BFLD     'b'
#define _C_VOID     'v'
#define _C_UNDEF    '?'
#define _C_PTR      '^'

#define _C_CHARPTR  '*'
#define _C_ATOM     '%'

#define _C_ARY_B    '['
#define _C_ARY_E    ']'
#define _C_UNION_B  '('
#define _C_UNION_E  ')'
#define _C_STRUCT_B '{'
#define _C_STRUCT_E '}'
#define _C_VECTOR   '!'

#define _C_COMPLEX  'j'
#define _C_CONST    'r'
#define _C_IN       'n'
#define _C_INOUT    'N'
#define _C_OUT      'o'
#define _C_BYCOPY   'O'
#define _C_ONEWAY   'V'

#include "runtime-deprecated.h"

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_H_INCLUDED__
