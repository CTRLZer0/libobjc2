#include "objc/runtime.h"
#include "objc/memory/arc.h"
#include "crt_compat.h"
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "class.h"
#include "properties.h"
#include "spinlock.h"
#include "allocation.h"
#include "helpers.hh"
#include "visibility.h"
#include "nsobject.h"
#include "gc_ops.h"
#include "lock.h"

extern "C"
{

/**
 * Public function for getting a property.  
 */
OBJC_PUBLIC
id objc_getProperty(id obj, SEL _cmd, ptrdiff_t offset, BOOL isAtomic)
{
	if (nil == obj) { return nil; }
	char *addr = (char*)obj;
	addr += offset;
	id ret;
	if (isAtomic)
	{
		{
			auto guard = acquire_locks_for_pointers(addr);
			ret = *(id*)addr;
			ret = objc_retain(ret);
		}
		ret = objc_autoreleaseReturnValue(ret);
	}
	else
	{
		ret = *(id*)addr;
		ret = objc_retainAutoreleaseReturnValue(ret);
	}
	return ret;
}

OBJC_PUBLIC
void objc_setProperty(id obj, SEL _cmd, ptrdiff_t offset, id arg, BOOL isAtomic, BOOL isCopy)
{
	if (nil == obj) { return; }
	char *addr = (char*)obj;
	addr += offset;

	if (isCopy)
	{
		arg = [arg copy];
	}
	else
	{
		arg = objc_retain(arg);
	}
	id old;
	if (isAtomic)
	{
		auto guard = acquire_locks_for_pointers(addr);
		old = *(id*)addr;
		*(id*)addr = arg;
	}
	else
	{
		old = *(id*)addr;
		*(id*)addr = arg;
	}
	objc_release(old);
}

OBJC_PUBLIC
void objc_setProperty_atomic(id obj, SEL _cmd, id arg, ptrdiff_t offset)
{
	char *addr = (char*)obj;
	addr += offset;
	arg = objc_retain(arg);
	id old;
	{
		auto guard = acquire_locks_for_pointers(addr);
		old = *(id*)addr;
		*(id*)addr = arg;
	}
	objc_release(old);
}

OBJC_PUBLIC
void objc_setProperty_atomic_copy(id obj, SEL _cmd, id arg, ptrdiff_t offset)
{
	char *addr = (char*)obj;
	addr += offset;
	arg = [arg copy];
	id old;
	{
		auto guard = acquire_locks_for_pointers(addr);
		old = *(id*)addr;
		*(id*)addr = arg;
	}
	objc_release(old);
}

OBJC_PUBLIC
void objc_setProperty_nonatomic(id obj, SEL _cmd, id arg, ptrdiff_t offset)
{
	char *addr = (char*)obj;
	addr += offset;
	arg = objc_retain(arg);
	id old = *(id*)addr;
	*(id*)addr = arg;
	objc_release(old);
}

OBJC_PUBLIC
void objc_setProperty_nonatomic_copy(id obj, SEL _cmd, id arg, ptrdiff_t offset)
{
	char *addr = (char*)obj;
	addr += offset;
	id old = *(id*)addr;
	*(id*)addr = [arg copy];
	objc_release(old);
}

OBJC_PUBLIC
void objc_copyCppObjectAtomic(void *dest, const void *src,
                              void (*copyHelper) (void *dest, const void *source))
{
	auto guard = acquire_locks_for_pointers(src, dest);
	copyHelper(dest, src);
}

OBJC_PUBLIC
void objc_getCppObjectAtomic(void *dest, const void *src,
                             void (*copyHelper) (void *dest, const void *source))
{
	auto guard = acquire_locks_for_pointers(src);
	copyHelper(dest, src);
}

OBJC_PUBLIC
void objc_setCppObjectAtomic(void *dest, const void *src,
                             void (*copyHelper) (void *dest, const void *source))
{
	auto guard = acquire_locks_for_pointers(dest);
	copyHelper(dest, src);
}

/**
 * Structure copy function.  This is provided for compatibility with the Apple
 * APIs (it's an ABI function, so it's semi-public), but it's a bad design so
 * it's not used.  The problem is that it does not identify which of the
 * pointers corresponds to the object, which causes some excessive locking to
 * be needed.
 */
OBJC_PUBLIC
void objc_copyPropertyStruct(void *dest,
                             void *src,
                             ptrdiff_t size,
                             BOOL atomic,
                             BOOL strong)
{
	if (atomic)
	{
		auto guard = acquire_locks_for_pointers(src, dest);
		memcpy(dest, src, size);
	}
	else
	{
		memcpy(dest, src, size);
	}
}

/**
 * Get property structure function.  Copies a structure from an ivar to another
 * variable.  Locks on the address of src.
 */
OBJC_PUBLIC
void objc_getPropertyStruct(void *dest,
                            void *src,
                            ptrdiff_t size,
                            BOOL atomic,
                            BOOL strong)
{
	if (atomic)
	{
		auto guard = acquire_locks_for_pointers(src);
		memcpy(dest, src, size);
	}
	else
	{
		memcpy(dest, src, size);
	}
}

/**
 * Set property structure function.  Copes a structure to an ivar.  Locks on
 * dest.
 */
OBJC_PUBLIC
void objc_setPropertyStruct(void *dest,
                            void *src,
                            ptrdiff_t size,
                            BOOL atomic,
                            BOOL strong)
{
	if (atomic)
	{
		auto guard = acquire_locks_for_pointers(dest);
		memcpy(dest, src, size);
	}
	else
	{
		memcpy(dest, src, size);
	}
}


OBJC_PUBLIC
objc_property_t class_getProperty(Class cls, const char *name)
{
	if ((Nil == cls) || (name == NULL))
	{
		return NULL;
	}
	struct objc_property_list *properties = cls->properties;
	while (NULL != properties)
	{
		for (int i=0 ; i<properties->count ; i++)
		{
			objc_property_t p = property_at_index(properties, i);
			if (strcmp(property_getName(p), name) == 0)
			{
				return p;
			}
		}
		properties = properties->next;
	}
	return NULL;
}

OBJC_PUBLIC
objc_property_t* class_copyPropertyList(Class cls, unsigned int *outCount)
{
	if (Nil == cls)
	{
		if (NULL != outCount) { *outCount = 0; }
		return NULL;
	}
	struct objc_property_list *properties = cls->properties;
	if (!properties)
	{
		if (NULL != outCount) { *outCount = 0; }
		return NULL;
	}
	unsigned int count = 0;
	for (struct objc_property_list *l=properties ; NULL!=l ; l=l->next)
	{
		if (l->count <= 0) { continue; }
		unsigned int nodeCount = (unsigned int)l->count;
		if (nodeCount > UINT_MAX - count)
		{
			if (NULL != outCount) { *outCount = 0; }
			return NULL;
		}
		count += nodeCount;
	}
	if (0 == count)
	{
		if (NULL != outCount) { *outCount = 0; }
		return NULL;
	}
	objc_property_t *list = allocate_zeroed_array<objc_property_t>(count);
	if (list == NULL)
	{
		if (NULL != outCount) { *outCount = 0; }
		return NULL;
	}
	if (NULL != outCount) { *outCount = count; }
	unsigned int out = 0;
	for (struct objc_property_list *l=properties ; NULL!=l ; l=l->next)
	{
		for (int i=0 ; i<l->count ; i++)
		{
			list[out++] = property_at_index(l, i);
		}
	}
	return list;
}
OBJC_PUBLIC
const char *property_getName(objc_property_t property)
{
	if (NULL == property) { return NULL; }

	const char *name = property->name;
	if (NULL == name) { return NULL; }
	if (name[0] == 0)
	{
		name += name[1];
	}
	return name;
}

/*
 * The compiler stores the type encoding of the getter.  We replace this with
 * the type encoding of the property itself.  We use a 0 byte at the start to
 * indicate that the swap has taken place.
 */
static const char *property_getTypeEncoding(objc_property_t property)
{
	if (NULL == property) { return NULL; }
	return property->type;
}

OBJC_PUBLIC
const char *property_getAttributes(objc_property_t property)
{
	if (NULL == property) { return NULL; }
	return property->attributes;
}


static BOOL nextPropertyAttributeToken(const char **cursor,
                                       char *name,
                                       const char **value,
                                       size_t *valueLength)
{
	if ((cursor == NULL) || (*cursor == NULL)) { return NO; }
	const char *token = *cursor;
	while (*token == ',') { token++; }
	if (*token == '\0')
	{
		*cursor = token;
		return NO;
	}
	const char *end = strchr(token, ',');
	if (end == NULL) { end = token + strlen(token); }
	*name = *token;
	*value = token + 1;
	*valueLength = (size_t)(end - (token + 1));
	*cursor = (*end == ',') ? end + 1 : end;
	return YES;
}

static const char *propertyAttributeName(char name)
{
	switch (name)
	{
		case 'T': return "T";
		case 'R': return "R";
		case 'C': return "C";
		case '&': return "&";
		case 'D': return "D";
		case 'W': return "W";
		case 'N': return "N";
		case 'G': return "G";
		case 'S': return "S";
		case 'V': return "V";
		default: return NULL;
	}
}

static BOOL propertyAttributeHasValue(char name)
{
	return (name == 'G') || (name == 'S') || (name == 'V');
}

OBJC_PUBLIC
objc_property_attribute_t *property_copyAttributeList(objc_property_t property,
                                                      unsigned int *outCount)
{
	if (outCount != NULL) { *outCount = 0; }
	if (property == NULL) { return NULL; }

	const char *types = property_getTypeEncoding(property);
	const char *attributes = property_getAttributes(property);
	size_t count = (types == NULL) ? 0 : 1;
	size_t valueBytes = (types == NULL) ? 0 : strlen(types) + 1;

	const char *cursor = attributes;
	char name;
	const char *value;
	size_t valueLength;
	while (nextPropertyAttributeToken(&cursor, &name, &value, &valueLength))
	{
		if ((name == 'T') || (propertyAttributeName(name) == NULL)) { continue; }
		size_t storedLength = propertyAttributeHasValue(name) ? valueLength : 0;
		size_t tokenBytes;
		if ((count == UINT_MAX) ||
		    !objc2_size_add(storedLength, 1, &tokenBytes) ||
		    !objc2_size_add(valueBytes, tokenBytes, &valueBytes))
		{
			return NULL;
		}
		count++;
	}
	if (count == 0) { return NULL; }

	size_t arrayBytes;
	size_t allocationSize;
	if (!objc2_size_multiply(count, sizeof(objc_property_attribute_t), &arrayBytes) ||
	    !objc2_size_add(arrayBytes, valueBytes, &allocationSize))
	{
		return NULL;
	}
	objc_property_attribute_t *result =
		static_cast<objc_property_attribute_t*>(calloc(1, allocationSize));
	if (result == NULL) { return NULL; }
	char *valueOut = reinterpret_cast<char*>(result) + arrayBytes;
	size_t out = 0;

	if (types != NULL)
	{
		result[out].name = "T";
		result[out].value = valueOut;
		size_t length = strlen(types);
		memcpy(valueOut, types, length + 1);
		valueOut += length + 1;
		out++;
	}

	cursor = attributes;
	while (nextPropertyAttributeToken(&cursor, &name, &value, &valueLength))
	{
		const char *attributeName = propertyAttributeName(name);
		if ((name == 'T') || (attributeName == NULL)) { continue; }
		result[out].name = attributeName;
		result[out].value = valueOut;
		size_t storedLength = propertyAttributeHasValue(name) ? valueLength : 0;
		if (storedLength != 0) { memcpy(valueOut, value, storedLength); }
		valueOut[storedLength] = '\0';
		valueOut += storedLength + 1;
		out++;
	}
	assert(out == count);
	if (outCount != NULL) { *outCount = (unsigned int)out; }
	return result;
}

static const objc_property_attribute_t *findAttribute(char attr,
                                                      const objc_property_attribute_t *attributes,
                                                      unsigned int attributeCount)
{
	if (attributes == NULL) { return NULL; }
	// This linear scan is N^2 in the worst case, but that's still probably
	// cheaper than sorting the array because N<12
	for (unsigned int i=0 ; i<attributeCount ; i++)
	{
		if ((attributes[i].name != NULL) && (attributes[i].name[0] == attr))
		{
			return &attributes[i];
		}
	}
	return NULL;
}
static char *addAttrIfExists(char a,
                             char *buffer,
                             const objc_property_attribute_t *attributes,
                             unsigned int attributeCount)
{
	const objc_property_attribute_t *attr = findAttribute(a, attributes, attributeCount);
	if (attr)
	{
		*(buffer++) = attr->name[0];
		if (attr->value)
		{
			size_t len = strlen(attr->value);
			memcpy(buffer, attr->value, len);
			buffer += len;
		}
		*(buffer++) = ',';
	}
	return buffer;
}

static const char *encodingFromAttrs(const objc_property_attribute_t *attributes,
                                     unsigned int attributeCount)
{
	if ((attributeCount != 0) && (attributes == NULL)) { return NULL; }
	// Length of the attributes string (keys, commas, values, and trailing null).
	size_t attributesSize;
	if (!objc2_size_multiply((size_t)attributeCount, 2, &attributesSize))
	{
		return NULL;
	}
	for (unsigned int i=0 ; i<attributeCount ; i++)
	{
		if (attributes[i].value)
		{
			size_t valueLength = strlen(attributes[i].value);
			if (valueLength > SIZE_MAX - attributesSize) { return NULL; }
			attributesSize += valueLength;
		}
	}
	if (attributesSize == 0)
	{
		return NULL;
	}

	char *buffer = static_cast<char*>(malloc(attributesSize));
	if (buffer == NULL) { return NULL; }

	char *out = buffer;
	out = addAttrIfExists('T', out, attributes, attributeCount);
	out = addAttrIfExists('R', out, attributes, attributeCount);
	out = addAttrIfExists('&', out, attributes, attributeCount);
	out = addAttrIfExists('C', out, attributes, attributeCount);
	out = addAttrIfExists('W', out, attributes, attributeCount);
	out = addAttrIfExists('D', out, attributes, attributeCount);
	out = addAttrIfExists('N', out, attributes, attributeCount);
	out = addAttrIfExists('G', out, attributes, attributeCount);
	out = addAttrIfExists('S', out, attributes, attributeCount);
	out = addAttrIfExists('V', out, attributes, attributeCount);
	if (out == buffer)
	{
		free(buffer);
		return NULL;
	}
	out--;
	*out = '\0';

	return buffer;
}

PRIVATE struct objc_property propertyFromAttrs(const objc_property_attribute_t *attributes,
                                                          unsigned int attributeCount,
                                                          const char *name)
{
	struct objc_property p;
	p.name = objc2_strdup(name);
	p.attributes = encodingFromAttrs(attributes, attributeCount);
	p.type = NULL;
	const objc_property_attribute_t *attr = findAttribute('T', attributes, attributeCount);
	if ((attr != NULL) && (attr->value != NULL))
	{
		p.type = objc2_strdup(attr->value);
	}
	p.getter = NULL;
	attr = findAttribute('G', attributes, attributeCount);
	if ((attr != NULL) && (attr->value != NULL))
	{
		// TODO: We should be able to construct the full type encoding if we
		// also have a type, but for now use an untyped selector.
		p.getter = sel_registerName(attr->value);
	}
	p.setter = NULL;
	attr = findAttribute('S', attributes, attributeCount);
	if ((attr != NULL) && (attr->value != NULL))
	{
		// TODO: We should be able to construct the full type encoding if we
		// also have a type, but for now use an untyped selector.
		p.setter = sel_registerName(attr->value);
	}
	return p;
}

static BOOL propertyConstructionFailed(const struct objc_property *property,
                                       const objc_property_attribute_t *attributes,
                                       unsigned int attributeCount)
{
	if (property->name == NULL) { return YES; }
	if ((attributeCount != 0) && (property->attributes == NULL)) { return YES; }
	const objc_property_attribute_t *type = findAttribute('T', attributes, attributeCount);
	return (type != NULL) && (type->value != NULL) && (property->type == NULL);
}

static void freeConstructedProperty(struct objc_property *property)
{
	free((void*)property->name);
	free((void*)property->attributes);
	free((void*)property->type);
}

OBJC_PUBLIC
BOOL class_addProperty(Class cls,
                       const char *name,
                       const objc_property_attribute_t *attributes, 
                       unsigned int attributeCount)
{
	if ((Nil == cls) || (NULL == name) || (class_getProperty(cls, name) != 0)) { return NO; }

	struct objc_property p = propertyFromAttrs(attributes, attributeCount, name);
	if (propertyConstructionFailed(&p, attributes, attributeCount))
	{
		freeConstructedProperty(&p);
		return NO;
	}

	struct objc_property_list *l = allocate_zeroed<struct objc_property_list>(sizeof(struct objc_property));
	if (l == NULL)
	{
		freeConstructedProperty(&p);
		return NO;
	}
	l->count = 1;
	l->size = sizeof(struct objc_property);
	memcpy(&l->properties, &p, sizeof(struct objc_property));
	LOCK_RUNTIME_FOR_SCOPE();
	l->next = cls->properties;
	cls->properties = l;
	return YES;
}

OBJC_PUBLIC
void class_replaceProperty(Class cls,
                           const char *name,
                           const objc_property_attribute_t *attributes,
                           unsigned int attributeCount)
{
	if ((Nil == cls) || (NULL == name)) { return; }
	objc_property_t old = class_getProperty(cls, name);
	if (NULL == old)
	{
		class_addProperty(cls, name, attributes, attributeCount);
		return;
	}
	struct objc_property p = propertyFromAttrs(attributes, attributeCount, name);
	if (propertyConstructionFailed(&p, attributes, attributeCount))
	{
		freeConstructedProperty(&p);
		return;
	}
	LOCK_RUNTIME_FOR_SCOPE();
	memcpy(old, &p, sizeof(struct objc_property));
}
static char *copyPropertyAttributeTokenValue(const char *attributes, char requested)
{
	const char *cursor = attributes;
	char name;
	const char *value;
	size_t valueLength;
	while (nextPropertyAttributeToken(&cursor, &name, &value, &valueLength))
	{
		if (name != requested) { continue; }
		char *copy = static_cast<char*>(malloc(valueLength + 1));
		if (copy == NULL) { return NULL; }
		if (valueLength != 0) { memcpy(copy, value, valueLength); }
		copy[valueLength] = '\0';
		return copy;
	}
	return NULL;
}

OBJC_PUBLIC
char *property_copyAttributeValue(objc_property_t property,
                                  const char *attributeName)
{
	if ((property == NULL) || (attributeName == NULL) || (attributeName[0] == '\0'))
	{
		return NULL;
	}
	if (attributeName[0] == 'T')
	{
		const char *types = property_getTypeEncoding(property);
		return (types == NULL) ? NULL : objc2_strdup(types);
	}
	if (propertyAttributeName(attributeName[0]) == NULL) { return NULL; }
	if (!propertyAttributeHasValue(attributeName[0]))
	{
		char *value = copyPropertyAttributeTokenValue(property_getAttributes(property),
		                                              attributeName[0]);
		if (value != NULL) { value[0] = '\0'; }
		return value;
	}
	return copyPropertyAttributeTokenValue(property_getAttributes(property),
	                                       attributeName[0]);
}


} // extern "C"
