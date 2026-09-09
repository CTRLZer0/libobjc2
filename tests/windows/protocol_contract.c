/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#include "test_support.h"
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
    Class protocolClass = (Class)objc_getClass("Protocol");
    Class incompleteProtocolClass = (Class)objc_getClass("__IncompleteProtocol");
    CHECK(protocolClass != Nil && incompleteProtocolClass != Nil);

    Protocol *base = objc_allocateProtocol("MosaicProtocolContractBase");
    CHECK(base != NULL);
    CHECK(object_getClass((id)base) == incompleteProtocolClass);
    SEL ping = sel_registerName("mosaicProtocolPing:");
    SEL pong = sel_registerName("mosaicProtocolPong:");
    SEL optional = sel_registerName("mosaicProtocolOptional");
    CHECK(ping != NULL && pong != NULL && optional != NULL);

    protocol_addMethodDescription(base, ping, "v@:@", YES, YES);
    protocol_addMethodDescription(base, pong, "v@:@", YES, YES);
    protocol_addMethodDescription(base, optional, "v@:", NO, YES);

    objc_property_attribute_t object_attr[] = {{"T", "@"}};
    protocol_addProperty(base, "firstProperty", object_attr, 1, YES, YES);
    protocol_addProperty(base, "secondProperty", object_attr, 1, YES, YES);
    protocol_addProperty(base, "optionalProperty", object_attr, 1, NO, YES);
    objc_registerProtocol(base);

    CHECK(objc_getProtocol("MosaicProtocolContractBase") == base);
    CHECK(object_getClass((id)base) == protocolClass);
    CHECK(strcmp(protocol_getName(base), "MosaicProtocolContractBase") == 0);

    struct objc_method_description description =
        protocol_getMethodDescription(base, pong, YES, YES);
    CHECK(description.name == pong);
    CHECK(strcmp(description.types, "v@:@") == 0);
    description = protocol_getMethodDescription(base, optional, NO, YES);
    CHECK(description.name == optional);

    unsigned int method_count = 0;
    struct objc_method_description *methods =
        protocol_copyMethodDescriptionList(base, YES, YES, &method_count);
    CHECK(method_count == 2 && methods != NULL);
    free(methods);

    unsigned int property_count = 0;
    objc_property_t *properties = protocol_copyPropertyList(base, &property_count);
    CHECK(property_count == 2 && properties != NULL);
    CHECK(property_list_contains(properties, property_count, "firstProperty"));
    CHECK(property_list_contains(properties, property_count, "secondProperty"));
    free(properties);
    properties = protocol_copyPropertyList2(base, &property_count, NO, YES);
    CHECK(property_count == 1 && properties != NULL);
    CHECK(property_list_contains(properties, property_count, "optionalProperty"));
    free(properties);
    CHECK(protocol_getProperty(base, "secondProperty", YES, YES) != NULL);
    CHECK(protocol_getProperty(base, "optionalProperty", NO, YES) != NULL);
    CHECK(protocol_getProperty(base, NULL, YES, YES) == NULL);

    unsigned int empty_count = 17;
    CHECK(protocol_copyMethodDescriptionList(NULL, YES, YES, &empty_count) == NULL);
    CHECK(empty_count == 0);
    empty_count = 17;
    CHECK(protocol_copyProtocolList(NULL, &empty_count) == NULL);
    CHECK(empty_count == 0);
    empty_count = 17;
    CHECK(protocol_copyPropertyList(NULL, &empty_count) == NULL);
    CHECK(empty_count == 0);
    CHECK(objc_allocateProtocol(NULL) == NULL);

    Protocol *adopted_a = objc_allocateProtocol("MosaicProtocolContractAdoptedA");
    Protocol *adopted_b = objc_allocateProtocol("MosaicProtocolContractAdoptedB");
    CHECK(adopted_a != NULL && adopted_b != NULL);
    objc_registerProtocol(adopted_a);
    objc_registerProtocol(adopted_b);

    Protocol *child = objc_allocateProtocol("MosaicProtocolContractChild");
    CHECK(child != NULL);
    protocol_addProtocol(child, base);
    protocol_addProtocol(child, adopted_a);
    protocol_addProtocol(child, adopted_b);

    unsigned int adopted_count = 0;
    Protocol **adopted = protocol_copyProtocolList(child, &adopted_count);
    CHECK(adopted_count == 3 && adopted != NULL);
    CHECK(protocol_list_contains(adopted, adopted_count, base));
    CHECK(protocol_list_contains(adopted, adopted_count, adopted_a));
    CHECK(protocol_list_contains(adopted, adopted_count, adopted_b));
    free(adopted);

    objc_registerProtocol(child);
    CHECK(object_getClass((id)child) == protocolClass);
    CHECK(protocol_conformsToProtocol(child, base) == YES);
    CHECK(protocol_conformsToProtocol(child, adopted_a) == YES);
    CHECK(protocol_conformsToProtocol(child, adopted_b) == YES);
    CHECK(protocol_isEqual(base, base) == YES);
    CHECK(protocol_isEqual(base, child) == NO);

    unsigned int known_count = 0;
    Protocol **known = objc_copyProtocolList(&known_count);
    CHECK(known != NULL && known_count >= 4);
    CHECK(protocol_list_contains(known, known_count, base));
    CHECK(protocol_list_contains(known, known_count, child));
    CHECK(protocol_list_contains(known, known_count, adopted_a));
    CHECK(protocol_list_contains(known, known_count, adopted_b));
    free(known);

    SEL late = sel_registerName("mosaicProtocolLateMutation");
    protocol_addMethodDescription(base, late, "v@:", YES, YES);
    description = protocol_getMethodDescription(base, late, YES, YES);
    CHECK(description.name == NULL);
    CHECK(objc_allocateProtocol("MosaicProtocolContractBase") == NULL);
    return 0;
}
