/* A means of providing more script-like means of sending arguments to a function.

These macros are intended as internal, so this file is not #included from apop.h,
which is what the user is expected to use. See the documentation if you're interested
in using this mechanism in out-of-Apophenia work.

Copyright (c) 2009 Ben Klemens. Licensed under the modified GNU GPL v2; see COPYING and COPYING2.  */

#define apop_varad_head(type, name) type variadic_##name(variadic_type_##name varad_in)

#define apop_varad_declare(type, name, ...) \
    typedef struct {                        \
                __VA_ARGS__ ;               \
            } variadic_type_##name;         \
    apop_varad_head(type, name);

#define apop_varad_var(name, value) name = varad_in.name ? varad_in.name : (value);
#define apop_varad_link(name,...) variadic_##name((variadic_type_##name) {__VA_ARGS__})
