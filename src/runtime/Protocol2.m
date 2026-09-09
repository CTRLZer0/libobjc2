/*
 * SPDX-License-Identifier: MIT AND AGPL-3.0-only
 * Original libobjc2 portions: MIT; CTRLZer0 modifications: AGPL-3.0-only.
 * Copyright (C) 2026 CTRLZer0 contributors for CTRLZer0 modifications.
 * Original upstream copyright and attribution remain under COPYING and the
 * preserved source / repository history. See LICENSE-CTRLZERO and NOTICE.md.
 */

#include "objc/runtime.h"
#include "protocol.h"
#include "class.h"
#include <stdio.h>
#include <string.h>

PRIVATE void objc_protocol2_link_anchor(void)
{
}

@implementation Protocol
// FIXME: This needs removing, but it's included for now because GNUstep's
// implementation of +[NSObject conformsToProtocol:] calls it.
- (BOOL)conformsTo: (Protocol*)p
{
	return protocol_conformsToProtocol(self, p);
}
- (id)retain
{
	return self;
}
- (void)release {}
+ (Class)class { return self; }
- (id)self { return self; }
@end
@implementation Protocol2 @end
@interface __IncompleteProtocol : Protocol2 @end
@implementation __IncompleteProtocol @end

/**
 * This class exists for the sole reason that the legacy GNU ABI did not
 * provide a way of registering protocols with the runtime.  With the new ABI,
 * every protocol in a compilation unit that is not referenced should be added
 * in a category on this class.  This ensures that the runtime sees every
 * protocol at least once and can perform uniquing.
 */
__attribute__((objc_root_class))
@interface __ObjC_Protocol_Holder_Ugly_Hack { id isa; } @end
@implementation __ObjC_Protocol_Holder_Ugly_Hack @end

@implementation Object @end
