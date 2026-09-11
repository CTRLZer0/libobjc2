#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "objc/runtime.h"
#include "objc/encoding/api.h"
#include "legacy.h"
#include "properties.h"
#include "protocol.h"
#include "class.h"
#include "loader.h"
#include "allocation.h"

PRIVATE size_t lengthOfTypeEncoding(const char *types);

static void *legacy_calloc_flexible_or_abort(size_t headerSize, int count, size_t elementSize)
{
	if (count < 0) { abort(); }
	size_t allocationSize;
	if (!objc2_flexible_array_size(headerSize, (size_t)count, elementSize, &allocationSize))
	{
		abort();
	}
	void *allocation = calloc(1, allocationSize);
	if (allocation == NULL) { abort(); }
	return allocation;
}

static void *legacy_malloc_flexible_or_abort(size_t headerSize, int count, size_t elementSize)
{
	if (count < 0) { abort(); }
	size_t allocationSize;
	if (!objc2_flexible_array_size(headerSize, (size_t)count, elementSize, &allocationSize))
	{
		abort();
	}
	void *allocation = malloc(allocationSize);
	if (allocation == NULL) { abort(); }
	return allocation;
}

static void *legacy_malloc_or_abort(size_t size)
{
	void *allocation = malloc(size);
	if (allocation == NULL) { abort(); }
	return allocation;
}

static size_t legacy_size_add_or_abort(size_t left, size_t right)
{
	size_t result;
	if (!objc2_size_add(left, right, &result)) { abort(); }
	return result;
}

enum objc_class_flags_gsv1
{
	/** This class structure represents a class. */
	objc_class_flag_class_gsv1 = (1<<0),
	/** This class structure represents a metaclass. */
	objc_class_flag_meta_gsv1 = (1<<1),
	/** 
	 * The class uses the new, Objective-C 2, runtime ABI.  This ABI defines an
	 * ABI version field inside the class, and so will be used for all
	 * subsequent versions that retain some degree of compatibility.
	 */
	objc_class_flag_new_abi_gsv1 = (1<<4)
};

static inline BOOL objc_test_class_flag_gsv1(struct objc_class_gsv1 *aClass,
                                               enum objc_class_flags_gsv1 flag)
{
	return (aClass->info & (unsigned long)flag) == (unsigned long)flag;
}
/**
 * Checks the version of a class.  Return values are:
 * 0. Legacy GCC ABI compatible class.
 * 1. First release of GNUstep ABI.
 * 2. Second release of the GNUstep ABI, adds strong / weak ivar bitmaps.
 * 3. Third release of the GNUstep ABI.  Many cleanups.
 */
static inline int objc_get_class_version_gsv1(struct objc_class_gsv1 *aClass)
{
	if (!objc_test_class_flag_gsv1(aClass, objc_class_flag_new_abi_gsv1))
	{
		return 0;
	}
	return aClass->abi_version + 1;
}

static objc_ivar_ownership ownershipForIvar(struct objc_class_gsv1 *cls, int idx)
{
	if (objc_get_class_version_gsv1(cls) < 2)
	{
		return ownership_unsafe;
	}
	if ((cls->strong_pointers != 0) && objc_bitfield_test(cls->strong_pointers, idx))
	{
		return ownership_strong;
	}
	if ((cls->weak_pointers != 0) && objc_bitfield_test(cls->weak_pointers, idx))
	{
		return ownership_weak;
	}
	return ownership_unsafe;
}

static struct objc_ivar_list *upgradeIvarList(struct objc_class_gsv1 *cls)
{
	struct objc_ivar_list_gcc *l = cls->ivars;
	if (l == NULL) { return NULL; }
	struct objc_ivar_list *n = legacy_calloc_flexible_or_abort(
		sizeof(struct objc_ivar_list), l->count, sizeof(struct objc_ivar));
	n->size = sizeof(struct objc_ivar);
	n->count = l->count;
	BOOL usesNewABI = objc_test_class_flag_gsv1(cls, objc_class_flag_new_abi_gsv1);
	if (usesNewABI && (l->count > 0) && (cls->ivar_offsets == NULL)) { abort(); }
	for (int i=0 ; i<l->count ; i++)
	{
		BOOL isBitfield = NO;
		int64_t bitfieldSize = 0;
		int64_t currentOffset = l->ivar_list[i].offset;
		int64_t nextOffset;
		if (i+1 < l->count)
		{
			nextOffset = l->ivar_list[i+1].offset;
			if (currentOffset == nextOffset)
			{
				isBitfield = YES;
				for (int j=i+2 ; j<l->count ; j++)
				{
					if (currentOffset != l->ivar_list[j].offset)
					{
						bitfieldSize = (int64_t)l->ivar_list[j].offset - currentOffset;
						break;
					}
				}
				if (bitfieldSize == 0)
				{
					bitfieldSize = (int64_t)cls->instance_size - currentOffset;
				}
			}
		}
		else
		{
			nextOffset = cls->instance_size;
		}
		if (nextOffset < 0) { nextOffset = -nextOffset; }
		int64_t size = nextOffset - currentOffset;
		int64_t storedSize = isBitfield ? bitfieldSize : size;
		if ((storedSize < 0) || ((uint64_t)storedSize > UINT32_MAX)) { abort(); }

		const char *type = l->ivar_list[i].type;
		n->ivar_list[i].name = l->ivar_list[i].name;
		n->ivar_list[i].type = type;
		n->ivar_list[i].size = (uint32_t)storedSize;
		if (usesNewABI)
		{
			n->ivar_list[i].offset = cls->ivar_offsets[i];
		}
		else
		{
			n->ivar_list[i].offset = &l->ivar_list[i].offset;
		}
		ivarSetAlign(&n->ivar_list[i], ((type == NULL) || type[0] == 0)
			? __alignof__(void*) : objc_alignof_type(type));
		if ((type != NULL) && (type[0] == '\0'))
		{
			ivarSetAlign(&n->ivar_list[i], (size_t)storedSize);
		}
		ivarSetOwnership(&n->ivar_list[i], ownershipForIvar(cls, i));
	}
	return n;
}

static struct objc_method_list *upgradeMethodList(struct objc_method_list_gcc *old)
{
	struct objc_method_list *head = NULL;
	struct objc_method_list **tail = &head;
	while ((old != NULL) && (old->count != 0))
	{
		struct objc_method_list *list = legacy_calloc_flexible_or_abort(
			sizeof(struct objc_method_list), old->count, sizeof(struct objc_method));
		list->count = old->count;
		list->size = sizeof(struct objc_method);
		for (int i=0 ; i<old->count ; i++)
		{
			list->methods[i].imp = old->methods[i].imp;
			list->methods[i].selector = old->methods[i].selector;
			list->methods[i].types = old->methods[i].types;
		}
		*tail = list;
		tail = &list->next;
		old = old->next;
	}
	if ((old != NULL) && (old->count < 0)) { abort(); }
	return head;
}

static inline BOOL checkAttribute(char field, int attr)
{
	return (field & attr) == attr;
}

static void upgradeProperty(struct objc_property *n, struct objc_property_gsv1 *o)
{
	if ((n == NULL) || (o == NULL) || (o->name == NULL)) { abort(); }
	char *typeEncoding;
	size_t typeSize;
	if (o->name[0] == '\0')
	{
		unsigned char nameOffset = (unsigned char)o->name[1];
		if (nameOffset < 2) { abort(); }
		n->name = o->name + nameOffset;
		n->attributes = o->name + 2;
		if (n->attributes[0] != 'T') { abort(); }
		const char *type_start = &n->attributes[1];
		const char *type_end = strchr(type_start, ',');
		if (type_end == NULL) { type_end = type_start + strlen(type_start); }
		typeSize = (size_t)(type_end - type_start);
		typeEncoding = legacy_malloc_or_abort(legacy_size_add_or_abort(typeSize, 1));
		memcpy(typeEncoding, type_start, typeSize);
		typeEncoding[typeSize] = 0;
	}
	else
	{
		if (o->getter_types == NULL) { abort(); }
		typeSize = lengthOfTypeEncoding(o->getter_types);
		typeEncoding = legacy_malloc_or_abort(legacy_size_add_or_abort(typeSize, 1));
		memcpy(typeEncoding, o->getter_types, typeSize);
		typeEncoding[typeSize] = 0;
	}
	n->type = typeEncoding;

	if (o->getter_name)
	{
		n->getter = sel_registerTypedName_np(o->getter_name, o->getter_types);
	}
	if (o->setter_name)
	{
		n->setter = sel_registerTypedName_np(o->setter_name, o->setter_types);
	}

	if (o->name[0] == '\0') { return; }

	n->name = o->name;
	const char *name = o->name;
	size_t nameSize = strlen(name);
	size_t encodingSize = legacy_size_add_or_abort(typeSize, nameSize);
	encodingSize = legacy_size_add_or_abort(encodingSize, 6);
	char flags[20];
	size_t i = 0;
	if (checkAttribute(o->attributes, OBJC_PR_readonly)) { flags[i++] = ','; flags[i++] = 'R'; }
	if (checkAttribute(o->attributes, OBJC_PR_retain)) { flags[i++] = ','; flags[i++] = '&'; }
	if (checkAttribute(o->attributes, OBJC_PR_copy)) { flags[i++] = ','; flags[i++] = 'C'; }
	if (checkAttribute(o->attributes2, OBJC_PR_weak)) { flags[i++] = ','; flags[i++] = 'W'; }
	if (checkAttribute(o->attributes2, OBJC_PR_dynamic)) { flags[i++] = ','; flags[i++] = 'D'; }
	if ((o->attributes & OBJC_PR_nonatomic) == OBJC_PR_nonatomic) { flags[i++] = ','; flags[i++] = 'N'; }
	encodingSize = legacy_size_add_or_abort(encodingSize, i);
	flags[i] = '\0';

	size_t getterLength = 0;
	size_t setterLength = 0;
	if ((o->attributes & OBJC_PR_getter) == OBJC_PR_getter)
	{
		if (o->getter_name == NULL) { abort(); }
		getterLength = strlen(o->getter_name);
		encodingSize = legacy_size_add_or_abort(encodingSize,
			legacy_size_add_or_abort(2, getterLength));
	}
	if ((o->attributes & OBJC_PR_setter) == OBJC_PR_setter)
	{
		if (o->setter_name == NULL) { abort(); }
		setterLength = strlen(o->setter_name);
		encodingSize = legacy_size_add_or_abort(encodingSize,
			legacy_size_add_or_abort(2, setterLength));
	}

	unsigned char *encoding = legacy_malloc_or_abort(encodingSize);
	unsigned char *insert = encoding;
	BOOL needsComma = NO;
	*(insert++) = 0;
	*(insert++) = 0;
	*(insert++) = 'T';
	memcpy(insert, typeEncoding, typeSize);
	insert += typeSize;
	needsComma = YES;
	memcpy(insert, flags, i);
	insert += i;
	if ((o->attributes & OBJC_PR_getter) == OBJC_PR_getter)
	{
		if (needsComma) { *(insert++) = ','; }
		needsComma = YES;
		*(insert++) = 'G';
		memcpy(insert, o->getter_name, getterLength);
		insert += getterLength;
	}
	if ((o->attributes & OBJC_PR_setter) == OBJC_PR_setter)
	{
		if (needsComma) { *(insert++) = ','; }
		needsComma = YES;
		*(insert++) = 'S';
		memcpy(insert, o->setter_name, setterLength);
		insert += setterLength;
	}
	if (needsComma) { *(insert++) = ','; }
	*(insert++) = 'V';
	memcpy(insert, name, nameSize);
	insert += nameSize;
	*(insert++) = '\0';
	assert((size_t)(insert - encoding) == encodingSize);

	n->attributes = (const char*)encoding;
}

static struct objc_property_list *upgradePropertyList(struct objc_property_list_gsv1 *l)
{
	if (l == NULL)
	{
		return NULL;
	}
	struct objc_property_list *n = legacy_calloc_flexible_or_abort(
		sizeof(struct objc_property_list), l->count, sizeof(struct objc_property));
	n->count = l->count;
	n->size = sizeof(struct objc_property);
	for (int i=0 ; i<l->count ; i++)
	{
		upgradeProperty(&n->properties[i], &l->properties[i]);
	}
	return n;
}

static int legacy_key;

PRIVATE struct objc_class_gsv1* objc_legacy_class_for_class(Class cls)
{
	return (struct objc_class_gsv1*)objc_getAssociatedObject((id)cls, &legacy_key);
}

PRIVATE Class objc_upgrade_class(struct objc_class_gsv1 *oldClass)
{
	if (oldClass == NULL) { abort(); }
	Class cls = calloc(1, sizeof(struct objc_class));
	if (cls == Nil) { abort(); }
	cls->isa = oldClass->isa;
	// super_class is left nil and we upgrade it later.
	cls->name = oldClass->name;
	cls->version = oldClass->version;
	cls->info = objc_class_flag_meta;
	cls->instance_size = oldClass->instance_size;
	cls->ivars = upgradeIvarList(oldClass);
	cls->methods = upgradeMethodList(oldClass->methods);
	cls->protocols = oldClass->protocols;
	cls->abi_version = oldClass->abi_version;
	cls->properties = upgradePropertyList(oldClass->properties);
	objc_register_selectors_from_class(cls);
	if (!objc_test_class_flag_gsv1(oldClass, objc_class_flag_meta_gsv1))
	{
		cls->info = 0;
		cls->isa = objc_upgrade_class((struct objc_class_gsv1*)cls->isa);
		objc_setAssociatedObject((id)cls, &legacy_key, (id)oldClass, OBJC_ASSOCIATION_ASSIGN);
	}
	else
	{
		cls->instance_size = sizeof(struct objc_class);
	}
	return cls;
}
PRIVATE struct objc_category *objc_upgrade_category(struct objc_category_gcc *old)
{
	if (old == NULL) { abort(); }
	struct objc_category *cat = calloc(1, sizeof(struct objc_category));
	if (cat == NULL) { abort(); }
	memcpy(cat, old, sizeof(struct objc_category_gcc));
	cat->instance_methods = upgradeMethodList(old->instance_methods);
	cat->class_methods = upgradeMethodList(old->class_methods);
	if (cat->instance_methods != NULL)
	{
		objc_register_selectors_from_list(cat->instance_methods);
	}
	if (cat->class_methods != NULL)
	{
		objc_register_selectors_from_list(cat->class_methods);
	}
	if (cat->protocols != NULL)
	{
		objc_init_protocols(cat->protocols);
	}
	return cat;
}

static struct objc_protocol_method_description_list*
upgrade_protocol_method_list_gcc(struct objc_protocol_method_description_list_gcc *l)
{
	if ((l == NULL) || (l->count == 0))
	{
		return NULL;
	}
	struct objc_protocol_method_description_list *n =
		legacy_malloc_flexible_or_abort(
			sizeof(struct objc_protocol_method_description_list), l->count,
			sizeof(struct objc_protocol_method_description));
	n->count = l->count;
	n->size = sizeof(struct objc_protocol_method_description);
	for (int i=0 ; i<n->count ; i++)
	{
		n->methods[i].selector = sel_registerTypedName_np(l->methods[i].name, l->methods[i].types);
		n->methods[i].types = l->methods[i].types;
	}
	return n;
}

PRIVATE struct objc_protocol *objc_upgrade_protocol_gcc(struct objc_protocol_gcc *p)
{
	// If the protocol has already been upgraded, the don't try to upgrade it twice.
	if (p->isa == (id)&_OBJC_CLASS_ProtocolGCC)
	{
		return objc_getProtocol(p->name);
	}
	p->isa = (id)&_OBJC_CLASS_ProtocolGCC;
	Protocol *proto =
		(Protocol*)class_createInstance(&_OBJC_CLASS_Protocol, 0);
	if (proto == NULL) { abort(); }
	proto->name = p->name;
	// Aliasing of this between the new and old structures means that when this
	// returns these will all be updated.
	proto->protocol_list = p->protocol_list;
	proto->instance_methods = upgrade_protocol_method_list_gcc(p->instance_methods);
	proto->class_methods = upgrade_protocol_method_list_gcc(p->class_methods);
	assert(proto->isa);
	return proto;
}

PRIVATE struct objc_protocol *objc_upgrade_protocol_gsv1(struct objc_protocol_gsv1 *p)
{
	// If the protocol has already been upgraded, the don't try to upgrade it twice.
	if (p->isa == (id)&_OBJC_CLASS_ProtocolGSv1)
	{
		return objc_getProtocol(p->name);
	}
	Protocol *n =
		(Protocol*)class_createInstance(&_OBJC_CLASS_Protocol, 0);
	if (n == NULL) { abort(); }
	n->instance_methods = upgrade_protocol_method_list_gcc(p->instance_methods);
	// Aliasing of this between the new and old structures means that when this
	// returns these will all be updated.
	n->name = p->name;
	n->protocol_list = p->protocol_list;
	n->class_methods = upgrade_protocol_method_list_gcc(p->class_methods);
	n->properties = upgradePropertyList(p->properties);
	n->optional_properties = upgradePropertyList(p->optional_properties);
	n->isa = (id)&_OBJC_CLASS_Protocol;
	// We do in-place upgrading of these, because they might be referenced
	// directly
	p->instance_methods = (struct objc_protocol_method_description_list_gcc*)n->instance_methods;
	p->class_methods = (struct objc_protocol_method_description_list_gcc*)n->class_methods;
	p->properties = (struct objc_property_list_gsv1*)n->properties;
	p->optional_properties = (struct objc_property_list_gsv1*)n->optional_properties;
	p->isa = (id)&_OBJC_CLASS_ProtocolGSv1;
	assert(p->isa);
	return n;
}

