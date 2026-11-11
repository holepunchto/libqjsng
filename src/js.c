#include <assert.h>
#include <intrusive.h>
#include <intrusive/list.h>
#include <js.h>
#include <limits.h>
#include <math.h>
#include <quickjs.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__GLIBC__)
#include <malloc.h>
#elif defined(_WIN32)
#include <malloc.h>
#endif

#define JS_HANDLE_SCOPE_PAGE_SIZE 32

typedef struct js_handle_scope_page_s js_handle_scope_page_t;
typedef struct js_callback_s js_callback_t;
typedef struct js_microtask_s js_microtask_t;
typedef struct js_finalizer_s js_finalizer_t;
typedef struct js_finalizer_list_s js_finalizer_list_t;
typedef struct js_delegate_s js_delegate_t;
typedef struct js_module_resolver_s js_module_resolver_t;
typedef struct js_arraybuffer_header_s js_arraybuffer_header_t;
typedef struct js_promise_rejection_s js_promise_rejection_t;
typedef struct js_threadsafe_queue_s js_threadsafe_queue_t;
typedef struct js_teardown_task_s js_teardown_task_t;
typedef struct js_teardown_queue_s js_teardown_queue_t;
typedef struct js_unit_s js_unit_t;

struct js_garbage_collection_tracking_s {
  js_garbage_collection_cb start;
  js_garbage_collection_cb end;
  void *data;
};

struct js_deferred_teardown_s {
  js_env_t *env;
};

struct js_teardown_task_s {
  enum {
    js_immediate_teardown,
    js_deferred_teardown,
  } type;

  union {
    struct {
      js_teardown_cb cb;
    } immediate;

    struct {
      js_deferred_teardown_t handle;
      js_deferred_teardown_cb cb;
    } deferred;
  };

  void *data;
  intrusive_list_node_t list;
};

struct js_teardown_queue_s {
  intrusive_list_t tasks;
};

struct js_platform_s {
  js_platform_options_t options;
  uv_loop_t *loop;
};

struct js_env_s {
  uv_loop_t *loop;
  uv_prepare_t prepare;
  uv_check_t check;
  uv_async_t teardown;
  int active_handles;

  js_platform_t *platform;
  js_handle_scope_t *scope;

  uint32_t refs;
  uint32_t depth;

  JSRuntime *runtime;
  JSContext *context;
  JSValue bindings;

  JSValue default_module_id;
  JSAtom function_bytecode_key;
  JSAtom error_source_key;
  JSAtom prototype_key;
  JSAtom name_key;
  JSAtom length_key;

  int64_t external_memory;

  js_module_resolver_t *resolvers;
  intrusive_list_t synthetic_modules;

  intrusive_list_t deferreds;
  intrusive_list_t finalizers;
  intrusive_list_t pending_finalizers;

  bool destroying;

  intrusive_list_t promise_rejections;

  js_teardown_queue_t teardown_queue;

  struct {
    js_uncaught_exception_cb uncaught_exception;
    void *uncaught_exception_data;

    js_unhandled_rejection_cb unhandled_rejection;
    void *unhandled_rejection_data;

    js_dynamic_import_cb dynamic_import;
    void *dynamic_import_data;
  } callbacks;
};

struct js_value_s {
  JSValue value;
};

struct js_unit_s {
  JSValue id;
  uint64_t source_hash;
  js_dynamic_import_cb dynamic_import;
  void *dynamic_import_data;
};

struct js_handle_scope_page_s {
  js_handle_scope_page_t *next;
  size_t len;
  js_value_t values[JS_HANDLE_SCOPE_PAGE_SIZE];
};

struct js_handle_scope_s {
  js_handle_scope_t *parent;
  js_handle_scope_page_t *page;
};

struct js_escapable_handle_scope_s {
  js_handle_scope_t *parent;
};

typedef enum {
  js_module_uninstantiated,
  js_module_instantiating,
  js_module_instantiated,
  js_module_evaluating,
  js_module_evaluated,
  js_module_errored,
} js_module_state_t;

struct js_module_s {
  JSContext *context;
  JSValue bytecode;
  JSValue unit;
  JSModuleDef *definition;
  js_module_meta_cb meta;
  void *meta_data;
  char *name;
  js_module_state_t state;
  bool synthetic;
  bool aborted;
  js_module_evaluate_cb evaluate;
  void *evaluate_data;
  intrusive_list_node_t list;
};

struct js_script_s {
  JSContext *context;
  JSValue bytecode;
  JSContext *foreign_context;
  JSValue foreign_bytecode;
  JSValue unit;
  char *name;
};

struct js_module_resolver_s {
  js_module_t *module;
  js_module_resolve_cb cb;
  void *data;
  js_module_resolver_t *next;
};

struct js_context_s {
  JSContext *context;
  JSContext *previous;
};

struct js_ref_s {
  JSValue value;
  JSValue weakref;
  uint32_t count;
  bool finalized;
};

struct js_deferred_s {
  JSValue resolve;
  JSValue reject;
  intrusive_list_node_t list;
};

struct js_string_view_s {
  JSValue string;
};

struct js_finalizer_s {
  void *data;
  js_finalize_cb finalize_cb;
  void *finalize_hint;
  bool tracked;
  intrusive_list_node_t list;
};

struct js_finalizer_list_s {
  js_finalizer_t finalizer;
  js_finalizer_list_t *next;
};

struct js_delegate_s {
  js_delegate_callbacks_t callbacks;
  void *data;
  js_finalizer_t finalizer;
};

struct js_callback_s {
  js_function_cb cb;
  void *data;
};

struct js_microtask_s {
  js_env_t *env;
  js_task_cb cb;
  void *data;
};

struct js_callback_info_s {
  js_callback_t *callback;
  int argc;
  JSValue *argv;
  JSValue receiver;
  JSValue new_target;
};

struct js_arraybuffer_header_s {
  atomic_int references;
  size_t len;
  uint8_t *data;
};

struct js_arraybuffer_backing_store_s {
  atomic_int references;
  size_t len;
  uint8_t *data;
  bool external;
  js_finalize_cb finalize_cb;
  void *finalize_hint;
};

struct js_promise_rejection_s {
  JSValue promise;
  JSValue reason;
  intrusive_list_node_t list;
};

static const uint8_t js_threadsafe_function_idle = 0x0;
static const uint8_t js_threadsafe_function_running = 0x1;
static const uint8_t js_threadsafe_function_pending = 0x2;

struct js_threadsafe_queue_s {
  void **queue;
  size_t len;
  size_t capacity;
  size_t head;
  size_t tail;
  bool closed;
  uv_mutex_t lock;
};

struct js_threadsafe_function_s {
  JSValue function;
  js_env_t *env;
  uv_async_t async;
  js_threadsafe_queue_t queue;
  atomic_int state;
  atomic_int thread_count;
  void *context;
  js_finalize_cb finalize_cb;
  void *finalize_hint;
  js_threadsafe_function_cb cb;
};

static const char *js__platform_identifier = "quickjs-ng";

static void
js__run_pending_finalizers(js_env_t *env);

static void
js__on_external_finalize(JSRuntime *runtime, JSValueConst value);

static JSClassID js__external_class_id;

static JSClassDef js__external_class = {
  .class_name = "External",
  .finalizer = js__on_external_finalize,
};

static void
js__on_finalizer_finalize(JSRuntime *runtime, JSValueConst value);

static JSClassID js__finalizer_class_id;

static JSClassDef js__finalizer_class = {
  .class_name = "Finalizer",
  .finalizer = js__on_finalizer_finalize,
};

static void
js__on_type_tag_finalize(JSRuntime *runtime, JSValueConst value);

static JSClassID js__type_tag_class_id;

static JSClassDef js__type_tag_class = {
  .class_name = "TypeTag",
  .finalizer = js__on_type_tag_finalize,
};

static void
js__on_function_finalize(JSRuntime *runtime, JSValueConst value);

static JSValue
js__on_function_call(JSContext *context, JSValueConst function, JSValueConst receiver, int argc, JSValueConst *argv, int flags);

static JSClassID js__function_class_id;

static JSClassDef js__function_class = {
  .class_name = "Function",
  .finalizer = js__on_function_finalize,
  .call = js__on_function_call,
};

static void
js__on_microtask_finalize(JSRuntime *runtime, JSValueConst value);

static JSClassID js__microtask_class_id;

static JSClassDef js__microtask_class = {
  .class_name = "Microtask",
  .finalizer = js__on_microtask_finalize,
};

static void
js__on_unit_finalize(JSRuntime *runtime, JSValueConst value);

static JSClassID js__unit_class_id;

static JSClassDef js__unit_class = {
  .class_name = "Unit",
  .finalizer = js__on_unit_finalize,
};

static void
js__on_dynamic_import(JSContext *context, const char *referrer, const char *specifier, JSValue token, JSValueConst attributes, void *opaque);

static int
js__on_delegate_get_own_property(JSContext *context, JSPropertyDescriptor *descriptor, JSValueConst object, JSAtom property);

static int
js__on_delegate_get_own_property_names(JSContext *context, JSPropertyEnum **properties, uint32_t *len, JSValueConst object);

static int
js__on_delegate_delete_property(JSContext *context, JSValueConst object, JSAtom property);

static int
js__on_delegate_set_property(JSContext *context, JSValueConst object, JSAtom property, JSValueConst value, JSValueConst receiver, int flags);

static void
js__on_delegate_finalize(JSRuntime *runtime, JSValueConst value);

static JSClassID js__delegate_class_id;

static JSClassDef js__delegate_class = {
  .class_name = "Delegate",
  .finalizer = js__on_delegate_finalize,
  .exotic = &(JSClassExoticMethods){
    .get_own_property = js__on_delegate_get_own_property,
    .get_own_property_names = js__on_delegate_get_own_property_names,
    .delete_property = js__on_delegate_delete_property,
    .set_property = js__on_delegate_set_property,
  },
};

int
js_create_platform(uv_loop_t *loop, const js_platform_options_t *options, js_platform_t **result) {
  js_platform_t *platform = malloc(sizeof(js_platform_t));

  platform->loop = loop;
  platform->options = options ? *options : (js_platform_options_t){};

  *result = platform;

  return 0;
}

int
js_destroy_platform(js_platform_t *platform) {
  free(platform);

  return 0;
}

int
js_get_platform_identifier(js_platform_t *platform, const char **result) {
  *result = js__platform_identifier;

  return 0;
}

int
js_get_platform_version(js_platform_t *platform, const char **result) {
  *result = JS_GetVersion();

  return 0;
}

int
js_get_platform_limits(js_platform_t *platform, js_platform_limits_t *result) {
  result->arraybuffer_length = INT32_MAX;
  result->string_length = 0x3fffffff;

  return 0;
}

int
js_get_platform_loop(js_platform_t *platform, uv_loop_t **result) {
  *result = platform->loop;

  return 0;
}

static char *
js__on_normalize_module(JSContext *context, const char *base_name, const char *name, void *opaque) {
  // QuickJS uses the normalized name as its module cache key and passes it to
  // the loader in place of the specifier. Its default normalizer rewrites
  // relative specifiers, which denies the host the specifier its own resolver
  // needs. Key on the referrer's resolved name and the verbatim specifier
  // instead, separated by a byte that occurs in neither.

  js_env_t *env = (js_env_t *) JS_GetContextOpaque(context);

  js_module_resolver_t *resolver = env->resolvers;

  const char *referrer = resolver && resolver->module && resolver->module->name
                           ? resolver->module->name
                           : base_name;

  size_t referrer_len = strlen(referrer);
  size_t name_len = strlen(name);

  char *cname = js_malloc(context, referrer_len + 1 + name_len + 1);

  if (cname == NULL) return NULL;

  memcpy(cname, referrer, referrer_len);
  cname[referrer_len] = '\x1f';
  memcpy(cname + referrer_len + 1, name, name_len + 1);

  return cname;
}

static JSModuleDef *
js__on_resolve_module(JSContext *context, const char *name, void *opaque) {
  int err;

  js_env_t *env = (js_env_t *) JS_GetContextOpaque(context);

  js_module_resolver_t *resolver = env->resolvers;

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  js_module_t *module;

  const char *separator = strrchr(name, '\x1f');

  js_value_t specifier = {
    .value = JS_NewString(env->context, separator ? separator + 1 : name),
  };

  js_value_t assertions = {
    .value = JS_NULL,
  };

  JSModuleDef *definition = NULL;

  if (resolver) {
    module = resolver->cb(
      env,
      &specifier,
      &assertions,
      resolver->module,
      resolver->data
    );

    if (module == NULL) goto done;

    // Resolve the dependency's own imports under its referrer frame before
    // returning it, so nested imports are attributed to the correct referrer.

    err = js_instantiate_module(
      env,
      module,
      resolver->cb,
      resolver->data
    );

    if (err < 0) goto done;

    definition = module->definition;
  } else {
    err = js_throw_error(env, NULL, "Dynamic import() is not supported");
    assert(err == 0);
  }

done:
  JS_FreeValue(env->context, specifier.value);

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  return definition;
}

static inline void
js__uncaught_exception(js_env_t *env, JSValue error) {
  int err;

  if (JS_IsUncatchableError(error)) {
    JS_FreeValue(env->context, error);

    return;
  }

  if (env->callbacks.uncaught_exception) {
    js_handle_scope_t *scope;
    err = js_open_handle_scope(env, &scope);
    assert(err == 0);

    env->callbacks.uncaught_exception(
      env,
      &(js_value_t){error},
      env->callbacks.uncaught_exception_data
    );

    err = js_close_handle_scope(env, scope);
    assert(err == 0);

    JS_FreeValue(env->context, error);
  } else {
    JS_Throw(env->context, error);
  }
}

static void
js__on_unhandled_rejection(JSContext *context, JSValue promise, JSValue reason) {
  int err;

  js_env_t *env = (js_env_t *) JS_GetContextOpaque(context);

  if (env->callbacks.unhandled_rejection) {
    js_handle_scope_t *scope;
    err = js_open_handle_scope(env, &scope);
    assert(err == 0);

    env->callbacks.unhandled_rejection(
      env,
      &(js_value_t){reason},
      &(js_value_t){promise},
      env->callbacks.unhandled_rejection_data
    );

    err = js_close_handle_scope(env, scope);
    assert(err == 0);
  }

  JS_FreeValue(context, promise);
  JS_FreeValue(context, reason);
}

static void
js__on_promise_rejection(JSContext *context, JSValueConst promise, JSValueConst reason, bool is_handled, void *opaque) {
  js_env_t *env = (js_env_t *) JS_GetContextOpaque(context);

  if (env->callbacks.unhandled_rejection == NULL) return;

  if (is_handled) {
    intrusive_list_for_each(next, &env->promise_rejections) {
      js_promise_rejection_t *rejection = intrusive_entry(next, js_promise_rejection_t, list);

      if (JS_VALUE_GET_PTR(rejection->promise) == JS_VALUE_GET_PTR(promise)) {
        intrusive_list_remove(&env->promise_rejections, &rejection->list);

        JS_FreeValue(context, rejection->promise);
        JS_FreeValue(context, rejection->reason);

        return free(rejection);
      }
    }
  } else {
    js_promise_rejection_t *rejection = malloc(sizeof(js_promise_rejection_t));

    rejection->promise = JS_DupValue(context, promise);
    rejection->reason = JS_DupValue(context, reason);

    intrusive_list_append(&env->promise_rejections, &rejection->list);
  }
}

static inline bool
js__is_terminating(js_env_t *env) {
  if (!JS_HasException(env->context)) return false;

  JSValue error = JS_GetException(env->context);

  bool terminating = JS_IsUncatchableError(error);

  JS_Throw(env->context, error);

  return terminating;
}

static inline void
js__run_pending_jobs(js_env_t *env) {
  int err;

  for (;;) {
    JSContext *context;

    err = JS_ExecutePendingJob(env->runtime, &context);

    if (err == 0) break;

    if (err < 0) {
      JSValue error = JS_GetException(context);

      js_handle_scope_t *scope;
      err = js_open_handle_scope(env, &scope);
      assert(err == 0);

      js__uncaught_exception(env, error);

      err = js_close_handle_scope(env, scope);
      assert(err == 0);
    }
  }
}

static inline void
js__run_microtasks(js_env_t *env) {
  // Terminating execution skips the checkpoint, leaving the queue as it is.
  if (js__is_terminating(env)) return;

  env->depth++;

  js__run_pending_jobs(env);

  while (!intrusive_list_empty(&env->promise_rejections)) {
    js_promise_rejection_t *rejection = intrusive_entry(env->promise_rejections.head, js_promise_rejection_t, list);

    intrusive_list_remove(&env->promise_rejections, &rejection->list);

    js__on_unhandled_rejection(env->context, rejection->promise, rejection->reason);

    free(rejection);

    // An exception thrown by the handler is left pending for the caller, and
    // the remaining rejections for the next checkpoint.
    if (JS_HasException(env->context)) break;

    js__run_pending_jobs(env);
  }

  env->depth--;

  // A checkpoint is where a collection would have run the finalizers of what
  // the JavaScript just run let go of.
  js__run_pending_finalizers(env);
}

static void
js__on_prepare(uv_prepare_t *handle) {
  // Nothing is queued on the loop's behalf, so there is nothing to run before
  // it blocks.
}

static void
js__on_check(uv_check_t *handle) {
  // Pending finalizers do not hold the loop open, as a collection would not be
  // forced for them either. Were a macrotask queue ever added, its liveness
  // would have to be held as work is queued rather than settled from here: the
  // loop runs timers after the check phase, so what their callbacks queue
  // cannot be observed from a check callback.
  js__run_pending_finalizers((js_env_t *) handle->data);
}

static inline size_t
js__usable_size(const void *ptr) {
#if defined(__APPLE__)
  return malloc_size(ptr);
#elif defined(_WIN32)
  return _msize((void *) ptr);
#elif defined(__linux__) || defined(__GLIBC__)
  return malloc_usable_size((void *) ptr);
#else
  return 0;
#endif
}

static void *
js__on_calloc(void *opaque, size_t count, size_t size) {
  return calloc(count, size);
}

static void *
js__on_malloc(void *opaque, size_t len) {
  return malloc(len);
}

static void
js__on_free(void *opaque, void *ptr) {
  free(ptr);
}

static void *
js__on_realloc(void *opaque, void *ptr, size_t len) {
  return realloc(ptr, len);
}

static size_t
js__on_usable_size(const void *ptr) {
  return js__usable_size(ptr);
}

static inline js_arraybuffer_header_t *
js__arraybuffer_header(void *ptr) {
  return (js_arraybuffer_header_t *) ((char *) ptr - sizeof(js_arraybuffer_header_t));
}

static void *
js__on_arraybuffer_malloc(void *opaque, size_t len) {
  js_arraybuffer_header_t *header = malloc(sizeof(js_arraybuffer_header_t) + len);

  if (header == NULL) return NULL;

  header->len = len;
  header->data = (uint8_t *) header + sizeof(*header);

  atomic_init(&header->references, 1);

  return header->data;
}

static void
js__on_arraybuffer_free(void *opaque, void *ptr) {
  js_arraybuffer_header_t *header = js__arraybuffer_header(ptr);

  if (atomic_fetch_sub_explicit(&header->references, 1, memory_order_acq_rel) == 1) {
    free(header);
  }
}

static void
js__on_arraybuffer_dup(void *opaque, void *ptr) {
  js_arraybuffer_header_t *header = js__arraybuffer_header(ptr);

  atomic_fetch_add_explicit(&header->references, 1, memory_order_acq_rel);
}

static void *
js__on_arraybuffer_realloc(JSRuntime *runtime, void *opaque, void *ptr, size_t len) {
  if (ptr == NULL) return js__on_arraybuffer_malloc(opaque, len);

  if (len == 0) {
    js__on_arraybuffer_free(opaque, ptr);

    return NULL;
  }

  js_arraybuffer_header_t *header = js__arraybuffer_header(ptr);

  // Memory shared with a backing store must stay where it is, so the buffer
  // moves to a copy instead.
  if (atomic_load_explicit(&header->references, memory_order_acquire) > 1) {
    void *copy = js__on_arraybuffer_malloc(opaque, len);

    if (copy == NULL) return NULL;

    memcpy(copy, ptr, len < header->len ? len : header->len);

    js__on_arraybuffer_free(opaque, ptr);

    return copy;
  }

  header = realloc(header, sizeof(js_arraybuffer_header_t) + len);

  if (header == NULL) return NULL;

  header->len = len;
  header->data = (uint8_t *) header + sizeof(*header);

  return header->data;
}

static void
js__on_handle_close(uv_handle_t *handle) {
  js_env_t *env = (js_env_t *) handle->data;

  if (--env->active_handles == 0) free(env);
}

static inline void
js__init_finalizer(js_env_t *env, js_finalizer_t *finalizer, void *data, js_finalize_cb finalize_cb, void *finalize_hint) {
  finalizer->data = data;
  finalizer->finalize_cb = finalize_cb;
  finalizer->finalize_hint = finalize_hint;
  finalizer->tracked = finalize_cb != NULL;

  if (finalizer->tracked) intrusive_list_prepend(&env->finalizers, &finalizer->list);
}

static inline void
js__untrack_finalizer(js_env_t *env, js_finalizer_t *finalizer) {
  if (finalizer->tracked) intrusive_list_remove(&env->finalizers, &finalizer->list);

  finalizer->tracked = false;
}

static inline void
js__run_finalizer(js_env_t *env, js_finalizer_t *finalizer) {
  js__untrack_finalizer(env, finalizer);

  js_finalize_cb finalize_cb = finalizer->finalize_cb;

  finalizer->finalize_cb = NULL;

  if (finalize_cb) finalize_cb(env, finalizer->data, finalizer->finalize_hint);
}

// The engine frees objects as soon as their last reference is dropped, which
// may happen inside a call made by native code that is not yet done with what
// the callback of a finalizer releases. Callbacks are therefore queued and run
// once control is back with the loop, as they would be after a collection.
static void
js__defer_finalize(js_env_t *env, void *data, js_finalize_cb finalize_cb, void *finalize_hint) {
  if (finalize_cb == NULL) return;

  js_finalizer_t *finalizer = malloc(sizeof(js_finalizer_t));

  finalizer->data = data;
  finalizer->finalize_cb = finalize_cb;
  finalizer->finalize_hint = finalize_hint;
  finalizer->tracked = false;

  intrusive_list_append(&env->pending_finalizers, &finalizer->list);
}

static inline void
js__defer_finalizer(js_env_t *env, js_finalizer_t *finalizer) {
  js__untrack_finalizer(env, finalizer);

  js__defer_finalize(env, finalizer->data, finalizer->finalize_cb, finalizer->finalize_hint);
}

static void
js__run_pending_finalizers(js_env_t *env) {
  while (!intrusive_list_empty(&env->pending_finalizers)) {
    js_finalizer_t *finalizer = intrusive_entry(env->pending_finalizers.head, js_finalizer_t, list);

    intrusive_list_remove(&env->pending_finalizers, &finalizer->list);

    finalizer->finalize_cb(env, finalizer->data, finalizer->finalize_hint);

    free(finalizer);
  }
}

static void
js__close_env(js_env_t *env) {
  intrusive_list_for_each(next, &env->promise_rejections) {
    js_promise_rejection_t *rejection = intrusive_entry(next, js_promise_rejection_t, list);

    intrusive_list_remove(&env->promise_rejections, &rejection->list);

    JS_FreeValue(env->context, rejection->promise);
    JS_FreeValue(env->context, rejection->reason);

    free(rejection);
  }

  intrusive_list_for_each(next, &env->deferreds) {
    js_deferred_t *deferred = intrusive_entry(next, js_deferred_t, list);

    intrusive_list_remove(&env->deferreds, &deferred->list);

    JS_FreeValue(env->context, deferred->resolve);
    JS_FreeValue(env->context, deferred->reject);

    free(deferred);
  }

  // The callback of a finalizer may release what keeps another alive, so both
  // lists are drained from the head until neither has anything left.
  for (;;) {
    js__run_pending_finalizers(env);

    if (intrusive_list_empty(&env->finalizers)) break;

    js__run_finalizer(env, intrusive_entry(env->finalizers.head, js_finalizer_t, list));
  }

  JS_FreeValue(env->context, env->default_module_id);
  JS_FreeAtom(env->context, env->function_bytecode_key);
  JS_FreeAtom(env->context, env->error_source_key);
  JS_FreeAtom(env->context, env->prototype_key);
  JS_FreeAtom(env->context, env->name_key);
  JS_FreeAtom(env->context, env->length_key);
  JS_FreeValue(env->context, env->bindings);
  JS_FreeContext(env->context);
  JS_FreeRuntime(env->runtime);

  // Freeing the runtime releases the backing stores of external ArrayBuffers
  // that were still alive, whose callbacks are not tracked as finalizers.
  js__run_pending_finalizers(env);

  uv_close((uv_handle_t *) &env->prepare, js__on_handle_close);
  uv_close((uv_handle_t *) &env->check, js__on_handle_close);
  uv_close((uv_handle_t *) &env->teardown, js__on_handle_close);
}

static void
js__on_teardown(uv_async_t *handle) {
  js_env_t *env = (js_env_t *) handle->data;

  if (env->refs == 0) js__close_env(env);
}

static JSAtom
js__create_private_key(JSContext *context, const char *description) {
  JSValue symbol = JS_NewPrivateSymbol(context, description);

  JSAtom atom = JS_ValueToAtom(context, symbol);

  JS_FreeValue(context, symbol);

  return atom;
}

int
js_create_env(uv_loop_t *loop, js_platform_t *platform, const js_env_options_t *options, js_env_t **result) {
  int err;

  JSRuntime *runtime = JS_NewRuntime2(
    &(JSMallocFunctions){
      .js_calloc = js__on_calloc,
      .js_malloc = js__on_malloc,
      .js_free = js__on_free,
      .js_realloc = js__on_realloc,
      .js_malloc_usable_size = js__on_usable_size,
    },
    NULL
  );

  JS_SetSharedArrayBufferFunctions(
    runtime,
    &(JSSharedArrayBufferFunctions){
      .sab_alloc = js__on_arraybuffer_malloc,
      .sab_free = js__on_arraybuffer_free,
      .sab_dup = js__on_arraybuffer_dup,
      .sab_opaque = NULL,
    }
  );

  JS_SetArrayBufferReallocFunc(runtime, js__on_arraybuffer_realloc, NULL);

  JS_SetMaxStackSize(runtime, 0);
  JS_SetCanBlock(runtime, false);
  JS_SetModuleLoaderFunc(runtime, js__on_normalize_module, js__on_resolve_module, NULL);
  JS_SetHostPromiseRejectionTracker(runtime, js__on_promise_rejection, NULL);

  if (options && options->memory_limit) {
    JS_SetMemoryLimit(runtime, options->memory_limit);
  } else {
    uint64_t constrained_memory = uv_get_constrained_memory();
    uint64_t total_memory = uv_get_total_memory();

    if (constrained_memory > 0 && constrained_memory < total_memory) {
      total_memory = constrained_memory;
    }

    if (total_memory > 0) {
      JS_SetMemoryLimit(runtime, total_memory);
    }
  }

  js_env_t *env = malloc(sizeof(js_env_t));

  env->loop = loop;
  env->active_handles = 3;

  env->platform = platform;
  env->scope = NULL;

  env->refs = 0;
  env->depth = 0;

  env->runtime = runtime;
  env->context = JS_NewContext(runtime);
  env->bindings = JS_NewObject(env->context);

  env->default_module_id = JS_NULL;

  env->function_bytecode_key = js__create_private_key(env->context, "functionBytecode");
  env->error_source_key = js__create_private_key(env->context, "errorSource");
  env->prototype_key = JS_NewAtom(env->context, "prototype");
  env->name_key = JS_NewAtom(env->context, "name");
  env->length_key = JS_NewAtom(env->context, "length");

  env->external_memory = 0;

  env->resolvers = NULL;

  env->destroying = false;

  intrusive_list_init(&env->promise_rejections);

  intrusive_list_init(&env->deferreds);
  intrusive_list_init(&env->synthetic_modules);
  intrusive_list_init(&env->finalizers);
  intrusive_list_init(&env->pending_finalizers);
  intrusive_list_init(&env->teardown_queue.tasks);

  env->callbacks.uncaught_exception = NULL;
  env->callbacks.uncaught_exception_data = NULL;

  env->callbacks.unhandled_rejection = NULL;
  env->callbacks.unhandled_rejection_data = NULL;

  env->callbacks.dynamic_import = NULL;
  env->callbacks.dynamic_import_data = NULL;

  JS_SetRuntimeOpaque(env->runtime, env);
  JS_SetContextOpaque(env->context, env);

  // Class IDs are allocated from the runtime, but as every runtime has the same
  // built-in classes, the IDs allocated for the first are valid for all.
  JS_NewClassID(runtime, &js__external_class_id);
  JS_NewClassID(runtime, &js__finalizer_class_id);
  JS_NewClassID(runtime, &js__type_tag_class_id);
  JS_NewClassID(runtime, &js__function_class_id);
  JS_NewClassID(runtime, &js__delegate_class_id);
  JS_NewClassID(runtime, &js__microtask_class_id);
  JS_NewClassID(runtime, &js__unit_class_id);

  err = JS_NewClass(runtime, js__external_class_id, &js__external_class);
  assert(err == 0);

  err = JS_NewClass(runtime, js__finalizer_class_id, &js__finalizer_class);
  assert(err == 0);

  err = JS_NewClass(runtime, js__type_tag_class_id, &js__type_tag_class);
  assert(err == 0);

  err = JS_NewClass(runtime, js__function_class_id, &js__function_class);
  assert(err == 0);

  err = JS_NewClass(runtime, js__delegate_class_id, &js__delegate_class);
  assert(err == 0);

  err = JS_NewClass(runtime, js__microtask_class_id, &js__microtask_class);
  assert(err == 0);

  err = JS_NewClass(runtime, js__unit_class_id, &js__unit_class);
  assert(err == 0);

  JS_SetDynamicImportFunc(runtime, js__on_dynamic_import, env);

  err = uv_prepare_init(loop, &env->prepare);
  assert(err == 0);

  err = uv_prepare_start(&env->prepare, js__on_prepare);
  assert(err == 0);

  env->prepare.data = (void *) env;

  // Neither handle should keep the loop alive; there is nothing queued on the
  // loop's behalf for them to hold it open for.
  uv_unref((uv_handle_t *) &env->prepare);

  err = uv_check_init(loop, &env->check);
  assert(err == 0);

  err = uv_check_start(&env->check, js__on_check);
  assert(err == 0);

  env->check.data = (void *) env;

  uv_unref((uv_handle_t *) &env->check);

  err = uv_async_init(loop, &env->teardown, js__on_teardown);
  assert(err == 0);

  env->teardown.data = (void *) env;

  uv_unref((uv_handle_t *) &env->teardown);

  *result = env;

  return 0;
}

int
js_destroy_env(js_env_t *env) {
  env->destroying = true;

  intrusive_list_for_each(next, &env->teardown_queue.tasks) {
    js_teardown_task_t *task = intrusive_entry(next, js_teardown_task_t, list);

    if (task->type == js_deferred_teardown) {
      task->deferred.cb(&task->deferred.handle, task->data);
    } else {
      task->immediate.cb(task->data);

      intrusive_list_remove(&env->teardown_queue.tasks, &task->list);

      free(task);
    }
  }

  if (env->refs == 0) {
    js__close_env(env);
  } else {
    uv_ref((uv_handle_t *) &env->teardown);
  }

  return 0;
}

int
js_on_uncaught_exception(js_env_t *env, js_uncaught_exception_cb cb, void *data) {
  env->callbacks.uncaught_exception = cb;
  env->callbacks.uncaught_exception_data = data;

  return 0;
}

int
js_on_unhandled_rejection(js_env_t *env, js_unhandled_rejection_cb cb, void *data) {
  env->callbacks.unhandled_rejection = cb;
  env->callbacks.unhandled_rejection_data = data;

  return 0;
}

int
js_on_dynamic_import(js_env_t *env, js_dynamic_import_cb cb, void *data) {
  env->callbacks.dynamic_import = cb;
  env->callbacks.dynamic_import_data = data;

  return 0;
}

int
js_get_env_loop(js_env_t *env, uv_loop_t **result) {
  *result = env->loop;

  return 0;
}

int
js_get_env_platform(js_env_t *env, js_platform_t **result) {
  *result = env->platform;

  return 0;
}

static inline int
js__error(js_env_t *env) {
  return JS_HasException(env->context) ? js_pending_exception : js_uncaught_exception;
}

static inline int
js__report_exception(js_env_t *env) {
  if (env->depth == 0) {
    JSValue error = JS_GetException(env->context);

    js__uncaught_exception(env, error);
  }

  return js__error(env);
}

static void
js__on_unit_finalize(JSRuntime *runtime, JSValueConst value) {
  js_unit_t *unit = (js_unit_t *) JS_GetOpaque(value, js__unit_class_id);

  JS_FreeValueRT(runtime, unit->id);

  free(unit);
}

static JSValue
js__create_unit(js_env_t *env, const char *name, uint64_t source_hash, JSValueConst code) {
  JSValue value = JS_NewObjectClass(env->context, js__unit_class_id);

  js_unit_t *unit = malloc(sizeof(js_unit_t));

  // Mint a unique identifier for the code so it can be recovered as the
  // referrer of any dynamic import().
  unit->id = JS_NewSymbol(env->context, name, false);
  unit->source_hash = source_hash;
  unit->dynamic_import = NULL;
  unit->dynamic_import_data = NULL;

  JS_SetOpaque(value, unit);

  if (!JS_IsNull(code)) JS_SetHostDefined(env->context, code, value);

  return value;
}

static inline js_unit_t *
js__get_unit(JSValueConst value) {
  return (js_unit_t *) JS_GetOpaque(value, js__unit_class_id);
}

static inline JSValue
js__get_unit_id(js_env_t *env, JSValueConst value) {
  return JS_DupValue(env->context, js__get_unit(value)->id);
}

static int
js__set_unit_dynamic_import(js_env_t *env, JSValueConst value, js_dynamic_import_cb cb, void *data) {
  int err;

  js_unit_t *unit = js__get_unit(value);

  if (unit == NULL) {
    err = js_throw_error(env, NULL, "Code without an identifier of its own cannot take a dynamic import handler");
    assert(err == 0);

    return js__error(env);
  }

  if (unit->dynamic_import) {
    err = js_throw_error(env, NULL, "A dynamic import handler has already been added");
    assert(err == 0);

    return js__error(env);
  }

  unit->dynamic_import = cb;
  unit->dynamic_import_data = data;

  return 0;
}

// A multiplicative hash taken a word at a time, which a code cache records for
// the source, name, and offset it was produced from so that it is rejected for
// any other.
static inline uint64_t
js__hash_word(uint64_t hash, uint64_t word) {
  hash = (hash ^ word) * 0x9e3779b97f4a7c15;

  return hash ^ (hash >> 32);
}

static uint64_t
js__hash(uint64_t hash, const void *data, size_t len) {
  const uint8_t *bytes = (const uint8_t *) data;

  for (; len >= sizeof(uint64_t); bytes += sizeof(uint64_t), len -= sizeof(uint64_t)) {
    uint64_t word;

    memcpy(&word, bytes, sizeof(word));

    hash = js__hash_word(hash, word);
  }

  for (; len > 0; bytes++, len--) {
    hash = js__hash_word(hash, *bytes);
  }

  return hash;
}

static uint64_t
js__hash_source(const char *str, size_t len, const char *name, int offset) {
  uint64_t hash = 0xcbf29ce484222325;

  hash = js__hash(hash, str, len);
  hash = js__hash(hash, name, strlen(name) + 1);
  hash = js__hash(hash, &offset, sizeof(offset));

  return hash;
}

typedef struct {
  uint32_t magic;
  uint32_t reserved;
  uint64_t source_hash;
} js_code_cache_header_t;

static const uint32_t js__code_cache_magic = 0x676e6a71;

// Returns `JS_UNINITIALIZED` if there is no usable cache.
static JSValue
js__read_code_cache(js_env_t *env, const void *data, size_t len, uint64_t source_hash, int flags, bool *rejected) {
  if (data == NULL) return JS_UNINITIALIZED;

  js_code_cache_header_t header;

  if (len < sizeof(header)) goto rejected;

  memcpy(&header, data, sizeof(header));

  if (header.magic != js__code_cache_magic || header.source_hash != source_hash) goto rejected;

  JSValue bytecode = JS_ReadObject(env->context, (const uint8_t *) data + sizeof(header), len - sizeof(header), flags);

  if (JS_IsException(bytecode)) {
    JS_FreeValue(env->context, JS_GetException(env->context));

    goto rejected;
  }

  return bytecode;

rejected:
  if (rejected) *rejected = true;

  return JS_UNINITIALIZED;
}

static int
js__create_code_cache(js_env_t *env, JSValueConst bytecode, uint64_t source_hash, void **data, size_t *len) {
  int err;

  size_t size;
  uint8_t *buffer = JS_WriteObject(env->context, &size, bytecode, JS_WRITE_OBJ_BYTECODE);

  if (buffer == NULL) {
    if (!JS_HasException(env->context)) {
      err = js_throw_error(env, NULL, "Failed to serialize the code cache");
      assert(err == 0);
    } else if (env->depth == 0) {
      JSValue error = JS_GetException(env->context);

      js__uncaught_exception(env, error);
    }

    return js__error(env);
  }

  js_code_cache_header_t header = {
    .magic = js__code_cache_magic,
    .reserved = 0,
    .source_hash = source_hash,
  };

  // `JS_WriteObject()` hands back a buffer owned by QuickJS that must be
  // released with `js_free()`, whereas the caller releases the cache with
  // `free()`.
  uint8_t *copy = malloc(sizeof(header) + size);

  memcpy(copy, &header, sizeof(header));
  memcpy(copy + sizeof(header), buffer, size);

  js_free(env->context, buffer);

  *data = copy;
  *len = sizeof(header) + size;

  return 0;
}

static JSValue
js__compile(js_env_t *env, const char *str, size_t len, const char *name, int offset, int flags) {
  JSEvalOptions options = {
    .version = JS_EVAL_OPTIONS_VERSION,
    .eval_flags = flags | JS_EVAL_FLAG_COMPILE_ONLY,
    .filename = name,
    .line_num = offset + 1,
  };

  return JS_Eval2(env->context, str, len, &options);
}

// Hands the source to an error raised while compiling it, so that it can be
// reported by `js_get_error_location()`.
static void
js__attach_error_source(js_env_t *env, JSValueConst source) {
  JSValue error = JS_GetException(env->context);

  if (JS_IsError(error)) {
    JS_DefinePropertyValue(env->context, error, env->error_source_key, JS_DupValue(env->context, source), JS_PROP_CONFIGURABLE);
  }

  JS_Throw(env->context, error);
}

// Reads the bytecode from the code cache if it was produced from the same
// source, or otherwise compiles the source. Returns `JS_EXCEPTION` with the
// error pending if compiling fails.
static JSValue
js__compile_with_code_cache(js_env_t *env, JSValueConst source, const char *str, size_t len, const char *name, int offset, int eval_flags, int read_flags, const void *cached_data, size_t cached_data_len, bool *cache_rejected, uint64_t *source_hash) {
  if (cache_rejected) *cache_rejected = false;

  *source_hash = js__hash_source(str, len, name, offset);

  JSValue bytecode = js__read_code_cache(env, cached_data, cached_data_len, *source_hash, read_flags, cache_rejected);

  if (JS_IsUninitialized(bytecode)) {
    bytecode = js__compile(env, str, len, name, offset, eval_flags);

    if (JS_IsException(bytecode)) js__attach_error_source(env, source);
  }

  return bytecode;
}

static char *
js__copy_name(const char *name, size_t len) {
  if (name == NULL) return strdup("");

  if (len == (size_t) -1) return strdup(name);

  char *copy = malloc(len + 1);

  memcpy(copy, name, len);

  copy[len] = '\0';

  return copy;
}

static inline JSValue
js__default_module_id(js_env_t *env);

static void
js__on_dynamic_import(JSContext *context, const char *referrer, const char *specifier, JSValue token, JSValueConst attributes, void *opaque) {
  int err;

  js_env_t *env = (js_env_t *) opaque;

  JSValue host_defined = JS_GetDynamicImportHostDefined(context, token);

  js_unit_t *unit = js__get_unit(host_defined);

  js_dynamic_import_cb cb = env->callbacks.dynamic_import;
  void *data = env->callbacks.dynamic_import_data;

  if (unit && unit->dynamic_import) {
    cb = unit->dynamic_import;
    data = unit->dynamic_import_data;
  }

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  js_value_t *result = NULL;

  if (cb) {
    js_value_t specifier_value = {JS_NewString(context, specifier)};
    js_value_t referrer_value = {JS_NewString(context, referrer)};
    js_value_t assertions = {JS_IsUndefined(attributes) ? JS_NewObject(context) : JS_DupValue(context, attributes)};
    js_value_t id = {unit ? js__get_unit_id(env, host_defined) : JS_DupValue(context, js__default_module_id(env))};

    result = cb(env, &specifier_value, &assertions, &referrer_value, &id, data);

    JS_FreeValue(context, specifier_value.value);
    JS_FreeValue(context, referrer_value.value);
    JS_FreeValue(context, assertions.value);
    JS_FreeValue(context, id.value);
  }

  if (result == NULL && !JS_HasException(context)) {
    err = js_throw_error(env, NULL, cb ? "Dynamic import() failed" : "Dynamic import() is not supported");
    assert(err == 0);
  }

  if (JS_HasException(context)) {
    JS_FinishDynamicImport(context, token, JS_EXCEPTION, JS_GetException(context));
  } else {
    JSValue promise = JS_IsPromise(result->value)
                        ? JS_DupValue(context, result->value)
                        : JS_NewSettledPromise(context, false, result->value);

    JS_FinishDynamicImport(context, token, promise, JS_UNDEFINED);
  }

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  JS_FreeValue(context, host_defined);
}

int
js_open_handle_scope(js_env_t *env, js_handle_scope_t **result) {
  // Allow continuing even with a pending exception

  js_handle_scope_t *scope = malloc(sizeof(js_handle_scope_t));

  scope->parent = env->scope;
  scope->page = NULL;

  env->scope = scope;

  *result = scope;

  return 0;
}

int
js_close_handle_scope(js_env_t *env, js_handle_scope_t *scope) {
  // Allow continuing even with a pending exception

  js_handle_scope_page_t *page = scope->page;

  while (page) {
    for (size_t i = page->len; i-- > 0;) {
      JS_FreeValue(env->context, page->values[i].value);
    }

    js_handle_scope_page_t *next = page->next;

    free(page);

    page = next;
  }

  env->scope = scope->parent;

  free(scope);

  return 0;
}

int
js_open_escapable_handle_scope(js_env_t *env, js_escapable_handle_scope_t **result) {
  return js_open_handle_scope(env, (js_handle_scope_t **) result);
}

int
js_close_escapable_handle_scope(js_env_t *env, js_escapable_handle_scope_t *scope) {
  return js_close_handle_scope(env, (js_handle_scope_t *) scope);
}

static inline js_value_t *
js__create_handle(js_env_t *env, js_handle_scope_t *scope) {
  assert(scope);

  js_handle_scope_page_t *page = scope->page;

  if (page == NULL || page->len == JS_HANDLE_SCOPE_PAGE_SIZE) {
    page = malloc(sizeof(js_handle_scope_page_t));
    page->next = scope->page;
    page->len = 0;

    scope->page = page;
  }

  return &page->values[page->len++];
}

int
js_escape_handle(js_env_t *env, js_escapable_handle_scope_t *scope, js_value_t *escapee, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, scope->parent);

  wrapper->value = JS_DupValue(env->context, escapee->value);

  *result = wrapper;

  return 0;
}

int
js_create_context(js_env_t *env, js_context_t **result) {
  // Allow continuing even with a pending exception

  JSContext *local = JS_NewContext(env->runtime);

  if (local == NULL) return js__error(env);

  js_context_t *context = malloc(sizeof(js_context_t));

  context->context = local;
  context->previous = NULL;

  *result = context;

  return 0;
}

int
js_destroy_context(js_env_t *env, js_context_t *context) {
  // Allow continuing even with a pending exception

  JS_FreeContext(context->context);

  free(context);

  return 0;
}

int
js_enter_context(js_env_t *env, js_context_t *context) {
  // Allow continuing even with a pending exception

  context->previous = env->context;

  env->context = context->context;

  return 0;
}

int
js_exit_context(js_env_t *env, js_context_t *context) {
  // Allow continuing even with a pending exception

  env->context = context->previous;

  context->previous = NULL;

  return 0;
}

int
js_get_bindings(js_env_t *env, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_DupValue(env->context, env->bindings);

  *result = wrapper;

  return 0;
}

int
js_run_script(js_env_t *env, const char *file, size_t len, int offset, js_value_t *source, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  char *name = js__copy_name(file, len);

  size_t str_len;
  const char *str = JS_ToCStringLen(env->context, &str_len, source->value);

  JSValue bytecode = js__compile(env, str, str_len, name, offset, JS_EVAL_TYPE_GLOBAL);

  JS_FreeCString(env->context, str);

  free(name);

  JSValue value;

  env->depth++;

  if (JS_IsException(bytecode)) {
    js__attach_error_source(env, source->value);

    value = JS_EXCEPTION;
  } else {
    value = JS_EvalFunction(env->context, bytecode);
  }

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (JS_IsException(value)) {
    return js__report_exception(env);
  }

  if (result == NULL) JS_FreeValue(env->context, value);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    *result = wrapper;
  }

  return 0;
}

int
js_prepare_script(js_env_t *env, const char *file, size_t len, int offset, js_value_t *source, js_script_t **result) {
  return js_prepare_script_with_code_cache(env, file, len, offset, source, NULL, 0, NULL, result);
}

int
js_prepare_script_with_code_cache(js_env_t *env, const char *file, size_t len, int offset, js_value_t *source, const void *cached_data, size_t cached_data_len, bool *cache_rejected, js_script_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  char *name = js__copy_name(file, len);

  size_t str_len;
  const char *str = JS_ToCStringLen(env->context, &str_len, source->value);

  uint64_t source_hash;

  JSValue bytecode = js__compile_with_code_cache(env, source->value, str, str_len, name, offset, JS_EVAL_TYPE_GLOBAL, JS_READ_OBJ_BYTECODE, cached_data, cached_data_len, cache_rejected, &source_hash);

  JS_FreeCString(env->context, str);

  if (JS_IsException(bytecode)) {
    free(name);

    return js__report_exception(env);
  }

  js_script_t *script = malloc(sizeof(js_script_t));

  script->context = JS_DupContext(env->context);
  script->bytecode = bytecode;
  script->foreign_context = NULL;
  script->foreign_bytecode = JS_UNDEFINED;
  script->name = name;
  script->unit = js__create_unit(env, name, source_hash, bytecode);

  *result = script;

  return 0;
}

int
js_create_script_code_cache(js_env_t *env, js_script_t *script, void **data, size_t *len) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__create_code_cache(env, script->bytecode, js__get_unit(script->unit)->source_hash, data, len);
}

// Compiled code belongs to the context it was compiled in, so a script run in
// another context is first carried over to it by way of its serialized form,
// and the copy kept for as long as it is run there. `JS_EvalFunction()`
// consumes the bytecode either way, which keeps the script runnable more than
// once.
static JSValue
js__script_bytecode(js_env_t *env, js_script_t *script) {
  if (script->context == env->context) return JS_DupValue(env->context, script->bytecode);

  if (script->foreign_context != env->context) {
    size_t len;
    uint8_t *data = JS_WriteObject(script->context, &len, script->bytecode, JS_WRITE_OBJ_BYTECODE);

    if (data == NULL) return JS_EXCEPTION;

    JSValue bytecode = JS_ReadObject(env->context, data, len, JS_READ_OBJ_BYTECODE);

    js_free(script->context, data);

    if (JS_IsException(bytecode)) return JS_EXCEPTION;

    JS_SetHostDefined(env->context, bytecode, script->unit);

    JS_FreeValue(env->context, script->foreign_bytecode);

    if (script->foreign_context) JS_FreeContext(script->foreign_context);

    script->foreign_context = JS_DupContext(env->context);
    script->foreign_bytecode = bytecode;
  }

  return JS_DupValue(env->context, script->foreign_bytecode);
}

int
js_run_prepared_script(js_env_t *env, js_script_t *script, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue bytecode = js__script_bytecode(env, script);

  if (JS_IsException(bytecode)) return js__error(env);

  env->depth++;

  JSValue value = JS_EvalFunction(env->context, bytecode);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (JS_IsException(value)) {
    return js__report_exception(env);
  }

  if (result == NULL) JS_FreeValue(env->context, value);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    *result = wrapper;
  }

  return 0;
}

int
js_delete_script(js_env_t *env, js_script_t *script) {
  // Allow continuing even with a pending exception

  JS_FreeValue(env->context, script->bytecode);
  JS_FreeValue(env->context, script->foreign_bytecode);
  JS_FreeValue(env->context, script->unit);

  JS_FreeContext(script->context);

  if (script->foreign_context) JS_FreeContext(script->foreign_context);

  free(script->name);
  free(script);

  return 0;
}

int
js_get_script_name(js_env_t *env, js_script_t *script, const char **result) {
  // Allow continuing even with a pending exception

  *result = script->name;

  return 0;
}

int
js_get_script_id(js_env_t *env, js_script_t *script, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = js__get_unit_id(env, script->unit);

  *result = wrapper;

  return 0;
}

int
js_on_script_dynamic_import(js_env_t *env, js_script_t *script, js_dynamic_import_cb cb, void *data) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__set_unit_dynamic_import(env, script->unit, cb, data);
}

int
js_create_module(js_env_t *env, const char *name, size_t len, int offset, js_value_t *source, js_module_meta_cb cb, void *data, js_module_t **result) {
  return js_create_module_with_code_cache(env, name, len, offset, source, NULL, 0, NULL, cb, data, result);
}

int
js_create_module_with_code_cache(js_env_t *env, const char *name, size_t len, int offset, js_value_t *source, const void *cached_data, size_t cached_data_len, bool *cache_rejected, js_module_meta_cb cb, void *data, js_module_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  char *module_name = js__copy_name(name, len);

  size_t str_len;
  const char *str = JS_ToCStringLen(env->context, &str_len, source->value);

  uint64_t source_hash;

  // Both a module read from the cache and one compiled from source are left
  // unresolved, to be resolved by `js_instantiate_module()`.
  JSValue bytecode = js__compile_with_code_cache(env, source->value, str, str_len, module_name, offset, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_DEFER_RESOLUTION, JS_READ_OBJ_BYTECODE | JS_READ_OBJ_DEFER_RESOLUTION, cached_data, cached_data_len, cache_rejected, &source_hash);

  JS_FreeCString(env->context, str);

  if (JS_IsException(bytecode)) {
    free(module_name);

    return js__report_exception(env);
  }

  js_module_t *module = malloc(sizeof(js_module_t));

  module->context = JS_DupContext(env->context);
  module->bytecode = bytecode;
  module->unit = js__create_unit(env, module_name, source_hash, bytecode);
  module->definition = (JSModuleDef *) JS_VALUE_GET_PTR(bytecode);
  module->meta = cb;
  module->meta_data = data;
  module->name = module_name;
  module->state = js_module_uninstantiated;
  module->synthetic = false;
  module->aborted = false;

  *result = module;

  return 0;
}

int
js_create_module_code_cache(js_env_t *env, js_module_t *module, void **data, size_t *len) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  // Synthetic modules carry no bytecode, and an evaluated module's bytecode has
  // been consumed by `js_run_module()`; in either case there is nothing to
  // serialize. Produce the cache after `js_create_module()` (or after
  // `js_instantiate_module()`) but before `js_run_module()`.
  if (JS_IsNull(module->bytecode)) {
    err = js_throw_error(env, NULL, "Cannot create a code cache for a synthetic or evaluated module");
    assert(err == 0);

    return js__error(env);
  }

  return js__create_code_cache(env, module->bytecode, js__get_unit(module->unit)->source_hash, data, len);
}

static int
js__on_evaluate_module(JSContext *context, JSModuleDef *definition) {
  int err;

  js_env_t *env = (js_env_t *) JS_GetContextOpaque(context);

  js_module_t *module = NULL;

  intrusive_list_for_each(next, &env->synthetic_modules) {
    js_module_t *candidate = intrusive_entry(next, js_module_t, list);

    if (candidate->definition == definition) {
      module = candidate;
      break;
    }
  }

  // The engine keeps the definition of a module after it has been deleted.
  if (module == NULL) {
    err = js_throw_error(env, NULL, "Cannot evaluate a deleted module");
    assert(err == 0);

    return -1;
  }

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  module->evaluate(env, module, module->evaluate_data);

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  if (!JS_HasException(context)) return 0;

  // An exception left pending by the callback aborts the evaluation, which the
  // module remembers so that running it can fail rather than report a rejection
  // for something that never ran.
  module->aborted = true;

  return -1;
}

int
js_create_synthetic_module(js_env_t *env, const char *name, size_t len, js_value_t *const export_names[], size_t names_len, js_module_evaluate_cb cb, void *data, js_module_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  js_module_t *module = malloc(sizeof(js_module_t));

  module->name = js__copy_name(name, len);
  module->context = JS_DupContext(env->context);
  module->bytecode = JS_NULL;
  module->unit = js__create_unit(env, module->name, 0, JS_NULL);
  module->definition = JS_NewCModule(env->context, module->name, js__on_evaluate_module);
  module->meta = NULL;
  module->meta_data = NULL;
  module->state = js_module_uninstantiated;
  module->synthetic = true;
  module->aborted = false;

  for (size_t i = 0; i < names_len; i++) {
    const char *str = JS_ToCString(env->context, export_names[i]->value);

    JS_AddModuleExport(env->context, module->definition, str);

    JS_FreeCString(env->context, str);
  }

  module->evaluate = cb;
  module->evaluate_data = data;

  intrusive_list_prepend(&env->synthetic_modules, &module->list);

  *result = module;

  return 0;
}

int
js_delete_module(js_env_t *env, js_module_t *module) {
  // Allow continuing even with a pending exception

  if (module->synthetic) intrusive_list_remove(&env->synthetic_modules, &module->list);

  JS_FreeValue(env->context, module->bytecode);
  JS_FreeValue(env->context, module->unit);

  JS_FreeContext(module->context);

  free(module->name);
  free(module);

  return 0;
}

int
js_get_module_name(js_env_t *env, js_module_t *module, const char **result) {
  // Allow continuing even with a pending exception

  *result = module->name;

  return 0;
}

// Returns the identifier shared by all compilation units that do not carry one
// of their own, such as scripts run with `js_run_script()`. The identifier is
// created lazily and remains stable for the lifetime of the environment.
static inline JSValue
js__default_module_id(js_env_t *env) {
  if (JS_IsNull(env->default_module_id)) {
    env->default_module_id = JS_NewSymbol(env->context, "module", false);
  }

  return env->default_module_id;
}

int
js_get_module_id(js_env_t *env, js_module_t *module, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = js__get_unit_id(env, module->unit);

  *result = wrapper;

  return 0;
}

int
js_get_default_module_id(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_DupValue(env->context, js__default_module_id(env));

  *result = wrapper;

  return 0;
}

int
js_on_module_dynamic_import(js_env_t *env, js_module_t *module, js_dynamic_import_cb cb, void *data) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (module->synthetic) {
    err = js_throw_error(env, NULL, "A synthetic module cannot take a dynamic import handler");
    assert(err == 0);

    return js__error(env);
  }

  return js__set_unit_dynamic_import(env, module->unit, cb, data);
}

int
js_get_module_namespace(js_env_t *env, js_module_t *module, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (module->state < js_module_instantiated) {
    err = js_throw_error(env, NULL, "Cannot get the namespace of an uninstantiated module");
    assert(err == 0);

    return js__error(env);
  }

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_GetModuleNamespace(env->context, module->definition);

  *result = wrapper;

  return 0;
}

int
js_set_module_export(js_env_t *env, js_module_t *module, js_value_t *name, js_value_t *value) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  const char *str = JS_ToCString(env->context, name->value);

  int success = JS_SetModuleExport(env->context, module->definition, str, JS_DupValue(env->context, value->value));

  JS_FreeCString(env->context, str);

  if (success < 0) {
    err = js_throw_error(env, NULL, "Could not set module export");
    assert(err == 0);

    return js__error(env);
  }

  return 0;
}

int
js_instantiate_module(js_env_t *env, js_module_t *module, js_module_resolve_cb cb, void *data) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  // A module being instantiated may be entered again as the graph around it is
  // walked, a cyclic import among them, but one being evaluated may not.
  if (module->state == js_module_evaluating) {
    err = js_throw_error(env, NULL, "Cannot instantiate a module that is already being instantiated or evaluated");
    assert(err == 0);

    return js__error(env);
  }

  // Synthetic modules have nothing to resolve, but are linked so that the
  // bindings of their exports exist.
  if (module->synthetic) {
    if (module->state == js_module_uninstantiated) {
      if (JS_LinkModule(env->context, JS_MKPTR(JS_TAG_MODULE, module->definition)) < 0) {
        return js__report_exception(env);
      }

      module->state = js_module_instantiated;
    }

    return 0;
  }

  if (JS_IsNull(module->bytecode)) return 0;

  js_module_resolver_t resolver = {
    .module = module,
    .cb = cb,
    .data = data,
    .next = env->resolvers,
  };

  env->resolvers = &resolver;

  module->state = js_module_instantiating;

  // Resolve the module's imports, driving the resolve callback; evaluation is
  // deferred to `js_run_module()`.

  env->depth++;

  // Only the outermost instantiation links, as the imports of the modules it is
  // reached through are still being resolved further in.
  int success = JS_ResolveModule(env->context, module->bytecode);

  if (success == 0 && resolver.next == NULL) {
    success = JS_LinkModule(env->context, module->bytecode);
  }

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  env->resolvers = resolver.next;

  if (success < 0) {
    module->state = js_module_uninstantiated;

    return js__report_exception(env);
  }

  module->state = js_module_instantiated;

  return 0;
}

int
js_run_module(js_env_t *env, js_module_t *module, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (module->state != js_module_instantiated && module->state != js_module_evaluated && module->state != js_module_errored) {
    err = js_throw_error(env, NULL, module->state == js_module_evaluating ? "Cannot run a module that is already evaluating" : "Cannot run an uninstantiated module");
    assert(err == 0);

    return js__error(env);
  }

  if (module->meta) {
    JSValue meta = JS_GetImportMeta(env->context, module->definition);

    module->meta(env, module, &(js_value_t){meta}, module->meta_data);

    JS_FreeValue(env->context, meta);

    if (JS_HasException(env->context)) {
      JSValue error = JS_GetException(env->context);

      js_deferred_t *deferred;
      err = js_create_promise(env, &deferred, result);
      if (err < 0) return err;

      js_reject_deferred(env, deferred, &(js_value_t){error});

      JS_FreeValue(env->context, error);

      module->state = js_module_errored;

      return 0;
    }
  }

  module->state = js_module_evaluating;

  env->depth++;

  // The bytecode is the module itself, which the engine holds on to, so the
  // module is evaluated by way of its definition and the reference kept for the
  // code cache given up here. An evaluated module hands back the result the
  // engine kept for it.
  JS_FreeValue(env->context, module->bytecode);

  module->bytecode = JS_NULL;

  JSValue value = JS_EvalFunction(env->context, JS_DupValue(env->context, JS_MKPTR(JS_TAG_MODULE, module->definition)));

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (module->aborted) {
    module->aborted = false;
    module->state = js_module_errored;

    // The engine took the exception the evaluate callback left pending into the
    // rejection of the promise it hands back, so it is recovered from there.
    JSValue error = JS_PromiseResult(env->context, value);

    JS_FreeValue(env->context, value);

    if (env->depth) {
      JS_Throw(env->context, error);
    } else {
      js__uncaught_exception(env, error);
    }

    return js__error(env);
  }

  if (JS_IsException(value)) {
    module->state = js_module_errored;

    JSValue error = JS_GetException(env->context);

    js_deferred_t *deferred;
    err = js_create_promise(env, &deferred, result);
    if (err < 0) return err;

    js_reject_deferred(env, deferred, &(js_value_t){error});

    JS_FreeValue(env->context, error);

    return 0;
  }

  module->state = js_module_evaluated;

  if (result == NULL) JS_FreeValue(env->context, value);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    *result = wrapper;
  }

  return 0;
}

static inline JSValue
js__deref_weak_reference(js_env_t *env, js_ref_t *reference) {
  JSValue value = JS_GetWeakRefTarget(env->context, reference->weakref);

  if (JS_IsUndefined(value)) reference->finalized = true;

  return value;
}

static inline void
js__set_weak_reference(js_env_t *env, js_ref_t *reference) {
  if (reference->finalized || !JS_IsObject(reference->value)) return;

  reference->weakref = JS_NewWeakRef(env->context, reference->value);

  JS_FreeValue(env->context, reference->value);

  reference->value = JS_UNDEFINED;
}

static inline void
js__clear_weak_reference(js_env_t *env, js_ref_t *reference) {
  if (JS_IsUndefined(reference->weakref)) return;

  reference->value = js__deref_weak_reference(env, reference);

  JS_FreeValue(env->context, reference->weakref);

  reference->weakref = JS_UNDEFINED;
}

int
js_create_reference(js_env_t *env, js_value_t *value, uint32_t count, js_ref_t **result) {
  // Allow continuing even with a pending exception

  js_ref_t *reference = malloc(sizeof(js_ref_t));

  reference->value = JS_DupValue(env->context, value->value);
  reference->weakref = JS_UNDEFINED;
  reference->count = count;
  reference->finalized = false;

  if (reference->count == 0) js__set_weak_reference(env, reference);

  *result = reference;

  return 0;
}

int
js_delete_reference(js_env_t *env, js_ref_t *reference) {
  // Allow continuing even with a pending exception

  JS_FreeValue(env->context, reference->weakref);
  JS_FreeValue(env->context, reference->value);

  free(reference);

  return 0;
}

int
js_reference_ref(js_env_t *env, js_ref_t *reference, uint32_t *result) {
  // Allow continuing even with a pending exception

  reference->count++;

  if (reference->count == 1) js__clear_weak_reference(env, reference);

  if (result) *result = reference->count;

  return 0;
}

int
js_reference_unref(js_env_t *env, js_ref_t *reference, uint32_t *result) {
  // Allow continuing even with a pending exception

  if (reference->count > 0) {
    reference->count--;

    if (reference->count == 0) js__set_weak_reference(env, reference);
  }

  if (result) *result = reference->count;

  return 0;
}

int
js_get_reference_value(js_env_t *env, js_ref_t *reference, js_value_t **result) {
  // Allow continuing even with a pending exception

  JSValue value;

  if (reference->finalized) value = JS_UNDEFINED;
  else if (JS_IsUndefined(reference->weakref)) value = JS_DupValue(env->context, reference->value);
  else value = js__deref_weak_reference(env, reference);

  if (reference->finalized) *result = NULL;
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    *result = wrapper;
  }

  return 0;
}

static JSValue
js__create_function(js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data) {
  JSValue prototype = JS_GetFunctionProto(env->context);

  JSValue function = JS_NewObjectProtoClass(env->context, prototype, js__function_class_id);

  JS_FreeValue(env->context, prototype);

  if (JS_IsException(function)) return function;

  js_callback_t *callback = malloc(sizeof(js_callback_t));

  callback->cb = cb;
  callback->data = data;

  JS_SetOpaque(function, callback);

  JS_SetConstructorBit(env->context, function, true);

  if (name == NULL) name = "", len = 0;
  else if (len == (size_t) -1) len = strlen(name);

  JS_DefinePropertyValue(env->context, function, env->length_key, JS_NewInt32(env->context, 0), JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValue(env->context, function, env->name_key, JS_NewStringLen(env->context, name, len), JS_PROP_CONFIGURABLE);

  return function;
}

int
js_define_class(js_env_t *env, const char *name, size_t len, js_function_cb constructor, void *data, js_property_descriptor_t const properties[], size_t properties_len, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  JSValue class = js__create_function(env, name, len, constructor, data);

  if (JS_IsException(class)) return js__error(env);

  JSValue prototype = JS_NewObject(env->context);

  JS_SetConstructor(env->context, class, prototype);

  size_t instance_properties_len = 0;
  size_t static_properties_len = 0;

  for (size_t i = 0; i < properties_len; i++) {
    const js_property_descriptor_t *property = &properties[i];

    if ((property->attributes & js_static) == 0) {
      instance_properties_len++;
    } else {
      static_properties_len++;
    }
  }

  if (instance_properties_len) {
    js_property_descriptor_t *instance_properties = malloc(sizeof(js_property_descriptor_t) * instance_properties_len);

    for (size_t i = 0, j = 0; i < properties_len; i++) {
      const js_property_descriptor_t *property = &properties[i];

      if ((property->attributes & js_static) == 0) {
        instance_properties[j++] = *property;
      }
    }

    err = js_define_properties(env, &(js_value_t){prototype}, instance_properties, instance_properties_len);
    assert(err == 0);

    free(instance_properties);
  }

  if (static_properties_len) {
    js_property_descriptor_t *static_properties = malloc(sizeof(js_property_descriptor_t) * static_properties_len);

    for (size_t i = 0, j = 0; i < properties_len; i++) {
      const js_property_descriptor_t *property = &properties[i];

      if ((property->attributes & js_static) != 0) {
        static_properties[j++] = *property;
      }
    }

    err = js_define_properties(env, &(js_value_t){class}, static_properties, static_properties_len);
    assert(err == 0);

    free(static_properties);
  }

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = class;

  JS_FreeValue(env->context, prototype);

  *result = wrapper;

  return 0;
}

int
js_define_properties(js_env_t *env, js_value_t *object, js_property_descriptor_t const properties[], size_t properties_len) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  for (size_t i = 0; i < properties_len; i++) {
    const js_property_descriptor_t *property = &properties[i];

    int flags = JS_PROP_HAS_WRITABLE | JS_PROP_HAS_ENUMERABLE | JS_PROP_HAS_CONFIGURABLE;

    if ((property->attributes & js_writable) != 0 || property->getter || property->setter) {
      flags |= JS_PROP_WRITABLE;
    }

    if ((property->attributes & js_enumerable) != 0) {
      flags |= JS_PROP_ENUMERABLE;
    }

    if ((property->attributes & js_configurable) != 0) {
      flags |= JS_PROP_CONFIGURABLE;
    }

    JSValue value = JS_UNDEFINED, getter = JS_UNDEFINED, setter = JS_UNDEFINED;

    if (property->getter || property->setter) {
      if (property->getter) {
        flags |= JS_PROP_HAS_GET;

        js_value_t *fn;
        err = js_create_function(env, "fn", -1, property->getter, property->data, &fn);
        assert(err == 0);

        getter = fn->value;
      }

      if (property->setter) {
        flags |= JS_PROP_HAS_SET;

        js_value_t *fn;
        err = js_create_function(env, "fn", -1, property->setter, property->data, &fn);
        assert(err == 0);

        setter = fn->value;
      }
    } else if (property->method) {
      flags |= JS_PROP_HAS_VALUE;

      js_value_t *fn;
      err = js_create_function(env, "fn", -1, property->method, property->data, &fn);
      assert(err == 0);

      value = fn->value;
    } else {
      flags |= JS_PROP_HAS_VALUE;

      value = property->value->value;
    }

    JSAtom atom = JS_ValueToAtom(env->context, property->name->value);

    err = JS_DefineProperty(env->context, object->value, atom, value, getter, setter, flags);

    JS_FreeAtom(env->context, atom);

    if (err < 0) return js__error(env);
  }

  return 0;
}

static int
js__get_own_external(js_env_t *env, JSValueConst object, const char *name, JSValue *result) {
  JSAtom atom = JS_NewAtom(env->context, name);

  JSPropertyDescriptor descriptor;

  int found = JS_GetOwnProperty(env->context, &descriptor, object, atom);

  JS_FreeAtom(env->context, atom);

  if (found < 0) return js__error(env);

  if (found == 0) {
    *result = JS_UNDEFINED;

    return 0;
  }

  JS_FreeValue(env->context, descriptor.getter);
  JS_FreeValue(env->context, descriptor.setter);

  *result = descriptor.value;

  return 0;
}

int
js_wrap(js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  js_finalizer_t *finalizer = malloc(sizeof(js_finalizer_t));

  JSValue external = JS_NewObjectClass(env->context, js__external_class_id);

  if (JS_IsException(external)) {
    free(finalizer);

    return js__error(env);
  }

  JSAtom atom = JS_NewAtom(env->context, "__native_external");

  // The define takes ownership of the value and frees it on failure, which would
  // run the finalizer of the external and with it a callback for a wrap that
  // never took effect. A reference is kept so the opaque can be attached once
  // the define has settled, leaving nothing to finalize if it did not.
  err = JS_DefinePropertyValue(env->context, object->value, atom, JS_DupValue(env->context, external), 0);

  JS_FreeAtom(env->context, atom);

  // The define declines with `false` rather than an exception when the object is
  // not extensible, or already carries the property of a wrap it was given
  // before, neither of which may be reported as a wrap that took.
  if (err <= 0) {
    if (err == 0) {
      err = js_throw_errorf(env, NULL, "Object could not be wrapped");
      assert(err == 0);
    }

    JS_FreeValue(env->context, external);

    free(finalizer);

    return js__error(env);
  }

  js__init_finalizer(env, finalizer, data, finalize_cb, finalize_hint);

  JS_SetOpaque(external, finalizer);

  JS_FreeValue(env->context, external);

  if (result) return js_create_reference(env, object, 0, result);

  return 0;
}

int
js_unwrap(js_env_t *env, js_value_t *object, void **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  JSValue external;
  err = js__get_own_external(env, object->value, "__native_external", &external);
  if (err < 0) return err;

  js_finalizer_t *finalizer = (js_finalizer_t *) JS_GetOpaque(external, js__external_class_id);

  JS_FreeValue(env->context, external);

  if (finalizer == NULL) {
    err = js_throw_type_error(env, NULL, "Object is not wrapped");
    assert(err == 0);

    return js__error(env);
  }

  *result = finalizer->data;

  return 0;
}

int
js_remove_wrap(js_env_t *env, js_value_t *object, void **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  JSValue external;
  err = js__get_own_external(env, object->value, "__native_external", &external);
  if (err < 0) return err;

  js_finalizer_t *finalizer = (js_finalizer_t *) JS_GetOpaque(external, js__external_class_id);

  if (finalizer == NULL) {
    JS_FreeValue(env->context, external);

    err = js_throw_type_error(env, NULL, "Object is not wrapped");
    assert(err == 0);

    return js__error(env);
  }

  if (result) *result = finalizer->data;

  // The property was defined as non-configurable and so cannot be deleted. The
  // external is emptied instead, which leaves the object unwrapped as far as
  // every lookup is concerned.
  JS_SetOpaque(external, NULL);

  JS_FreeValue(env->context, external);

  js__untrack_finalizer(env, finalizer);

  free(finalizer);

  return 0;
}

static int
js__on_delegate_get_own_property(JSContext *context, JSPropertyDescriptor *descriptor, JSValueConst object, JSAtom name) {
  js_env_t *env = (js_env_t *) JS_GetContextOpaque(context);

  js_delegate_t *delegate = (js_delegate_t *) JS_GetOpaque(object, js__delegate_class_id);

  if (delegate->callbacks.has) {
    JSValue property = JS_AtomToValue(env->context, name);

    bool exists = delegate->callbacks.has(env, &(js_value_t){property}, delegate->data);

    JS_FreeValue(env->context, property);

    if (JS_HasException(env->context)) return js__error(env);

    if (!exists) return 0;
  }

  if (delegate->callbacks.get) {
    JSValue property = JS_AtomToValue(env->context, name);

    js_value_t *result = delegate->callbacks.get(env, &(js_value_t){property}, delegate->data);

    JS_FreeValue(env->context, property);

    if (JS_HasException(env->context)) return js__error(env);

    if (result == NULL) return 0;

    if (descriptor) {
      descriptor->flags = JS_PROP_ENUMERABLE;
      descriptor->value = result->value;
      descriptor->getter = JS_UNDEFINED;
      descriptor->setter = JS_UNDEFINED;
    }

    return 1;
  }

  return 0;
}

static int
js__on_delegate_get_own_property_names(JSContext *context, JSPropertyEnum **pproperties, uint32_t *plen, JSValueConst object) {
  int err;

  js_env_t *env = (js_env_t *) JS_GetContextOpaque(context);

  js_delegate_t *delegate = (js_delegate_t *) JS_GetOpaque(object, js__delegate_class_id);

  if (delegate->callbacks.own_keys) {
    js_value_t *result = delegate->callbacks.own_keys(env, delegate->data);

    if (JS_HasException(env->context)) return js__error(env);

    uint32_t len;
    err = js_get_array_length(env, result, &len);
    assert(err == 0);

    JSPropertyEnum *properties = js_mallocz(env->context, len * sizeof(JSPropertyEnum));

    for (uint32_t i = 0; i < len; i++) {
      js_value_t *value;
      err = js_get_element(env, result, i, &value);
      assert(err == 0);

      properties[i].atom = JS_ValueToAtom(env->context, value->value);
    }

    *pproperties = properties;
    *plen = len;

    return 0;
  }

  *pproperties = NULL;
  *plen = 0;

  return 0;
}

static int
js__on_delegate_delete_property(JSContext *context, JSValueConst object, JSAtom name) {
  js_env_t *env = (js_env_t *) JS_GetContextOpaque(context);

  js_delegate_t *delegate = (js_delegate_t *) JS_GetOpaque(object, js__delegate_class_id);

  if (delegate->callbacks.delete_property) {
    JSValue property = JS_AtomToValue(env->context, name);

    bool success = delegate->callbacks.delete_property(env, &(js_value_t){property}, delegate->data);

    JS_FreeValue(env->context, property);

    if (JS_HasException(env->context)) return js__error(env);

    return success;
  }

  return 0;
}

static int
js__on_delegate_set_property(JSContext *context, JSValueConst object, JSAtom name, JSValueConst value, JSValueConst receiver, int flags) {
  js_env_t *env = (js_env_t *) JS_GetContextOpaque(context);

  js_delegate_t *delegate = (js_delegate_t *) JS_GetOpaque(object, js__delegate_class_id);

  if (delegate->callbacks.set) {
    JSValue property = JS_AtomToValue(env->context, name);

    bool success = delegate->callbacks.set(env, &(js_value_t){property}, &(js_value_t){value}, delegate->data);

    JS_FreeValue(env->context, property);

    if (JS_HasException(env->context)) return js__error(env);

    return success;
  }

  return 0;
}

static void
js__on_delegate_finalize(JSRuntime *runtime, JSValueConst value) {
  js_env_t *env = (js_env_t *) JS_GetRuntimeOpaque(runtime);

  js_delegate_t *delegate = (js_delegate_t *) JS_GetOpaque(value, js__delegate_class_id);

  js__defer_finalizer(env, &delegate->finalizer);

  free(delegate);
}

int
js_create_delegate(js_env_t *env, const js_delegate_callbacks_t *callbacks, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_delegate_t *delegate = malloc(sizeof(js_delegate_t));

  delegate->data = data;

  js__init_finalizer(env, &delegate->finalizer, data, finalize_cb, finalize_hint);

  memcpy(&delegate->callbacks, callbacks, sizeof(js_delegate_callbacks_t));

  JSValue object = JS_NewObjectClass(env->context, js__delegate_class_id);

  JS_SetOpaque(object, delegate);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = object;

  *result = wrapper;

  return 0;
}

static void
js__on_finalizer_finalize(JSRuntime *runtime, JSValueConst value) {
  js_env_t *env = (js_env_t *) JS_GetRuntimeOpaque(runtime);

  js_finalizer_list_t *next = (js_finalizer_list_t *) JS_GetOpaque(value, js__finalizer_class_id);
  js_finalizer_list_t *prev;

  while (next) {
    js__defer_finalizer(env, &next->finalizer);

    prev = next;
    next = next->next;

    free(prev);
  }
}

int
js_add_finalizer(js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result) {
  // Allow continuing even with a pending exception

  int err;

  js_finalizer_list_t *prev = malloc(sizeof(js_finalizer_list_t));

  js__init_finalizer(env, &prev->finalizer, data, finalize_cb, finalize_hint);

  JSAtom atom = JS_NewAtom(env->context, "__native_finalizer");

  JSValue external;

  if (JS_HasProperty(env->context, object->value, atom) == 1) {
    external = JS_GetProperty(env->context, object->value, atom);
  } else {
    external = JS_NewObjectClass(env->context, js__finalizer_class_id);

    JS_SetOpaque(external, NULL);

    err = JS_DefinePropertyValue(env->context, object->value, atom, external, 0);
    assert(err >= 0);

    JS_DupValue(env->context, external);
  }

  JS_FreeAtom(env->context, atom);

  prev->next = (js_finalizer_list_t *) JS_GetOpaque(external, js__finalizer_class_id);

  JS_SetOpaque(external, prev);

  JS_FreeValue(env->context, external);

  if (result) return js_create_reference(env, object, 0, result);

  return 0;
}

static void
js__on_type_tag_finalize(JSRuntime *runtime, JSValueConst value) {
  js_env_t *env = (js_env_t *) JS_GetRuntimeOpaque(runtime);

  js_type_tag_t *tag = (js_type_tag_t *) JS_GetOpaque(value, js__type_tag_class_id);

  free(tag);
}

int
js_add_type_tag(js_env_t *env, js_value_t *object, const js_type_tag_t *tag) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  JSValue tagged;
  err = js__get_own_external(env, object->value, "__native_type_tag", &tagged);
  if (err < 0) return err;

  bool exists = JS_GetOpaque(tagged, js__type_tag_class_id) != NULL;

  JS_FreeValue(env->context, tagged);

  if (exists) {
    err = js_throw_errorf(env, NULL, "Object is already type tagged");
    assert(err == 0);

    return js__error(env);
  }

  js_type_tag_t *existing = malloc(sizeof(js_type_tag_t));

  existing->lower = tag->lower;
  existing->upper = tag->upper;

  JSValue external = JS_NewObjectClass(env->context, js__type_tag_class_id);

  if (JS_IsException(external)) {
    free(existing);

    return js__error(env);
  }

  JS_SetOpaque(external, existing);

  JSAtom atom = JS_NewAtom(env->context, "__native_type_tag");

  // Defining the property fails on an object that is frozen or otherwise not
  // extensible, and the define frees the external, and with it the tag it
  // holds, on its way out.
  err = JS_DefinePropertyValue(env->context, object->value, atom, external, 0);

  JS_FreeAtom(env->context, atom);

  // The define owns the external either way, and frees the tag along with it,
  // so nothing is left to release here.
  if (err <= 0) {
    if (err == 0) {
      err = js_throw_errorf(env, NULL, "Object could not be type tagged");
      assert(err == 0);
    }

    return js__error(env);
  }

  return 0;
}

int
js_check_type_tag(js_env_t *env, js_value_t *object, const js_type_tag_t *tag, bool *result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  JSValue external;
  err = js__get_own_external(env, object->value, "__native_type_tag", &external);
  if (err < 0) return err;

  js_type_tag_t *existing = (js_type_tag_t *) JS_GetOpaque(external, js__type_tag_class_id);

  JS_FreeValue(env->context, external);

  *result = existing != NULL && existing->lower == tag->lower && existing->upper == tag->upper;

  return 0;
}

int
js_create_int32(js_env_t *env, int32_t value, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewInt32(env->context, value);

  *result = wrapper;

  return 0;
}

int
js_create_uint32(js_env_t *env, uint32_t value, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewUint32(env->context, value);

  *result = wrapper;

  return 0;
}

int
js_create_int64(js_env_t *env, int64_t value, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewInt64(env->context, value);

  *result = wrapper;

  return 0;
}

int
js_create_double(js_env_t *env, double value, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewFloat64(env->context, value);

  *result = wrapper;

  return 0;
}

int
js_create_bigint_int64(js_env_t *env, int64_t value, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewBigInt64(env->context, value);

  *result = wrapper;

  return 0;
}

int
js_create_bigint_uint64(js_env_t *env, uint64_t value, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewBigUint64(env->context, value);

  *result = wrapper;

  return 0;
}

int
js_create_bigint_words(js_env_t *env, int sign, const uint64_t *words, size_t len, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue value = JS_NewBigIntWords(env->context, sign, words, len);

  if (JS_IsException(value)) return js__error(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = value;

  *result = wrapper;

  return 0;
}

static inline int
js__check_string_length(js_env_t *env, size_t len) {
  int err;

  if (len == (size_t) -1 || len <= 0x3fffffff) return 0;

  err = js_throw_range_error(env, NULL, "Invalid string length");
  assert(err == 0);

  return js__error(env);
}

int
js_create_string_utf8(js_env_t *env, const utf8_t *str, size_t len, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  err = js__check_string_length(env, len);
  if (err < 0) return err;

  JSValue value;

  if (len == (size_t) -1) {
    value = JS_NewString(env->context, (char *) str);
  } else {
    value = JS_NewStringLen(env->context, (char *) str, len);
  }

  if (JS_IsException(value)) {
    return js__report_exception(env);
  }

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = value;

  *result = wrapper;

  return 0;
}

int
js_create_string_utf16le(js_env_t *env, const utf16_t *str, size_t len, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  err = js__check_string_length(env, len);
  if (err < 0) return err;

  if (len == (size_t) -1) {
    len = 0;

    while (str[len] != 0) len++;
  }

  JSValue value = JS_NewStringUTF16(env->context, str, len);

  if (JS_IsException(value)) {
    return js__report_exception(env);
  }

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = value;

  *result = wrapper;

  return 0;
}

int
js_create_string_latin1(js_env_t *env, const latin1_t *str, size_t len, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  err = js__check_string_length(env, len);
  if (err < 0) return err;

  if (len == (size_t) -1) len = strlen((char *) str);

  size_t utf8_len = utf8_length_from_latin1(str, len);

  utf8_t *utf8 = malloc(utf8_len);

  latin1_convert_to_utf8(str, len, utf8);

  JSValue value = JS_NewStringLen(env->context, (char *) utf8, utf8_len);

  free(utf8);

  if (JS_IsException(value)) {
    return js__report_exception(env);
  }

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = value;

  *result = wrapper;

  return 0;
}

int
js_create_external_string_utf8(js_env_t *env, utf8_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied) {
  int err;
  err = js_create_string_utf8(env, str, len, result);
  if (err < 0) return err;

  if (copied) *copied = true;

  if (finalize_cb) finalize_cb(env, str, finalize_hint);

  return 0;
}

int
js_create_external_string_utf16le(js_env_t *env, utf16_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied) {
  int err;
  err = js_create_string_utf16le(env, str, len, result);
  if (err < 0) return err;

  if (copied) *copied = true;

  if (finalize_cb) finalize_cb(env, str, finalize_hint);

  return 0;
}

int
js_create_external_string_latin1(js_env_t *env, latin1_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied) {
  int err;
  err = js_create_string_latin1(env, str, len, result);
  if (err < 0) return err;

  if (copied) *copied = true;

  if (finalize_cb) finalize_cb(env, str, finalize_hint);

  return 0;
}

int
js_create_property_key_utf8(js_env_t *env, const utf8_t *str, size_t len, js_value_t **result) {
  return js_create_string_utf8(env, str, len, result);
}

int
js_create_property_key_utf16le(js_env_t *env, const utf16_t *str, size_t len, js_value_t **result) {
  return js_create_string_utf16le(env, str, len, result);
}

int
js_create_property_key_latin1(js_env_t *env, const latin1_t *str, size_t len, js_value_t **result) {
  return js_create_string_latin1(env, str, len, result);
}

int
js_create_symbol(js_env_t *env, js_value_t *description, js_value_t **result) {
  // Allow continuing even with a pending exception

  const char *str = description == NULL ? NULL : JS_ToCString(env->context, description->value);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewSymbol(env->context, str, false);

  if (str) JS_FreeCString(env->context, str);

  *result = wrapper;

  return 0;
}

int
js_symbol_for(js_env_t *env, const char *description, size_t len, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue symbol;

  char *copy = js__copy_name(description, len);

  symbol = JS_NewSymbol(env->context, copy, true);

  free(copy);

  if (JS_IsException(symbol)) return js__error(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = symbol;

  *result = wrapper;

  return 0;
}

int
js_create_object(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewObject(env->context);

  *result = wrapper;

  return 0;
}

int
js_create_object_with_prototype(js_env_t *env, js_value_t *prototype, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewObjectProto(env->context, prototype->value);

  *result = wrapper;

  return 0;
}

int
js_create_object_with_properties(js_env_t *env, js_value_t *prototype, js_value_t *const property_names[], js_value_t *const property_values[], size_t property_count, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewObjectProto(env->context, prototype->value);

  for (size_t i = 0; i < property_count; i++) {
    JSAtom atom = JS_ValueToAtom(env->context, property_names[i]->value);

    JS_SetProperty(env->context, wrapper->value, atom, JS_DupValue(env->context, property_values[i]->value));

    JS_FreeAtom(env->context, atom);
  }

  *result = wrapper;

  return 0;
}

static void
js__on_function_finalize(JSRuntime *runtime, JSValueConst value) {
  js_callback_t *callback = (js_callback_t *) JS_GetOpaque(value, js__function_class_id);

  free(callback);
}

static JSValue
js__on_function_call(JSContext *context, JSValueConst function, JSValueConst receiver, int argc, JSValueConst *argv, int flags) {
  int err;

  js_env_t *env = (js_env_t *) JS_GetRuntimeOpaque(JS_GetRuntime(context));

  js_callback_t *callback = (js_callback_t *) JS_GetOpaque(function, js__function_class_id);

  bool is_construct = (flags & JS_CALL_FLAG_CONSTRUCTOR) != 0;

  // A construct call passes the new target in place of the receiver, which is
  // instead created from the prototype of the new target.
  JSValueConst new_target = is_construct ? receiver : JS_UNDEFINED;

  JSValue instance = JS_UNDEFINED;

  if (is_construct) {
    JSValue prototype = JS_GetProperty(context, new_target, env->prototype_key);

    if (JS_IsException(prototype)) return JS_EXCEPTION;

    if (JS_IsObject(prototype)) {
      instance = JS_NewObjectProto(context, prototype);
    } else {
      instance = JS_NewObject(context);
    }

    JS_FreeValue(context, prototype);

    if (JS_IsException(instance)) return JS_EXCEPTION;

    receiver = instance;
  }

  js_callback_info_t callback_info = {
    .callback = callback,
    .argc = argc,
    .argv = argv,
    .receiver = receiver,
    .new_target = new_target,
  };

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  js_value_t *result = callback->cb(env, &callback_info);

  JSValue value;

  if (JS_HasException(env->context)) {
    value = JS_EXCEPTION;
  } else if (result == NULL || (is_construct && !JS_IsObject(result->value))) {
    value = JS_DupValue(env->context, instance);
  } else {
    value = JS_DupValue(env->context, result->value);
  }

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  JS_FreeValue(env->context, instance);

  return value;
}

int
js_create_function(js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue function = js__create_function(env, name, len, cb, data);

  if (JS_IsException(function)) return js__error(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = function;

  *result = wrapper;

  return 0;
}

// Synthesize the wrapper source `(function name(args) {\n source\n})` that binds
// the argument list and yields the function as its completion value. The name
// is bound only within the function itself, so functions of the same name do
// not collide. The returned buffer is owned by the caller (`free()`), with its
// length in `*result_len`.

static char *
js__function_source(js_env_t *env, const char *name, size_t name_len, js_value_t *const args[], size_t args_len, js_value_t *source, size_t *result_len) {
  if (name && name_len == (size_t) -1) name_len = strlen(name);

  if (name == NULL) name_len = 0;

  size_t source_len;
  const char *source_str = JS_ToCStringLen(env->context, &source_len, source->value);

  size_t *arg_lens = malloc(args_len * sizeof(size_t));
  const char **arg_strs = malloc(args_len * sizeof(char *));

  size_t buf_len = strlen("(function ") + name_len + strlen("(") + strlen(") {\n") + source_len + strlen("\n})");

  for (size_t i = 0; i < args_len; i++) {
    arg_strs[i] = JS_ToCStringLen(env->context, &arg_lens[i], args[i]->value);

    if (i != 0) buf_len += strlen(", ");

    buf_len += arg_lens[i];
  }

  char *buf = malloc(buf_len + 1 /* NULL */);

  size_t offset = 0;

#define V(str, len) \
  memcpy(&buf[offset], str, len); \
  offset += len;

  V("(function ", strlen("(function "));
  V(name, name_len);
  V("(", 1);

  for (size_t i = 0; i < args_len; i++) {
    if (i != 0) {
      V(", ", 2);
    }

    V(arg_strs[i], arg_lens[i]);

    JS_FreeCString(env->context, arg_strs[i]);
  }

  V(") {\n", strlen(") {\n"));
  V(source_str, source_len);
  V("\n})", strlen("\n})"));
#undef V

  buf[offset] = '\0';

  JS_FreeCString(env->context, source_str);

  free(arg_lens);
  free(arg_strs);

  *result_len = buf_len;

  return buf;
}

int
js_compile_function(js_env_t *env, const char *name, size_t name_len, const char *file, size_t file_len, js_value_t *const args[], size_t args_len, int offset, js_value_t *source, js_value_t **result) {
  return js_compile_function_with_code_cache(env, name, name_len, file, file_len, args, args_len, offset, source, NULL, 0, NULL, result);
}

int
js_compile_function_with_code_cache(js_env_t *env, const char *name, size_t name_len, const char *file, size_t file_len, js_value_t *const args[], size_t args_len, int offset, js_value_t *source, const void *cached_data, size_t cached_data_len, bool *cache_rejected, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (file == NULL) file = "";

  // The serializable form is the compiled-but-unevaluated program template. It
  // is materialized into the returned closure by evaluating a dup of it, and
  // stashed on the closure so it can be serialized later.

  size_t buf_len;
  char *buf = js__function_source(env, name, name_len, args, args_len, source, &buf_len);

  uint64_t source_hash;

  // The synthesized wrapper puts the source on the line after the one it
  // starts on.
  JSValue bytecode = js__compile_with_code_cache(env, source->value, buf, buf_len, file, offset - 1, JS_EVAL_TYPE_GLOBAL, JS_READ_OBJ_BYTECODE, cached_data, cached_data_len, cache_rejected, &source_hash);

  free(buf);

  if (JS_IsException(bytecode)) return js__report_exception(env);

  // The unit is held by the bytecode it is bound to.
  JSValue unit = js__create_unit(env, file, source_hash, bytecode);

  JS_FreeValue(env->context, unit);

  JSValue function = JS_EvalFunction(env->context, JS_DupValue(env->context, bytecode));

  if (JS_IsException(function)) {
    JS_FreeValue(env->context, bytecode);

    return js__report_exception(env);
  }

  JSValue length = JS_GetPropertyStr(env->context, function, "length");

  double arity;

  JS_ToFloat64(env->context, &arity, length);

  JS_FreeValue(env->context, length);

  // An argument name that is not an identifier can still parse as part of the
  // parameter list, an empty name and one smuggling several parameters among
  // them, and the engine takes those without a word. Either way the function
  // ends up with a different arity than it was asked for.
  if (arity != (double) args_len) {
    JS_FreeValue(env->context, bytecode);
    JS_FreeValue(env->context, function);

    err = js_throw_errorf(env, NULL, "Could not compile function");
    assert(err == 0);

    return js__error(env);
  }

  // Stash the program template so `js_create_function_code_cache()` can
  // serialize it. Ownership of `bytecode` transfers to the property.

  JS_DefinePropertyValue(env->context, function, env->function_bytecode_key, bytecode, 0);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = function;

  *result = wrapper;

  return 0;
}

int
js_create_function_code_cache(js_env_t *env, js_value_t *function, void **data, size_t *len) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  // Only a function from `js_compile_function()` carries the program template
  // needed to serialize; any other function has no cache to produce.
  JSValue bytecode = JS_GetProperty(env->context, function->value, env->function_bytecode_key);

  JSValue unit = JS_GetHostDefined(function->value);

  if (JS_IsUndefined(bytecode) || js__get_unit(unit) == NULL) {
    JS_FreeValue(env->context, bytecode);
    JS_FreeValue(env->context, unit);

    err = js_throw_error(env, NULL, "Cannot create a code cache for this function");
    assert(err == 0);

    return js__error(env);
  }

  err = js__create_code_cache(env, bytecode, js__get_unit(unit)->source_hash, data, len);

  JS_FreeValue(env->context, bytecode);
  JS_FreeValue(env->context, unit);

  return err;
}

int
js_create_function_with_source(js_env_t *env, const char *name, size_t name_len, const char *file, size_t file_len, js_value_t *const args[], size_t args_len, int offset, js_value_t *source, js_value_t **result) {
  return js_compile_function(env, name, name_len, file, file_len, args, args_len, offset, source, result);
}

int
js_on_function_dynamic_import(js_env_t *env, js_value_t *function, js_dynamic_import_cb cb, void *data) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue unit = JS_GetHostDefined(function->value);

  int err = js__set_unit_dynamic_import(env, unit, cb, data);

  JS_FreeValue(env->context, unit);

  return err;
}

int
js_get_function_id(js_env_t *env, js_value_t *function, js_value_t **result) {
  // Allow continuing even with a pending exception

  // Functions defined by code without a unit, such as scripts run with
  // `js_run_script()`, are attributed to the environment's default identifier.
  JSValue unit = JS_GetHostDefined(function->value);

  JSValue id = js__get_unit(unit) ? js__get_unit_id(env, unit) : JS_DupValue(env->context, js__default_module_id(env));

  JS_FreeValue(env->context, unit);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = id;

  *result = wrapper;

  return 0;
}

int
js_create_typed_function(js_env_t *env, const char *name, size_t len, js_function_cb cb, const js_callback_signature_t *signature, const void *address, void *data, js_value_t **result) {
  return js_create_function(env, name, len, cb, data, result);
}

int
js_create_array(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewArray(env->context);

  *result = wrapper;

  return 0;
}

int
js_create_array_with_length(js_env_t *env, size_t len, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NewArray(env->context);

  JS_SetLength(env->context, wrapper->value, len);

  *result = wrapper;

  return 0;
}

int
js_create_array_with_elements(js_env_t *env, js_value_t *const elements[], size_t element_count, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  JSValue stack[16];

  JSValue *values = element_count <= 16 ? stack : malloc(element_count * sizeof(JSValue));

  for (size_t i = 0; i < element_count; i++) {
    values[i] = JS_DupValue(env->context, elements[i]->value);
  }

  wrapper->value = JS_NewArrayFrom(env->context, element_count, values);

  if (values != stack) free(values);

  *result = wrapper;

  return 0;
}

static void
js__on_external_finalize(JSRuntime *runtime, JSValueConst value) {
  js_env_t *env = (js_env_t *) JS_GetRuntimeOpaque(runtime);

  js_finalizer_t *finalizer = (js_finalizer_t *) JS_GetOpaque(value, js__external_class_id);

  // A wrap that failed to install leaves behind an external that never received
  // an opaque of its own.
  if (finalizer == NULL) return;

  js__defer_finalizer(env, finalizer);

  free(finalizer);
}

int
js_create_external(js_env_t *env, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_finalizer_t *finalizer = malloc(sizeof(js_finalizer_t));

  js__init_finalizer(env, finalizer, data, finalize_cb, finalize_hint);

  JSValue external = JS_NewObjectClass(env->context, js__external_class_id);

  JS_SetOpaque(external, finalizer);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = external;

  *result = wrapper;

  return 0;
}

int
js_create_date(js_env_t *env, double time, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue date = JS_NewDate(env->context, time);

  if (JS_IsException(date)) return js__error(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = date;

  *result = wrapper;

  return 0;
}

typedef JSValue (*js_error_constructor_t)(JSContext *, const char *, ...);

static JSValue
js__create_error(js_env_t *env, js_error_constructor_t constructor, JSValueConst code, JSValue message) {
  JSValue error = constructor(env->context, "%s", "");

  JS_DefinePropertyValueStr(env->context, error, "message", message, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  if (!JS_IsUndefined(code)) {
    JS_DefinePropertyValueStr(env->context, error, "code", JS_DupValue(env->context, code), JS_PROP_C_W_E);
  }

  return error;
}

static int
js__throw_error(js_env_t *env, js_error_constructor_t constructor, const char *code, const char *message) {
  JSValue code_value = code ? JS_NewString(env->context, code) : JS_UNDEFINED;

  JS_Throw(env->context, js__create_error(env, constructor, code_value, JS_NewString(env->context, message)));

  JS_FreeValue(env->context, code_value);

  return 0;
}

int
js_create_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = js__create_error(env, JS_NewPlainError, code ? code->value : JS_UNDEFINED, JS_DupValue(env->context, message->value));

  *result = wrapper;

  return 0;
}

int
js_create_type_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = js__create_error(env, JS_NewTypeError, code ? code->value : JS_UNDEFINED, JS_DupValue(env->context, message->value));

  *result = wrapper;

  return 0;
}

int
js_create_range_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = js__create_error(env, JS_NewRangeError, code ? code->value : JS_UNDEFINED, JS_DupValue(env->context, message->value));

  *result = wrapper;

  return 0;
}

int
js_create_syntax_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = js__create_error(env, JS_NewSyntaxError, code ? code->value : JS_UNDEFINED, JS_DupValue(env->context, message->value));

  *result = wrapper;

  return 0;
}

int
js_create_reference_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = js__create_error(env, JS_NewReferenceError, code ? code->value : JS_UNDEFINED, JS_DupValue(env->context, message->value));

  *result = wrapper;

  return 0;
}

int
js_get_error_location(js_env_t *env, js_value_t *error, js_error_location_t *result) {
  // Allow continuing even with a pending exception

  JSValue name = JS_UNDEFINED;
  JSValue source = JS_UNDEFINED;

  int line, column;

  if (JS_GetErrorLocation(env->context, error->value, &name, &line, &column) == 0) {
    // Columns are reported counting from 1, and only the start of the offending
    // source text is known.
    result->line = line;
    result->column_start = column - 1;
    result->column_end = column;

    source = JS_GetProperty(env->context, error->value, env->error_source_key);
  } else {
    result->line = 0;
    result->column_start = -1;
    result->column_end = -1;
  }

  js_value_t *wrapper;

  wrapper = js__create_handle(env, env->scope);
  wrapper->value = name;
  result->name = wrapper;

  wrapper = js__create_handle(env, env->scope);
  wrapper->value = source;
  result->source = wrapper;

  return 0;
}

int
js_create_promise(js_env_t *env, js_deferred_t **deferred, js_value_t **promise) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue functions[2];

  JSValue value = JS_NewPromiseCapability(env->context, functions);

  if (JS_IsException(value)) return js__error(env);

  *deferred = malloc(sizeof(js_deferred_t));

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = value;

  (*deferred)->resolve = functions[0];
  (*deferred)->reject = functions[1];

  // A deferred that is never settled still holds the functions that would have
  // settled it, which the environment gives back on teardown.
  intrusive_list_prepend(&env->deferreds, &(*deferred)->list);

  *promise = wrapper;

  return 0;
}

static inline int
js__conclude_deferred(js_env_t *env, js_deferred_t *deferred, js_value_t *resolution, bool resolved) {
  JSValue global = JS_GetGlobalObject(env->context);

  // Resolving with a thenable reads its `then` property, which may run
  // JavaScript.
  env->depth++;

  JSValue result = JS_Call(env->context, resolved ? deferred->resolve : deferred->reject, global, 1, &resolution->value);

  env->depth--;

  JS_FreeValue(env->context, global);
  JS_FreeValue(env->context, result);
  JS_FreeValue(env->context, deferred->resolve);
  JS_FreeValue(env->context, deferred->reject);

  intrusive_list_remove(&env->deferreds, &deferred->list);

  free(deferred);

  if (JS_IsException(result)) return js__report_exception(env);

  if (env->depth == 0) js__run_microtasks(env);

  return 0;
}

int
js_resolve_deferred(js_env_t *env, js_deferred_t *deferred, js_value_t *resolution) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__conclude_deferred(env, deferred, resolution, true);
}

int
js_reject_deferred(js_env_t *env, js_deferred_t *deferred, js_value_t *resolution) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__conclude_deferred(env, deferred, resolution, false);
}

int
js_get_promise_state(js_env_t *env, js_value_t *promise, js_promise_state_t *result) {
  // Allow continuing even with a pending exception

  switch (JS_PromiseState(env->context, promise->value)) {
  case JS_PROMISE_PENDING:
  default:
    *result = js_promise_pending;
    break;
  case JS_PROMISE_FULFILLED:
    *result = js_promise_fulfilled;
    break;
  case JS_PROMISE_REJECTED:
    *result = js_promise_rejected;
    break;
  }

  return 0;
}

int
js_get_promise_result(js_env_t *env, js_value_t *promise, js_value_t **result) {
  // Allow continuing even with a pending exception

  assert(JS_PromiseState(env->context, promise->value) != JS_PROMISE_PENDING);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_PromiseResult(env->context, promise->value);

  *result = wrapper;

  return 0;
}

int
js_create_arraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (len > INT32_MAX) goto err;

  uint8_t *bytes = js__on_arraybuffer_malloc(NULL, len);

  if (bytes == NULL) goto err;

  memset(bytes, 0, len);

  if (data) {
    *data = bytes;
  }

  JSValue arraybuffer = JS_NewArrayBuffer(env->context, bytes, len, 0, js__on_arraybuffer_realloc, NULL, false);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = arraybuffer;

  *result = wrapper;

  return 0;

err:
  err = js_throw_range_error(env, NULL, "Array buffer allocation failed");
  assert(err == 0);

  return js__error(env);
}

static void
js__release_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store, bool deferred) {
  if (--backing_store->references > 0) return;

  if (backing_store->external) {
    if (deferred) {
      js__defer_finalize(env, backing_store->data, backing_store->finalize_cb, backing_store->finalize_hint);
    } else if (backing_store->finalize_cb) {
      backing_store->finalize_cb(env, backing_store->data, backing_store->finalize_hint);
    }
  } else if (backing_store->data) {
    js__on_arraybuffer_free(NULL, backing_store->data);
  }

  free(backing_store);
}

static void *
js__on_backed_arraybuffer_realloc(JSRuntime *runtime, void *opaque, void *ptr, size_t len) {
  // Memory that belongs to a backing store cannot move, so the buffer cannot be
  // resized.
  if (len > 0) return NULL;

  js__release_backing_store((js_env_t *) JS_GetRuntimeOpaque(runtime), (js_arraybuffer_backing_store_t *) opaque, true);

  return NULL;
}

static JSValue
js__create_backed_arraybuffer(js_env_t *env, js_arraybuffer_backing_store_t *backing_store, bool shared) {
  return JS_NewArrayBuffer(env->context, backing_store->data, backing_store->len, 0, js__on_backed_arraybuffer_realloc, backing_store, shared);
}

static int
js__create_external_arraybuffer(js_env_t *env, void *data, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, bool shared, js_value_t **result) {
  js_arraybuffer_backing_store_t *backing_store = malloc(sizeof(js_arraybuffer_backing_store_t));

  backing_store->references = 1;
  backing_store->len = data ? len : 0;
  backing_store->data = data;
  backing_store->external = true;
  backing_store->finalize_cb = finalize_cb;
  backing_store->finalize_hint = finalize_hint;

  JSValue arraybuffer = js__create_backed_arraybuffer(env, backing_store, shared);

  if (JS_IsException(arraybuffer)) return js__error(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = arraybuffer;

  *result = wrapper;

  return 0;
}

int
js_create_arraybuffer_with_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store, void **data, size_t *len, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  if (data) {
    *data = backing_store->data;
  }

  if (len) {
    *len = backing_store->len;
  }

  JSValue arraybuffer;

  if (backing_store->data || backing_store->external) {
    backing_store->references++;

    arraybuffer = js__create_backed_arraybuffer(env, backing_store, false);
  } else {
    arraybuffer = JS_NewArrayBuffer(env->context, NULL, 0, 0, NULL, NULL, false);
  }

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = arraybuffer;

  *result = wrapper;

  return 0;
}

int
js_create_unsafe_arraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (len > INT32_MAX) goto err;

  uint8_t *bytes = js__on_arraybuffer_malloc(NULL, len);

  if (bytes == NULL) goto err;

  if (data) {
    *data = bytes;
  }

  JSValue arraybuffer = JS_NewArrayBuffer(env->context, bytes, len, 0, js__on_arraybuffer_realloc, NULL, false);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = arraybuffer;

  *result = wrapper;

  return 0;

err:
  err = js_throw_range_error(env, NULL, "Array buffer allocation failed");
  assert(err == 0);

  return js__error(env);
}

int
js_create_external_arraybuffer(js_env_t *env, void *data, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__create_external_arraybuffer(env, data, len, finalize_cb, finalize_hint, false, result);
}

int
js_detach_arraybuffer(js_env_t *env, js_value_t *arraybuffer) {
  // Allow continuing even with a pending exception

  void *opaque;

  JSReallocArrayBufferDataFunc *realloc_func = JS_GetArrayBufferReallocFunc(arraybuffer->value, &opaque);

  // Detaching finalizes the backing store right away rather than deferring it
  // as a collection would, so it is held across the detach and released here.
  js_arraybuffer_backing_store_t *backing_store = realloc_func == js__on_backed_arraybuffer_realloc ? opaque : NULL;

  if (backing_store) backing_store->references++;

  JS_DetachArrayBuffer(env->context, arraybuffer->value);

  if (backing_store) js__release_backing_store(env, backing_store, false);

  return 0;
}

static int
js__get_backing_store(js_env_t *env, JSValueConst arraybuffer, js_arraybuffer_backing_store_t **result) {
  void *opaque;

  JSReallocArrayBufferDataFunc *realloc_func = JS_GetArrayBufferReallocFunc(arraybuffer, &opaque);

  if (realloc_func == js__on_backed_arraybuffer_realloc) {
    js_arraybuffer_backing_store_t *backing_store = (js_arraybuffer_backing_store_t *) opaque;

    backing_store->references++;

    *result = backing_store;

    return 0;
  }

  js_arraybuffer_backing_store_t *backing_store = malloc(sizeof(js_arraybuffer_backing_store_t));

  backing_store->references = 1;
  backing_store->external = false;

  // Any other buffer that is not detached holds memory with a header, whether
  // allocated by the runtime or by the SharedArrayBuffer functions.
  if (JS_IsDetachedArrayBuffer(arraybuffer)) {
    backing_store->data = NULL;
    backing_store->len = 0;
  } else {
    backing_store->data = JS_GetArrayBuffer(env->context, &backing_store->len, arraybuffer);

    js__on_arraybuffer_dup(NULL, backing_store->data);
  }

  *result = backing_store;

  return 0;
}

int
js_get_arraybuffer_backing_store(js_env_t *env, js_value_t *arraybuffer, js_arraybuffer_backing_store_t **result) {
  // Allow continuing even with a pending exception

  return js__get_backing_store(env, arraybuffer->value, result);
}

int
js_create_sharedarraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  js_arraybuffer_header_t *header = malloc(sizeof(js_arraybuffer_header_t) + len);

  header->len = len;
  header->data = (uint8_t *) header + sizeof(*header);

  atomic_init(&header->references, 0);

  memset(header->data, 0, len);

  if (data) *data = header->data;

  JSValue sharedarraybuffer = JS_NewArrayBuffer(env->context, header->data, header->len, 0, NULL, NULL, true);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = sharedarraybuffer;

  *result = wrapper;

  return 0;
}

int
js_create_external_sharedarraybuffer(js_env_t *env, void *data, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__create_external_arraybuffer(env, data, len, finalize_cb, finalize_hint, true, result);
}

int
js_create_sharedarraybuffer_with_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store, void **data, size_t *len, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  if (data) {
    *data = backing_store->data;
  }

  if (len) {
    *len = backing_store->len;
  }

  JSValue sharedarraybuffer;

  if (backing_store->external) {
    backing_store->references++;

    sharedarraybuffer = js__create_backed_arraybuffer(env, backing_store, true);
  } else {
    sharedarraybuffer = JS_NewArrayBuffer(env->context, backing_store->data, backing_store->len, 0, NULL, NULL, true);
  }

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = sharedarraybuffer;

  *result = wrapper;

  return 0;
}

int
js_create_unsafe_sharedarraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  js_arraybuffer_header_t *header = malloc(sizeof(js_arraybuffer_header_t) + len);

  header->len = len;
  header->data = (uint8_t *) header + sizeof(*header);

  atomic_init(&header->references, 0);

  if (data) *data = header->data;

  JSValue sharedarraybuffer = JS_NewArrayBuffer(env->context, header->data, header->len, 0, NULL, NULL, true);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = sharedarraybuffer;

  *result = wrapper;

  return 0;
}

int
js_get_sharedarraybuffer_backing_store(js_env_t *env, js_value_t *sharedarraybuffer, js_arraybuffer_backing_store_t **result) {
  // Allow continuing even with a pending exception

  return js__get_backing_store(env, sharedarraybuffer->value, result);
}

int
js_release_arraybuffer_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store) {
  // Allow continuing even with a pending exception

  js__release_backing_store(env, backing_store, false);

  return 0;
}

int
js_create_typedarray(js_env_t *env, js_typedarray_type_t type, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSTypedArrayEnum array_type;

  switch (type) {
#define V(enum, type) \
  case type: \
    array_type = enum; \
    break;

    V(JS_TYPED_ARRAY_INT8, js_int8array);
    V(JS_TYPED_ARRAY_UINT8, js_uint8array);
    V(JS_TYPED_ARRAY_UINT8C, js_uint8clampedarray);
    V(JS_TYPED_ARRAY_INT16, js_int16array);
    V(JS_TYPED_ARRAY_UINT16, js_uint16array);
    V(JS_TYPED_ARRAY_INT32, js_int32array);
    V(JS_TYPED_ARRAY_UINT32, js_uint32array);
    V(JS_TYPED_ARRAY_FLOAT16, js_float16array);
    V(JS_TYPED_ARRAY_FLOAT32, js_float32array);
    V(JS_TYPED_ARRAY_FLOAT64, js_float64array);
    V(JS_TYPED_ARRAY_BIG_INT64, js_bigint64array);
    V(JS_TYPED_ARRAY_BIG_UINT64, js_biguint64array);
#undef V
  }

  JSValue argv[3] = {arraybuffer->value, JS_NewInt64(env->context, offset), JS_NewInt64(env->context, len)};

  JSValue typedarray = JS_NewTypedArray(env->context, 3, argv, array_type);

  if (JS_IsException(typedarray)) return js__error(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = typedarray;

  *result = wrapper;

  return 0;
}

int
js_create_dataview(js_env_t *env, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue argv[3] = {arraybuffer->value, JS_NewInt64(env->context, offset), JS_NewInt64(env->context, len)};

  JSValue dataview = JS_NewDataView(env->context, 3, argv);

  if (JS_IsException(dataview)) return js__error(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = dataview;

  *result = wrapper;

  return 0;
}

int
js_coerce_to_boolean(js_env_t *env, js_value_t *value, js_value_t **result) {
  // Allow continuing even with a pending exception

  JSValue boolean = JS_NewBool(env->context, JS_ToBool(env->context, value->value) == 1);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = boolean;

  *result = wrapper;

  return 0;
}

int
js_coerce_to_number(js_env_t *env, js_value_t *value, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  env->depth++;

  JSValue number = JS_ToNumber(env->context, value->value);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (JS_IsException(number)) return js__report_exception(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = number;

  *result = wrapper;

  return 0;
}

int
js_coerce_to_string(js_env_t *env, js_value_t *value, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  env->depth++;

  JSValue string = JS_ToString(env->context, value->value);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (JS_IsException(string)) return js__report_exception(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = string;

  *result = wrapper;

  return 0;
}

int
js_coerce_to_object(js_env_t *env, js_value_t *value, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue object = JS_ToObject(env->context, value->value);

  if (JS_IsException(object)) return js__error(env);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = object;

  *result = wrapper;

  return 0;
}

int
js_typeof(js_env_t *env, js_value_t *value, js_value_type_t *result) {
  // Allow continuing even with a pending exception

  if (JS_IsNumber(value->value)) {
    *result = js_number;
  } else if (JS_IsBigInt(value->value)) {
    *result = js_bigint;
  } else if (JS_IsString(value->value)) {
    *result = js_string;
  } else if (JS_IsFunction(env->context, value->value)) {
    *result = js_function;
  } else if (JS_IsObject(value->value)) {
    *result = JS_GetOpaque(value->value, js__external_class_id) != NULL
                ? js_external
                : js_object;
  } else if (JS_IsBool(value->value)) {
    *result = js_boolean;
  } else if (JS_IsUndefined(value->value)) {
    *result = js_undefined;
  } else if (JS_IsSymbol(value->value)) {
    *result = js_symbol;
  } else if (JS_IsNull(value->value)) {
    *result = js_null;
  }

  return 0;
}

int
js_instanceof(js_env_t *env, js_value_t *object, js_value_t *constructor, bool *result) {
  if (JS_HasException(env->context)) return js__error(env);

  env->depth++;

  int success = JS_IsInstanceOf(env->context, object->value, constructor->value);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) return js__report_exception(env);

  *result = success;

  return 0;
}

int
js_is_undefined(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsUndefined(value->value);

  return 0;
}

int
js_is_null(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsNull(value->value);

  return 0;
}

int
js_is_boolean(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsBool(value->value);

  return 0;
}

int
js_is_boolean_object(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsBooleanObject(value->value);

  return 0;
}

int
js_is_number(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsNumber(value->value);

  return 0;
}

int
js_is_number_object(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsNumberObject(value->value);

  return 0;
}

int
js_is_int32(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  if (JS_IsNumber(value->value)) {
    double number;

    JS_ToFloat64(env->context, &number, value->value);

    double integral;

    *result = modf(number, &integral) == 0.0 && integral >= INT32_MIN && integral <= INT32_MAX;
  } else {
    *result = false;
  }

  return 0;
}

int
js_is_uint32(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  if (JS_IsNumber(value->value)) {
    double number;

    JS_ToFloat64(env->context, &number, value->value);

    double integral;

    *result = modf(number, &integral) == 0.0 && integral >= 0.0 && integral <= UINT32_MAX;
  } else {
    *result = false;
  }

  return 0;
}

int
js_is_string(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsString(value->value);

  return 0;
}

int
js_is_string_object(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsStringObject(value->value);

  return 0;
}

int
js_is_symbol(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsSymbol(value->value);

  return 0;
}

int
js_is_symbol_object(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsSymbolObject(value->value);

  return 0;
}

int
js_is_object(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsObject(value->value);

  return 0;
}

int
js_is_function(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsFunction(env->context, value->value);

  return 0;
}

int
js_is_async_function(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsAsyncFunction(value->value);

  return 0;
}

int
js_is_generator_function(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsGeneratorFunction(value->value);

  return 0;
}

int
js_is_generator(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsGenerator(value->value);

  return 0;
}

int
js_is_arguments(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsArguments(value->value);

  return 0;
}

int
js_is_array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  // The engine answers for the target of a proxy as well, whereas a proxy is a
  // proxy no matter what it wraps.
  *result = !JS_IsProxy(value->value) && JS_IsArray(value->value);

  return 0;
}

int
js_is_external(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsObject(value->value) && JS_GetOpaque(value->value, js__external_class_id) != NULL;

  return 0;
}

int
js_is_wrapped(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  int err;

  *result = false;

  if (!JS_IsObject(value->value)) return 0;

  JSValue external;
  err = js__get_own_external(env, value->value, "__native_external", &external);
  if (err < 0) return err;

  *result = JS_GetOpaque(external, js__external_class_id) != NULL;

  JS_FreeValue(env->context, external);

  return 0;
}

int
js_is_delegate(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsObject(value->value) && JS_GetOpaque(value->value, js__delegate_class_id) != NULL;

  return 0;
}

int
js_is_bigint(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsBigInt(value->value);

  return 0;
}

int
js_is_bigint_object(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsBigIntObject(value->value);

  return 0;
}

int
js_is_date(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsDate(value->value);

  return 0;
}

int
js_is_regexp(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsRegExp(value->value);

  return 0;
}

int
js_is_error(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsError(value->value);

  return 0;
}

int
js_is_promise(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsPromise(value->value);

  return 0;
}

int
js_is_proxy(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsProxy(value->value);

  return 0;
}

int
js_is_map(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsMap(value->value);

  return 0;
}

int
js_is_map_iterator(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsMapIterator(value->value);

  return 0;
}

int
js_is_set(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsSet(value->value);

  return 0;
}

int
js_is_set_iterator(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsSetIterator(value->value);

  return 0;
}

int
js_is_weak_map(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsWeakMap(value->value);

  return 0;
}

int
js_is_weak_set(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsWeakSet(value->value);

  return 0;
}

int
js_is_weak_ref(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsWeakRef(value->value);

  return 0;
}

int
js_is_arraybuffer(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsArrayBuffer(value->value);

  return 0;
}

int
js_is_detached_arraybuffer(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsDetachedArrayBuffer(value->value);

  return 0;
}

int
js_is_detachable_arraybuffer(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsArrayBuffer(value->value);

  return 0;
}

int
js_is_sharedarraybuffer(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsSharedArrayBuffer(value->value);

  return 0;
}

int
js_is_typedarray(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) >= 0;

  return 0;
}

int
js_is_int8array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_INT8;

  return 0;
}

int
js_is_uint8array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_UINT8;

  return 0;
}

int
js_is_uint8clampedarray(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_UINT8C;

  return 0;
}

int
js_is_int16array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_INT16;

  return 0;
}

int
js_is_uint16array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_UINT16;

  return 0;
}

int
js_is_int32array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_INT32;

  return 0;
}

int
js_is_uint32array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_UINT32;

  return 0;
}

int
js_is_float16array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_FLOAT16;

  return 0;
}

int
js_is_float32array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_FLOAT32;

  return 0;
}

int
js_is_float64array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_FLOAT64;

  return 0;
}

int
js_is_bigint64array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_BIG_INT64;

  return 0;
}

int
js_is_biguint64array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_GetTypedArrayType(value->value) == JS_TYPED_ARRAY_BIG_UINT64;

  return 0;
}

int
js_is_dataview(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsDataView(value->value);

  return 0;
}

int
js_is_module_namespace(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsModuleNamespace(value->value);

  return 0;
}

int
js_get_object_type(js_env_t *env, js_value_t *value, js_object_type_t *result) {
  // Allow continuing even with a pending exception

  int err;

  bool is;

  // Classify by way of the individual predicates, in the order of precedence
  // documented for `js_object_type_t`, so that the two cannot drift apart. The
  // saving is in classifying with a single call, not in the predicates
  // themselves, which are all cheap.
#define V(type, predicate) \
  err = predicate(env, value, &is); \
  assert(err == 0); \
  if (is) { \
    *result = type; \
    return 0; \
  }

  V(js_array, js_is_array)
  V(js_arguments, js_is_arguments)
  V(js_date, js_is_date)
  V(js_regexp, js_is_regexp)
  V(js_error, js_is_error)
  V(js_promise, js_is_promise)
  V(js_proxy, js_is_proxy)
  V(js_generator, js_is_generator)
  V(js_map, js_is_map)
  V(js_set, js_is_set)
  V(js_map_iterator, js_is_map_iterator)
  V(js_set_iterator, js_is_set_iterator)
  V(js_weak_map, js_is_weak_map)
  V(js_weak_set, js_is_weak_set)
  V(js_weak_ref, js_is_weak_ref)
  V(js_arraybuffer, js_is_arraybuffer)
  V(js_sharedarraybuffer, js_is_sharedarraybuffer)
  V(js_typedarray, js_is_typedarray)
  V(js_dataview, js_is_dataview)
  V(js_module_namespace, js_is_module_namespace)
  V(js_boolean_object, js_is_boolean_object)
  V(js_number_object, js_is_number_object)
  V(js_string_object, js_is_string_object)
  V(js_symbol_object, js_is_symbol_object)
  V(js_bigint_object, js_is_bigint_object)
  V((js_object_type_t) js_external, js_is_external)
#undef V

  *result = (js_object_type_t) js_object;

  return 0;
}

int
js_strict_equals(js_env_t *env, js_value_t *a, js_value_t *b, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_IsStrictEqual(env->context, a->value, b->value);

  return 0;
}

int
js_get_global(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_GetGlobalObject(env->context);

  *result = wrapper;

  return 0;
}

int
js_get_object_global(js_env_t *env, js_value_t *object, js_value_t **result) {
  return js_get_null(env, result);
}

int
js_get_undefined(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_UNDEFINED;

  *result = wrapper;

  return 0;
}

int
js_get_null(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_NULL;

  *result = wrapper;

  return 0;
}

int
js_get_boolean(js_env_t *env, bool value, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = value ? JS_TRUE : JS_FALSE;

  *result = wrapper;

  return 0;
}

int
js_get_value_bool(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_ToBool(env->context, value->value);

  return 0;
}

int
js_get_value_int32(js_env_t *env, js_value_t *value, int32_t *result) {
  // Allow continuing even with a pending exception

  JS_ToInt32(env->context, result, value->value);

  return 0;
}

int
js_get_value_uint32(js_env_t *env, js_value_t *value, uint32_t *result) {
  // Allow continuing even with a pending exception

  JS_ToUint32(env->context, result, value->value);

  return 0;
}

int
js_get_value_int64(js_env_t *env, js_value_t *value, int64_t *result) {
  // Allow continuing even with a pending exception

  double number;

  JS_ToFloat64(env->context, &number, value->value);

  // The conversion offered by the engine wraps around the range rather than
  // clamping to it, so the number is narrowed here instead.
  if (!isfinite(number)) *result = 0;
  else if (number <= (double) INT64_MIN) *result = INT64_MIN;
  else if (number >= (double) INT64_MAX) *result = INT64_MAX;
  else *result = (int64_t) number;

  return 0;
}

int
js_get_value_double(js_env_t *env, js_value_t *value, double *result) {
  // Allow continuing even with a pending exception

  JS_ToFloat64(env->context, result, value->value);

  return 0;
}

int
js_get_value_bigint_int64(js_env_t *env, js_value_t *value, int64_t *result, bool *lossless) {
  // Allow continuing even with a pending exception

  JS_ToBigInt64(env->context, result, value->value);

  if (lossless) {
    int sign;
    uint64_t magnitude = 0;
    size_t len = 1;

    JS_ToBigIntWords(env->context, value->value, &sign, &magnitude, &len);

    *lossless = len <= 1 && (sign ? magnitude <= (uint64_t) INT64_MAX + 1 : magnitude <= INT64_MAX);
  }

  return 0;
}

int
js_get_value_bigint_uint64(js_env_t *env, js_value_t *value, uint64_t *result, bool *lossless) {
  // Allow continuing even with a pending exception

  JS_ToBigUint64(env->context, result, value->value);

  if (lossless) {
    int sign;
    uint64_t magnitude = 0;
    size_t len = 1;

    JS_ToBigIntWords(env->context, value->value, &sign, &magnitude, &len);

    *lossless = len == 0 || (len == 1 && !sign);
  }

  return 0;
}

int
js_get_value_bigint_words(js_env_t *env, js_value_t *value, int *sign, uint64_t *words, size_t len, size_t *result) {
  // Allow continuing even with a pending exception

  size_t needed = len;

  int err = JS_ToBigIntWords(env->context, value->value, sign, words, &needed);
  assert(err == 0);

  if (result) *result = words == NULL || needed < len ? needed : len;

  return 0;
}

int
js_get_value_string_utf8(js_env_t *env, js_value_t *value, utf8_t *str, size_t len, size_t *result) {
  // Allow continuing even with a pending exception

  size_t cstr_len;
  const char *cstr = JS_ToCStringLen(env->context, &cstr_len, value->value);

  if (str == NULL) {
    *result = cstr_len;
  } else if (len != 0) {
    size_t written = cstr_len < len ? cstr_len : len;

    memcpy(str, cstr, written);

    if (written < len) str[written] = '\0';

    if (result) *result = written;
  } else if (result) *result = 0;

  JS_FreeCString(env->context, cstr);

  return 0;
}

int
js_get_value_string_utf16le(js_env_t *env, js_value_t *value, utf16_t *str, size_t len, size_t *result) {
  // Allow continuing even with a pending exception

  JSValue string = JS_ToString(env->context, value->value);

  size_t string_len;
  bool wide;

  const void *data = JS_GetStringBuffer(string, &string_len, &wide);

  if (str == NULL) {
    *result = string_len;
  } else if (len != 0) {
    size_t written = string_len < len ? string_len : len;

    if (wide) {
      memcpy(str, data, written * sizeof(utf16_t));
    } else {
      for (size_t i = 0; i < written; i++) str[i] = ((const latin1_t *) data)[i];
    }

    if (written < len) str[written] = 0;

    if (result) *result = written;
  } else if (result) *result = 0;

  JS_FreeValue(env->context, string);

  return 0;
}

int
js_get_value_string_latin1(js_env_t *env, js_value_t *value, latin1_t *str, size_t len, size_t *result) {
  // Allow continuing even with a pending exception

  size_t cstr_len;
  const char *cstr = JS_ToCStringLen(env->context, &cstr_len, value->value);

  size_t latin1_len = latin1_length_from_utf8((utf8_t *) cstr, cstr_len);

  if (str == NULL) {
    *result = latin1_len;
  } else if (len != 0) {
    size_t written = latin1_len < len ? latin1_len : len;

    utf8_convert_to_latin1((utf8_t *) cstr, cstr_len, str);

    if (written < len) str[written] = '\0';

    if (result) *result = written;
  } else if (result) *result = 0;

  JS_FreeCString(env->context, cstr);

  return 0;
}

int
js_get_value_external(js_env_t *env, js_value_t *value, void **result) {
  // Allow continuing even with a pending exception

  js_finalizer_t *finalizer = (js_finalizer_t *) JS_GetOpaque(value->value, js__external_class_id);

  *result = finalizer->data;

  return 0;
}

int
js_get_value_date(js_env_t *env, js_value_t *value, double *result) {
  // Allow continuing even with a pending exception

  JS_ToFloat64(env->context, result, value->value);

  return 0;
}

int
js_get_array_length(js_env_t *env, js_value_t *array, uint32_t *result) {
  // Allow continuing even with a pending exception

  JSValue length = JS_GetPropertyStr(env->context, array->value, "length");

  JS_ToUint32(env->context, result, length);

  JS_FreeValue(env->context, length);

  return 0;
}

int
js_get_array_elements(js_env_t *env, js_value_t *array, js_value_t **elements, size_t len, size_t offset, uint32_t *result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  uint32_t written = 0;

  env->depth++;

  uint32_t m;
  err = js_get_array_length(env, array, &m);
  assert(err == 0);

  for (uint32_t i = 0, n = len, j = offset; i < n && j < m; i++, j++) {
    JSValue value = JS_GetPropertyUint32(env->context, array->value, j);

    if (env->depth == 1) js__run_microtasks(env);

    if (JS_IsException(value)) {
      env->depth--;

      return js__report_exception(env);
    }

    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    elements[i] = wrapper;

    written++;
  }

  env->depth--;

  if (result) *result = written;

  return 0;
}

int
js_set_array_elements(js_env_t *env, js_value_t *array, js_value_t *const elements[], size_t len, size_t offset) {
  if (JS_HasException(env->context)) return js__error(env);

  env->depth++;

  for (uint32_t i = 0, n = len, j = offset; i < n; i++, j++) {
    int success = JS_SetPropertyUint32(env->context, array->value, j, JS_DupValue(env->context, elements[i]->value));

    if (env->depth == 1) js__run_microtasks(env);

    if (success < 0) {
      env->depth--;

      return js__report_exception(env);
    }
  }

  env->depth--;

  return 0;
}

int
js_get_prototype(js_env_t *env, js_value_t *object, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_GetPrototype(env->context, object->value);

  *result = wrapper;

  return 0;
}

int
js_set_prototype(js_env_t *env, js_value_t *object, js_value_t *prototype) {
  if (JS_HasException(env->context)) return js__error(env);

  int success = JS_SetPrototype(env->context, object->value, prototype->value);

  if (success < 0) return js__error(env);

  return 0;
}

static inline int
js__set_integrity_level(js_env_t *env, js_value_t *object, const char *method) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue global = JS_GetGlobalObject(env->context);

  JSValue constructor = JS_GetPropertyStr(env->context, global, "Object");

  JS_FreeValue(env->context, global);

  if (JS_IsException(constructor)) return js__error(env);

  JSValue function = JS_GetPropertyStr(env->context, constructor, method);

  JS_FreeValue(env->context, constructor);

  if (JS_IsException(function)) return js__error(env);

  env->depth++;

  JSValue result = JS_Call(env->context, function, JS_UNDEFINED, 1, &object->value);

  JS_FreeValue(env->context, function);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (JS_IsException(result)) {
    return js__report_exception(env);
  }

  JS_FreeValue(env->context, result);

  return 0;
}

int
js_seal(js_env_t *env, js_value_t *object) {
  return js__set_integrity_level(env, object, "seal");
}

int
js_freeze(js_env_t *env, js_value_t *object) {
  return js__set_integrity_level(env, object, "freeze");
}

int
js_get_property_names(js_env_t *env, js_value_t *object, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  JSPropertyEnum *properties;
  uint32_t len;

  env->depth++;

  err = JS_GetOwnPropertyNames(env->context, &properties, &len, object->value, JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (err < 0) {
    return js__report_exception(env);
  }

  JSValue array = JS_NewArray(env->context);

  for (uint32_t i = 0; i < len; i++) {
    err = JS_SetPropertyUint32(env->context, array, i, JS_AtomToValue(env->context, properties[i].atom));
    if (err < 0) goto err;
  }

  JS_FreePropertyEnum(env->context, properties, len);

  if (result == NULL) JS_FreeValue(env->context, array);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = array;

    *result = wrapper;
  }

  return 0;

err:
  JS_FreePropertyEnum(env->context, properties, len);

  return js__error(env);
}

static int
js__atom_is_array_index(JSContext *context, JSAtom atom, uint32_t *index) {
  JSValue key = JS_AtomToString(context, atom);

  if (JS_IsException(key)) return -1;

  size_t len;
  const char *str = JS_ToCStringLen(context, &len, key);

  if (str == NULL) {
    JS_FreeValue(context, key);
    return -1;
  }

  bool result = len > 0;
  uint64_t value = 0;

  for (size_t i = 0; i < len; i++) {
    char c = str[i];

    if (c < '0' || c > '9') {
      result = false;
      break;
    }

    if (i == 0 && c == '0' && len > 1) {
      result = false;
      break;
    }

    value = value * 10 + (c - '0');

    if (value > UINT32_MAX) {
      result = false;
      break;
    }
  }

  JS_FreeCString(context, str);
  JS_FreeValue(context, key);

  if (!result || value == UINT32_MAX) return false;

  if (index) *index = (uint32_t) value;

  return true;
}

int
js_get_filtered_property_names(js_env_t *env, js_value_t *object, js_key_collection_mode_t mode, js_property_filter_t property_filter, js_index_filter_t index_filter, js_key_conversion_mode_t key_conversion, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (mode != js_key_own_only) {
    err = js_throw_error(env, NULL, "Unsupported operation");
    assert(err == 0);

    return js__error(env);
  }

  int flags = JS_GPN_SET_ENUM;

  if (property_filter & js_property_only_enumerable) flags |= JS_GPN_ENUM_ONLY;
  if (!(property_filter & js_property_skip_strings)) flags |= JS_GPN_STRING_MASK;
  if (!(property_filter & js_property_skip_symbols)) flags |= JS_GPN_SYMBOL_MASK;

  JSPropertyEnum *properties;
  uint32_t len;

  env->depth++;

  err = JS_GetOwnPropertyNames(env->context, &properties, &len, object->value, flags);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (err < 0) {
    return js__report_exception(env);
  }

  JSValue array = JS_NewArray(env->context);
  uint32_t j = 0;

  for (uint32_t i = 0; i < len; i++) {
    JSAtom atom = properties[i].atom;
    uint32_t index_value;
    int index_result = js__atom_is_array_index(env->context, atom, &index_value);

    if (index_result < 0) goto err;

    if (index_filter == js_index_skip_indices && index_result) continue;

    JSPropertyDescriptor descriptor;
    err = JS_GetOwnProperty(env->context, &descriptor, object->value, atom);

    if (err < 0) goto err;
    else if (!err) continue;

    bool include = property_filter == js_property_all_properties;

    if (property_filter & js_property_only_writable) {
      include = include || descriptor.flags & JS_PROP_WRITABLE;
    }
    if (property_filter & js_property_only_enumerable) {
      include = include || descriptor.flags & JS_PROP_ENUMERABLE;
    }
    if (property_filter & js_property_only_configurable) {
      include = include || descriptor.flags & JS_PROP_CONFIGURABLE;
    }

    JS_FreeValue(env->context, descriptor.getter);
    JS_FreeValue(env->context, descriptor.setter);
    JS_FreeValue(env->context, descriptor.value);

    JSValue value = JS_AtomToValue(env->context, atom);

    if (JS_IsException(value)) goto err;

    if (include) {
      if (key_conversion == js_key_keep_numbers && index_result) {
        JS_FreeValue(env->context, value);
        value = JS_NewUint32(env->context, index_value);
      }

      err = JS_SetPropertyUint32(env->context, array, j++, value);

      if (err < 0) {
        JS_FreeValue(env->context, value);
        goto err;
      }
    } else {
      JS_FreeValue(env->context, value);
    }
  }

  JS_FreePropertyEnum(env->context, properties, len);

  if (result == NULL) JS_FreeValue(env->context, array);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = array;

    *result = wrapper;
  }

  return 0;

err:
  JS_FreePropertyEnum(env->context, properties, len);
  JS_FreeValue(env->context, array);

  return js__error(env);
}

int
js_get_property(js_env_t *env, js_value_t *object, js_value_t *key, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSAtom atom = JS_ValueToAtom(env->context, key->value);

  env->depth++;

  JSValue value = JS_GetProperty(env->context, object->value, atom);

  JS_FreeAtom(env->context, atom);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (JS_IsException(value)) {
    return js__report_exception(env);
  }

  if (result == NULL) JS_FreeValue(env->context, value);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    *result = wrapper;
  }

  return 0;
}

int
js_has_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSAtom atom = JS_ValueToAtom(env->context, key->value);

  env->depth++;

  int success = JS_HasProperty(env->context, object->value, atom);

  JS_FreeAtom(env->context, atom);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) {
    return js__report_exception(env);
  }

  if (result) *result = success == 1;

  return 0;
}

int
js_has_own_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSAtom atom = JS_ValueToAtom(env->context, key->value);

  env->depth++;

  int success = JS_GetOwnProperty(env->context, NULL, object->value, atom);

  JS_FreeAtom(env->context, atom);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) {
    return js__report_exception(env);
  }

  if (result) *result = success == 1;

  return 0;
}

int
js_set_property(js_env_t *env, js_value_t *object, js_value_t *key, js_value_t *value) {
  if (JS_HasException(env->context)) return js__error(env);

  JSAtom atom = JS_ValueToAtom(env->context, key->value);

  env->depth++;

  int success = JS_SetProperty(env->context, object->value, atom, JS_DupValue(env->context, value->value));

  JS_FreeAtom(env->context, atom);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) {
    return js__report_exception(env);
  }

  return 0;
}

int
js_delete_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSAtom atom = JS_ValueToAtom(env->context, key->value);

  env->depth++;

  int success = JS_DeleteProperty(env->context, object->value, atom, 0);

  JS_FreeAtom(env->context, atom);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) {
    return js__report_exception(env);
  }

  if (result) *result = success == 1;

  return 0;
}

int
js_get_named_property(js_env_t *env, js_value_t *object, const char *name, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  env->depth++;

  JSValue value = JS_GetPropertyStr(env->context, object->value, name);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (JS_IsException(value)) {
    return js__report_exception(env);
  }

  if (result == NULL) JS_FreeValue(env->context, value);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    *result = wrapper;
  }

  return 0;
}

int
js_has_named_property(js_env_t *env, js_value_t *object, const char *name, bool *result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSAtom atom = JS_NewAtom(env->context, name);

  env->depth++;

  int success = JS_HasProperty(env->context, object->value, atom);

  JS_FreeAtom(env->context, atom);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) {
    return js__report_exception(env);
  }

  if (result) *result = success == 1;

  return 0;
}

int
js_set_named_property(js_env_t *env, js_value_t *object, const char *name, js_value_t *value) {
  if (JS_HasException(env->context)) return js__error(env);

  env->depth++;

  int success = JS_SetPropertyStr(env->context, object->value, name, JS_DupValue(env->context, value->value));

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) {
    return js__report_exception(env);
  }

  return 0;
}

int
js_delete_named_property(js_env_t *env, js_value_t *object, const char *name, bool *result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSAtom atom = JS_NewAtom(env->context, name);

  env->depth++;

  int success = JS_DeleteProperty(env->context, object->value, atom, 0);

  JS_FreeAtom(env->context, atom);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) {
    return js__report_exception(env);
  }

  if (result) *result = success == 1;

  return 0;
}

int
js_get_element(js_env_t *env, js_value_t *object, uint32_t index, js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  env->depth++;

  JSValue value = JS_GetPropertyUint32(env->context, object->value, index);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (JS_IsException(value)) {
    return js__report_exception(env);
  }

  if (result == NULL) JS_FreeValue(env->context, value);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    *result = wrapper;
  }

  return 0;
}

int
js_has_element(js_env_t *env, js_value_t *object, uint32_t index, bool *result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSAtom atom = JS_NewAtomUInt32(env->context, index);

  env->depth++;

  int success = JS_HasProperty(env->context, object->value, atom);

  JS_FreeAtom(env->context, atom);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) {
    return js__report_exception(env);
  }

  if (result) *result = success == 1;

  return 0;
}

int
js_set_element(js_env_t *env, js_value_t *object, uint32_t index, js_value_t *value) {
  if (JS_HasException(env->context)) return js__error(env);

  env->depth++;

  int success = JS_SetPropertyUint32(env->context, object->value, index, JS_DupValue(env->context, value->value));

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) {
    return js__report_exception(env);
  }

  return 0;
}

int
js_delete_element(js_env_t *env, js_value_t *object, uint32_t index, bool *result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSAtom atom = JS_NewAtomUInt32(env->context, index);

  env->depth++;

  int success = JS_DeleteProperty(env->context, object->value, atom, 0);

  JS_FreeAtom(env->context, atom);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (success < 0) {
    return js__report_exception(env);
  }

  if (result) *result = success == 1;

  return 0;
}

int
js_get_string_view(js_env_t *env, js_value_t *string, js_string_encoding_t *encoding, const void **str, size_t *len, js_string_view_t **result) {
  // Allow continuing even with a pending exception

  js_string_view_t *view = malloc(sizeof(js_string_view_t));

  // A string concatenated at runtime may be a rope, which is flattened here so
  // its characters can be borrowed. The view holds on to the flat string for
  // as long as it is open.
  view->string = JS_ToString(env->context, string->value);

  size_t length;
  bool wide;

  const void *data = JS_GetStringBuffer(view->string, &length, &wide);

  if (encoding) *encoding = wide ? js_utf16le : js_latin1;

  if (str) *str = data;

  if (len) *len = length;

  *result = view;

  return 0;
}

int
js_release_string_view(js_env_t *env, js_string_view_t *view) {
  // Allow continuing even with a pending exception

  JS_FreeValue(env->context, view->string);

  free(view);

  return 0;
}

int
js_get_callback_info(js_env_t *env, const js_callback_info_t *info, size_t *argc, js_value_t *argv[], js_value_t **receiver, void **data) {
  // Allow continuing even with a pending exception

  if (argv) {
    size_t i = 0, n = info->argc < *argc ? info->argc : *argc;

    for (; i < n; i++) {
      js_value_t *wrapper = js__create_handle(env, env->scope);

      wrapper->value = JS_DupValue(env->context, info->argv[i]);

      argv[i] = wrapper;
    }

    n = *argc;

    if (i < n) {
      js_value_t *wrapper = js__create_handle(env, env->scope);

      wrapper->value = JS_UNDEFINED;

      for (; i < n; i++) {
        argv[i] = wrapper;
      }
    }
  }

  if (argc) {
    *argc = info->argc;
  }

  if (receiver) {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = JS_DupValue(env->context, info->receiver);

    *receiver = wrapper;
  }

  if (data) {
    *data = info->callback->data;
  }

  return 0;
}

int
js_get_typed_callback_info(const js_typed_callback_info_t *info, js_env_t **env, void **data) {
  // Allow continuing even with a pending exception

  return 0;
}

int
js_get_new_target(js_env_t *env, const js_callback_info_t *info, js_value_t **result) {
  // Allow continuing even with a pending exception

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = JS_DupValue(env->context, info->new_target);

  *result = wrapper;

  return 0;
}

int
js_get_arraybuffer_info(js_env_t *env, js_value_t *arraybuffer, void **pdata, size_t *plen) {
  // Allow continuing even with a pending exception

  size_t len = 0;

  uint8_t *data = NULL;

  if (!JS_IsDetachedArrayBuffer(arraybuffer->value)) {
    data = JS_GetArrayBuffer(env->context, &len, arraybuffer->value);
  }

  if (pdata) {
    *pdata = data;
  }

  if (plen) {
    *plen = len;
  }

  return 0;
}

int
js_get_sharedarraybuffer_info(js_env_t *env, js_value_t *sharedarraybuffer, void **pdata, size_t *plen) {
  return js_get_arraybuffer_info(env, sharedarraybuffer, pdata, plen);
}

int
js_get_typedarray_info(js_env_t *env, js_value_t *typedarray, js_typedarray_type_t *ptype, void **pdata, size_t *plen, js_value_t **parraybuffer, size_t *poffset) {
  // Allow continuing even with a pending exception

  size_t offset, byte_len, bytes_per_element;

  JSValue arraybuffer = JS_GetTypedArrayBuffer(env->context, typedarray->value, &offset, &byte_len, &bytes_per_element);

  if (ptype) {
    switch (JS_GetTypedArrayType(typedarray->value)) {
#define V(enum, type) \
  case enum: \
    *ptype = type; \
    break;

      V(JS_TYPED_ARRAY_INT8, js_int8array);
      V(JS_TYPED_ARRAY_UINT8, js_uint8array);
      V(JS_TYPED_ARRAY_UINT8C, js_uint8clampedarray);
      V(JS_TYPED_ARRAY_INT16, js_int16array);
      V(JS_TYPED_ARRAY_UINT16, js_uint16array);
      V(JS_TYPED_ARRAY_INT32, js_int32array);
      V(JS_TYPED_ARRAY_UINT32, js_uint32array);
      V(JS_TYPED_ARRAY_FLOAT16, js_float16array);
      V(JS_TYPED_ARRAY_FLOAT32, js_float32array);
      V(JS_TYPED_ARRAY_FLOAT64, js_float64array);
      V(JS_TYPED_ARRAY_BIG_INT64, js_bigint64array);
      V(JS_TYPED_ARRAY_BIG_UINT64, js_biguint64array);
#undef V
    }
  }

  if (pdata) {
    size_t size;

    *pdata = JS_GetArrayBuffer(env->context, &size, arraybuffer) + offset;
  }

  if (plen) {
    *plen = byte_len / bytes_per_element;
  }

  if (parraybuffer) {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = JS_DupValue(env->context, arraybuffer);

    *parraybuffer = wrapper;
  }

  if (poffset) {
    *poffset = offset;
  }

  JS_FreeValue(env->context, arraybuffer);

  return 0;
}

int
js_get_dataview_info(js_env_t *env, js_value_t *dataview, void **pdata, size_t *plen, js_value_t **parraybuffer, size_t *poffset) {
  // Allow continuing even with a pending exception

  size_t offset, byte_len;

  JSValue arraybuffer = JS_GetDataViewBuffer(env->context, dataview->value, &offset, &byte_len);

  if (pdata) {
    size_t size;

    *pdata = JS_GetArrayBuffer(env->context, &size, arraybuffer) + offset;
  }

  if (plen) {
    *plen = byte_len;
  }

  if (parraybuffer) {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = JS_DupValue(env->context, arraybuffer);

    *parraybuffer = wrapper;
  }

  if (poffset) {
    *poffset = offset;
  }

  JS_FreeValue(env->context, arraybuffer);

  return 0;
}

int
js_call_function(js_env_t *env, js_value_t *recv, js_value_t *function, size_t argc, js_value_t *const argv[], js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue *args = malloc(argc * sizeof(JSValue));

  for (size_t i = 0; i < argc; i++) {
    args[i] = argv[i]->value;
  }

  env->depth++;

  JSValue value = JS_Call(env->context, function->value, recv->value, argc, args);

  free(args);

  if (env->depth == 1) js__run_microtasks(env);

  env->depth--;

  if (JS_IsException(value)) {
    return js__report_exception(env);
  }

  if (result == NULL) JS_FreeValue(env->context, value);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    *result = wrapper;
  }

  return 0;
}

int
js_call_function_with_checkpoint(js_env_t *env, js_value_t *receiver, js_value_t *function, size_t argc, js_value_t *const argv[], js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue *args = malloc(argc * sizeof(JSValue));

  for (size_t i = 0; i < argc; i++) {
    args[i] = argv[i]->value;
  }

  env->depth++;

  JSValue value = JS_Call(env->context, function->value, receiver->value, argc, args);

  free(args);

  js__run_microtasks(env);

  env->depth--;

  // Within a call into JavaScript, an exception thrown at the checkpoint must
  // reach that call even if the function itself returned.
  if (JS_IsException(value) || (env->depth && JS_HasException(env->context))) {
    JS_FreeValue(env->context, value);

    return js__report_exception(env);
  }

  if (result == NULL) JS_FreeValue(env->context, value);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    *result = wrapper;
  }

  return 0;
}

static JSValue
js__on_microtask(JSContext *context, int argc, JSValueConst *argv) {
  return JS_Call(context, argv[0], JS_UNDEFINED, 0, NULL);
}

int
js_queue_microtask(js_env_t *env, js_value_t *function) {
  if (JS_HasException(env->context)) return js__error(env);

  JS_EnqueueJob(env->context, js__on_microtask, 1, &function->value);

  if (env->depth == 0) js__run_microtasks(env);

  return 0;
}

static void
js__on_microtask_finalize(JSRuntime *runtime, JSValueConst value) {
  free(JS_GetOpaque(value, js__microtask_class_id));
}

static JSValue
js__on_microtask_with_callback(JSContext *context, int argc, JSValueConst *argv) {
  int err;

  js_microtask_t *task = (js_microtask_t *) JS_GetOpaque(argv[0], js__microtask_class_id);

  js_handle_scope_t *scope;
  err = js_open_handle_scope(task->env, &scope);
  assert(err == 0);

  task->cb(task->env, task->data);

  err = js_close_handle_scope(task->env, scope);
  assert(err == 0);

  return JS_HasException(context) ? JS_EXCEPTION : JS_UNDEFINED;
}

int
js_queue_microtask_with_callback(js_env_t *env, js_task_cb cb, void *data) {
  if (JS_HasException(env->context)) return js__error(env);

  js_microtask_t *task = malloc(sizeof(js_microtask_t));

  task->env = env;
  task->cb = cb;
  task->data = data;

  JSValue external = JS_NewObjectClass(env->context, js__microtask_class_id);

  JS_SetOpaque(external, task);

  JS_EnqueueJob(env->context, js__on_microtask_with_callback, 1, &external);

  JS_FreeValue(env->context, external);

  if (env->depth == 0) js__run_microtasks(env);

  return 0;
}

int
js_new_instance(js_env_t *env, js_value_t *constructor, size_t argc, js_value_t *const argv[], js_value_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  JSValue *args = malloc(argc * sizeof(JSValue));

  for (size_t i = 0; i < argc; i++) {
    args[i] = argv[i]->value;
  }

  env->depth++;

  JSValue value = JS_CallConstructor(env->context, constructor->value, argc, args);

  free(args);

  env->depth--;

  if (JS_IsException(value)) {
    return js__report_exception(env);
  }

  if (result == NULL) JS_FreeValue(env->context, value);
  else {
    js_value_t *wrapper = js__create_handle(env, env->scope);

    wrapper->value = value;

    *result = wrapper;
  }

  return 0;
}

static inline bool
js__threadsafe_queue_push(js_threadsafe_queue_t *queue, void *data) {
  uv_mutex_lock(&queue->lock);

  if (queue->closed) {
    uv_mutex_unlock(&queue->lock);

    return false;
  }

  if (queue->len == queue->capacity) {
    size_t capacity = queue->capacity ? queue->capacity << 1u : 16;

    void **resized = realloc(queue->queue, capacity * sizeof(void *));

    if (queue->head > 0) {
      size_t head_len = queue->capacity - queue->head;

      if (head_len > 0) {
        memmove(&resized[capacity - head_len], &resized[queue->head], head_len * sizeof(void *));
      }

      queue->head = capacity - head_len;
    }

    queue->queue = resized;
    queue->capacity = capacity;
  }

  queue->queue[queue->tail] = data;
  queue->tail = (queue->tail + 1) & (queue->capacity - 1);
  queue->len++;

  uv_mutex_unlock(&queue->lock);

  return true;
}

static inline bool
js__threadsafe_queue_pop(js_threadsafe_queue_t *queue, void **result) {
  uv_mutex_lock(&queue->lock);

  if (queue->len == 0) {
    uv_mutex_unlock(&queue->lock);

    return false;
  }

  void *data = queue->queue[queue->head];

  queue->head = (queue->head + 1) & (queue->capacity - 1);
  queue->len--;

  if (queue->capacity > 16 && queue->len <= queue->capacity / 4) {
    size_t capacity = queue->capacity >> 1u;

    void **resized = malloc(capacity * sizeof(void *));

    for (size_t i = 0; i < queue->len; i++) {
      resized[i] = queue->queue[(queue->head + i) & (queue->capacity - 1)];
    }

    free(queue->queue);

    queue->queue = resized;
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = queue->len;
  }

  uv_mutex_unlock(&queue->lock);

  *result = data;

  return true;
}

static inline void
js__threadsafe_queue_close(js_threadsafe_queue_t *queue) {
  uv_mutex_lock(&queue->lock);

  queue->closed = true;

  uv_mutex_unlock(&queue->lock);
}

static inline void
js__threadsafe_function_finalize(js_threadsafe_function_t *function) {
  js_env_t *env = function->env;

  if (function->finalize_cb) {
    function->finalize_cb(env, function->context, function->finalize_hint);
  }

  JS_FreeValue(env->context, function->function);

  free(function);
}

static void
js__on_threadsafe_function_close(uv_handle_t *handle) {
  js_threadsafe_function_t *function = handle->data;

  js__threadsafe_function_finalize(function);
}

static inline void
js__threadsafe_function_close(js_threadsafe_function_t *function) {
  uv_close((uv_handle_t *) &function->async, js__on_threadsafe_function_close);
}

static inline bool
js__threadsafe_function_call(js_threadsafe_function_t *function) {
  int err;

  js_env_t *env = function->env;

  void *data;

  if (js__threadsafe_queue_pop(&function->queue, &data)) {
    js_handle_scope_t *scope;
    err = js_open_handle_scope(env, &scope);
    assert(err == 0);

    if (function->cb) {
      function->cb(env, &(js_value_t){function->function}, function->context, data);
    } else {
      js_value_t *receiver;
      err = js_get_undefined(env, &receiver);
      assert(err == 0);

      err = js_call_function(env, receiver, &(js_value_t){function->function}, 0, NULL, NULL);
      (void) err;
    }

    err = js_close_handle_scope(env, scope);
    assert(err == 0);

    return true;
  }

  if (atomic_load_explicit(&function->thread_count, memory_order_relaxed) == 0) {
    js__threadsafe_function_close(function);
  }

  return false;
}

static inline void
js__threadsafe_function_signal(js_threadsafe_function_t *function) {
  int err;

  int state = atomic_fetch_or_explicit(&function->state, js_threadsafe_function_pending, memory_order_acq_rel);

  if (state & js_threadsafe_function_running) {
    return;
  }

  err = uv_async_send(&function->async);
  assert(err == 0);
}

static inline void
js__threadsafe_function_dispatch(js_threadsafe_function_t *function) {
  bool done = false;

  int iterations = 1024;

  while (!done && --iterations >= 0) {
    atomic_store_explicit(&function->state, js_threadsafe_function_running, memory_order_release);

    done = js__threadsafe_function_call(function) == false;

    if (atomic_exchange_explicit(&function->state, js_threadsafe_function_idle, memory_order_acq_rel) != js_threadsafe_function_running) {
      done = false;
    }
  }

  if (!done) js__threadsafe_function_signal(function);
}

static void
js__on_threadsafe_function_async(uv_async_t *handle) {
  js_threadsafe_function_t *function = handle->data;

  js__threadsafe_function_dispatch(function);
}

int
js_create_threadsafe_function(js_env_t *env, js_value_t *function, size_t queue_limit, size_t initial_thread_count, js_finalize_cb finalize_cb, void *finalize_hint, void *context, js_threadsafe_function_cb cb, js_threadsafe_function_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (function == NULL && cb == NULL) {
    err = js_throw_error(env, NULL, "Either a function or a callback must be provided");
    assert(err == 0);

    return js__error(env);
  };

  if (initial_thread_count == 0) {
    err = js_throw_error(env, NULL, "Initial thread count must be greater than 0");
    assert(err == 0);

    return js__error(env);
  }

  js_threadsafe_function_t *threadsafe_function = malloc(sizeof(js_threadsafe_function_t));

  threadsafe_function->env = env;
  threadsafe_function->context = context;
  threadsafe_function->finalize_cb = finalize_cb;
  threadsafe_function->finalize_hint = finalize_hint;
  threadsafe_function->cb = cb;

  atomic_init(&threadsafe_function->state, js_threadsafe_function_idle);
  atomic_init(&threadsafe_function->thread_count, initial_thread_count);

  if (function) {
    threadsafe_function->function = JS_DupValue(env->context, function->value);
  } else {
    threadsafe_function->function = JS_NULL;
  }

  err = uv_async_init(env->loop, &threadsafe_function->async, js__on_threadsafe_function_async);
  assert(err == 0);

  threadsafe_function->async.data = threadsafe_function;

  js_threadsafe_queue_t *queue = &threadsafe_function->queue;

  queue->queue = NULL;
  queue->len = 0;
  queue->capacity = 0;
  queue->head = 0;
  queue->tail = 0;
  queue->closed = false;

  err = uv_mutex_init(&queue->lock);
  assert(err == 0);

  *result = threadsafe_function;

  return 0;
}

int
js_get_threadsafe_function_context(js_threadsafe_function_t *function, void **result) {
  // Allow continuing even with a pending exception

  *result = function->context;

  return 0;
}

int
js_call_threadsafe_function(js_threadsafe_function_t *function, void *data, js_threadsafe_function_call_mode_t mode) {
  if (atomic_load_explicit(&function->thread_count, memory_order_relaxed) == 0) return -1;

  if (js__threadsafe_queue_push(&function->queue, data)) {
    js__threadsafe_function_signal(function);

    return 0;
  }

  return -1;
}

int
js_acquire_threadsafe_function(js_threadsafe_function_t *function) {
  int thread_count = atomic_load_explicit(&function->thread_count, memory_order_relaxed);

  while (thread_count != 0) {
    if (
      atomic_compare_exchange_weak_explicit(
        &function->thread_count,
        &thread_count,
        thread_count + 1,
        memory_order_acquire,
        memory_order_relaxed
      )
    ) {
      return 0;
    }
  }

  return -1;
}

int
js_release_threadsafe_function(js_threadsafe_function_t *function, js_threadsafe_function_release_mode_t mode) {
  int thread_count = atomic_load_explicit(&function->thread_count, memory_order_relaxed);

  bool abort = mode == js_threadsafe_function_abort;

  while (thread_count != 0) {
    if (
      atomic_compare_exchange_weak_explicit(
        &function->thread_count,
        &thread_count,
        abort ? 0 : thread_count - 1,
        memory_order_acquire,
        memory_order_relaxed
      )
    ) {
      if (abort || thread_count == 1) {
        js__threadsafe_queue_close(&function->queue);

        js__threadsafe_function_signal(function);
      }

      return 0;
    }
  }

  return -1;
}

int
js_ref_threadsafe_function(js_env_t *env, js_threadsafe_function_t *function) {
  // Allow continuing even with a pending exception

  uv_ref((uv_handle_t *) &function->async);

  return 0;
}

int
js_unref_threadsafe_function(js_env_t *env, js_threadsafe_function_t *function) {
  // Allow continuing even with a pending exception

  uv_unref((uv_handle_t *) &function->async);

  return 0;
}

int
js_add_teardown_callback(js_env_t *env, js_teardown_cb callback, void *data) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (env->destroying) {
    err = js_throw_error(env, NULL, "Teardown queue has already drained");
    assert(err == 0);

    return js__error(env);
  }

  intrusive_list_for_each(next, &env->teardown_queue.tasks) {
    js_teardown_task_t *task = intrusive_entry(next, js_teardown_task_t, list);

    if (task->type == js_immediate_teardown && task->immediate.cb == callback && task->data == data) {
      err = js_throw_error(env, NULL, "Teardown callback has already been registered");
      assert(err == 0);

      return js__error(env);
    }
  }

  js_teardown_task_t *task = malloc(sizeof(js_teardown_task_t));

  task->type = js_immediate_teardown;
  task->immediate.cb = callback;
  task->data = data;

  intrusive_list_prepend(&env->teardown_queue.tasks, &task->list);

  return 0;
}

int
js_remove_teardown_callback(js_env_t *env, js_teardown_cb callback, void *data) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (env->destroying) return 0;

  intrusive_list_for_each(next, &env->teardown_queue.tasks) {
    js_teardown_task_t *task = intrusive_entry(next, js_teardown_task_t, list);

    if (task->type == js_immediate_teardown && task->immediate.cb == callback && task->data == data) {
      intrusive_list_remove(&env->teardown_queue.tasks, &task->list);

      free(task);

      return 0;
    }
  }

  err = js_throw_error(env, NULL, "Teardown callback has not been registered");
  assert(err == 0);

  return js__error(env);
}

int
js_add_deferred_teardown_callback(js_env_t *env, js_deferred_teardown_cb callback, void *data, js_deferred_teardown_t **result) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  if (env->destroying) {
    err = js_throw_error(env, NULL, "Teardown queue has already drained");
    assert(err == 0);

    return js__error(env);
  }

  intrusive_list_for_each(next, &env->teardown_queue.tasks) {
    js_teardown_task_t *task = intrusive_entry(next, js_teardown_task_t, list);

    if (task->type == js_deferred_teardown && task->deferred.cb == callback && task->data == data) {
      err = js_throw_error(env, NULL, "Teardown callback has already been registered");
      assert(err == 0);

      return js__error(env);
    }
  }

  js_teardown_task_t *task = malloc(sizeof(js_teardown_task_t));

  task->type = js_deferred_teardown;
  task->deferred.cb = callback;
  task->deferred.handle.env = env;
  task->data = data;

  intrusive_list_prepend(&env->teardown_queue.tasks, &task->list);

  env->refs++;

  if (result) *result = &task->deferred.handle;

  return 0;
}

int
js_finish_deferred_teardown_callback(js_deferred_teardown_t *handle) {
  // Allow continuing even with a pending exception

  int err;

  js_env_t *env = handle->env;

  intrusive_list_for_each(next, &env->teardown_queue.tasks) {
    js_teardown_task_t *task = intrusive_entry(next, js_teardown_task_t, list);

    if (task->type == js_deferred_teardown && &task->deferred.handle == handle) {
      intrusive_list_remove(&env->teardown_queue.tasks, &task->list);

      if (--env->refs == 0 && env->destroying) {
        err = uv_async_send(&env->teardown);
        assert(err == 0);
      }

      free(task);

      return 0;
    }
  }

  return -1;
}

int
js_throw(js_env_t *env, js_value_t *error) {
  if (JS_HasException(env->context)) return js__error(env);

  JS_Throw(env->context, JS_DupValue(env->context, error->value));

  return 0;
}

static inline int
js__vformat(char **result, size_t *size, const char *message, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);

  int res = vsnprintf(NULL, 0, message, args_copy);

  va_end(args_copy);

  if (res < 0) return res;

  *size = res + 1 /* NULL */;
  *result = malloc(*size);

  va_copy(args_copy, args);

  vsnprintf(*result, *size, message, args_copy);

  va_end(args_copy);

  return 0;
}

int
js_throw_error(js_env_t *env, const char *code, const char *message) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__throw_error(env, JS_NewPlainError, code, message);
}

int
js_throw_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  size_t len;
  char *formatted;
  err = js__vformat(&formatted, &len, message, args);
  assert(err == 0);

  err = js_throw_error(env, code, formatted);

  free(formatted);

  return err;
}

int
js_throw_errorf(js_env_t *env, const char *code, const char *message, ...) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  va_list args;
  va_start(args, message);

  err = js_throw_verrorf(env, code, message, args);

  va_end(args);

  return err;
}

int
js_throw_type_error(js_env_t *env, const char *code, const char *message) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__throw_error(env, JS_NewTypeError, code, message);
}

int
js_throw_type_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  size_t len;
  char *formatted;
  err = js__vformat(&formatted, &len, message, args);
  assert(err == 0);

  err = js_throw_type_error(env, code, formatted);

  free(formatted);

  return err;
}

int
js_throw_type_errorf(js_env_t *env, const char *code, const char *message, ...) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  va_list args;
  va_start(args, message);

  err = js_throw_type_verrorf(env, code, message, args);

  va_end(args);

  return err;
}

int
js_throw_range_error(js_env_t *env, const char *code, const char *message) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__throw_error(env, JS_NewRangeError, code, message);
}

int
js_throw_range_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  size_t len;
  char *formatted;
  err = js__vformat(&formatted, &len, message, args);
  assert(err == 0);

  err = js_throw_range_error(env, code, formatted);

  free(formatted);

  return err;
}

int
js_throw_range_errorf(js_env_t *env, const char *code, const char *message, ...) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  va_list args;
  va_start(args, message);

  err = js_throw_range_verrorf(env, code, message, args);

  va_end(args);

  return err;
}

int
js_throw_syntax_error(js_env_t *env, const char *code, const char *message) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__throw_error(env, JS_NewSyntaxError, code, message);
}

int
js_throw_syntax_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  size_t len;
  char *formatted;
  err = js__vformat(&formatted, &len, message, args);
  assert(err == 0);

  err = js_throw_syntax_error(env, code, formatted);

  free(formatted);

  return err;
}

int
js_throw_syntax_errorf(js_env_t *env, const char *code, const char *message, ...) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  va_list args;
  va_start(args, message);

  err = js_throw_syntax_verrorf(env, code, message, args);

  va_end(args);

  return err;
}

int
js_throw_reference_error(js_env_t *env, const char *code, const char *message) {
  if (JS_HasException(env->context)) return js__error(env);

  return js__throw_error(env, JS_NewReferenceError, code, message);
}

int
js_throw_reference_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  size_t len;
  char *formatted;
  err = js__vformat(&formatted, &len, message, args);
  assert(err == 0);

  err = js_throw_reference_error(env, code, formatted);

  free(formatted);

  return err;
}

int
js_throw_reference_errorf(js_env_t *env, const char *code, const char *message, ...) {
  if (JS_HasException(env->context)) return js__error(env);

  int err;

  va_list args;
  va_start(args, message);

  err = js_throw_reference_verrorf(env, code, message, args);

  va_end(args);

  return err;
}

int
js_is_exception_pending(js_env_t *env, bool *result) {
  // Allow continuing even with a pending exception

  *result = JS_HasException(env->context);

  return 0;
}

int
js_get_and_clear_last_exception(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  JSValue error = JS_GetException(env->context);

  if (JS_IsUninitialized(error)) return js_get_undefined(env, result);

  js_value_t *wrapper = js__create_handle(env, env->scope);

  wrapper->value = error;

  *result = wrapper;

  return 0;
}

int
js_fatal_exception(js_env_t *env, js_value_t *error) {
  // Allow continuing even with a pending exception

  if (env->callbacks.uncaught_exception == NULL) return 0;

  js__uncaught_exception(env, JS_DupValue(env->context, error->value));

  return 0;
}

int
js_terminate_execution(js_env_t *env) {
  // Allow continuing even with a pending exception

  JS_ThrowInternalError(env->context, "terminated");

  JSValue error = JS_GetException(env->context);

  JS_SetUncatchableError(env->context, error);

  JS_Throw(env->context, error);

  return 0;
}

int
js_adjust_external_memory(js_env_t *env, int64_t change_in_bytes, int64_t *result) {
  // Allow continuing even with a pending exception

  env->external_memory += change_in_bytes;

  if (result) *result = env->external_memory;

  return 0;
}

int
js_request_garbage_collection(js_env_t *env) {
  // Allow continuing even with a pending exception

  if (env->platform->options.expose_garbage_collection) {
    JS_RunGC(env->runtime);

    js__run_pending_finalizers(env);
  }

  return 0;
}

static void
js__on_garbage_collection(JSRuntime *runtime, JSGCHookType type, void *opaque) {
  js_garbage_collection_tracking_t *tracking = (js_garbage_collection_tracking_t *) opaque;

  js_garbage_collection_cb cb = type == JS_GC_HOOK_BEFORE ? tracking->start : tracking->end;

  if (cb) cb(js_garbage_collection_type_mark_compact, tracking->data);
}

int
js_enable_garbage_collection_tracking(js_env_t *env, const js_garbage_collection_tracking_options_t *options, void *data, js_garbage_collection_tracking_t **result) {
  // Allow continuing even with a pending exception

  js_garbage_collection_tracking_t *tracking = malloc(sizeof(js_garbage_collection_tracking_t));

  tracking->start = options->start;
  tracking->end = options->end;
  tracking->data = data;

  JS_SetGCHook(env->runtime, js__on_garbage_collection, tracking);

  *result = tracking;

  return 0;
}

int
js_disable_garbage_collection_tracking(js_env_t *env, js_garbage_collection_tracking_t *tracking) {
  // Allow continuing even with a pending exception

  JS_SetGCHook(env->runtime, NULL, NULL);

  free(tracking);

  return 0;
}

int
js_get_heap_statistics(js_env_t *env, js_heap_statistics_t *result) {
  // Allow continuing even with a pending exception

  JSMemoryUsage usage;

  JS_ComputeMemoryUsage(env->runtime, &usage);

  // The used size is an estimate made by the engine, which the allocated size
  // measured by the allocator need not cover.
  result->total_heap_size = (size_t) (usage.malloc_size > usage.memory_used_size ? usage.malloc_size : usage.memory_used_size);
  result->used_heap_size = (size_t) usage.memory_used_size;

  if (result->version >= 1) {
    result->external_memory = (size_t) env->external_memory;
  }

  return 0;
}

int
js_get_heap_space_statistics(js_env_t *env, js_heap_space_statistics_t statistics[], size_t len, size_t offset, size_t *result) {
  if (result) *result = 0;

  return 0;
}

int
js_create_inspector(js_env_t *env, js_inspector_t **result) {
  int err;

  err = js_throw_error(env, NULL, "Unsupported operation");
  assert(err == 0);

  return js__error(env);
}

int
js_destroy_inspector(js_env_t *env, js_inspector_t *inspector) {
  int err;

  err = js_throw_error(env, NULL, "Unsupported operation");
  assert(err == 0);

  return js__error(env);
}

int
js_on_inspector_response(js_env_t *env, js_inspector_t *inspector, js_inspector_message_cb cb, void *data) {
  return 0;
}

int
js_on_inspector_paused(js_env_t *env, js_inspector_t *inspector, js_inspector_paused_cb cb, void *data) {
  return 0;
}

int
js_connect_inspector(js_env_t *env, js_inspector_t *inspector) {
  int err;

  err = js_throw_error(env, NULL, "Unsupported operation");
  assert(err == 0);

  return js__error(env);
}

int
js_send_inspector_request(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len) {
  int err;

  err = js_throw_error(env, NULL, "Unsupported operation");
  assert(err == 0);

  return js__error(env);
}

int
js_attach_context_to_inspector(js_env_t *env, js_inspector_t *inspector, js_context_t *context, const char *name, size_t len) {
  int err;

  err = js_throw_error(env, NULL, "Unsupported operation");
  assert(err == 0);

  return js__error(env);
}

int
js_detach_context_from_inspector(js_env_t *env, js_inspector_t *inspector, js_context_t *context) {
  int err;

  err = js_throw_error(env, NULL, "Unsupported operation");
  assert(err == 0);

  return js__error(env);
}
