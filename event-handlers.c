/* ********************************************************************** */
/* Handle Value-Changed Events */
/* ********************************************************************** */

#include "pipeglade.h"
#include "toaeditor.h"
#include "toaentry.h"
#include <string.h>

/* ********************************************************************** */
/* ********************************************************************** */

static void
event_button_clicked(GtkButton * wgt, gpointer builder)
{
  builder = builder;
  send_command(wgt, FALSE);
}

static void
event_checkmenuitem_toggled(GtkCheckMenuItem * wgt, gpointer builder)
{
  builder = builder;
  send_value(wgt, &value_checkmenuitem, FALSE);
}

static void
event_colorbutton_colorset(GtkColorButton * wgt, gpointer builder)
{
  builder = builder;
  send_value(wgt, &value_colorbutton, FALSE);
}

static void
event_combobox_changed(GtkComboBox * wgt, gpointer builder)
{
  builder = builder;
  /* only fire for menu selection; GtkEntry will handle text changes */
  if (gtk_combo_box_get_active(wgt) != -1) {
    send_value(wgt, &value_combobox, FALSE);
  }
}

static void
event_comboboxtext_changed(GtkComboBox * wgt, gpointer builder)
{
  builder = builder;
  /* only fire for menu selection; GtkEntry will handle text changes */
  if (gtk_combo_box_get_active(wgt) != -1) {
    send_value(wgt, &value_comboboxtext, FALSE);
  }
}

static void
event_entry_activate(GtkEntry * wgt, gpointer builder)
{
  GtkWidget * par;
  builder = builder;
  /* use the combobox to get the right widget name */
  par = gtk_widget_get_parent(GTK_WIDGET(wgt));
  if (par && GTK_IS_COMBO_BOX_TEXT(par)) {
    send_value(par, &value_comboboxtext, FALSE);
  } else {
    send_value(wgt, &value_entry, FALSE);
  }
}

/* make focus-out an "activate" event */
static void
event_entry_change(GtkEditable * wgt, gpointer builder)
{
  builder = builder;
  g_object_set_data(G_OBJECT(wgt), "pipeglade-modified", GINT_TO_POINTER(TRUE));
}

static void
event_dialogbutton_close(GtkButton * wgt, gpointer builder)
{
  GtkWidget * top;
  builder = builder;
  top = gtk_widget_get_toplevel(GTK_WIDGET(wgt));
  if (gtk_widget_is_toplevel(top)) gtk_widget_hide(top);
}

static void
event_dialog_response(GtkDialog * wgt, gint response_id, gpointer builder)
{
  builder = builder;
  send_response(G_OBJECT(wgt), response_id, NULL, &value_none, FALSE);
}

static gboolean
event_entry_focusinevent(GtkWidget * wgt, GdkEvent * event, gpointer builder)
{
  event = event;
  builder = builder;
  g_object_set_data(G_OBJECT(wgt), "pipeglade-modified", GINT_TO_POINTER(FALSE));
  return FALSE;
}

static gboolean
event_entry_focusoutevent(GtkWidget * wgt, GdkEvent * event, gpointer builder)
{
  gboolean modified;
  event = event;
  modified = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(wgt), "pipeglade-modified"));
  if (modified) event_entry_activate(GTK_ENTRY(wgt), builder);
  return FALSE;
}

static void
event_filechooserbutton_fileset(GtkFileChooserButton * wgt, gpointer builder)
{
  builder = builder;
  send_value(wgt, &value_filechooserbutton, FALSE);
}

static void
event_filechooserdialog_response(GtkDialog * wgt, gint response_id, gpointer builder)
{
  builder = builder;
  send_response(G_OBJECT(wgt), response_id, NULL, &value_filechooserdialog, FALSE);
}

static void
event_fontbutton_fontset(GtkFontButton * wgt, gpointer builder)
{
  builder = builder;
  send_value(wgt, &value_fontbutton, FALSE);
}

static void
event_iconview_itemactivated(GtkIconView * wgt, GtkTreePath * path, gpointer builder)
{
  path = path;
  builder = builder;
  send_selection(wgt, &value_iconview, FALSE);
}

static void
event_menuitem_activate(GtkMenuItem * wgt, gpointer builder)
{
  builder = builder;
  send_command(wgt, FALSE);
}

static void
event_scale_valuechanged(GtkRange * wgt, gpointer builder)
{
  builder = builder;
  send_value(wgt, &value_scale, FALSE);
}

static void
event_spinbutton_valuechanged(GtkSpinButton * wgt, gpointer builder)
{
  builder = builder;
  send_value(wgt, &value_spinbutton, FALSE);
}

static void
event_switch_active(GObject * obj, GParamSpec * pspec, gpointer builder)
{
  pspec = pspec;
  builder = builder;
  send_value(obj, &value_switch, FALSE);
}

static void
event_togglebutton_toggled(GtkToggleButton * wgt, gpointer builder)
{
  builder = builder;
  send_value(wgt, &value_togglebutton, FALSE);
}

static void
event_toggletoolbutton_toggled(GtkToggleToolButton * wgt, gpointer builder)
{
  builder = builder;
  send_value(wgt, &value_toggletoolbutton, FALSE);
}

static void
event_toolbutton_clicked(GtkToolButton * wgt, gpointer builder)
{
  builder = builder;
  send_command(wgt, FALSE);
}

static void
event_treeselection_changed(GtkTreeSelection * wgt, gpointer builder)
{
  GPtrArray * pa = g_ptr_array_new_with_free_func((GDestroyNotify)g_free);
  builder = builder;
  gtk_tree_selection_selected_foreach(wgt, &treeselection_cb, pa);
  send_msg(G_OBJECT(gtk_tree_selection_get_tree_view(wgt)), "selection", pa->len, (char**)pa->pdata, FALSE);
  g_ptr_array_unref(pa);
}

static void
event_treeview_adjustment_valuechanged(GtkAdjustment * adj, gpointer scroller)
{
  GtkTreeView * view = GTK_TREE_VIEW(scroller);
  GtkTreePath * startpath = NULL;
  GtkTreePath * endpath = NULL;
  gchar * strs[2];
  adj = adj;
  if (!gtk_tree_view_get_visible_range(view, &startpath, &endpath)) {
    send_msg(G_OBJECT(view), "scroll", 0, NULL, FALSE);
    return;
  }
  strs[0] = gtk_tree_path_to_string(startpath);
  strs[1] = gtk_tree_path_to_string(endpath);
  send_msg(G_OBJECT(view), "scroll", 2, strs, FALSE);
  g_free(strs[1]);
  g_free(strs[0]);
  gtk_tree_path_free(endpath);
  gtk_tree_path_free(startpath);
}

static void
event_treeviewcolumn_clicked(GtkTreeViewColumn * wgt, gpointer builder)
{
  builder = builder;
  send_command(wgt, FALSE);
}

static void
event_window_destroy(GtkWidget * wgt, gpointer builder)
{
  wgt = wgt;
  builder = builder;
  gtk_main_quit();
}

/* ********************************************************************** */
/* ********************************************************************** */

typedef struct pg_event_info_s {
  const char * type;         /* type of widget */
  const char * name;         /* name of event */
  GCallback handle;          /* event handler (void (*)(void)) */
} pg_event_info_t;

static const pg_event_info_t g_event_info[] = {
  { "GtkButton", "clicked", (GCallback)&event_button_clicked },
  { "GtkCheckButton", "toggled", (GCallback)&event_togglebutton_toggled },
  { "GtkCheckMenuItem", "toggled", (GCallback)&event_checkmenuitem_toggled },
  { "GtkColorButton", "color-set", (GCallback)&event_colorbutton_colorset },
  { "GtkComboBox", "changed", (GCallback)&event_combobox_changed },
  { "GtkComboBoxText", "changed", (GCallback)&event_comboboxtext_changed },
  { "GtkDialog", "response", (GCallback)&event_dialog_response },
  { "GtkEntry", "activate", (GCallback)&event_entry_activate },
  { "GtkEntry", "changed", (GCallback)&event_entry_change },
  { "GtkEntry", "focus-in-event", (GCallback)&event_entry_focusinevent },
  { "GtkEntry", "focus-out-event", (GCallback)&event_entry_focusoutevent },
  { "GtkFileChooserButton", "file-set", (GCallback)&event_filechooserbutton_fileset },
  { "GtkFileChooserDialog", "response", (GCallback)&event_filechooserdialog_response },
  { "GtkFontButton", "font-set", (GCallback)&event_fontbutton_fontset },
  { "GtkIconView", "item-activated", (GCallback)&event_iconview_itemactivated },
  { "GtkMenuItem", "activate", (GCallback)&event_menuitem_activate },
  { "GtkRadioButton", "toggled", (GCallback)&event_togglebutton_toggled },
  { "GtkRadioMenuItem", "toggled", (GCallback)&event_checkmenuitem_toggled },
  { "GtkScale", "value-changed", (GCallback)&event_scale_valuechanged },
  { "GtkSpinButton", "value-changed", (GCallback)&event_spinbutton_valuechanged },
  { "GtkSwitch", "notify::active", (GCallback)&event_switch_active },
  { "GtkToggleButton", "toggled", (GCallback)&event_togglebutton_toggled },
  { "GtkToggleToolButton", "toggled", (GCallback)&event_toggletoolbutton_toggled },
  { "GtkToolButton", "clicked", (GCallback)&event_toolbutton_clicked },
  { NULL, NULL, NULL }
};

/* ********************************************************************** */
/* ********************************************************************** */

static void
connect_signals_cb(GObject * obj, GtkBuilder * builder)
{
  GtkWidget * top;

  if (!GTK_IS_WIDGET(obj)) return;
  if (!gtk_widget_get_name(GTK_WIDGET(obj))) return;

  /* don't attach to menuitems that are submenus */
  if (GTK_IS_MENU_ITEM(obj) && gtk_menu_item_get_submenu(GTK_MENU_ITEM(obj))) {
    return;
  }

  /* attach a special handler to dialog action buttons: close */
  if (GTK_IS_BUTTON(obj) &&
      (top = gtk_widget_get_toplevel(GTK_WIDGET(obj))) &&
      gtk_widget_is_toplevel(top) &&
      GTK_IS_DIALOG(top)) {
    g_signal_connect(obj, "clicked", (GCallback)&event_dialogbutton_close, builder);
    return;
  }

  /* attach special handlers to treeviews: selections, column headers, and scroll */
  if (GTK_IS_TREE_VIEW(obj)) {
    gint col, maxcol = gtk_tree_model_get_n_columns(gtk_tree_view_get_model(GTK_TREE_VIEW(obj)));
    GtkTreeSelection * sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(obj));
    GtkAdjustment * adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(obj));
    g_signal_connect(G_OBJECT(sel), "changed", (GCallback)&event_treeselection_changed, builder);
    g_signal_connect(G_OBJECT(adj), "value-changed", (GCallback)&event_treeview_adjustment_valuechanged, obj); /* not builder! */
    for (col = 0; col < maxcol; ++col) {
      GtkTreeViewColumn * tcol = gtk_tree_view_get_column(GTK_TREE_VIEW(obj), col);
      if (GTK_IS_BUILDABLE(tcol) && gtk_buildable_get_name(GTK_BUILDABLE(tcol))) {
        g_signal_connect(G_OBJECT(tcol), "clicked", (GCallback)&event_treeviewcolumn_clicked, builder);
      }
    }
  /* attach special handlers to entries: undo-history */
  } else if (GTK_IS_ENTRY(obj)) {
    toa_entry_make_undoable(GTK_ENTRY(obj));
  /* attach special handlers to textviews: undo-history */
  } else if (GTK_IS_TEXT_VIEW(obj)) {
    toa_text_view_make_undoable(GTK_TEXT_VIEW(obj));
  }

  /* attach normal handlers */
  {
    const char * type = g_type_name(G_TYPE_FROM_INSTANCE(obj));
    if (type) {
      size_t i;
      for (i = 0; g_event_info[i].type; ++i) {
        const pg_event_info_t * ip = g_event_info + i;
        if (strcmp(ip->type, type)) continue;
        g_signal_connect(obj, ip->name, ip->handle, builder);
      }
    }
  }
}

pg_error_t
connect_signals(GtkBuilder * builder, GObject ** window)
{
  GSList * objs = gtk_builder_get_objects(builder);
  g_slist_foreach(objs, (GFunc)connect_signals_cb, builder);
  g_slist_free(objs);
  /* attach special handler to main window: quit */
  *window = gtk_builder_get_object(builder, "window");
  if (!GTK_IS_WIDGET(*window)) return PG_ERROR_MISSINGWINDOW;
  g_signal_connect(G_OBJECT(*window), "destroy", G_CALLBACK(event_window_destroy), builder);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */
/*
 * Copyright (c) 2014, 2015 Bert Burgemeister <trebbu@googlemail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
