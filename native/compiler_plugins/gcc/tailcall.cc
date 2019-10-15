// This file is GPL2 licensed
#include <gcc-plugin.h>
#include <plugin-version.h>
#include <tree.h>
#include <stringpool.h>
#include <attribs.h>

int plugin_is_GPL_compatible;

static tree process_tailcall(tree* tp, int* walk_subtrees ATTRIBUTE_UNUSED, void* data ATTRIBUTE_UNUSED) {
    static bool tailcall = false;
    const char* name = get_name(*tp);
    if (name && !strcmp(name, "__tailcall__"))
        tailcall = true;
    if (tailcall && TREE_CODE(*tp) == CALL_EXPR) {
        CALL_EXPR_MUST_TAIL_CALL(*tp) = true;
        tailcall = false;
    }
    return NULL_TREE;
}

static void walk_callback(void *gcc_data ATTRIBUTE_UNUSED, void *user_data ATTRIBUTE_UNUSED) {
    tree fndecl = (tree)gcc_data;
    gcc_assert(TREE_CODE(fndecl) == FUNCTION_DECL);
    tree* attrs = &TYPE_ATTRIBUTES(TREE_TYPE(fndecl));
    if (lookup_attribute("tailcallable", *attrs))
        *attrs = tree_cons(get_identifier("noreturn"), 0, *attrs);
    walk_tree(&DECL_SAVED_TREE(fndecl), process_tailcall, 0, 0);
}

static attribute_spec tailcallable_attr = {
    "tailcallable", 0, 0, false, true, true,
#if GCC_VERSION >= 8000
    true, 0, 0
#else
    0, true
#endif
};

static void attribute_callback(void *gcc_data ATTRIBUTE_UNUSED, void *user_data ATTRIBUTE_UNUSED) {
    register_attribute(&tailcallable_attr);
}

int plugin_init(plugin_name_args *plugin_info, plugin_gcc_version *version) {
    if (!plugin_default_version_check(version, &gcc_version))
        return 1;
    const char *plugin_name = plugin_info->base_name;
    register_callback(plugin_name, PLUGIN_ATTRIBUTES, attribute_callback, 0);
    register_callback(plugin_name, PLUGIN_PRE_GENERICIZE, walk_callback, 0);
    return 0;
}
