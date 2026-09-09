#include "objc/runtime.h"
#include "protocol.h"
#include "properties.h"
#include "class.h"
#include "lock.h"
#include "legacy.h"
#include "crt_compat.h"
#include "allocation.h"
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#define BUFFER_TYPE struct objc_protocol_list *
#include "buffer.h"

// Get the functions for string hashing
#include "string_hash.h"

static int protocol_compare(const char *name,
                            const struct objc_protocol *protocol)
{
	return string_compare(name, protocol->name);
}
static int protocol_hash(const struct objc_protocol *protocol)
{
	return string_hash(protocol->name);
}
#define MAP_TABLE_NAME protocol
#define MAP_TABLE_COMPARE_FUNCTION protocol_compare
#define MAP_TABLE_HASH_KEY string_hash
#define MAP_TABLE_HASH_VALUE protocol_hash
#include "hash_table.h"

static protocol_table *known_protocol_table;
mutex_t protocol_table_lock;



PRIVATE void init_protocol_table(void)
{
	protocol_initialize(&known_protocol_table, 128);
	INIT_LOCK(protocol_table_lock);
}

static void protocol_table_insert(const struct objc_protocol *protocol)
{
	protocol_insert(known_protocol_table, (void*)protocol);
}

struct objc_protocol *protocol_for_name(const char *name)
{
	return protocol_table_get(known_protocol_table, name);
}

static BOOL protocol_hasClassProperties(struct objc_protocol *p)
{
	return p->isa == (id)&_OBJC_CLASS_Protocol;
}

static BOOL protocol_hasOptionalMethodsAndProperties(struct objc_protocol *p)
{
	if (p->isa == (id)&_OBJC_CLASS_ProtocolGCC)
	{
		return NO;
	}
	return YES;
}

static int isEmptyProtocol(struct objc_protocol *aProto)
{
	int isEmpty =
		((aProto->instance_methods == NULL) ||
			(aProto->instance_methods->count == 0)) &&
		((aProto->class_methods == NULL) ||
			(aProto->class_methods->count == 0)) &&
		((aProto->protocol_list == NULL) ||
		  (aProto->protocol_list->count == 0));
	if (protocol_hasOptionalMethodsAndProperties(aProto))
	{
		isEmpty &= (aProto->optional_instance_methods == NULL) ||
			(aProto->optional_instance_methods->count == 0);
		isEmpty &= (aProto->optional_class_methods == NULL) ||
			(aProto->optional_class_methods->count == 0);
		isEmpty &= (aProto->properties == 0) || (aProto->properties->count == 0);
		isEmpty &= (aProto->optional_properties == 0) || (aProto->optional_properties->count == 0);
	}
	return isEmpty;
}

// FIXME: Make p1 adopt all of the stuff in p2
static void makeProtocolEqualToProtocol(struct objc_protocol *p1,
                                        struct objc_protocol *p2)
{
#define COPY(x) p1->x = p2->x
	COPY(instance_methods);
	COPY(class_methods);
	COPY(protocol_list);
	if (protocol_hasOptionalMethodsAndProperties(p1) &&
		protocol_hasOptionalMethodsAndProperties(p2))
	{
		COPY(optional_instance_methods);
		COPY(optional_class_methods);
		COPY(properties);
		COPY(optional_properties);
	}
#undef COPY
}

static struct objc_protocol *unique_protocol(struct objc_protocol *aProto)
{
	struct objc_protocol *oldProtocol =
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

static BOOL init_protocols(struct objc_protocol_list *protocols)
{
	for (unsigned i=0 ; i<protocols->count ; i++)
	{
		struct objc_protocol *aProto = protocols->list[i];
		// Don't initialise a protocol twice
		if ((aProto->isa == (id)&_OBJC_CLASS_ProtocolGCC) ||
		    (aProto->isa == (id)&_OBJC_CLASS_ProtocolGSv1) ||
		    (aProto->isa == (id)&_OBJC_CLASS_Protocol))
		{
			continue;
		}

		// Protocols in the protocol list have their class pointers set to the
		// version of the protocol class that they expect.
		enum protocol_version version =
			(enum protocol_version)(uintptr_t)aProto->isa;
		switch (version)
		{
			default:
				fprintf(stderr, "Unknown protocol version");
				abort();
#ifdef OLDABI_COMPAT
			case protocol_version_gcc:
				protocols->list[i] = objc_upgrade_protocol_gcc((struct objc_protocol_gcc *)aProto);
				assert(aProto->isa == (id)&_OBJC_CLASS_ProtocolGCC);
				assert(protocols->list[i]->isa == (id)&_OBJC_CLASS_Protocol);
				aProto = protocols->list[i];
				break;
			case protocol_version_gsv1:
				protocols->list[i] = objc_upgrade_protocol_gsv1((struct objc_protocol_gsv1 *)aProto);
				assert(aProto->isa == (id)&_OBJC_CLASS_ProtocolGSv1);
				assert(protocols->list[i]->isa == (id)&_OBJC_CLASS_Protocol);
				aProto = protocols->list[i];
				break;
#endif
			case protocol_version_gsv2:
				aProto->isa = (id)&_OBJC_CLASS_Protocol;
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
	LOCK_FOR_SCOPE(&protocol_table_lock);
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
	LOCK_FOR_SCOPE(&protocol_table_lock);
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

static struct objc_protocol_method_description_list *
get_method_list(Protocol *p,
                BOOL isRequiredMethod,
                BOOL isInstanceMethod)
{
	struct objc_protocol_method_description_list *list;
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
		if (!protocol_hasOptionalMethodsAndProperties(p)) { return NULL; }

		if (isInstanceMethod)
		{
			list = p->optional_instance_methods;
		}
		else
		{
			list = p->optional_class_methods;
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
	struct objc_protocol_method_description_list *list =
		get_method_list(p, isRequiredMethod, isInstanceMethod);
	if ((NULL == list) || (list->count <= 0)) { return NULL; }
	size_t allocationSize;
	if (!objc2_flexible_array_size(0, (size_t)list->count,
	                               sizeof(struct objc_method_description),
	                               &allocationSize)) { return NULL; }
	struct objc_method_description *out = malloc(allocationSize);
	if (NULL == out) { return NULL; }
	for (int i=0 ; i<list->count ; i++)
	{
		struct objc_protocol_method_description *method = protocol_method_at_index(list, i);
		out[i].name = method->selector;
		out[i].types = sel_getType_np(method->selector);
	}
	*count = (unsigned int)list->count;
	return out;
}

Protocol*__unsafe_unretained* protocol_copyProtocolList(Protocol *p, unsigned int *count)
{
	if (NULL == count) { return NULL; }
	*count = 0;
	if ((NULL == p) || (NULL == p->protocol_list)) { return NULL; }
	size_t protocolCount = p->protocol_list->count;
	if ((0 == protocolCount) || (protocolCount > UINT_MAX)) { return NULL; }
	size_t allocationSize;
	if (!objc2_flexible_array_size(0, protocolCount, sizeof(Protocol*),
	                               &allocationSize)) { return NULL; }
	Protocol **out = malloc(allocationSize);
	if (NULL == out) { return NULL; }
	for (size_t i=0 ; i<protocolCount ; i++)
	{
		out[i] = (Protocol*)p->protocol_list->list[i];
	}
	*count = (unsigned int)protocolCount;
	return out;
}

objc_property_t *protocol_copyPropertyList2(Protocol *p, unsigned int *outCount,
		BOOL isRequiredProperty, BOOL isInstanceProperty)
{
	if (NULL == outCount) { return NULL; }
	*outCount = 0;
	if (NULL == p) { return NULL; }
	if (!protocol_hasOptionalMethodsAndProperties(p)) { return NULL; }
	if (!isInstanceProperty && !protocol_hasClassProperties(p)) { return NULL; }
	struct objc_property_list *properties =
	    isInstanceProperty ?
	        (isRequiredProperty ? p->properties : p->optional_properties) :
	        (isRequiredProperty ? p->class_properties : p->optional_class_properties);
	if (NULL == properties) { return NULL; }
	size_t count = 0;
	for (struct objc_property_list *list = properties; list != NULL; list = list->next)
	{
		if ((list->count < 0) || (list->size < (int)sizeof(struct objc_property))) { return NULL; }
		if ((size_t)list->count > UINT_MAX - count) { return NULL; }
		count += (size_t)list->count;
	}
	if ((0 == count) || (count > UINT_MAX)) { return NULL; }
	size_t allocationSize;
	if (!objc2_flexible_array_size(0, count, sizeof(objc_property_t),
	                               &allocationSize)) { return NULL; }
	objc_property_t *out = malloc(allocationSize);
	if (NULL == out) { return NULL; }
	size_t index = 0;
	for (struct objc_property_list *list = properties; list != NULL; list = list->next)
	{
		for (int i=0 ; i<list->count ; i++)
		{
			out[index++] = property_at_index(list, i);
		}
	}
	*outCount = (unsigned int)count;
	return out;
}

objc_property_t *protocol_copyPropertyList(Protocol *p,
                                           unsigned int *outCount)
{
	return protocol_copyPropertyList2(p, outCount, YES, YES);
}

objc_property_t protocol_getProperty(Protocol *p,
                                     const char *name,
                                     BOOL isRequiredProperty,
                                     BOOL isInstanceProperty)
{
	if ((NULL == p) || (NULL == name)) { return NULL; }
	if (!protocol_hasOptionalMethodsAndProperties(p))
	{
		return NULL;
	}
	if (!isInstanceProperty && !protocol_hasClassProperties(p))
	{
		return NULL;
	}
	struct objc_property_list *properties =
	    isInstanceProperty ?
	        (isRequiredProperty ? p->properties : p->optional_properties) :
	        (isRequiredProperty ? p->class_properties : p->optional_class_properties);
	while (NULL != properties)
	{
		for (int i=0 ; i<properties->count ; i++)
		{
			objc_property_t prop = property_at_index(properties, i);
			if (strcmp(property_getName(prop), name) == 0)
			{
				return prop;
			}
		}
		properties = properties->next;
	}
	return NULL;
}

static struct objc_protocol_method_description *
get_method_description(Protocol *p,
                       SEL aSel,
                       BOOL isRequiredMethod,
                       BOOL isInstanceMethod)
{
	if ((NULL == p) || (NULL == aSel)) { return NULL; }
	struct objc_protocol_method_description_list *list =
		get_method_list(p, isRequiredMethod, isInstanceMethod);
	if (NULL == list)
	{
		return NULL;
	}
	for (int i=0 ; i<list->count ; i++)
	{
		SEL s = protocol_method_at_index(list, i)->selector;
		if (sel_isEqual(s, aSel) ||
		    (strcmp(sel_getName(s), sel_getName(aSel)) == 0))
		{
			return protocol_method_at_index(list, i);
		}
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
	struct objc_protocol_method_description *m = 
		get_method_description(p, aSel, isRequiredMethod, isInstanceMethod);
	if (m != NULL)
	{
		SEL s = m->selector;
		d.name = aSel;
		d.types = sel_getType_np(s);
	}
	return d;
}

const char *_protocol_getMethodTypeEncoding(Protocol *p,
                                            SEL aSel,
                                            BOOL isRequiredMethod,
                                            BOOL isInstanceMethod)
{
	struct objc_protocol_method_description *m = 
		get_method_description(p, aSel, isRequiredMethod, isInstanceMethod);
	if (m != NULL)
	{
		return m->types;
	}
	return NULL;
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
	if (p == other ||
		p->name == other->name ||
		0 == strcmp(p->name, other->name))
	{
		return YES;
	}
	return NO;
}

Protocol*__unsafe_unretained* objc_copyProtocolList(unsigned int *outCount)
{
	if (NULL != outCount) { *outCount = 0; }
	LOCK_FOR_SCOPE(&protocol_table_lock);
	size_t total = known_protocol_table->table_used;
	if ((0 == total) || (total > UINT_MAX)) { return NULL; }
	size_t allocationSize;
	if (!objc2_flexible_array_size(0, total, sizeof(Protocol*),
	                               &allocationSize)) { return NULL; }
	Protocol **protocols = malloc(allocationSize);
	if (NULL == protocols) { return NULL; }
	struct protocol_table_enumerator *enumerator = NULL;
	Protocol *next;
	size_t count = 0;
	while ((count < total) && (next = protocol_next(known_protocol_table, &enumerator)))
	{
		protocols[count++] = next;
	}
	if (NULL != enumerator) { free(enumerator); }
	if (NULL != outCount) { *outCount = (unsigned int)count; }
	return protocols;
}

Protocol *objc_allocateProtocol(const char *name)
{
	if ((NULL == name) || ('\0' == name[0]) || (objc_getProtocol(name) != NULL)) { return NULL; }
	Protocol *p = (Protocol*)class_createInstance(&_OBJC_CLASS___IncompleteProtocol, 0);
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
	if (NULL == proto) { return; }
	LOCK_FOR_SCOPE(&protocol_table_lock);
	if ((NULL == proto->name) || (protocol_for_name(proto->name) != NULL)) { return; }
	if (proto->isa != (id)&_OBJC_CLASS___IncompleteProtocol) { return; }
	proto->isa = (id)&_OBJC_CLASS_Protocol;
	protocol_table_insert(proto);
}
PRIVATE void registerProtocol(Protocol *proto)
{
	LOCK_FOR_SCOPE(&protocol_table_lock);
	proto->isa = (id)&_OBJC_CLASS_Protocol;
	if (protocol_for_name(proto->name) == NULL)
	{
		protocol_table_insert(proto);
	}
}
void protocol_addMethodDescription(Protocol *aProtocol,
                                   SEL name,
                                   const char *types,
                                   BOOL isRequiredMethod,
                                   BOOL isInstanceMethod)
{
	if ((NULL == aProtocol) || (NULL == name) || (NULL == types)) { return; }
	if (aProtocol->isa != (id)(id)&_OBJC_CLASS___IncompleteProtocol) { return; }
	struct objc_protocol_method_description_list **listPtr;
	if (isInstanceMethod)
	{
		if (isRequiredMethod)
		{
			listPtr = &aProtocol->instance_methods;
		}
		else
		{
			listPtr = &aProtocol->optional_instance_methods;
		}
	}
	else
	{
		if (isRequiredMethod)
		{
			listPtr = &aProtocol->class_methods;
		}
		else
		{
			listPtr = &aProtocol->optional_class_methods;
		}
	}
	SEL typedSelector = sel_registerTypedName_np(sel_getName(name), types);
	char *typeCopy = objc2_strdup(types);
	if ((NULL == typedSelector) || (NULL == typeCopy))
	{
		free(typeCopy);
		return;
	}
	struct objc_protocol_method_description_list *oldList = *listPtr;
	if ((oldList != NULL) && ((oldList->count < 0) ||
	    (oldList->size < (int)sizeof(struct objc_protocol_method_description))))
	{
		free(typeCopy);
		return;
	}
	size_t oldCount = oldList ? (size_t)oldList->count : 0;
	if (oldCount >= INT_MAX) { free(typeCopy); return; }
	size_t stride = oldList ? (size_t)oldList->size : sizeof(struct objc_protocol_method_description);
	size_t allocationSize;
	if (!objc2_flexible_array_size(sizeof(struct objc_protocol_method_description_list),
	                               oldCount + 1, stride, &allocationSize))
	{
		free(typeCopy);
		return;
	}
	struct objc_protocol_method_description_list *list = realloc(oldList, allocationSize);
	if (NULL == list) { free(typeCopy); return; }
	if (0 == oldCount) { list->size = sizeof(struct objc_protocol_method_description); }
	list->count = (int)(oldCount + 1);
	*listPtr = list;
	int index = (int)oldCount;
	struct objc_protocol_method_description *method = protocol_method_at_index(list, index);
	memset(method, 0, stride);
	method->selector = typedSelector;
	method->types = typeCopy;
}
void protocol_addProtocol(Protocol *aProtocol, Protocol *addition)
{
	if ((NULL == aProtocol) || (NULL == addition)) { return; }
	if (aProtocol->isa != (id)&_OBJC_CLASS___IncompleteProtocol) { return; }
	size_t oldCount = aProtocol->protocol_list ? aProtocol->protocol_list->count : 0;
	if (oldCount == SIZE_MAX) { return; }
	size_t allocationSize;
	if (!objc2_flexible_array_size(sizeof(struct objc_protocol_list), oldCount + 1,
	                               sizeof(Protocol*), &allocationSize)) { return; }
	struct objc_protocol_list *updated = realloc(aProtocol->protocol_list, allocationSize);
	if (NULL == updated) { return; }
	if (0 == oldCount) { updated->next = NULL; }
	updated->count = oldCount + 1;
	updated->list[oldCount] = (Protocol*)addition;
	aProtocol->protocol_list = updated;
}
void protocol_addProperty(Protocol *aProtocol,
                          const char *name,
                          const objc_property_attribute_t *attributes,
                          unsigned int attributeCount,
                          BOOL isRequiredProperty,
                          BOOL isInstanceProperty)
{
	if ((NULL == aProtocol) || (NULL == name)) { return; }
	if (aProtocol->isa != (id)&_OBJC_CLASS___IncompleteProtocol) { return; }
	struct objc_property_list **listPtr =
	    isInstanceProperty ?
	        (isRequiredProperty ? &aProtocol->properties : &aProtocol->optional_properties) :
	        (isRequiredProperty ? &aProtocol->class_properties : &aProtocol->optional_class_properties);
	struct objc_property_list *oldList = *listPtr;
	if ((oldList != NULL) && ((oldList->count < 0) ||
	    (oldList->size < (int)sizeof(struct objc_property)))) { return; }
	size_t oldCount = oldList ? (size_t)oldList->count : 0;
	if (oldCount >= INT_MAX) { return; }
	size_t stride = oldList ? (size_t)oldList->size : sizeof(struct objc_property);
	size_t allocationSize;
	if (!objc2_flexible_array_size(sizeof(struct objc_property_list), oldCount + 1,
	                               stride, &allocationSize)) { return; }
	struct objc_property_list *list = realloc(oldList, allocationSize);
	if (NULL == list) { return; }
	if (0 == oldCount)
	{
		list->size = sizeof(struct objc_property);
		list->next = NULL;
	}
	list->count = (int)(oldCount + 1);
	*listPtr = list;
	int index = (int)oldCount;
	struct objc_property property = propertyFromAttrs(attributes, attributeCount, name);
	struct objc_property *slot = property_at_index(list, index);
	memset(slot, 0, stride);
	memcpy(slot, &property, sizeof(property));
}

