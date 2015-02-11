/* toaeditor.c
 * Extensions to GtkTextView and GtkTextBuffer to implement Undo/Redo.
 * Why isn't this a built-in part of these widgets?
 *
 * Keys:
 *   Ctrl-Z -> Undo
 *   Shift-Ctrl-Z -> Redo
 *   Ctrl-Y -> Redo
 *
 * Popup menu items:
 *   Undo
 *   Redo
 *   Separator
 */
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "toaeditor.h"

#define DATA_GET(o,name)          g_object_get_data(G_OBJECT(o), name)
#define DATA_SET(o,name,val)      g_object_set_data(G_OBJECT(o), name, val)
#define DATA_GET_UINT(o,name)     GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(o), name))
#define DATA_SET_UINT(o,name,val) g_object_set_data(G_OBJECT(o), name, GUINT_TO_POINTER(val))

/* GTK+ uses both bytes and utf8-chars.
 * This avoids paired calls to strlen and g_utf8_strlen.
 */
static gint
toa_strlen(const gchar * str, gint * len)
{
  gint n = 0;
  const gchar * p = str;
  while (*str) { str = g_utf8_next_char(str); ++n; }
  if (len) *len = (gint)(str - p);
  return n;
}

/* ********************************************************************** */
/* ********************************************************************** */

/* GUndoList Info and Callbacks
 * Performs the actual undo/redo actions.
 *
 * ACTION_SELECT tracks the group's selection bounds so that the text
 * can be selected when it is re-inserted. The selection has to be
 * performed at the start of a delete sequence (delete-undo runs backwards)
 * and at the end of an insert sequence (insert-redo runs forwards).
 */

typedef struct _ToaUndoInfo {
  GtkTextBuffer * buffer; /* the buffer this action belongs to */
  gint            start;  /* offset into buffer in utf8 chars (not bytes) */
  gint            end;    /* offset into buffer in utf8 chars (not bytes) */
  gchar *         text;   /* text inserted/deleted (our malloc) */
  gint            len;    /* length of text in bytes (not utf8 chars) */
  gint            clen;   /* length of text in utf8 chars (not bytes) */
} ToaUndoInfo;

static ToaUndoInfo *
info_new(GtkTextBuffer * buffer)
{
  ToaUndoInfo * info = g_new(ToaUndoInfo, 1);
  info->buffer = buffer;
  info->start  = -1;
  info->end    = -1;
  info->text   = NULL;
  info->len    = -1;
  info->clen   = -1;
  return info;
}

static void
ulist_free_cb(gpointer data)
{
  ToaUndoInfo * info = (ToaUndoInfo *)data;
  if (info->text) g_free(info->text);
  g_free(data);
}

static void
ulist_insert_cb(gpointer data)
{
  ToaUndoInfo * info = (ToaUndoInfo *)data;
  GtkTextIter start, end;
  gtk_text_buffer_get_iter_at_offset(info->buffer, &start, info->start);
  gtk_text_buffer_insert(info->buffer, &start, info->text, info->len); /* invalidates iters */
  gtk_text_buffer_get_iter_at_offset(info->buffer, &end, info->end);
  gtk_text_buffer_place_cursor(info->buffer, &end);
}

static void
ulist_delete_cb(gpointer data)
{
  ToaUndoInfo * info = (ToaUndoInfo *)data;
  GtkTextIter start, end;
  gtk_text_buffer_get_iter_at_offset(info->buffer, &start, info->start);
  gtk_text_buffer_get_iter_at_offset(info->buffer, &end, info->end);
  gtk_text_buffer_delete(info->buffer, &start, &end); /* invalidates iters */
  gtk_text_buffer_get_iter_at_offset(info->buffer, &start, info->start);
  gtk_text_buffer_place_cursor(info->buffer, &start);
}

static void
ulist_select_cb(gpointer data)
{
  ToaUndoInfo * info = (ToaUndoInfo *)data;
  GtkTextIter start, end;
  gtk_text_buffer_get_iter_at_offset(info->buffer, &end, info->end);
  gtk_text_buffer_get_iter_at_offset(info->buffer, &start, info->start);
  gtk_text_buffer_select_range(info->buffer, &end, &start); /* ins,bounds */
}

#define ACTION_INSERT &action_insert
static GUndoActionType action_insert = {
  ulist_delete_cb, ulist_insert_cb, ulist_free_cb
};
#define ACTION_DELETE &action_delete
static GUndoActionType action_delete = {
  ulist_insert_cb, ulist_delete_cb, ulist_free_cb
};
#define ACTION_SELECT &action_select
static GUndoActionType action_select = {
  ulist_select_cb, ulist_select_cb, ulist_free_cb
};

/* ********************************************************************** */
/* ********************************************************************** */

/* Action Sequences
 * Characters are inputted / backspaced / delete-keyed one at a time.
 * Selection-deletion and clipboard-pasting may also break the text
 * into sections.
 *
 * Probably not a bug: Delete/Backspace can be mixed in a single sequence,
 * provided that there is no insertion-point movement.
 */
typedef enum _SeqType {
  SEQ_NONE = 0,
  SEQ_INSERT,
  SEQ_DELETE
} SeqType;

static void
add_selection_info(GtkTextBuffer * buffer, gboolean nullify)
{
  GUndoList * ulist = toa_text_buffer_get_undo_list(buffer);
  ToaUndoInfo * sel = DATA_GET(buffer, "undo-select-info");
  g_undo_list_add_action(ulist, ACTION_SELECT, sel);
  if (nullify) DATA_SET(buffer, "undo-select-info", NULL);
}

static void
buffer_seq_set(GtkTextBuffer * buffer, SeqType extend)
{
  GUndoList * ulist = toa_text_buffer_get_undo_list(buffer);
  SeqType seq = DATA_GET_UINT(buffer, "undo-seq-type");
  if (seq != extend) {
    if (seq != SEQ_NONE) {
      if (seq == SEQ_INSERT) add_selection_info(buffer, TRUE);
      g_undo_list_end_group(ulist);
    }
    seq = extend;
    if (seq != SEQ_NONE) {
      ToaUndoInfo * info = info_new(buffer);
      g_undo_list_begin_group(ulist);
      DATA_SET(buffer, "undo-select-info", info);
      if (seq == SEQ_DELETE) add_selection_info(buffer, FALSE);
    }
  }
  DATA_SET_UINT(buffer, "undo-seq-type", seq);
}

/* ********************************************************************** */
/* ********************************************************************** */

/* Buffer event callbacks.
 * Record user actions.
 */

static void
add_undo_action(GtkTextBuffer * buffer, GUndoActionType * act,
                GtkTextIter * start, GtkTextIter * end,
                gchar * text, gint textlen)
{
  GUndoList * ulist = toa_text_buffer_get_undo_list(buffer);
  ToaUndoInfo * sel = DATA_GET(buffer, "undo-select-info");
  ToaUndoInfo * info = info_new(buffer);
  if (text) { /* ins */
    info->text  = g_strndup(text, (gsize)textlen);
    info->len   = 0;
    info->clen  = toa_strlen(info->text, &(info->len));
    info->start = gtk_text_iter_get_offset(start);
    info->end   = info->start + info->clen;
  } else { /* del */
    info->text  = gtk_text_buffer_get_text(buffer, start, end, FALSE);
    info->len   = 0;
    info->clen  = toa_strlen(info->text, &(info->len));
    info->start = gtk_text_iter_get_offset(start);
    info->end   = gtk_text_iter_get_offset(end);
  }
  g_undo_list_add_action(ulist, act, info);

  /* update group selection info */
  if (sel->start < 0 || sel->end < 0) {
    sel->start = info->start;
    sel->end = info->end;
  } else {
    /* backspace goes backwards, everything else goes forwards */
    if (sel->start > info->start) sel->start = info->start;
    else                          sel->end += info->clen;
  }
}

static void
buffer_insert_cb(GtkWidget * widget, GtkTextIter * start,
                 gchar * txt, gint txtlen, gpointer data)
{
  GtkTextBuffer * buffer = GTK_TEXT_BUFFER(widget);
  data = data;
  if (DATA_GET(buffer, "undo-user-action")) {
    buffer_seq_set(buffer, SEQ_INSERT);
    add_undo_action(buffer, ACTION_INSERT, start, NULL, txt, txtlen);
  }
}

static void
buffer_delete_cb(GtkWidget * widget, GtkTextIter * start,
                 GtkTextIter * end, gpointer data)
{
  GtkTextBuffer * buffer = GTK_TEXT_BUFFER(widget);
  data = data;
  if (DATA_GET(buffer, "undo-user-action")) {
    buffer_seq_set(buffer, SEQ_DELETE);
    add_undo_action(buffer, ACTION_DELETE, start, end, NULL, 0);
  }
}

static void
buffer_user_action_cb(GtkWidget * widget, gpointer data)
{
  GtkTextBuffer * buffer = GTK_TEXT_BUFFER(widget);
  DATA_SET(buffer, "undo-user-action", data);
}

/* End an action sequence if the user performs a non-sequential action. */
extern void
buffer_mark_set_cb(GtkTextBuffer * buffer, GtkTextIter * iter,
                   GtkTextMark * mark, gpointer data)
{
  GtkTextMark * insert = gtk_text_buffer_get_insert(buffer);
  data = data;
  iter = iter;
  if (mark == insert) buffer_seq_set(buffer, SEQ_NONE);
}

/* GC */
static void
buffer_destroy_cb(gpointer data, GObject * where_the_object_was)
{
  GUndoList * ulist = (GUndoList *)data;
  g_object_unref(G_OBJECT(ulist));
  where_the_object_was = where_the_object_was;
}

GUndoList *
toa_text_buffer_make_undoable(GtkTextBuffer * buffer)
{
  GUndoList * ulist = toa_text_buffer_get_undo_list(buffer);
  if (!ulist) {
    ulist = g_undo_list_new();
    DATA_SET(buffer, "undo-list", ulist);
    g_object_weak_ref(G_OBJECT(buffer), buffer_destroy_cb, ulist);
    g_signal_connect(buffer, "mark-set", G_CALLBACK(buffer_mark_set_cb), NULL);
    g_signal_connect(buffer, "insert-text", G_CALLBACK(buffer_insert_cb), NULL);
    g_signal_connect(buffer, "delete-range", G_CALLBACK(buffer_delete_cb), NULL);
    g_signal_connect(buffer, "begin-user-action", G_CALLBACK(buffer_user_action_cb), GINT_TO_POINTER(TRUE));
    g_signal_connect(buffer, "end-user-action", G_CALLBACK(buffer_user_action_cb), GINT_TO_POINTER(FALSE));
  }
  return ulist;
}

/* ********************************************************************** */
/* ********************************************************************** */

/* TextView bindings
 * Adds keyboard shortcuts and popup menu entries.
 */

/* First, the keyboard
 * Is there a better way? I can't figure out all that Accel crap.
 */
typedef enum _ToaKey {
  KEY_UNDO,
  KEY_REDO,
  KEY_MAX
} ToaKey;

typedef struct _ToaKeyBinding {
  ToaKey value;
  guint keyval;
  guint mods;
} ToaKeyBinding;

/* Duplicate key letter cases because CAPS LOCK inverts them.
 * We don't have to worry about the Caps key, only the Shift key.
 */
static ToaKeyBinding keys[] = {
  { KEY_UNDO, GDK_KEY_z, GDK_CONTROL_MASK },
  { KEY_UNDO, GDK_KEY_Z, GDK_CONTROL_MASK },
  { KEY_REDO, GDK_KEY_z, GDK_CONTROL_MASK | GDK_SHIFT_MASK },
  { KEY_REDO, GDK_KEY_Z, GDK_CONTROL_MASK | GDK_SHIFT_MASK },
  { KEY_REDO, GDK_KEY_y, GDK_CONTROL_MASK },
  { KEY_REDO, GDK_KEY_Y, GDK_CONTROL_MASK },
  { KEY_MAX, 0, 0 }
};

/* The only mods we care about: Control, Shift, Alt. */
/* FIXME: Is it right to ignore other mods? */
#define MOD_MASK (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)

static ToaKey
get_key(GdkEventKey * event)
{
  ToaKeyBinding * k = keys;
  while (k->value < KEY_MAX) {
    if (event->keyval == k->keyval && (event->state & MOD_MASK) == k->mods)
      return k->value;
    ++k;
  }
  return KEY_MAX;
}

static gboolean
view_key_press_cb(GtkWidget * widget, GdkEventKey * event, gpointer data)
{
  GtkTextView * view = GTK_TEXT_VIEW(widget);
  GtkTextBuffer * buffer = gtk_text_view_get_buffer(view);
  gint key = get_key(event);
  gboolean doScroll = FALSE;
  data = data;

  if (key < KEY_MAX) {
    switch (key) {
      case KEY_UNDO: doScroll = toa_text_buffer_undo(buffer); break;
      case KEY_REDO: doScroll = toa_text_buffer_redo(buffer); break;
    }
    if (doScroll)
      gtk_text_view_scroll_to_mark(view, gtk_text_buffer_get_insert(buffer),
                                   0.1, FALSE, 0.0, 0.0);
  }
  return (key < KEY_MAX);
}

/* And now, the context menu.
 */
static void
menu_undo_cb(GtkMenuItem * item, gpointer data)
{
  item = item;
  toa_text_view_undo(GTK_TEXT_VIEW(data));
}

static void
menu_redo_cb(GtkMenuItem * item, gpointer data)
{
  item = item;
  toa_text_view_redo(GTK_TEXT_VIEW(data));
}

static void
prepend_menuitem(GtkTextView * view, GtkMenu * menu, const gchar * id,
                 GCallback cb, gboolean sensitive)
{
  GtkWidget * item;
  if (!id) {
    item = gtk_separator_menu_item_new();
  } else {
    item = gtk_image_menu_item_new_from_stock(id, NULL);
    gtk_widget_set_sensitive(item, sensitive);
    g_signal_connect(item, "activate", cb, view);
  }
  gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);
  gtk_widget_show(item);
}

static void
view_populate_cb(GtkTextView * view, GtkMenu * menu, gpointer data)
{
  GtkTextBuffer * buffer = gtk_text_view_get_buffer(view);
  GUndoList * ulist = toa_text_buffer_get_undo_list(buffer);
  gboolean can_undo, can_redo;
  data = data;
  buffer_seq_set(buffer, SEQ_NONE);
  can_redo = g_undo_list_can_redo(ulist);
  can_undo = g_undo_list_can_undo(ulist);
  prepend_menuitem(view, menu, NULL, NULL, FALSE);
  prepend_menuitem(view, menu, GTK_STOCK_REDO, (GCallback)menu_redo_cb, can_redo);
  prepend_menuitem(view, menu, GTK_STOCK_UNDO, (GCallback)menu_undo_cb, can_undo);
}

GUndoList *
toa_text_view_make_undoable(GtkTextView * view)
{
  GtkTextBuffer * buffer = gtk_text_view_get_buffer(view);
  GUndoList * ulist = toa_text_buffer_make_undoable(buffer);
  g_signal_connect(view, "key-press-event", G_CALLBACK(view_key_press_cb), NULL);
  g_signal_connect(view, "populate-popup", G_CALLBACK(view_populate_cb), NULL);
  return ulist;
}

/* ********************************************************************** */
/* ********************************************************************** */

GUndoList *
toa_text_buffer_get_undo_list(GtkTextBuffer * buffer)
{
  return DATA_GET(buffer, "undo-list");
}

gboolean
toa_text_buffer_undo(GtkTextBuffer * buffer)
{
  GUndoList * ulist = toa_text_buffer_get_undo_list(buffer);
  buffer_seq_set(buffer, SEQ_NONE);
  return g_undo_list_undo(ulist);
}

gboolean
toa_text_buffer_redo(GtkTextBuffer * buffer)
{
  GUndoList * ulist = toa_text_buffer_get_undo_list(buffer);
  buffer_seq_set(buffer, SEQ_NONE);
  return g_undo_list_redo(ulist);
}

GUndoList *
toa_text_view_get_undo_list(GtkTextView * view)
{
  GtkTextBuffer * buffer = gtk_text_view_get_buffer(view);
  return toa_text_buffer_get_undo_list(buffer);
}

gboolean
toa_text_view_undo(GtkTextView * view)
{
  GtkTextBuffer * buffer = gtk_text_view_get_buffer(view);
  return toa_text_buffer_undo(buffer);
}

gboolean
toa_text_view_redo(GtkTextView * view)
{
  GtkTextBuffer * buffer = gtk_text_view_get_buffer(view);
  return toa_text_buffer_redo(buffer);
}

/* ********************************************************************** */
/* ********************************************************************** */

GtkWidget *
toa_editor_new(GtkTextBuffer ** buf)
{
  GtkWidget * scroll = gtk_scrolled_window_new(NULL, NULL);
  GtkWidget * widget = gtk_text_view_new();
  GtkTextView * view = GTK_TEXT_VIEW(widget);

  toa_text_view_make_undoable(view);

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_ETCHED_IN);

  gtk_text_view_set_left_margin(view, 5);
  *buf = gtk_text_view_get_buffer(view);
  gtk_container_add(GTK_CONTAINER(scroll), widget);
  gtk_widget_show(widget);
  return scroll;
}

/* ********************************************************************** */
/* ********************************************************************** */
