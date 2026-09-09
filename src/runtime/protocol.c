/*
 * SPDX-License-Identifier: MIT
 * CTRLZer0 modifications are licensed under the same MIT terms as libobjc2.
 * Copyright (C) 2026 CTRLZer0 contributors for CTRLZer0 modifications.
 * Original upstream copyright and attribution remain under COPYING and the
 * preserved source / repository history. See COPYING and NOTICE.md.
 */

#include "objc/runtime.h"
#include "crt_compat.h"
#include "protocol.h"
#include "properties.h"
#include "class.h"
#include "lock.h"
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#define BUFFER_TYPE struct objc_protocol_list
#include "buffer.h"

// Get the functions for string hashing
#include "string_hash.h"

static int protocol_compare(const char *name,
                            const struct objc_protocol2 *protocol)
{
	return string_compare(name, protocol->name);
}
static int protocol_hash(const struct objc_protocol2 *protocol)
{
	return string_hash(protocol->name);
}
#define MAP_TABLE_NAME protocol
#define MAP_TABLE_COMPARE_FUNCTION protocol_compare
#define MAP_TABLE_HASH_KEY string_hash
#define MAP_TABLE_HASH_VALUE protocol_hash
#include "hash_table.h"

PRIVATE void objc_protocol2_link_anchor(void);

static protocol_table *known_protocol_table;

static void *resize_tail_array(void *storage, size_t header_size,
                               size_t element_size, size_t count)
{
	if (count > (SIZE_MAX - header_size) / element_size) { return NULL; }
	size_t bytes = header_size + element_size * count;
	return storage ? realloc(storage, bytes) : calloc(1, bytes);
}

void init_protocol_table(void)
{
	objc_protocol2_link_anchor();
	protocol_initialize(&known_protocol_table, 128);
}

static void protocol_table_insert(const struct objc_protocol2 *protocol)
{
	protocol_insert(known_protocol_table, (void*)protocol);
}

struct objc_protocol2 *protocol_for_name(const char *name)
{
	return protocol_table_get(known_protocol_table, name);
}

static id ObjC2ProtocolClass = nil;

static id protocol2Class(void)
{
	if (ObjC2ProtocolClass == nil)
	{
		ObjC2ProtocolClass = objc_getClass("Protocol2");
	}
	return ObjC2ProtocolClass;
}

static id incompleteProtocolClass(void)
{
	static id IncompleteProtocolClass = nil;
	if (IncompleteProtocolClass == nil)
	{
		IncompleteProtocolClass = objc_getClass("__IncompleteProtocol");
	}
	return IncompleteProtocolClass;
}


static int isEmptyProtocol(struct objc_protocol2 *aProto)
{
	int isEmpty =
		((aProto->instance_methods == NULL) ||
			(aProto->instance_methods->count == 0)) &&
		((aProto->class_methods == NULL) ||
			(aProto->class_methods->count == 0)) &&
		((aProto->protocol_list == NULL) ||
		  (aProto->protocol_list->count == 0));
	id protocol2 = protocol2Class();
	if (protocol2 != nil && aProto->isa == protocol2)
	{
		struct objc_protocol2 *p2 = (struct objc_protocol2*)aProto;
		isEmpty &= (p2->optional_instance_methods == NULL) ||
			(p2->optional_instance_methods->count == 0);
		isEmpty &= (p2->optional_class_methods == NULL) ||
			(p2->optional_class_methods->count == 0);
		isEmpty &= (p2->properties == NULL) || (p2->properties->count == 0);
		isEmpty &= (p2->optional_properties == NULL) ||
			(p2->optional_properties->count == 0);
	}
	return isEmpty;
}

// FIXME: Make p1 adopt all of the stuff in p2
static void makeProtocolEqualToProtocol(struct objc_protocol2 *p1,
                                        struct objc_protocol2 *p2)
{
#define COPY(x) p1->x = p2->x
	COPY(instance_methods);
	COPY(class_methods);
	COPY(protocol_list);
	id protocol2 = protocol2Class();
	if (protocol2 != nil && p1->isa == protocol2 && p2->isa == protocol2)
	{
		COPY(optional_instance_methods);
		COPY(optional_class_methods);
		COPY(properties);
		COPY(optional_properties);
	}
#undef COPY
}

static struct objc_protocol2 *unique_protocol(struct objc_protocol2 *aProto)
{
	protocol2Class();
	struct objc_protocol2 *oldProtocol =
		protocol_for_name(aProto->name);
	if (NULL == oldProtocol)
	{
		// This is the first time we've seen this protocol, so add it to the
		// hash table and ignore it.
		protocol_table_insert(aProto);
		return aProto;
	}
	if (isEmptyProtocol(oldProtocol))
	{
		if (isEmptyProtocol(aProto))
		{
			return aProto;
			// Add protocol to a list somehow.
		}
		else
		{
			// This protocol is not empty, so we use its definitions
			makeProtocolEqualToProtocol(oldProtocol, aProto);
			return aProto;
		}
	}
	else
	{
		if (isEmptyProtocol(aProto))
		{
			makeProtocolEqualToProtocol(aProto, oldProtocol);
			return oldProtocol;
		}
		else
		{
			return oldProtocol;
			//FIXME: We should really perform a check here to make sure the
			//protocols are actually the same.
		}
	}
}

static id protocol_class;
static id protocol_class2;
enum protocol_version
{
	/**
	 * Legacy (GCC-compatible) protocol version.
	 */
	protocol_version_legacy = 2,
	/**
	 * New (Objective-C 2-compatible) protocol version.
	 */
	protocol_version_objc2 = 3
};

static BOOL init_protocols(struct objc_protocol_list *protocols)
{
	// Protocol2 is a subclass of Protocol, so if we have loaded Protocol2 we
	// must have also loaded Protocol.
	if (nil == protocol_class2)
	{
		protocol_class = objc_getClass("Protocol");
		protocol_class2 = protocol2Class();
	}
	if (nil == protocol_class2 || nil == protocol_class)
	{
		return NO;
	}

	for (unsigned i=0 ; i<protocols->count ; i++)
	{
		struct objc_protocol2 *aProto = protocols->list[i];
		// Don't initialise a protocol twice
		if (aProto->isa == protocol_class ||
			aProto->isa == protocol_class2) { continue ;}

		// Protocols in the protocol list have their class pointers set to the
		// version of the protocol class that they expect.
		enum protocol_version version =
			(enum protocol_version)(uintptr_t)aProto->isa;
		switch (version)
		{
			default:
				fprintf(stderr, "Unknown protocol version");
				abort();
			case protocol_version_legacy:
				aProto->isa = protocol_class;
				break;
			case protocol_version_objc2:
				aProto->isa = protocol_class2;
				break;
		}
		// Initialize all of the protocols that this protocol refers to
		if (NULL != aProto->protocol_list)
		{
			init_protocols(aProto->protocol_list);
		}
		// Replace this protocol with a unique version of it.
		protocols->list[i] = unique_protocol(aProto);
	}
	return YES;
}

PRIVATE void objc_init_protocols(struct objc_protocol_list *protocols)
{
	if (!init_protocols(protocols))
	{
		set_buffered_object_at_index(protocols, buffered_objects++);
		return;
	}
	if (buffered_objects == 0) { return; }

	// If we can load one protocol, then we can load all of them.
	for (unsigned i=0 ; i<buffered_objects ; i++)
	{
		struct objc_protocol_list *c = buffered_object_at_index(i);
		if (NULL != c)
		{
			init_protocols(c);
			set_buffered_object_at_index(NULL, i);
		}
	}
	compact_buffer();
}

// Public functions:
Protocol *objc_getProtocol(const char *name)
{
	if (NULL == name) { return NULL; }
	return (Protocol*)protocol_for_name(name);
}

BOOL protocol_conformsToProtocol(Protocol *p1, Protocol *p2)
{
	if (NULL == p1 || NULL == p2) { return NO; }

	// A protocol trivially conforms to itself
	if (strcmp(p1->name, p2->name) == 0) { return YES; }

	for (struct objc_protocol_list *list = p1->protocol_list ;
		list != NULL ; list = list->next)
	{
		for (int i=0 ; i<list->count ; i++)
		{
			if (strcmp(list->list[i]->name, p2->name) == 0)
			{
				return YES;
			}
			if (protocol_conformsToProtocol((Protocol*)list->list[i], p2))
			{
				return YES;
			}
		}
	}
	return NO;
}

BOOL class_conformsToProtocol(Class cls, Protocol *protocol)
{
	if (Nil == cls || NULL == protocol) { return NO; }
	for ( ; Nil != cls ; cls = class_getSuperclass(cls))
	{
		for (struct objc_protocol_list *protocols = cls->protocols;
			protocols != NULL ; protocols = protocols->next)
		{
			for (int i=0 ; i<protocols->count ; i++)
			{
				Protocol *p1 = (Protocol*)protocols->list[i];
				if (protocol_conformsToProtocol(p1, protocol))
				{
					return YES;
				}
			}
		}
	}
	return NO;
}

static struct objc_method_description_list *
get_method_list(Protocol *p,
                BOOL isRequiredMethod,
                BOOL isInstanceMethod)
{
	id protocol2 = protocol2Class();
	struct objc_method_description_list *list;
	if (isRequiredMethod)
	{
		if (isInstanceMethod)
		{
			list = p->instance_methods;
		}
		else
		{
			list = p->class_methods;
		}
	}
	else
	{
		if (p->isa != protocol2) { return NULL; }


		if (isInstanceMethod)
		{
			list = ((Protocol2*)p)->optional_instance_methods;
		}
		else
		{
			list = ((Protocol2*)p)->optional_class_methods;
		}
	}
	return list;
}

struct objc_method_description *protocol_copyMethodDescriptionList(Protocol *p,
	BOOL isRequiredMethod, BOOL isInstanceMethod, unsigned int *count)
{
	if (NULL == count) { return NULL; }
	*count = 0;
	if (NULL == p) { return NULL; }
	struct objc_method_description_list *list =
		get_method_list(p, isRequiredMethod, isInstanceMethod);
	if (NULL == list || list->count <= 0) { return NULL; }

	struct objc_method_description *out =
		calloc((size_t)list->count, sizeof(*out));
	if (NULL == out) { return NULL; }
	for (int i=0 ; i < list->count ; i++)
	{
		out[i].name = sel_registerTypedName_np(list->methods[i].name,
		                                       list->methods[i].types);
		out[i].types = list->methods[i].types;
	}
	*count = (unsigned int)list->count;
	return out;
}

Protocol*__unsafe_unretained* protocol_copyProtocolList(Protocol *p, unsigned int *count)
{
	if (NULL == count) { return NULL; }
	*count = 0;
	if (NULL == p || NULL == p->protocol_list || p->protocol_list->count == 0 ||
		p->protocol_list->count > UINT_MAX)
	{
		return NULL;
	}

	size_t protocolCount = p->protocol_list->count;
	Protocol **out = calloc(protocolCount, sizeof(*out));
	if (NULL == out) { return NULL; }
	for (size_t i=0 ; i<protocolCount ; i++)
	{
		out[i] = (Protocol*)p->protocol_list->list[i];
	}
	*count = (unsigned int)protocolCount;
	return out;
}

objc_property_t *protocol_copyPropertyList(Protocol *protocol,
                                           unsigned int *outCount)
{
	if (NULL != outCount) { *outCount = 0; }
	if (NULL == protocol) { return NULL; }
	id protocol2 = protocol2Class();
	if (protocol2 == nil || protocol->isa != protocol2)
	{
		return NULL;
	}
	Protocol2 *p = (Protocol2*)protocol;
	struct objc_property_list *properties = p->properties;
	unsigned int count = 0;
	if (NULL != properties)
	{
		count = properties->count;
	}
	if (NULL != p->optional_properties)
	{
		count += p->optional_properties->count;
	}
	if (0 == count)
	{
		return NULL;
	}
	objc_property_t *list = calloc(count, sizeof(*list));
	if (NULL == list) { return NULL; }
	unsigned int out = 0;
	if (properties)
	{
		for (int i=0 ; i<properties->count ; i++)
		{
			list[out++] = &properties->properties[i];
		}
	}
	properties = p->optional_properties;
	if (properties)
	{
		for (int i=0 ; i<properties->count ; i++)
		{
			list[out++] = &properties->properties[i];
		}
	}
	if (NULL != outCount) { *outCount = count; }
	return list;
}

objc_property_t protocol_getProperty(Protocol *protocol,
                                     const char *name,
                                     BOOL isRequiredProperty,
                                     BOOL isInstanceProperty)
{
	if (NULL == protocol || NULL == name) { return NULL; }
	// Class properties are not supported yet (there is no language syntax for
	// defining them!)
	if (!isInstanceProperty) { return NULL; }
	id protocol2 = protocol2Class();
	if (protocol2 == nil || protocol->isa != protocol2)
	{
		return NULL;
	}
	Protocol2 *p = (Protocol2*)protocol;
	struct objc_property_list *properties =
	    isRequiredProperty ? p->properties : p->optional_properties;
	while (NULL != properties)
	{
		for (int i=0 ; i<properties->count ; i++)
		{
			objc_property_t prop = &properties->properties[i];
			if (strcmp(property_getName(prop), name) == 0)
			{
				return prop;
			}
		}
		properties = properties->next;
	}
	return NULL;
}


struct objc_method_description
protocol_getMethodDescription(Protocol *p,
                              SEL aSel,
                              BOOL isRequiredMethod,
                              BOOL isInstanceMethod)
{
	struct objc_method_description d = {0,0};
	if (NULL == p || NULL == aSel) { return d; }
	struct objc_method_description_list *list =
		get_method_list(p, isRequiredMethod, isInstanceMethod);
	if (NULL == list)
	{
		return d;
	}
	// TODO: We could make this much more efficient if
	for (int i=0 ; i<list->count ; i++)
	{
		SEL s = sel_registerTypedName_np(list->methods[i].name, 0);
		if (sel_isEqual(s, aSel))
		{
			d.name = s;
			d.types = list->methods[i].types;
			break;
		}
	}
	return d;
}


const char *protocol_getName(Protocol *p)
{
	if (NULL != p)
	{
		return p->name;
	}
	return NULL;
}

BOOL protocol_isEqual(Protocol *p, Protocol *other)
{
	if (NULL == p || NULL == other)
	{
		return NO;
	}
	if (p == other || p->name == other->name)
	{
		return YES;
	}
	if (NULL != p->name && NULL != other->name &&
		0 == strcmp(p->name, other->name))
	{
		return YES;
	}
	return NO;
}

Protocol*__unsafe_unretained* objc_copyProtocolList(unsigned int *outCount)
{
	if (NULL != outCount) { *outCount = 0; }
	if (NULL == known_protocol_table || known_protocol_table->table_used == 0)
	{
		return NULL;
	}
	if (known_protocol_table->table_used > UINT_MAX) { return NULL; }
	unsigned int total = (unsigned int)known_protocol_table->table_used;
	Protocol **p = calloc(total, sizeof(*p));
	if (NULL == p) { return NULL; }

	struct protocol_table_enumerator *e = NULL;
	Protocol *next;
	unsigned int count = 0;
	while ((count < total) && (next = protocol_next(known_protocol_table, &e)))
	{
		p[count++] = next;
	}
	if (NULL != outCount) { *outCount = count; }
	return p;
}


Protocol *objc_allocateProtocol(const char *name)
{
	if (NULL == name || objc_getProtocol(name) != NULL) { return NULL; }
	id incomplete = incompleteProtocolClass();
	if (incomplete == nil) { return NULL; }
	Protocol *p = (Protocol*)class_createInstance((Class)incomplete, 0);
	if (NULL == p) { return NULL; }
	p->name = objc2_strdup(name);
	if (NULL == p->name)
	{
		object_dispose((id)p);
		return NULL;
	}
	return p;
}
void objc_registerProtocol(Protocol *proto)
{
	if (NULL == proto || NULL == proto->name) { return; }
	LOCK_RUNTIME_FOR_SCOPE();
	if (objc_getProtocol(proto->name) != NULL) { return; }
	if (incompleteProtocolClass() != proto->isa) { return; }
	id protocol2 = protocol2Class();
	if (protocol2 == nil) { return; }
	proto->isa = protocol2;
	protocol_table_insert((struct objc_protocol2*)proto);
}
void protocol_addMethodDescription(Protocol *aProtocol,
                                   SEL name,
                                   const char *types,
                                   BOOL isRequiredMethod,
                                   BOOL isInstanceMethod)
{
	if ((NULL == aProtocol) || (NULL == name) || (NULL == types)) { return; }
	if (incompleteProtocolClass() != aProtocol->isa) { return; }
	Protocol2 *proto = (Protocol2*)aProtocol;
	struct objc_method_description_list **listPtr;
	if (isInstanceMethod)
	{
		listPtr = isRequiredMethod ? &proto->instance_methods :
			&proto->optional_instance_methods;
	}
	else
	{
		listPtr = isRequiredMethod ? &proto->class_methods :
			&proto->optional_class_methods;
	}

	size_t oldCount = *listPtr ? (size_t)(*listPtr)->count : 0;
	if (oldCount >= INT32_MAX) { return; }
	size_t newCount = oldCount + 1;
	struct objc_method_description_list *list = resize_tail_array(*listPtr,
		sizeof(*list), sizeof(list->methods[0]), newCount);
	if (NULL == list) { return; }
	*listPtr = list;
	list->count = (int)newCount;

	struct objc_selector *method = &list->methods[newCount - 1];
	method->name = sel_getName(name);
	method->types = types;
	method->hash = 0;
}

void protocol_addProtocol(Protocol *aProtocol, Protocol *addition)
{
	if ((NULL == aProtocol) || (NULL == addition)) { return; }
	if (incompleteProtocolClass() != aProtocol->isa) { return; }
	Protocol2 *proto = (Protocol2*)aProtocol;
	struct objc_protocol_list *list = proto->protocol_list;
	size_t oldCount = list ? list->count : 0;
	if (oldCount == SIZE_MAX) { return; }
	size_t newCount = oldCount + 1;
	struct objc_protocol_list *resized = resize_tail_array(list,
		sizeof(*resized), sizeof(resized->list[0]), newCount);
	if (NULL == resized) { return; }
	resized->count = newCount;
	resized->list[newCount - 1] = (Protocol2*)addition;
	proto->protocol_list = resized;
}
void protocol_addProperty(Protocol *aProtocol,
                          const char *name,
                          const objc_property_attribute_t *attributes,
                          unsigned int attributeCount,
                          BOOL isRequiredProperty,
                          BOOL isInstanceProperty)
{
	if ((NULL == aProtocol) || (NULL == name)) { return; }
	if (incompleteProtocolClass() != aProtocol->isa) { return; }
	if (!isInstanceProperty) { return; }
	Protocol2 *proto = (Protocol2*)aProtocol;
	struct objc_property_list **listPtr;
	if (isRequiredProperty)
	{
		listPtr = &proto->properties;
	}
	else
	{
		listPtr = &proto->optional_properties;
	}
	size_t oldCount = *listPtr ? (size_t)(*listPtr)->count : 0;
	if (oldCount >= INT32_MAX) { return; }
	size_t newCount = oldCount + 1;
	struct objc_property_list *list = resize_tail_array(*listPtr,
		sizeof(*list), sizeof(list->properties[0]), newCount);
	if (NULL == list) { return; }
	*listPtr = list;
	list->count = (int)newCount;
	int index = (int)newCount - 1;
	const char *iVarName = NULL;
	struct objc_property p = propertyFromAttrs(attributes, attributeCount, &iVarName);
	p.name = name;
	constructPropertyAttributes(&p, iVarName);
	memcpy(&(list->properties[index]), &p, sizeof(p));
}

