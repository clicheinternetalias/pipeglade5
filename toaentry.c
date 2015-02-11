/* toaeditor.c
 * Extensions to GtkEntry and GtkEntryBuffer to implement Undo/Redo.
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
 *
 * FIXME: GtkEntry and GtkEntryBuffer are so fucking hostile towards Undo.
 * This file is the best I could do. Ideally, I would like to have the same
 * behavior as the GtkTextBuffer undo system, but that has proven impossible
 * with the current GtkEntryBuffer implementation.
 */
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "toaentry.h"

#define DATA_GET(o,name)             g_object_get_data(G_OBJECT(o), name)
#define DATA_SET(o,name,val)         g_object_set_data(G_OBJECT(o), name, val)
#define DATA_SET_STRING(o,name,val)  g_object_set_data_full(G_OBJECT(o), name, val, (GDestroyNotify)g_free)
#define DATA_GET_UINT(o,name)        GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(o), name))
#define DATA_SET_UINT(o,name,val)    g_object_set_data(G_OBJECT(o), name, GUINT_TO_POINTER(val))

/* ********************************************************************** */
/* ********************************************************************** */

/* GUndoList Info and Callbacks
 * Performs the actual undo/redo actions.
 */

typedef struct _ToaUndoInfo {
  GtkEntryBuffer * buffer;    /* the buffer this action belongs to */
  gchar *          text;      /* entire content of text buffer (our malloc) */
  gchar *          oldtext;   /* previous content of text buffer (our malloc) */
} ToaUndoInfo;

static ToaUndoInfo *
info_new(GtkEntryBuffer * buffer)
{
  ToaUndoInfo * info = g_new(ToaUndoInfo, 1);
  info->buffer  = buffer;
  info->text    = NULL;
  info->oldtext = NULL;
  return info;
}

static void
ulist_free_cb(gpointer data)
{
  ToaUndoInfo * info = (ToaUndoInfo *)data;
  if (info->text) g_free(info->text);
  if (info->oldtext) g_free(info->oldtext);
  g_free(data);
}

static void
ulist_undo_cb(gpointer data)
{
  ToaUndoInfo * info = (ToaUndoInfo *)data;
  DATA_SET_UINT(info->buffer, "undo-ignore", TRUE);
  gtk_entry_buffer_set_text(info->buffer, info->oldtext, -1);
  DATA_SET_UINT(info->buffer, "undo-ignore", FALSE);
}

static void
ulist_redo_cb(gpointer data)
{
  ToaUndoInfo * info = (ToaUndoInfo *)data;
  DATA_SET_UINT(info->buffer, "undo-ignore", TRUE);
  gtk_entry_buffer_set_text(info->buffer, info->text, -1);
  DATA_SET_UINT(info->buffer, "undo-ignore", FALSE);
}

static GUndoActionType action = {
  ulist_undo_cb, ulist_redo_cb, ulist_free_cb
};

/* ********************************************************************** */
/* ********************************************************************** */

/* Buffer event callbacks.
 * Record user actions.
 */

static void
buffer_insert_cb(GtkEntryBuffer * buffer, guint start,
                 gchar * txt, guint txtlen, gpointer data)
{
  start = start;
  txt = txt;
  txtlen = txtlen;
  data = data;
  if (!DATA_GET_UINT(buffer, "undo-ignore")) {
    GUndoList * ulist = toa_entry_buffer_get_undo_list(buffer);
    ToaUndoInfo * info = info_new(buffer);
    info->oldtext = g_strdup(DATA_GET(buffer, "undo-prev-text"));
    info->text    = g_strdup(gtk_entry_buffer_get_text(buffer));
    g_undo_list_add_action(ulist, &action, info);
  }
  DATA_SET_STRING(buffer, "undo-prev-text", g_strdup(gtk_entry_buffer_get_text(buffer)));
}

static void
buffer_delete_cb(GtkEntryBuffer * buffer, guint start,
                 guint txtlen, gpointer data)
{
  start = start;
  txtlen = txtlen;
  data = data;
  if (!DATA_GET_UINT(buffer, "undo-ignore")) {
    GUndoList * ulist = toa_entry_buffer_get_undo_list(buffer);
    ToaUndoInfo * info = info_new(buffer);
    info->oldtext = g_strdup(DATA_GET(buffer, "undo-prev-text"));
    info->text    = g_strdup(gtk_entry_buffer_get_text(buffer));
    g_undo_list_add_action(ulist, &action, info);
  }
  DATA_SET_STRING(buffer, "undo-prev-text", g_strdup(gtk_entry_buffer_get_text(buffer)));
}

static void
buffer_destroy_cb(gpointer data, GObject * where_the_object_was)
{
  GUndoList * ulist = (GUndoList *)data;
  g_object_unref(G_OBJECT(ulist));
  where_the_object_was = where_the_object_was;
}

GUndoList *
toa_entry_buffer_make_undoable(GtkEntryBuffer * buffer)
{
  GUndoList * ulist = toa_entry_buffer_get_undo_list(buffer);
  if (!ulist) {
    ulist = g_undo_list_new();
    DATA_SET(buffer, "undo-list", ulist);
    DATA_SET_STRING(buffer, "undo-prev-text", g_strdup(gtk_entry_buffer_get_text(buffer)));
    g_object_weak_ref(G_OBJECT(buffer), buffer_destroy_cb, ulist);
    g_signal_connect(buffer, "inserted-text", G_CALLBACK(buffer_insert_cb), NULL);
    g_signal_connect(buffer, "deleted-text", G_CALLBACK(buffer_delete_cb), NULL);
  }
  return ulist;
}

/* ********************************************************************** */
/* ********************************************************************** */

/* Entry bindings
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
  GtkEntry * entry = GTK_ENTRY(widget);
  GtkEntryBuffer * buffer = gtk_entry_get_buffer(entry);
  gint key = get_key(event);
  data = data;

  if (key < KEY_MAX) {
    switch (key) {
      case KEY_UNDO: toa_entry_buffer_undo(buffer); break;
      case KEY_REDO: toa_entry_buffer_redo(buffer); break;
    }
  }
  return (key < KEY_MAX);
}

/* And now, the context menu.
 */
static void
menu_undo_cb(GtkMenuItem * item, gpointer data)
{
  item = item;
  toa_entry_undo(GTK_ENTRY(data));
}

static void
menu_redo_cb(GtkMenuItem * item, gpointer data)
{
  item = item;
  toa_entry_redo(GTK_ENTRY(data));
}

static void
prepend_menuitem(GtkEntry * entry, GtkMenu * menu, const gchar * id,
                 GCallback cb, gboolean sensitive)
{
  GtkWidget * item;
  if (!id) {
    item = gtk_separator_menu_item_new();
  } else {
    item = gtk_image_menu_item_new_from_stock(id, NULL);
    gtk_widget_set_sensitive(item, sensitive);
    g_signal_connect(item, "activate", cb, entry);
  }
  gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);
  gtk_widget_show(item);
}

static void
view_populate_cb(GtkEntry * entry, GtkMenu * menu, gpointer data)
{
  GtkEntryBuffer * buffer = gtk_entry_get_buffer(entry);
  GUndoList * ulist = toa_entry_buffer_get_undo_list(buffer);
  gboolean can_undo, can_redo;
  data = data;
  can_redo = g_undo_list_can_redo(ulist);
  can_undo = g_undo_list_can_undo(ulist);
  prepend_menuitem(entry, menu, NULL, NULL, FALSE);
  prepend_menuitem(entry, menu, GTK_STOCK_REDO, (GCallback)menu_redo_cb, can_redo);
  prepend_menuitem(entry, menu, GTK_STOCK_UNDO, (GCallback)menu_undo_cb, can_undo);
}

GUndoList *
toa_entry_make_undoable(GtkEntry * entry)
{
  GtkEntryBuffer * buffer = gtk_entry_get_buffer(entry);
  GUndoList * ulist = toa_entry_buffer_make_undoable(buffer);
  g_signal_connect(entry, "key-press-event", G_CALLBACK(view_key_press_cb), NULL);
  g_signal_connect(entry, "populate-popup", G_CALLBACK(view_populate_cb), NULL);
  return ulist;
}

/* ********************************************************************** */
/* ********************************************************************** */

GUndoList *
toa_entry_buffer_get_undo_list(GtkEntryBuffer * buffer)
{
  return DATA_GET(buffer, "undo-list");
}

gboolean
toa_entry_buffer_undo(GtkEntryBuffer * buffer)
{
  GUndoList * ulist = toa_entry_buffer_get_undo_list(buffer);
  return g_undo_list_undo(ulist);
}

gboolean
toa_entry_buffer_redo(GtkEntryBuffer * buffer)
{
  GUndoList * ulist = toa_entry_buffer_get_undo_list(buffer);
  return g_undo_list_redo(ulist);
}

GUndoList *
toa_entry_get_undo_list(GtkEntry * entry)
{
  GtkEntryBuffer * buffer = gtk_entry_get_buffer(entry);
  return toa_entry_buffer_get_undo_list(buffer);
}

gboolean
toa_entry_undo(GtkEntry * entry)
{
  GtkEntryBuffer * buffer = gtk_entry_get_buffer(entry);
  return toa_entry_buffer_undo(buffer);
}

gboolean
toa_entry_redo(GtkEntry * entry)
{
  GtkEntryBuffer * buffer = gtk_entry_get_buffer(entry);
  return toa_entry_buffer_redo(buffer);
}

/* ********************************************************************** */
/* ********************************************************************** */
