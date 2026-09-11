#include "test_support.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "objc/runtime.h"
#include "objc/dispatch/message.h"
#include "objc/support/developer.h"
#include "objc/mosaic.h"
#include "class.h"
#include "method.h"
#include "ivar.h"

typedef uintptr_t (*word_send_fn)(id, SEL);

struct objc_init
{
    uint64_t version;
    void *sel_begin, *sel_end;
    Class *cls_begin, *cls_end;
    Class *cls_ref_begin, *cls_ref_end;
    void *cat_begin, *cat_end;
    void *proto_begin, *proto_end;
    void *proto_ref_begin, *proto_ref_end;
    void *alias_begin, *alias_end;
    void *strings_begin, *strings_end;
};
extern void __objc_load(struct objc_init *init);
static uintptr_t empty_range;
static enum mosaic_objc_runtime_event_kind events[16];
static const char *event_details[16];
static size_t event_count;

static void event_sink(const struct mosaic_objc_runtime_event *event, void *context)
{
    (void)context;
    if ((event->kind != MOSAIC_OBJC_EVENT_CLASS_RELOADED) &&
        (event->kind != MOSAIC_OBJC_EVENT_CLASS_RELOAD_REJECTED))
    {
        return;
    }
    CHECK(event_count < (sizeof(events) / sizeof(events[0])));
    events[event_count] = event->kind;
    event_details[event_count] = event->detail;
    event_count++;
}

static uintptr_t original_value(id self, SEL _cmd)
{
    (void)self; (void)_cmd;
    return 0x1111u;
}

static uintptr_t reloaded_value(id self, SEL _cmd)
{
    (void)self; (void)_cmd;
    return 0x2222u;
}
static uintptr_t reloaded_extra(id self, SEL _cmd)
{
    (void)self; (void)_cmd;
    return 0x3333u;
}

static uintptr_t original_class_value(id self, SEL _cmd)
{
    (void)self; (void)_cmd;
    return 0x4444u;
}

static uintptr_t reloaded_class_value(id self, SEL _cmd)
{
    (void)self; (void)_cmd;
    return 0x5555u;
}

struct one_method_list
{
    struct objc_method_list *next;
    int count;
    size_t size;
    struct objc_method method;
};

struct one_ivar_list
{
    int count;
    size_t size;
    struct objc_ivar ivar;
};

static void init_method_list(struct one_method_list *list, SEL selector, IMP imp,
                             const char *types)
{
    memset(list, 0, sizeof(*list));
    list->count = 1;
    list->size = sizeof(struct objc_method);
    list->method.selector = selector;
    list->method.imp = imp;
    list->method.types = types;
}
static void init_candidate(struct objc_class *cls, struct objc_class *meta,
                           const char *name, size_t instance_size,
                           struct objc_method_list *instance_methods,
                           struct objc_method_list *class_methods)
{
    memset(cls, 0, sizeof(*cls));
    memset(meta, 0, sizeof(*meta));
    meta->name = name;
    meta->info = objc_class_flag_meta;
    meta->methods = class_methods;
    cls->isa = meta;
    cls->name = name;
    cls->instance_size = (long)instance_size;
    cls->methods = instance_methods;
}

static void load_candidate(Class candidate)
{
    Class slots[1] = { candidate };
    struct objc_init init = {0};
    init.sel_begin = init.sel_end = &empty_range;
    init.cls_begin = slots;
    init.cls_end = slots + 1;
    init.cls_ref_begin = init.cls_ref_end = slots + 1;
    init.cat_begin = init.cat_end = &empty_range;
    init.proto_begin = init.proto_end = &empty_range;
    init.proto_ref_begin = init.proto_ref_end = &empty_range;
    init.alias_begin = init.alias_end = &empty_range;
    init.strings_begin = init.strings_end = &empty_range;
    __objc_load(&init);
    CHECK(init.version == ULONG_MAX);
}

int main(void)
{
    mosaic_objc_runtime_initialize();
    mosaic_objc_runtimeSetEventSink(event_sink, NULL);
    objc_setDeveloperMode_np(objc_developer_mode_safe_reload);

    SEL value_sel = sel_registerTypedName_np("safeReloadValue", "Q@:");
    SEL extra_sel = sel_registerTypedName_np("safeReloadExtra", "Q@:");
    SEL class_sel = sel_registerTypedName_np("safeReloadClassValue", "Q@:");
    CHECK(value_sel != NULL && extra_sel != NULL && class_sel != NULL);
    Class original = objc_allocateClassPair(Nil, "MosaicSafeReloadClass", 0);
    CHECK(original != Nil);
    CHECK(class_addMethod(original, value_sel,
                          (IMP)(void*)original_value, "Q@:") == YES);
    CHECK(class_addMethod(object_getClass((id)original), class_sel,
                          (IMP)(void*)original_class_value, "Q@:") == YES);
    objc_registerClassPair(original);

    word_send_fn send = (word_send_fn)(void*)objc_msgSend;
    id existing = class_createInstance(original, 0);
    CHECK(existing != nil);
    CHECK(send(existing, value_sel) == 0x1111u);
    CHECK(send((id)original, class_sel) == 0x4444u);

    struct one_method_list reload_primary;
    struct one_method_list reload_extra;
    struct one_method_list reload_class;
    init_method_list(&reload_primary, (SEL)(void*)"safeReloadValue",
                     (IMP)(void*)reloaded_value, "Q@:");
    init_method_list(&reload_extra, (SEL)(void*)"safeReloadExtra",
                     (IMP)(void*)reloaded_extra, "Q@:");
    init_method_list(&reload_class, (SEL)(void*)"safeReloadClassValue",
                     (IMP)(void*)reloaded_class_value, "Q@:");
    reload_primary.next = (struct objc_method_list *)&reload_extra;

    struct objc_class candidate;
    struct objc_class candidate_meta;
    init_candidate(&candidate, &candidate_meta, "MosaicSafeReloadClass",
                   class_getInstanceSize(original), (struct objc_method_list *)&reload_primary,
                   (struct objc_method_list *)&reload_class);

    load_candidate(&candidate);
    CHECK(objc_lookUpClass("MosaicSafeReloadClass") == original);
    CHECK(object_getClass(existing) == original);
    CHECK(event_count == 1);
    CHECK(events[0] == MOSAIC_OBJC_EVENT_CLASS_RELOADED);
    CHECK(event_details[0] != NULL && strcmp(event_details[0], "layout-compatible") == 0);
    CHECK(original->methods != (struct objc_method_list *)&reload_primary);
    Method live_value = class_getInstanceMethod(original, value_sel);
    Method live_extra = class_getInstanceMethod(original, extra_sel);
    Method live_class = class_getInstanceMethod(object_getClass((id)original), class_sel);
    CHECK(live_value != NULL && method_getImplementation(live_value) == (IMP)(void*)reloaded_value);
    CHECK(live_extra != NULL && method_getImplementation(live_extra) == (IMP)(void*)reloaded_extra);
    CHECK(live_class != NULL && method_getImplementation(live_class) == (IMP)(void*)reloaded_class_value);
    memset(&reload_primary, 0, sizeof(reload_primary));
    memset(&reload_extra, 0, sizeof(reload_extra));
    memset(&reload_class, 0, sizeof(reload_class));
    CHECK(send(existing, value_sel) == 0x2222u);
    CHECK(send(existing, extra_sel) == 0x3333u);
    CHECK(send((id)original, class_sel) == 0x5555u);

    struct one_method_list rejected_method;
    init_method_list(&rejected_method, (SEL)(void*)"safeReloadValue",
                     (IMP)(void*)original_value, "Q@:");
    struct objc_class rejected;
    struct objc_class rejected_meta;
    init_candidate(&rejected, &rejected_meta, "MosaicSafeReloadClass",
                   class_getInstanceSize(original), (struct objc_method_list *)&rejected_method, NULL);

    int rejected_offset = (int)class_getInstanceSize(original);
    struct one_ivar_list added_ivar;
    memset(&added_ivar, 0, sizeof(added_ivar));
    added_ivar.count = 1;
    added_ivar.size = sizeof(struct objc_ivar);
    added_ivar.ivar.name = "newStorage";
    added_ivar.ivar.type = "@";
    added_ivar.ivar.offset = &rejected_offset;
    added_ivar.ivar.size = (uint32_t)sizeof(void*);
    rejected.ivars = (struct objc_ivar_list *)&added_ivar;

    load_candidate(&rejected);
    CHECK(objc_lookUpClass("MosaicSafeReloadClass") == original);
    CHECK(object_getClass(existing) == original);
    CHECK(send(existing, value_sel) == 0x2222u);
    CHECK(send(existing, extra_sel) == 0x3333u);
    CHECK(send((id)original, class_sel) == 0x5555u);
    CHECK(event_count == 2);
    CHECK(events[1] == MOSAIC_OBJC_EVENT_CLASS_RELOAD_REJECTED);
    CHECK(event_details[1] != NULL && strcmp(event_details[1], "ivar-presence") == 0);

    SEL rejected_extra_sel = sel_registerTypedName_np("safeReloadRejectedExtra", "Q@:");
    CHECK(rejected_extra_sel != NULL);
    struct one_method_list signature_extra;
    struct one_method_list signature_conflict;
    init_method_list(&signature_extra, (SEL)(void*)"safeReloadRejectedExtra",
                     (IMP)(void*)reloaded_extra, "Q@:");
    init_method_list(&signature_conflict, (SEL)(void*)"safeReloadValue",
                     (IMP)(void*)original_value, "I@:");
    signature_extra.next = (struct objc_method_list *)&signature_conflict;
    struct objc_class signature_rejected;
    struct objc_class signature_rejected_meta;
    init_candidate(&signature_rejected, &signature_rejected_meta, "MosaicSafeReloadClass",
                   class_getInstanceSize(original),
                   (struct objc_method_list *)&signature_extra, NULL);
    load_candidate(&signature_rejected);
    CHECK(send(existing, value_sel) == 0x2222u);
    CHECK(class_getInstanceMethod(original, rejected_extra_sel) == NULL);
    CHECK(event_count == 3);
    CHECK(events[2] == MOSAIC_OBJC_EVENT_CLASS_RELOAD_REJECTED);
    CHECK(event_details[2] != NULL && strcmp(event_details[2], "method-signature") == 0);

    SEL child_sel = sel_registerTypedName_np("safeReloadChildValue", "Q@:");
    Class base = objc_allocateClassPair(Nil, "MosaicSafeReloadBase", 0);
    CHECK(base != Nil);
    objc_registerClassPair(base);
    Class child = objc_allocateClassPair(base, "MosaicSafeReloadChild", 0);
    CHECK(child != Nil);
    CHECK(class_addMethod(child, child_sel, (IMP)(void*)original_value, "Q@:") == YES);
    objc_registerClassPair(child);
    id child_object = class_createInstance(child, 0);
    CHECK(child_object != nil && send(child_object, child_sel) == 0x1111u);
    struct one_method_list child_reload_method;
    init_method_list(&child_reload_method, (SEL)(void*)"safeReloadChildValue",
                     (IMP)(void*)reloaded_value, "Q@:");
    struct objc_class child_candidate;
    struct objc_class child_candidate_meta;
    init_candidate(&child_candidate, &child_candidate_meta, "MosaicSafeReloadChild",
                   class_getInstanceSize(child),
                   (struct objc_method_list *)&child_reload_method, NULL);
    child_candidate.super_class = (Class)(void*)"MosaicSafeReloadBase";
    load_candidate(&child_candidate);
    CHECK(objc_lookUpClass("MosaicSafeReloadChild") == child);
    CHECK(class_getSuperclass(child) == base);
    CHECK(object_getClass(child_object) == child);
    CHECK(send(child_object, child_sel) == 0x2222u);
    CHECK(event_count == 4 && events[3] == MOSAIC_OBJC_EVENT_CLASS_RELOADED);
    object_dispose(child_object);

    mosaic_objc_runtimeSetEventSink(NULL, NULL);
    objc_setDeveloperMode_np(objc_developer_mode_user);
    object_dispose(existing);
    return 0;
}
