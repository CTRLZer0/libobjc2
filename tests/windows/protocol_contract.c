/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
 * and provenance details.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"

static int protocol_list_contains(Protocol **list, unsigned int count,
                                  Protocol *target)
{
    for (unsigned int i = 0; i < count; i++)
    {
        if (list[i] == target) { return 1; }
    }
    return 0;
}

static int property_list_contains(objc_property_t *list, unsigned int count,
                                  const char *name)
{
    for (unsigned int i = 0; i < count; i++)
    {
        if (strcmp(property_getName(list[i]), name) == 0) { return 1; }
    }
    return 0;
}

int main(void)
{
    mosaic_objc_runtime_initialize();
    Class protocol2 = (Class)objc_getClass("Protocol2");
    assert(protocol2 != Nil);

    Protocol *base = objc_allocateProtocol("MosaicProtocolContractBase");
    assert(base != NULL);
    SEL ping = sel_registerName("mosaicProtocolPing:");
    SEL pong = sel_registerName("mosaicProtocolPong:");
    SEL optional = sel_registerName("mosaicProtocolOptional");
    assert(ping != NULL && pong != NULL && optional != NULL);

    protocol_addMethodDescription(base, ping, "v@:@", YES, YES);
    protocol_addMethodDescription(base, pong, "v@:@", YES, YES);
    protocol_addMethodDescription(base, optional, "v@:", NO, YES);

    objc_property_attribute_t object_attr[] = {{"T", "@"}};
    protocol_addProperty(base, "firstProperty", object_attr, 1, YES, YES);
    protocol_addProperty(base, "secondProperty", object_attr, 1, YES, YES);
    protocol_addProperty(base, "optionalProperty", object_attr, 1, NO, YES);
    objc_registerProtocol(base);

    assert(objc_getProtocol("MosaicProtocolContractBase") == base);
    assert(object_getClass((id)base) == protocol2);
    assert(strcmp(protocol_getName(base), "MosaicProtocolContractBase") == 0);

    struct objc_method_description description =
        protocol_getMethodDescription(base, pong, YES, YES);
    assert(description.name == pong);
    assert(strcmp(description.types, "v@:@") == 0);
    description = protocol_getMethodDescription(base, optional, NO, YES);
    assert(description.name == optional);

    unsigned int method_count = 0;
    struct objc_method_description *methods =
        protocol_copyMethodDescriptionList(base, YES, YES, &method_count);
    assert(method_count == 2 && methods != NULL);
    free(methods);

    unsigned int property_count = 0;
    objc_property_t *properties = protocol_copyPropertyList(base, &property_count);
    assert(property_count == 3 && properties != NULL);
    assert(property_list_contains(properties, property_count, "firstProperty"));
    assert(property_list_contains(properties, property_count, "secondProperty"));
    assert(property_list_contains(properties, property_count, "optionalProperty"));
    free(properties);
    assert(protocol_getProperty(base, "secondProperty", YES, YES) != NULL);
    assert(protocol_getProperty(base, "optionalProperty", NO, YES) != NULL);
    assert(protocol_getProperty(base, NULL, YES, YES) == NULL);

    unsigned int empty_count = 17;
    assert(protocol_copyMethodDescriptionList(NULL, YES, YES, &empty_count) == NULL);
    assert(empty_count == 0);
    empty_count = 17;
    assert(protocol_copyProtocolList(NULL, &empty_count) == NULL);
    assert(empty_count == 0);
    empty_count = 17;
    assert(protocol_copyPropertyList(NULL, &empty_count) == NULL);
    assert(empty_count == 0);
    assert(objc_allocateProtocol(NULL) == NULL);

    Protocol *adopted_a = objc_allocateProtocol("MosaicProtocolContractAdoptedA");
    Protocol *adopted_b = objc_allocateProtocol("MosaicProtocolContractAdoptedB");
    assert(adopted_a != NULL && adopted_b != NULL);
    objc_registerProtocol(adopted_a);
    objc_registerProtocol(adopted_b);

    Protocol *child = objc_allocateProtocol("MosaicProtocolContractChild");
    assert(child != NULL);
    protocol_addProtocol(child, base);
    protocol_addProtocol(child, adopted_a);
    protocol_addProtocol(child, adopted_b);

    unsigned int adopted_count = 0;
    Protocol **adopted = protocol_copyProtocolList(child, &adopted_count);
    assert(adopted_count == 3 && adopted != NULL);
    assert(protocol_list_contains(adopted, adopted_count, base));
    assert(protocol_list_contains(adopted, adopted_count, adopted_a));
    assert(protocol_list_contains(adopted, adopted_count, adopted_b));
    free(adopted);

    objc_registerProtocol(child);
    assert(object_getClass((id)child) == protocol2);
    assert(protocol_conformsToProtocol(child, base) == YES);
    assert(protocol_conformsToProtocol(child, adopted_a) == YES);
    assert(protocol_conformsToProtocol(child, adopted_b) == YES);
    assert(protocol_isEqual(base, base) == YES);
    assert(protocol_isEqual(base, child) == NO);

    unsigned int known_count = 0;
    Protocol **known = objc_copyProtocolList(&known_count);
    assert(known != NULL && known_count >= 4);
    assert(protocol_list_contains(known, known_count, base));
    assert(protocol_list_contains(known, known_count, child));
    assert(protocol_list_contains(known, known_count, adopted_a));
    assert(protocol_list_contains(known, known_count, adopted_b));
    free(known);

    SEL late = sel_registerName("mosaicProtocolLateMutation");
    protocol_addMethodDescription(base, late, "v@:", YES, YES);
    description = protocol_getMethodDescription(base, late, YES, YES);
    assert(description.name == NULL);
    assert(objc_allocateProtocol("MosaicProtocolContractBase") == NULL);
    return 0;
}
