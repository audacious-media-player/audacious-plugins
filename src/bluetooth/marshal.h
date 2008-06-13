
#ifndef __marshal_MARSHAL_H__
#define __marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:STRING,UINT,INT (marshal.list:1) */
extern void marshal_VOID__STRING_UINT_INT (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);

/* VOID:STRING,STRING (marshal.list:2) */
extern void marshal_VOID__STRING_STRING (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

G_END_DECLS

#endif /* __marshal_MARSHAL_H__ */

