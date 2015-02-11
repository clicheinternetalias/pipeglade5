
#ifndef _TOA_ENTRY_H_
#define _TOA_ENTRY_H_ 1

#include <gtk/gtk.h>
#include "gundo.h"

extern GUndoList * toa_entry_buffer_make_undoable(GtkEntryBuffer * buffer);
extern GUndoList * toa_entry_make_undoable(GtkEntry * entry);

extern GUndoList * toa_entry_buffer_get_undo_list(GtkEntryBuffer * buffer);
extern gboolean    toa_entry_buffer_undo(GtkEntryBuffer * buffer);
extern gboolean    toa_entry_buffer_redo(GtkEntryBuffer * buffer);
extern GUndoList * toa_entry_get_undo_list(GtkEntry * entry);
extern gboolean    toa_entry_undo(GtkEntry * entry);
extern gboolean    toa_entry_redo(GtkEntry * entry);

#endif /* _TOA_ENTRY_H_ */
