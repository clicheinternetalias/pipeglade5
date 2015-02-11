/* ********************************************************************** */
/* Parse and Execute a Command */
/* ********************************************************************** */

#include "pipeglade.h"
#include "toaeditor.h"
#include "toaentry.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
tok_bool(gboolean * rv, const char * s)
{
  long int i = strtol(s, NULL, 0);
  *rv = (i) ? TRUE : (gboolean)!strcmp(s, "true");
  return PG_OKAY;
}

static pg_error_t
tok_int(gint * rv, const char * s)
{
  long int i;
  errno = 0;
  i = strtol(s, NULL, 0);
  if (errno) return PG_ERROR_BADARG;
  *rv = (gint)i;
  return PG_OKAY;
}

static pg_error_t
tok_uint(guint * rv, const char * s)
{
  unsigned long int i;
  errno = 0;
  i = strtoul(s, NULL, 0);
  if (errno) return PG_ERROR_BADARG;
  *rv = (guint)i;
  return PG_OKAY;
}

static pg_error_t
tok_long(glong * rv, const char * s)
{
  long int i;
  errno = 0;
  i = strtol(s, NULL, 0);
  if (errno) return PG_ERROR_BADARG;
  *rv = (glong)i;
  return PG_OKAY;
}

static pg_error_t
tok_ulong(gulong * rv, const char * s)
{
  unsigned long int i;
  errno = 0;
  i = strtoul(s, NULL, 0);
  if (errno) return PG_ERROR_BADARG;
  *rv = (gulong)i;
  return PG_OKAY;
}

static pg_error_t
tok_int64(gint64 * rv, const char * s)
{
  gint64 i;
  errno = 0;
  i = strtoll(s, NULL, 0);
  if (errno) return PG_ERROR_BADARG;
  *rv = (gint64)i;
  return PG_OKAY;
}

static pg_error_t
tok_uint64(guint64 * rv, const char * s)
{
  guint64 i;
  errno = 0;
  i = strtoull(s, NULL, 0);
  if (errno) return PG_ERROR_BADARG;
  *rv = (guint64)i;
  return PG_OKAY;
}

static pg_error_t
tok_float(gfloat * rv, const char * s)
{
  double i;
  errno = 0;
  i = strtod(s, NULL);
  if (errno) return PG_ERROR_BADARG;
  *rv = (gfloat)i;
  return PG_OKAY;
}

static pg_error_t
tok_double(gdouble * rv, const char * s)
{
  double i;
  errno = 0;
  i = strtod(s, NULL);
  if (errno) return PG_ERROR_BADARG;
  *rv = (gdouble)i;
  return PG_OKAY;
}

static pg_error_t
tok_color(GdkRGBA * rv, const char * s)
{
  return gdk_rgba_parse(rv, s) ? PG_OKAY : PG_ERROR_BADARG;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
store_clear(GtkTreeModel * model)
{
  if (GTK_IS_LIST_STORE(model)) {
    gtk_list_store_clear(GTK_LIST_STORE(model));
  } else {
    gtk_tree_store_clear(GTK_TREE_STORE(model));
  }
  return PG_OKAY;
}
static void
store_insert_before(GtkTreeModel * model, GtkTreeIter * iter, GtkTreeIter * parent, GtkTreeIter * sibling)
{
  if (GTK_IS_LIST_STORE(model)) {
    gtk_list_store_insert_before(GTK_LIST_STORE(model), iter, sibling);
  } else {
    gtk_tree_store_insert_before(GTK_TREE_STORE(model), iter, parent, sibling);
  }
}

static void
store_move_before(GtkTreeModel * model, GtkTreeIter * iter, GtkTreeIter * sibling)
{
  if (GTK_IS_LIST_STORE(model)) {
    gtk_list_store_move_before(GTK_LIST_STORE(model), iter, sibling);
  } else {
    gtk_tree_store_move_before(GTK_TREE_STORE(model), iter, sibling);
  }
}

static gint
store_count_rows(GtkTreeModel * model, GtkTreeIter * start, GtkTreeIter * end)
{
  /* Why can't we just compare iters for equality!? */
  gint rv;
  GtkTreePath * sp = gtk_tree_model_get_path(model, start);
  GtkTreePath * ep = gtk_tree_model_get_path(model, end);
  gint sd = 0;
  gint ed = 0;
  gint * si = gtk_tree_path_get_indices_with_depth(sp, &sd);
  gint * ei = gtk_tree_path_get_indices_with_depth(ep, &ed);
  if (sd != ed || memcmp(si, ei, ((size_t)sd - 1) * sizeof(gint))) {
    rv = -1; /* nodes are not siblings */
  } else {
    rv = (ei[ed - 1]) - (si[sd - 1]);
  }
  gtk_tree_path_free(sp);
  gtk_tree_path_free(ep);
  return rv;
}

static pg_error_t
store_remove(GtkTreeModel * model, GtkTreeIter * start, GtkTreeIter * end)
{
  gint rows;
  if (end) {
    rows = store_count_rows(model, start, end);
    if (rows < 0) return PG_ERROR_BADARG;
  } else {
    rows = gtk_tree_model_iter_n_children(model, start); /* more than enough, but not too much */
  }
  if (GTK_IS_LIST_STORE(model)) {
    while (rows-- && gtk_list_store_remove(GTK_LIST_STORE(model), start)) /**/;
  } else {
    while (rows-- && gtk_tree_store_remove(GTK_TREE_STORE(model), start)) /**/;
  }
  return PG_OKAY;
}

static void
store_set_value(GtkTreeModel * model, GtkTreeIter * iter, gint column, GValue * value)
{
  if (GTK_IS_LIST_STORE(model)) {
    gtk_list_store_set_value(GTK_LIST_STORE(model), iter, column, value);
  } else {
    gtk_tree_store_set_value(GTK_TREE_STORE(model), iter, column, value);
  }
}

static pg_error_t
treemodel_set_iter(GtkTreeModel * model, GtkTreeIter * iter, gint col, const char * value)
{
  GType coltype;
  GValue gval = G_VALUE_INIT;

  if (col < 0 || col >= gtk_tree_model_get_n_columns(model)) return PG_ERROR_BOUNDS;

  coltype = gtk_tree_model_get_column_type(model, col);
  g_value_init(&gval, coltype);

  switch (coltype) {
    case G_TYPE_BOOLEAN: {
      gboolean v = FALSE;
      Q(tok_bool(&v, value));
      g_value_set_boolean(&gval, v);
      break;
    }
    case G_TYPE_INT: {
      gint v = 0;
      Q(tok_int(&v, value));
      g_value_set_int(&gval, v);
      break;
    }
    case G_TYPE_UINT: {
      guint v = 0;
      Q(tok_uint(&v, value));
      g_value_set_uint(&gval, v);
      break;
    }
    case G_TYPE_LONG: {
      glong v = 0;
      Q(tok_long(&v, value));
      g_value_set_long(&gval, v);
      break;
    }
    case G_TYPE_ULONG: {
      gulong v = 0;
      Q(tok_ulong(&v, value));
      g_value_set_ulong(&gval, v);
      break;
    }
    case G_TYPE_INT64: {
      gint64 v = 0;
      Q(tok_int64(&v, value));
      g_value_set_int64(&gval, v);
      break;
    }
    case G_TYPE_UINT64: {
      guint64 v = 0;
      Q(tok_uint64(&v, value));
      g_value_set_uint64(&gval, v);
      break;
    }
    case G_TYPE_FLOAT: {
      gfloat v = 0.0;
      Q(tok_float(&v, value));
      g_value_set_float(&gval, v);
      break;
    }
    case G_TYPE_DOUBLE: {
      gdouble v = 0.0;
      Q(tok_double(&v, value));
      g_value_set_double(&gval, v);
      break;
    }
    case G_TYPE_STRING: {
      g_value_set_string(&gval, value);
      break;
    }
    default: {
      if (coltype == GDK_TYPE_PIXBUF) {
        GError * gerr = NULL;
        GdkPixbuf * pixbuf = gdk_pixbuf_new_from_file(value, &gerr);
        if (gerr) { /*fprintf(stderr, "%s\n", gerr->message);*/ g_error_free(gerr); return PG_ERROR_BADARG; }
        value = g_strdup(value);
        g_object_set_data_full(G_OBJECT(pixbuf), "pipeglade-filename", (gpointer)value, (GDestroyNotify)g_free);
        g_value_set_object(&gval, pixbuf);
        g_object_unref(pixbuf);
      } else {
        return PG_ERROR_BADARG;
      }
      break;
    }
  }
  store_set_value(model, iter, col, &gval);
  g_value_unset(&gval);
  return PG_OKAY;
}

static pg_error_t
treemodel_set_cell(GtkTreeModel * model, const char * where, gint col, const char * value)
{
  GtkTreeIter iter;
  if (!gtk_tree_model_get_iter_from_string(model, &iter, where)) return PG_ERROR_BOUNDS;
  return treemodel_set_iter(model, &iter, col, value);
}

static pg_error_t
treemodel_set_row(GtkTreeModel * model, const char * where, size_t argc, const char ** argv)
{
  GtkTreeIter iter;
  size_t col;
  if (!gtk_tree_model_get_iter_from_string(model, &iter, where)) return PG_ERROR_BOUNDS;
  for (col = 0; col < argc; ++col) {
    Q(treemodel_set_iter(model, &iter, (gint)col, argv[col]));
  }
  return PG_OKAY;
}

static GtkTreePath *
treemodel_string_to_path(GtkTreeModel * model, const char * where)
{
  GtkTreePath * path;
  if (!strcmp(where, "end")) {
    gint rows = gtk_tree_model_iter_n_children(model, NULL);
    path = gtk_tree_path_new_from_indices(rows - 1, -1);
  } else {
    path = gtk_tree_path_new_from_string(where);
  }
  return path;
}

static pg_error_t
treemodel_insert(GtkTreeModel * model, const char * where, const char * parent, size_t argc, const char ** argv)
{
  GtkTreeIter parent_iter;
  GtkTreeIter iter;
  GtkTreeIter * piter = NULL;
  size_t col;
  if (parent) {
    if (!gtk_tree_model_get_iter_from_string(model, &parent_iter, parent)) return PG_ERROR_BOUNDS;
    piter = &parent_iter;
  }
  if (!strcmp(where, "end")) {
    store_insert_before(model, &iter, piter, NULL);
  } else {
    if (!gtk_tree_model_get_iter_from_string(model, &iter, where)) return PG_ERROR_BOUNDS;
    store_insert_before(model, &iter, piter, &iter);
  }
  for (col = 0; col < argc; ++col) {
    Q(treemodel_set_iter(model, &iter, (gint)col, argv[col]));
  }
  return PG_OKAY;
}

static pg_error_t
treemodel_move(GtkTreeModel * model, const char * src, const char * dst)
{
  GtkTreeIter isrc, idst;
  if (!gtk_tree_model_get_iter_from_string(model, &isrc, src)) return PG_ERROR_BOUNDS;
  if (!strcmp(dst, "end")) {
    store_move_before(model, &isrc, NULL);
  } else {
    if (!gtk_tree_model_get_iter_from_string(model, &idst, dst)) return PG_ERROR_BOUNDS;
    store_move_before(model, &isrc, &idst);
  }
  return PG_OKAY;
}

static pg_error_t
treemodel_remove(GtkTreeModel * model, const char * first, const char * last)
{
  GtkTreeIter istart, iend;
  GtkTreeIter * piend = NULL;
  if (!gtk_tree_model_get_iter_from_string(model, &istart, first)) return PG_ERROR_BOUNDS;
  if (!last) { /* only deleting one row; make end exclusive */
    iend = istart;
    if (gtk_tree_model_iter_next(model, &iend)) piend = &iend; /* stay NULL if removing last row */
  } else if (strcmp(last, "end")) { /* deleteing until another row */
    if (!gtk_tree_model_get_iter_from_string(model, &iend, last)) return PG_ERROR_BOUNDS;
    piend = &iend;
  } /* else: deleteing until the end */
  return store_remove(model, &istart, piend);
}

static pg_error_t
treemodel_send(GObject * obj, GtkTreeModel * model, const char * path, const char * column)
{
  struct value_treecell_s ctxt;
  ctxt.model = model;
  if (!gtk_tree_model_get_iter_from_string(model, &(ctxt.iter), path)) return PG_ERROR_BOUNDS;
  ctxt.col = 0;
  ctxt.maxcol = gtk_tree_model_get_n_columns(model);
  if (column) {
    Q(tok_int(&(ctxt.col), column));
    if (ctxt.col < 0 || ctxt.col >= ctxt.maxcol) return PG_ERROR_BOUNDS;
    ctxt.maxcol = ctxt.col + 1;
  }
  return send_func(obj, "value", &ctxt, &value_treecell, TRUE);
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_none_close(GObject * obj, size_t argc, const char ** argv)
{
  GtkWidget * top;
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  top = gtk_widget_get_toplevel(GTK_WIDGET(obj));
  if (gtk_widget_is_toplevel(top)) gtk_widget_hide(top);
  return PG_OKAY;
}

static pg_error_t
input_none_hide(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  gtk_widget_hide(GTK_WIDGET(obj));
  return PG_OKAY;
}

static pg_error_t
input_none_main_quit(GObject * obj, size_t argc, const char ** argv)
{
  obj = obj;
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  gtk_main_quit();
  return PG_OKAY;
}

static pg_error_t
input_none_set_sensitive(GObject * obj, size_t argc, const char ** argv)
{
  gboolean b = FALSE;
  if (argc != 1) return PG_ERROR_ARGC;
  Q(tok_bool(&b, argv[0]));
  gtk_widget_set_sensitive(GTK_WIDGET(obj), b);
  return PG_OKAY;
}

static pg_error_t
input_none_set_visible(GObject * obj, size_t argc, const char ** argv)
{
  gboolean b = FALSE;
  if (argc != 1) return PG_ERROR_ARGC;
  Q(tok_bool(&b, argv[0]));
  gtk_widget_set_visible(GTK_WIDGET(obj), b);
  return PG_OKAY;
}

static pg_error_t
input_none_show(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  gtk_widget_show(GTK_WIDGET(obj));
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_checkmenuitem_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_checkmenuitem, TRUE);
}

static pg_error_t
input_checkmenuitem_set_active(GObject * obj, size_t argc, const char ** argv)
{
  gboolean b = FALSE;
  if (argc != 1) return PG_ERROR_ARGC;
  Q(tok_bool(&b, argv[0]));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(obj), b);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_colorbutton_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_colorbutton, TRUE);
}

static pg_error_t
input_colorbutton_set_color(GObject * obj, size_t argc, const char ** argv)
{
  GdkRGBA color;
  if (argc != 1) return PG_ERROR_ARGC;
  Q(tok_color(&color, argv[0]));
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(obj), &color);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_combobox_append(GObject * obj, size_t argc, const char ** argv)
{
  return treemodel_insert(gtk_combo_box_get_model(GTK_COMBO_BOX(obj)),
                          "end", NULL, argc, argv);
}

static pg_error_t
input_combobox_append_to(GObject * obj, size_t argc, const char ** argv)
{
  if (argc < 1) return PG_ERROR_ARGC;
  return treemodel_insert(gtk_combo_box_get_model(GTK_COMBO_BOX(obj)),
                          "end", argv[0], argc - 1, argv + 1);
}

static pg_error_t
input_combobox_insert(GObject * obj, size_t argc, const char ** argv)
{
  if (argc < 1) return PG_ERROR_ARGC;
  return treemodel_insert(gtk_combo_box_get_model(GTK_COMBO_BOX(obj)),
                          argv[0], NULL, argc - 1, argv + 1);
}

static pg_error_t
input_combobox_move(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 2) return PG_ERROR_ARGC;
  return treemodel_move(gtk_combo_box_get_model(GTK_COMBO_BOX(obj)), argv[0], argv[1]);
}

static pg_error_t
input_combobox_remove(GObject * obj, size_t argc, const char ** argv)
{
  if (argc == 0) return store_clear(gtk_combo_box_get_model(GTK_COMBO_BOX(obj)));
  if (argc > 2) return PG_ERROR_ARGC;
  return treemodel_remove(gtk_combo_box_get_model(GTK_COMBO_BOX(obj)),
                          argv[0], argc == 2 ? argv[1] : NULL);
}

static pg_error_t
input_combobox_select(GObject * obj, size_t argc, const char ** argv)
{
  GtkComboBox * view = GTK_COMBO_BOX(obj);
  gint idx = 0;
  if (argc != 1) return PG_ERROR_ARGC;
  Q(tok_int(&idx, argv[0]));
  gtk_combo_box_set_active(view, idx);
  return PG_OKAY;
}

static pg_error_t
input_combobox_select_id(GObject * obj, size_t argc, const char ** argv)
{
  GtkComboBox * view = GTK_COMBO_BOX(obj);
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_combo_box_set_active_id(view, argv[0]);
  return PG_OKAY;
}

static pg_error_t
input_combobox_send_selection(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_selection(obj, &value_combobox, TRUE);
}

static pg_error_t
input_combobox_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc < 1 || argc > 2) return PG_ERROR_ARGC;
  return treemodel_send(obj, gtk_combo_box_get_model(GTK_COMBO_BOX(obj)),
                        argv[0], argc == 2 ? argv[1] : NULL);
}

static pg_error_t
input_combobox_set_row(GObject * obj, size_t argc, const char ** argv)
{
  if (argc <= 1) return PG_ERROR_ARGC;
  return treemodel_set_row(gtk_combo_box_get_model(GTK_COMBO_BOX(obj)),
                           argv[0], argc - 1, argv + 1);
}

static pg_error_t
input_combobox_set_cell(GObject * obj, size_t argc, const char ** argv)
{
  GtkTreeModel * model = gtk_combo_box_get_model(GTK_COMBO_BOX(obj));
  gint col = 0;
  if (argc != 3) return PG_ERROR_ARGC;
  Q(tok_int(&col, argv[1]));
  return treemodel_set_cell(model, argv[0], col, argv[2]);
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
comboboxtext_insert(GObject * obj, gint idx, size_t argc, const char ** argv)
{
  size_t i;
  if (idx == -1) {
    for (i = 0; i < argc; ++i) {
      gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(obj), idx, argv[i]);
    }
  } else {
    for (i = argc; i--; ) {
      gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(obj), idx, argv[i]);
    }
  }
  return PG_OKAY;
}

static pg_error_t
input_comboboxtext_append(GObject * obj, size_t argc, const char ** argv)
{
  if (argc < 1) return PG_ERROR_ARGC;
  return comboboxtext_insert(obj, -1, argc, argv);
}

static pg_error_t
input_comboboxtext_insert(GObject * obj, size_t argc, const char ** argv)
{
  gint b = 0;
  if (argc < 2) return PG_ERROR_ARGC;
  if (!strcmp(argv[0], "end")) b = -1;
  else Q(tok_int(&b, argv[0]));
  return comboboxtext_insert(obj, b, argc - 1, argv + 1);
}

static pg_error_t
input_comboboxtext_remove(GObject * obj, size_t argc, const char ** argv)
{
  gint start = 0;
  gint end = 0;
  if (argc == 0) {
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(obj));
    return PG_OKAY;
  } else if (argc == 1) {
    Q(tok_int(&start, argv[0]));
    end = start;
  } else if (argc == 2) {
    Q(tok_int(&start, argv[0]));
    if (!strcmp(argv[1], "end")) {
      end = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(gtk_combo_box_get_model(GTK_COMBO_BOX(obj))), NULL) - 1;
    } else {
      Q(tok_int(&end, argv[1]));
    }
  } else {
    return PG_ERROR_ARGC;
  }
  while (start <= end) {  
    gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(obj), start);
    --end;
  }
  return PG_OKAY;
}

static pg_error_t
input_comboboxtext_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_comboboxtext, TRUE);
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_entry_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_entry, TRUE);
}

static pg_error_t
input_entry_set_text(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_entry_set_text(GTK_ENTRY(obj), argv[0]);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_filechooserbutton_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_filechooserbutton, TRUE);
}

static pg_error_t
input_filechooserbutton_set_filename(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(obj), argv[0]);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_filechooserdialog_set_filename(GObject * obj, size_t argc, const char ** argv)
{
  GtkFileChooser * fc = GTK_FILE_CHOOSER(obj);
  if (argc != 1) return PG_ERROR_ARGC;
  /* according to the docs, current_name is only when saving to a new file */
  if (gtk_file_chooser_get_action(fc) == GTK_FILE_CHOOSER_ACTION_SAVE &&
      !g_file_test(argv[0], G_FILE_TEST_EXISTS)) {
    gtk_file_chooser_set_current_name(fc, argv[0]);
  } else {
    gtk_file_chooser_set_filename(fc, argv[0]);
  }
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_fontbutton_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_fontbutton, TRUE);
}

static pg_error_t
input_fontbutton_set_font(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_font_button_set_font_name(GTK_FONT_BUTTON(obj), argv[0]);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_iconview_append(GObject * obj, size_t argc, const char ** argv)
{
  return treemodel_insert(gtk_icon_view_get_model(GTK_ICON_VIEW(obj)),
                          "end", NULL, argc, argv);
}

static pg_error_t
input_iconview_insert(GObject * obj, size_t argc, const char ** argv)
{
  if (argc < 1) return PG_ERROR_ARGC;
  return treemodel_insert(gtk_icon_view_get_model(GTK_ICON_VIEW(obj)),
                          argv[0], NULL, argc - 1, argv + 1);
}

static pg_error_t
input_iconview_move(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 2) return PG_ERROR_ARGC;
  return treemodel_move(gtk_icon_view_get_model(GTK_ICON_VIEW(obj)), argv[0], argv[1]);
}

static pg_error_t
input_iconview_remove(GObject * obj, size_t argc, const char ** argv)
{
  if (argc == 0) return store_clear(gtk_icon_view_get_model(GTK_ICON_VIEW(obj)));
  if (argc > 2) return PG_ERROR_ARGC;
  return treemodel_remove(gtk_icon_view_get_model(GTK_ICON_VIEW(obj)),
                          argv[0], argc == 2 ? argv[1] : NULL);
}

static pg_error_t
input_iconview_scroll(GObject * obj, size_t argc, const char ** argv)
{
  GtkIconView * view = GTK_ICON_VIEW(obj);
  GtkTreeModel * model = gtk_icon_view_get_model(view);
  GtkTreePath * path;
  if (argc != 1) return PG_ERROR_ARGC;
  path = treemodel_string_to_path(model, argv[0]);
  gtk_icon_view_scroll_to_path(view, path, TRUE, 0.5, 0.5);
  gtk_tree_path_free(path);
  return PG_OKAY;
}

static pg_error_t
input_iconview_select(GObject * obj, size_t argc, const char ** argv)
{
  GtkIconView * view = GTK_ICON_VIEW(obj);
  GtkTreeModel * model = gtk_icon_view_get_model(view);
  GtkTreePath * path;
  if (argc != 1) return PG_ERROR_ARGC;
  path = treemodel_string_to_path(model, argv[0]);
  gtk_icon_view_select_path(view, path);
  gtk_tree_path_free(path);
  return PG_OKAY;
}

static pg_error_t
input_iconview_send_selection(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_selection(obj, &value_iconview, TRUE);
}

static pg_error_t
input_iconview_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc < 1 || argc > 2) return PG_ERROR_ARGC;
  return treemodel_send(obj, gtk_icon_view_get_model(GTK_ICON_VIEW(obj)),
                        argv[0], argc == 2 ? argv[1] : NULL);
}

static pg_error_t
input_iconview_set_row(GObject * obj, size_t argc, const char ** argv)
{
  if (argc <= 1) return PG_ERROR_ARGC;
  return treemodel_set_row(gtk_icon_view_get_model(GTK_ICON_VIEW(obj)),
                           argv[0], argc - 1, argv + 1);
}

static pg_error_t
input_iconview_set_cell(GObject * obj, size_t argc, const char ** argv)
{
  GtkTreeModel * model = gtk_icon_view_get_model(GTK_ICON_VIEW(obj));
  gint col = 0;
  if (argc != 3) return PG_ERROR_ARGC;
  Q(tok_int(&col, argv[1]));
  return treemodel_set_cell(model, argv[0], col, argv[2]);
}

static pg_error_t
input_iconview_unselect(GObject * obj, size_t argc, const char ** argv)
{
  GtkIconView * view = GTK_ICON_VIEW(obj);
  GtkTreeModel * model = gtk_icon_view_get_model(view);
  GtkTreePath * path;
  if (argc != 1) return PG_ERROR_ARGC;
  path = treemodel_string_to_path(model, argv[0]);
  gtk_icon_view_unselect_path(view, path);
  gtk_tree_path_free(path);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_image_set_file(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_image_set_from_file(GTK_IMAGE(obj), argv[0]);
  return PG_OKAY;
}

static pg_error_t
input_image_set_icon(GObject * obj, size_t argc, const char ** argv)
{
  GtkIconSize size;
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_image_get_icon_name(GTK_IMAGE(obj), NULL, &size);
  gtk_image_set_from_icon_name(GTK_IMAGE(obj), argv[0], size);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_label_set_text(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_label_set_text(GTK_LABEL(obj), argv[0]);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_progressbar_set_fraction(GObject * obj, size_t argc, const char ** argv)
{
  gdouble b = 0;
  if (argc != 1) return PG_ERROR_ARGC;
  Q(tok_double(&b, argv[0]));
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(obj), b);
  return PG_OKAY;
}

static pg_error_t
input_progressbar_set_text(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(obj), argv[0]);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_scale_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_scale, TRUE);
}

static pg_error_t
input_scale_set_value(GObject * obj, size_t argc, const char ** argv)
{
  gdouble b = 0;
  if (argc != 1) return PG_ERROR_ARGC;
  Q(tok_double(&b, argv[0]));
  gtk_range_set_value(GTK_RANGE(obj), b);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_spinbutton_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_spinbutton, TRUE);
}

/* input_spinbutton_set_value -> input_entry_set_text */

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_spinner_start(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  gtk_spinner_start(GTK_SPINNER(obj));
  return PG_OKAY;
}

static pg_error_t
input_spinner_stop(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  gtk_spinner_stop(GTK_SPINNER(obj));
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_statusbar_pop(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  gtk_statusbar_pop(GTK_STATUSBAR(obj), 0);
  return PG_OKAY;
}

static pg_error_t
input_statusbar_push(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_statusbar_push(GTK_STATUSBAR(obj), 0, argv[0]);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_switch_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_switch, TRUE);
}

static pg_error_t
input_switch_set_active(GObject * obj, size_t argc, const char ** argv)
{
  gboolean b = FALSE;
  if (argc != 1) return PG_ERROR_ARGC;
  Q(tok_bool(&b, argv[0]));
  gtk_switch_set_active(GTK_SWITCH(obj), b);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

/* command               - entire buffer
 * command 'line'        - current line
 * command 'selection'   - current selection
 * command N             - line N
 * command N M           - line N to line M
 * command N n M m       - line/offset N/n to line/offset M/m
 */
static pg_error_t
textview_bounds(GObject * obj, size_t argc, const char ** argv,
                GtkTextIter * ia, GtkTextIter * ib)
{
  GtkTextBuffer * textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(obj));
  gint al = 0, ao = 0, bl = 0, bo = 0;
  if (argc == 0) {
    gtk_text_buffer_get_bounds(textbuf, ia, ib);
  } else if (argc == 1) {
    if (!strcmp(argv[0], "selection")) {
      gtk_text_buffer_get_selection_bounds(textbuf, ia, ib);
    } else {
      if (!strcmp(argv[0], "line")) {
        gtk_text_buffer_get_iter_at_mark(textbuf, ia, gtk_text_buffer_get_insert(textbuf));
      } else {
        Q(tok_int(&al, argv[0]));
        gtk_text_buffer_get_iter_at_line(textbuf, ia, al);
      }
      *ib = *ia;
      gtk_text_iter_set_line_offset(ia, 0);
      if (!gtk_text_iter_ends_line(ib)) gtk_text_iter_forward_to_line_end(ib);
    }
  } else if (argc == 2) {
    Q(tok_int(&al, argv[0]));
    Q(tok_int(&bl, argv[1]));
    gtk_text_buffer_get_iter_at_line(textbuf, ia, al);
    gtk_text_buffer_get_iter_at_line(textbuf, ib, bl);
  } else if (argc == 4) {
    Q(tok_int(&al, argv[0]));
    Q(tok_int(&ao, argv[1]));
    Q(tok_int(&bl, argv[2]));
    Q(tok_int(&bo, argv[3]));
    gtk_text_buffer_get_iter_at_line_offset(textbuf, ia, al, ao);
    gtk_text_buffer_get_iter_at_line_offset(textbuf, ib, bl, bo);
  } else {
    return PG_ERROR_ARGC;
  }
  return PG_OKAY;
}

static pg_error_t
input_textview_append(GObject * obj, size_t argc, const char ** argv)
{
  GtkTextBuffer * textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(obj));
  GtkTextIter iter;
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_text_buffer_get_end_iter(textbuf, &iter);
  gtk_text_buffer_insert_interactive(textbuf, &iter, argv[0], -1, TRUE);
  return PG_OKAY;
}

static pg_error_t
input_textview_delete(GObject * obj, size_t argc, const char ** argv)
{
  GtkTextBuffer * textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(obj));
  GtkTextIter ia, ib;
  Q(textview_bounds(obj, argc, argv, &ia, &ib));
  gtk_text_buffer_delete_interactive(textbuf, &ia, &ib, TRUE);
  return PG_OKAY;
}

static pg_error_t
input_textview_insert(GObject * obj, size_t argc, const char ** argv)
{
  GtkTextBuffer * textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(obj));
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_text_buffer_insert_interactive_at_cursor(textbuf, argv[0], -1, TRUE);
  return PG_OKAY;
}

static pg_error_t
input_textview_moveto(GObject * obj, size_t argc, const char ** argv)
{
  GtkTextBuffer * textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(obj));
  GtkTextIter ia;
  if (argc == 1) {
    if (!strcmp(argv[0], "end")) {
      gtk_text_buffer_get_end_iter(textbuf, &ia);
    } else {
      gint al = 0;
      Q(tok_int(&al, argv[0]));
      gtk_text_buffer_get_iter_at_line(textbuf, &ia, al);
    }
  } else if (argc == 2) {
    gint al = 0, ao = 0;
    Q(tok_int(&al, argv[0]));
    Q(tok_int(&ao, argv[1]));
    gtk_text_buffer_get_iter_at_line_offset(textbuf, &ia, al, ao);
  } else {
    return PG_ERROR_ARGC;
  }
  gtk_text_buffer_place_cursor(textbuf, &ia);
  return PG_OKAY;
}

static pg_error_t
input_textview_scroll_to_cursor(GObject * obj, size_t argc, const char ** argv)
{
  GtkTextBuffer * textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(obj));
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(obj), gtk_text_buffer_get_insert(textbuf), 0., TRUE, 0.5, 0.5);
  return PG_OKAY;
}

static pg_error_t
input_textview_select(GObject * obj, size_t argc, const char ** argv)
{
  GtkTextBuffer * textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(obj));
  GtkTextIter ia, ib;
  Q(textview_bounds(obj, argc, argv, &ia, &ib));
  gtk_text_buffer_select_range(textbuf, &ia, &ib);
  return PG_OKAY;
}

static pg_error_t
input_textview_send_selection(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_selection(obj, &value_textview_selection, TRUE);
}

static pg_error_t
input_textview_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_textview, TRUE);
}

static pg_error_t
input_textview_set_text(GObject * obj, size_t argc, const char ** argv)
{
  GtkTextIter start, end;
  GtkTextBuffer * textbuf;
  GUndoList * ulist;
  if (argc != 1) return PG_ERROR_ARGC;
  textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(obj));
  ulist = toa_text_buffer_get_undo_list(textbuf);
  g_undo_list_begin_group(ulist);
  gtk_text_buffer_get_bounds(textbuf, &start, &end);
  gtk_text_buffer_delete_interactive(textbuf, &start, &end, TRUE);
  gtk_text_buffer_get_iter_at_offset(textbuf, &start, 0);
  gtk_text_buffer_insert_interactive(textbuf, &start, argv[0], -1, TRUE);
/*  gtk_text_buffer_set_text(textbuf, argv[0], -1); why no interactive? */
  g_undo_list_end_group(ulist);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_togglebutton_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_togglebutton, TRUE);
}

static pg_error_t
input_togglebutton_set_active(GObject * obj, size_t argc, const char ** argv)
{
  gboolean b = FALSE;
  if (argc != 1) return PG_ERROR_ARGC;
  Q(tok_bool(&b, argv[0]));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(obj), b);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_toggletoolbutton_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  return send_value(obj, &value_toggletoolbutton, TRUE);
}

static pg_error_t
input_toggletoolbutton_set_active(GObject * obj, size_t argc, const char ** argv)
{
  gboolean b = FALSE;
  if (argc != 1) return PG_ERROR_ARGC;
  Q(tok_bool(&b, argv[0]));
  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(obj), b);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_treeview_append(GObject * obj, size_t argc, const char ** argv)
{
  return treemodel_insert(gtk_tree_view_get_model(GTK_TREE_VIEW(obj)),
                          "end", NULL, argc, argv);
}

static pg_error_t
input_treeview_append_to(GObject * obj, size_t argc, const char ** argv)
{
  if (argc < 1) return PG_ERROR_ARGC;
  return treemodel_insert(gtk_tree_view_get_model(GTK_TREE_VIEW(obj)),
                          NULL, argv[0], argc - 1, argv + 1);
}

static pg_error_t
input_treeview_insert(GObject * obj, size_t argc, const char ** argv)
{
  if (argc < 1) return PG_ERROR_ARGC;
  return treemodel_insert(gtk_tree_view_get_model(GTK_TREE_VIEW(obj)),
                          argv[0], NULL, argc - 1, argv + 1);
}

static pg_error_t
input_treeview_move(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 2) return PG_ERROR_ARGC;
  return treemodel_move(gtk_tree_view_get_model(GTK_TREE_VIEW(obj)), argv[0], argv[1]);
}

static pg_error_t
input_treeview_remove(GObject * obj, size_t argc, const char ** argv)
{
  if (argc == 0) return store_clear(gtk_tree_view_get_model(GTK_TREE_VIEW(obj)));
  if (argc > 2) return PG_ERROR_ARGC;
  return treemodel_remove(gtk_tree_view_get_model(GTK_TREE_VIEW(obj)),
                          argv[0], argc == 2 ? argv[1] : NULL);
}

static pg_error_t
input_treeview_scroll(GObject * obj, size_t argc, const char ** argv)
{
  GtkTreeView * view = GTK_TREE_VIEW(obj);
  GtkTreeModel * model = gtk_tree_view_get_model(view);
  GtkTreePath * path;
  gint col = 0;
  if (argc < 1 || argc > 2) return PG_ERROR_ARGC;
  path = treemodel_string_to_path(model, argv[0]);
  if (argc == 2) Q(tok_int(&col, argv[1]));
  gtk_tree_view_scroll_to_cell(view, path, gtk_tree_view_get_column(view, col), TRUE, 0.5, 0.5);
  gtk_tree_path_free(path);
  return PG_OKAY;
}

static pg_error_t
input_treeview_send_selection(GObject * obj, size_t argc, const char ** argv)
{
  GPtrArray * pa = g_ptr_array_new_with_free_func((GDestroyNotify)g_free);
  GtkTreeSelection * sel;
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(obj));
  gtk_tree_selection_selected_foreach(sel, &treeselection_cb, pa);
  send_msg(obj, "selection", pa->len, (char**)pa->pdata, TRUE);
  g_ptr_array_unref(pa);
  return PG_OKAY;
}

static pg_error_t
input_treeview_send_value(GObject * obj, size_t argc, const char ** argv)
{
  if (argc < 1 || argc > 2) return PG_ERROR_ARGC;
  return treemodel_send(obj, gtk_tree_view_get_model(GTK_TREE_VIEW(obj)),
                        argv[0], argc == 2 ? argv[1] : NULL);
}

static pg_error_t
input_treeview_set_row(GObject * obj, size_t argc, const char ** argv)
{
  if (argc <= 1) return PG_ERROR_ARGC;
  return treemodel_set_row(gtk_tree_view_get_model(GTK_TREE_VIEW(obj)),
                           argv[0], argc - 1, argv + 1);
}

static pg_error_t
input_treeview_set_cell(GObject * obj, size_t argc, const char ** argv)
{
  GtkTreeModel * model = gtk_tree_view_get_model(GTK_TREE_VIEW(obj));
  gint col = 0;
  if (argc != 3) return PG_ERROR_ARGC;
  Q(tok_int(&col, argv[1]));
  return treemodel_set_cell(model, argv[0], col, argv[2]);
}

/* ********************************************************************** */
/* ********************************************************************** */

static pg_error_t
input_window_cursor(GObject * obj, size_t argc, const char ** argv)
{

  GdkWindow * win = gtk_widget_get_window(GTK_WIDGET(obj));
  GdkCursor * cur = NULL;
  if (argc > 1) return PG_ERROR_ARGC;
  if (argc == 1) {
    cur = gdk_cursor_new_from_name(gdk_window_get_display(win), argv[0]);
    if (!cur) return PG_ERROR_BADARG;
  }
  gdk_window_set_cursor(win, cur);
  if (cur) g_object_unref(G_OBJECT(cur));
  return PG_OKAY;
}


static pg_error_t
input_window_maximize(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 0) return PG_ERROR_ARGC;
  argv = argv;
  gtk_window_maximize(GTK_WINDOW(obj));
  return PG_OKAY;
}

static pg_error_t
input_window_set_title(GObject * obj, size_t argc, const char ** argv)
{
  if (argc != 1) return PG_ERROR_ARGC;
  gtk_window_set_title(GTK_WINDOW(obj), argv[0]);
  return PG_OKAY;
}

/* ********************************************************************** */
/* ********************************************************************** */

typedef pg_error_t (*pg_input_fn)(GObject * obj, size_t argc, const char ** argv);

typedef struct pg_input_action_info_s {
  const char * action;         /* name of action */
  pg_input_fn handle;          /* callback to process incoming message */
} pg_input_action_info_t;

typedef struct pg_input_info_s {
  const char * type;                        /* widget type */
  const pg_input_action_info_t * actions;   /* array of known actions */
} pg_input_info_t;

static const pg_input_action_info_t input_none[] = {
  { "close", &input_none_close },
  { "hide", &input_none_hide },
  { "main_quit", &input_none_main_quit },
  { "set_sensitive", &input_none_set_sensitive },
  { "set_visible", &input_none_set_visible },
  { "show", &input_none_show },
  { NULL, NULL }
};
static const pg_input_action_info_t input_checkbutton[] = {
  { "send_value", &input_togglebutton_send_value },
  { "set_active", &input_togglebutton_set_active },
  { NULL, NULL }
};
static const pg_input_action_info_t input_checkmenuitem[] = {
  { "send_value", &input_checkmenuitem_send_value },
  { "set_active", &input_checkmenuitem_set_active },
  { NULL, NULL }
};
static const pg_input_action_info_t input_colorbutton[] = {
  { "send_value", &input_colorbutton_send_value },
  { "set_color", &input_colorbutton_set_color },
  { NULL, NULL }
};
static const pg_input_action_info_t input_combobox[] = {
  { "append", &input_combobox_append },
  { "append_to", &input_combobox_append_to },
  { "insert", &input_combobox_insert },
  { "move", &input_combobox_move },
  { "remove", &input_combobox_remove },
  { "select", &input_combobox_select },
  { "select_id", &input_combobox_select_id },
  { "send_selection", &input_combobox_send_selection },
  { "send_value", &input_combobox_send_value },
  { "set_row", &input_combobox_set_row },
  { "set_cell", &input_combobox_set_cell },
  { NULL, NULL }
};
static const pg_input_action_info_t input_comboboxtext[] = {
  { "append", &input_comboboxtext_append },
  { "insert", &input_comboboxtext_insert },
  { "move", &input_combobox_move }, /* borrow from combobox */
  { "remove", &input_comboboxtext_remove },
  { "select", &input_combobox_select }, /* borrow from combobox */
  { "send_value", &input_comboboxtext_send_value },
  { "set_row", &input_combobox_set_row }, /* borrow from combobox */
  { NULL, NULL }
};
static const pg_input_action_info_t input_entry[] = {
  { "send_value", &input_entry_send_value },
  { "set_text", &input_entry_set_text },
  { NULL, NULL }
};
static const pg_input_action_info_t input_filechooserbutton[] = {
  { "send_value", &input_filechooserbutton_send_value },
  { "set_filename", &input_filechooserbutton_set_filename },
  { NULL, NULL }
};
static const pg_input_action_info_t input_filechooserdialog[] = {
  { "set_filename", &input_filechooserdialog_set_filename },
  { NULL, NULL }
};
static const pg_input_action_info_t input_fontbutton[] = {
  { "send_value", &input_fontbutton_send_value },
  { "set_font", &input_fontbutton_set_font },
  { NULL, NULL }
};
static const pg_input_action_info_t input_iconview[] = {
  { "append", &input_iconview_append },
  { "insert", &input_iconview_insert },
  { "move", &input_iconview_move },
  { "remove", &input_iconview_remove },
  { "scroll", &input_iconview_scroll },
  { "select", &input_iconview_select },
  { "send_selection", &input_iconview_send_selection },
  { "send_value", &input_iconview_send_value },
  { "set_row", &input_iconview_set_row },
  { "set_cell", &input_iconview_set_cell },
  { "unselect", &input_iconview_unselect },
  { NULL, NULL }
};
static const pg_input_action_info_t input_image[] = {
  { "set_file", &input_image_set_file },
  { "set_icon", &input_image_set_icon },
  { NULL, NULL }
};
static const pg_input_action_info_t input_label[] = {
  { "set_text", &input_label_set_text },
  { NULL, NULL }
};
static const pg_input_action_info_t input_progressbar[] = {
  { "set_fraction", &input_progressbar_set_fraction },
  { "set_text", &input_progressbar_set_text },
  { NULL, NULL }
};
static const pg_input_action_info_t input_radiobutton[] = {
  { "send_value", &input_togglebutton_send_value },
  { "set_active", &input_togglebutton_set_active },
  { NULL, NULL }
};
static const pg_input_action_info_t input_radiomenuitem[] = {
  { "send_value", &input_checkmenuitem_send_value },
  { "set_active", &input_checkmenuitem_set_active },
  { NULL, NULL }
};
static const pg_input_action_info_t input_radiotoolbutton[] = {
  { "send_value", &input_toggletoolbutton_send_value },
  { "set_active", &input_toggletoolbutton_set_active },
  { NULL, NULL }
};
static const pg_input_action_info_t input_scale[] = {
  { "send_value", &input_scale_send_value },
  { "set_value", &input_scale_set_value },
  { NULL, NULL }
};
static const pg_input_action_info_t input_spinbutton[] = {
  { "send_value", &input_spinbutton_send_value },
  { "set_value", &input_entry_set_text }, /* cheating */
  { NULL, NULL }
};
static const pg_input_action_info_t input_spinner[] = {
  { "start", &input_spinner_start },
  { "stop", &input_spinner_stop },
  { NULL, NULL }
};
static const pg_input_action_info_t input_statusbar[] = {
  { "pop", &input_statusbar_pop },
  { "push", &input_statusbar_push },
  { NULL, NULL }
};
static const pg_input_action_info_t input_switch[] = {
  { "send_value", &input_switch_send_value },
  { "set_active", &input_switch_set_active },
  { NULL, NULL }
};
static const pg_input_action_info_t input_textview[] = {
  { "append", &input_textview_append },
  { "delete", &input_textview_delete },
  { "insert", &input_textview_insert },
  { "moveto", &input_textview_moveto },
  { "scroll_to_cursor", &input_textview_scroll_to_cursor },
  { "select", &input_textview_select },
  { "send_selection", &input_textview_send_selection },
  { "send_value", &input_textview_send_value },
  { "set_text", &input_textview_set_text },
  { NULL, NULL }
};
static const pg_input_action_info_t input_togglebutton[] = {
  { "send_value", &input_togglebutton_send_value },
  { "set_active", &input_togglebutton_set_active },
  { NULL, NULL }
};
static const pg_input_action_info_t input_toggletoolbutton[] = {
  { "send_value", &input_toggletoolbutton_send_value },
  { "set_active", &input_toggletoolbutton_set_active },
  { NULL, NULL }
};
static const pg_input_action_info_t input_treeview[] = {
  { "append", &input_treeview_append },
  { "append_to", &input_treeview_append_to },
  { "insert", &input_treeview_insert },
  { "move", &input_treeview_move },
  { "remove", &input_treeview_remove },
  { "scroll", &input_treeview_scroll },
  { "send_selection", &input_treeview_send_selection },
  { "send_value", &input_treeview_send_value },
  { "set_row", &input_treeview_set_row },
  { "set_cell", &input_treeview_set_cell },
  { NULL, NULL }
};
static const pg_input_action_info_t input_window[] = {
  { "cursor", &input_window_cursor },
  { "maximize", &input_window_maximize },
  { "set_title", &input_window_set_title },
  { NULL, NULL }
};

static const pg_input_info_t g_input_info[] = {
  { NULL, input_none },
  { "GtkCheckButton", input_checkbutton },
  { "GtkCheckMenuItem", input_checkmenuitem },
  { "GtkColorButton", input_colorbutton },
  { "GtkComboBox", input_combobox },
  { "GtkComboBoxText", input_comboboxtext },
  { "GtkEntry", input_entry },
  { "GtkFileChooserButton", input_filechooserbutton },
  { "GtkFileChooserDialog", input_filechooserdialog },
  { "GtkFontButton", input_fontbutton },
  { "GtkIconView", input_iconview },
  { "GtkImage", input_image },
  { "GtkLabel", input_label },
  { "GtkProgressBar", input_progressbar },
  { "GtkRadioButton", input_radiobutton },
  { "GtkRadioMenuItem", input_radiomenuitem },
  { "GtkRadioToolButton", input_radiotoolbutton },
  { "GtkScale", input_scale },
  { "GtkSpinButton", input_spinbutton },
  { "GtkSpinner", input_spinner },
  { "GtkStatusbar", input_statusbar },
  { "GtkSwitch", input_switch },
  { "GtkTextView", input_textview },
  { "GtkToggleButton", input_togglebutton },
  { "GtkToggleToolButton", input_toggletoolbutton },
  { "GtkTreeView", input_treeview },
  { "GtkWindow", input_window },
  { NULL, NULL }
};

/* ********************************************************************** */
/* ********************************************************************** */

typedef struct pg_input_s {
  char * name;
  char * action;
  GPtrArray * args;
} pg_input_t;

/* Identify a comment line.
 */
int
cmd_iscomment(const char * p)
{
  while (*p && g_ascii_isspace(*p)) ++p;
  return (*p == '#');
}

/* Tokenize a whitespace-separated or quoted word.
 */
static pg_error_t
cmd_tok_word(char ** rv, const char ** pp)
{
  GString * word = g_string_new(NULL);
  int quote = 0;
  const char * p = *pp;

  while (*p && g_ascii_isspace(*p)) ++p; /* skip leading spaces */
  if (*p) {
    if (*p == pg_syntax_open) { quote = 1; ++p; }
    while (*p) {
      if (quote ? (*p == pg_syntax_close) : g_ascii_isspace(*p)) { quote = 0; ++p; break; }
      if (*p == pg_syntax_esc) {
        switch (*++p) {
          case '\0': Q(PG_ERROR_EOL); break;
          case 'n':  g_string_append_c(word, '\n'); break;
          case 'r':  g_string_append_c(word, '\r'); break;
          case 't':  g_string_append_c(word, '\t'); break;
          default:   g_string_append_c(word, *p); break; /* literal escape */
        }
      } else {
        g_string_append_c(word, *p);
      }
      ++p;
    }
    if (quote) Q(PG_ERROR_EOL);
    while (*p && g_ascii_isspace(*p)) ++p; /* skip trailing spaces too */
  }
  *rv = g_string_free(word, FALSE);
  *pp = p;
  return PG_OKAY;
}

/* Split a line into name, action, and arguments.
 */
static pg_error_t
cmd_parse(pg_input_t * cmd, const char * line)
{
  const char * p = line;

  Q(cmd_tok_word(&(cmd->name), &p));
  if (!cmd->name || !*cmd->name) Q(PG_ERROR_BADCMD);
  Q(cmd_tok_word(&(cmd->action), &p));
  if (!cmd->action || !*cmd->action) Q(PG_ERROR_BADCMD);

  cmd->args = g_ptr_array_new_with_free_func((GDestroyNotify)g_free);
  while (*p) {
    char * str = NULL;
    Q(cmd_tok_word(&str, &p));
    g_ptr_array_add(cmd->args, str);
  }
  return PG_OKAY;
}

static void
cmd_destroy(pg_input_t * cmd)
{
  if (cmd->name) free(cmd->name);
  if (cmd->action) free(cmd->action);
  if (cmd->args) {
    g_ptr_array_unref(cmd->args);
    cmd->args = NULL;
  }
}

pg_error_t
cmd_execute(GtkBuilder * builder, const char * msg)
{
  pg_error_t err = PG_OKAY;
  pg_input_t cmd;
  GObject * obj = NULL;
  const char * type = NULL;
  size_t i, j;

  memset(&cmd, 0, sizeof(cmd));
  QG(cmd_parse(&cmd, msg));

  if (strcmp(cmd.name, "_")) {
    if (!(obj = gtk_builder_get_object(builder, cmd.name))) QG(PG_ERROR_BADNAME);
    type = g_type_name(G_TYPE_FROM_INSTANCE(obj));
  }

  /* Widgets match their typed actions and the untyped actions.
   * NULL matches only the untyped actions.
   * Note: we might search two action lists: untyped and typed.
   */
  for (i = 0; g_input_info[i].actions; ++i) {
    const pg_input_info_t * ip = g_input_info + i;
    if (ip->type && (!type || strcmp(ip->type, type))) continue;
    for (j = 0; ip->actions[j].handle; ++j) {
      const pg_input_action_info_t * ap = ip->actions + j;
      if (strcmp(ap->action, cmd.action)) continue;
      QG(ap->handle(obj, cmd.args->len, (const char**)cmd.args->pdata));
      goto done;
    }
  }
  QG(PG_ERROR_UNKOWNCMD);
done:;
error:
  cmd_destroy(&cmd);
  return err;
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
