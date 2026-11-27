#ifndef TKGPU_OBJECT_INTERNAL_H
#define TKGPU_OBJECT_INTERNAL_H
#include "TKGPUObject.H"
#include "TKGPUAbiObject.H"
struct tg_object {
    TKGPU_ABI_OBJECT_FIELDS;
    unsigned char *binary;
    size_t binary_length;
    tg_object **list_items;
    size_t list_count;
    int list_ready;
    uint16_t *unicode;
    int unicode_length;
};
void tg_object_destroy(tg_object *o);
void tg_object_clear_type(tg_object *o);
void tg_object_clear_caches(tg_object *o);
/* Compatibility constructors use the Tcl zero-reference convention. */
Tcl_Obj *tg_compat_new_object(Tcl_Obj *o);
#endif
