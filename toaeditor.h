
#ifndef _TOA_EDITOR_H_
#define _TOA_EDITOR_H_ 1

#include <gtk/gtk.h>
#include "gundo.h"

extern GtkWidget * toa_editor_new(GtkTextBuffer ** buf);

extern GUndoList * toa_text_buffer_make_undoable(GtkTextBuffer * buffer);
extern GUndoList * toa_text_view_make_undoable(GtkTextView * view);

extern GUndoList * toa_text_buffer_get_undo_list(GtkTextBuffer * buffer);
extern gboolean    toa_text_buffer_undo(GtkTextBuffer * buffer);
extern gboolean    toa_text_buffer_redo(GtkTextBuffer * buffer);
extern GUndoList * toa_text_view_get_undo_list(GtkTextView * view);
extern gboolean    toa_text_view_undo(GtkTextView * view);
extern gboolean    toa_text_view_redo(GtkTextView * view);

#endif /* _TOA_EDITOR_H_ */
