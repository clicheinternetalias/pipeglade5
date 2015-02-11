/* ********************************************************************** */
/* Convert Widget Values to Strings */
/* ********************************************************************** */

#include "pipeglade.h"
#include <inttypes.h>

/* ********************************************************************** */
/* ********************************************************************** */

int
value_none(GObject * obj, char ** val, void * ctxt)
{
  obj = obj;
  val = val;
  ctxt = ctxt;
  return 0;
}

static char *
value_bool(int val)
{
  return val ? "1" : "0";
}

int
value_checkmenuitem(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  *val = value_bool(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(obj)));
  return 0;
}

int
value_colorbutton(GObject * obj, char ** val, void * ctxt)
{
  GdkRGBA color;
  ctxt = ctxt;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(obj), &color);
  *val = gdk_rgba_to_string(&color);
  return 1;
}

int
value_combobox(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  if (gtk_combo_box_get_has_entry(GTK_COMBO_BOX(obj))) {
    *val = (char*)gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(obj))));
  } else {
    *val = (char*)gtk_combo_box_get_active_id(GTK_COMBO_BOX(obj));
  }
  return 0;
}

int
value_comboboxtext(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  if (gtk_combo_box_get_has_entry(GTK_COMBO_BOX(obj))) {
    *val = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(obj));
    return 1;
  }
  *val = (char*)gtk_combo_box_get_active_id(GTK_COMBO_BOX(obj));
  return 0;
}

int
value_dialog_response(char ** rv, int id)
{
  switch (id) {
    case GTK_RESPONSE_NONE:   *rv = "none"; break;
    case GTK_RESPONSE_REJECT: *rv = "reject"; break;
    case GTK_RESPONSE_ACCEPT: *rv = "accept"; break;
    case GTK_RESPONSE_OK:     *rv = "ok"; break;
    case GTK_RESPONSE_CANCEL: *rv = "cancel"; break;
    case GTK_RESPONSE_CLOSE:  *rv = "close"; break;
    case GTK_RESPONSE_YES:    *rv = "yes"; break;
    case GTK_RESPONSE_NO:     *rv = "no"; break;
    case GTK_RESPONSE_APPLY:  *rv = "apply"; break;
    case GTK_RESPONSE_HELP:   *rv = "help"; break;
    default: g_asprintf(rv, "%d", id); return 1; break;
  }
  return 0;
}

int
value_entry(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  *val = (char*)gtk_entry_get_text(GTK_ENTRY(obj));
  return 0;
}

int
value_filechooserbutton(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  *val = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(obj));
  return 1;
}

int
value_filechooserdialog(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  *val = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(obj));
  return 1;
}

int
value_fontbutton(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  *val = (char*)gtk_font_button_get_font_name(GTK_FONT_BUTTON(obj));
  return 0;
}

int
value_iconview(GObject * obj, char ** val, void * ctxt)
{
  GList * items = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(obj));
  ctxt = ctxt;
  *val = gtk_tree_path_to_string((GtkTreePath*)items->data);
  g_list_free_full(items, (GDestroyNotify)&gtk_tree_path_free);
  return 1;
}

int
value_scale(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  g_asprintf(val, "%f", gtk_range_get_value(GTK_RANGE(obj)));
  return 1;
}

int
value_spinbutton(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt; /* let's be sneaky to avoid a to-double/to-string round trip */
  *val = (char*)gtk_entry_get_text(GTK_ENTRY(obj));
  return 0;
}

int
value_switch(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  *val = value_bool(gtk_switch_get_active(GTK_SWITCH(obj)));
  return 0;
}

int
value_textview(GObject * obj, char ** val, void * ctxt)
{
  GtkTextBuffer * textbuf;
  GtkTextIter a, b;
  ctxt = ctxt;
  textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(obj));
  gtk_text_buffer_get_bounds(textbuf, &a, &b);
  *val = gtk_text_buffer_get_text(textbuf, &a, &b, FALSE);
  return 1;
}

int
value_textview_selection(GObject * obj, char ** val, void * ctxt)
{
  GtkTextBuffer * textbuf;
  GtkTextIter a, b;
  ctxt = ctxt;
  textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(obj));
  gtk_text_buffer_get_selection_bounds(textbuf, &a, &b);
  *val = gtk_text_buffer_get_text(textbuf, &a, &b, FALSE);
  return 1;
}

int
value_togglebutton(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  *val = value_bool(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(obj)));
  return 0;
}

int
value_toggletoolbutton(GObject * obj, char ** val, void * ctxt)
{
  ctxt = ctxt;
  *val = value_bool(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(obj)));
  return 0;
}

int
value_treecell(GObject * obj, char ** val, void * ctxt)
{
  struct value_treecell_s * info = ctxt;
  GValue gval = G_VALUE_INIT;
  char * str = NULL;
  GType coltype;
  obj = obj;

  if (info->col == info->maxcol) return 1;
  gtk_tree_model_get_value(info->model, &(info->iter), info->col, &gval);
  coltype = gtk_tree_model_get_column_type(info->model, info->col);
  switch (coltype) {
    case G_TYPE_BOOLEAN: g_asprintf(&str, "%d", g_value_get_boolean(&gval)); break;
    case G_TYPE_INT:     g_asprintf(&str, "%d", g_value_get_int(&gval)); break;
    case G_TYPE_UINT:    g_asprintf(&str, "%u", g_value_get_uint(&gval)); break;
    case G_TYPE_LONG:    g_asprintf(&str, "%ld", g_value_get_long(&gval)); break;
    case G_TYPE_ULONG:   g_asprintf(&str, "%lu", g_value_get_ulong(&gval)); break;
    case G_TYPE_INT64:   g_asprintf(&str, "%"PRId64, g_value_get_int64(&gval)); break;
    case G_TYPE_UINT64:  g_asprintf(&str, "%"PRIu64, g_value_get_uint64(&gval)); break;
    case G_TYPE_FLOAT:   g_asprintf(&str, "%f", g_value_get_float(&gval)); break;
    case G_TYPE_DOUBLE:  g_asprintf(&str, "%f", g_value_get_double(&gval)); break;
    case G_TYPE_STRING:  g_asprintf(&str, "%s", g_value_get_string(&gval)); break;
    default: {
      if (coltype == GDK_TYPE_PIXBUF) {
        GObject * goval = g_value_get_object(&gval);
        char * fname = g_object_get_data(goval, "pipeglade-filename");
        g_asprintf(&str, "%s", fname);
      } else {
        goto error;
      }
      break;
    }
  }
  info->col++;
  *val = str;
error:
  g_value_unset(&gval);
  return 1;
}

void
treeselection_cb(GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, void * ctxt)
{
  GPtrArray * pa = ctxt;
  model = model;
  iter = iter;
  g_ptr_array_add(pa, gtk_tree_path_to_string(path));
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
