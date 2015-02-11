/**
 * GUndo: Multilevel Undo/Redo for GLib / GTK+
 * Modified 2014 by the other anonymous
 * - Updated to Glib-2 / GTK+-2
 * - Added length queries and index-based access.
 * - Changed can-undo/can-redo signals into properties.
 * - Added signals for changed, undo, redo, grouping.
 * - Changed name from GundoSequence to GUndoList.
 * - Added maxlen.
 *
 * Gtk Undo: Multilevel undo/redo for Gtk
 * Copyright (C) 1999  Nat Pryce
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifdef G_UNDO_LIST_TEST
#define G_UNDO_LIST_HAVE_GTK 1
#endif
#include "gundo.h"

struct _GUndoList {
  GObject     base;
  GArray *    actions;
  guint       maxlen;
  guint       next_redo;
  GUndoList * group;
  GUndoList * parent;
};

typedef struct _UndoAction {
  GUndoActionType * type;
  gpointer          data;
} UndoAction;
#define MAXLEN_MAX  (G_MAXUINT / sizeof(UndoAction))

/* ********************************************************************** */
/* GObject Class */
/* ********************************************************************** */

static void
destroy_action(gpointer data)
{
  UndoAction * act = (UndoAction*)data;
  if (act->type->free) act->type->free(act->data);
}

static void
free_old_actions(GUndoList * ulist, guint maxlen)
{
  if (maxlen < ulist->actions->len) {
    guint cnt = ulist->actions->len - maxlen;
    g_array_remove_range(ulist->actions, 0, cnt);
    ulist->next_redo -= cnt;
  }
}

G_DEFINE_TYPE(GUndoList, g_undo_list, G_TYPE_OBJECT)

#define NOTIFY(ULIST,PROP) \
        g_object_notify_by_pspec(G_OBJECT(ULIST), properties[PROP])
#define EMIT(ULIST,SIGNAL) \
        g_signal_emit(G_OBJECT(ULIST), signals[SIGNAL], 0)

enum UndoSignalType {
  SIGNAL_CHANGED,
  SIGNAL_UNDONE,
  SIGNAL_REDONE,
  SIGNAL_GROUP_BEGIN,
  SIGNAL_GROUP_END,
  SIGNAL_GROUP_CANCEL,
  N_SIGNALS
};
static guint signals[N_SIGNALS];

enum {
  PROP_0,
  PROP_MAXLEN,
  PROP_CAN_UNDO,
  PROP_CAN_REDO,
  N_PROPERTIES
};
static GParamSpec * properties[N_PROPERTIES] = {0};

static void
g_undo_list_get_property(GObject * obj, guint prop,
                         GValue * value, GParamSpec * pspec)
{
  GUndoList * ulist = G_UNDO_LIST(obj);
  
  switch (prop) {
    case PROP_MAXLEN:
      g_value_set_uint(value, ulist->maxlen);
      break;
    case PROP_CAN_UNDO:
      g_value_set_boolean(value, g_undo_list_can_undo(ulist));
      break;
    case PROP_CAN_REDO:
      g_value_set_boolean(value, g_undo_list_can_redo(ulist));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
      break;
  }
}

static void
g_undo_list_set_property(GObject * obj, guint prop,
                         const GValue * value, GParamSpec * pspec)
{
  GUndoList * ulist = G_UNDO_LIST(obj);
  value = value;
  switch (prop) {
    case PROP_MAXLEN:
      g_undo_list_set_max_length(ulist, g_value_get_uint(value));
      break;
    case PROP_CAN_UNDO:
    case PROP_CAN_REDO:
      /* Where is G_OBJECT_WARN_READONLY_PROPERTY_ID ? */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
      break;
  }
}

static void
g_undo_list_dispose(GObject * obj)
{
  GUndoList * ulist;
  GObjectClass * base;
  g_return_if_fail(obj != NULL);

  ulist = G_UNDO_LIST(obj);
  if (ulist->group) {
    g_object_unref(G_OBJECT(ulist->group));
    ulist->group = NULL;
  }
  if (ulist->actions) {
    g_array_free(ulist->actions, TRUE);
    ulist->actions = NULL;
  }
  base = G_OBJECT_CLASS(g_undo_list_parent_class);
  if (base->dispose) base->dispose(obj);
}

static void
g_undo_list_init(GUndoList * ulist)
{
  ulist->actions = g_array_new(FALSE, FALSE, sizeof(UndoAction));
  g_array_set_clear_func(ulist->actions, &destroy_action);
  ulist->next_redo = 0;
  ulist->group = NULL;
}

static void
g_undo_list_class_init(GUndoListClass * klass)
{
  GObjectClass * base = G_OBJECT_CLASS(klass);

  base->dispose = g_undo_list_dispose;
  base->get_property = g_undo_list_get_property;
  base->set_property = g_undo_list_set_property;

  klass->changed = NULL;
  klass->undone = NULL;
  klass->redone = NULL;
  klass->group_begin = NULL;
  klass->group_end = NULL;
  klass->group_cancel = NULL;

  properties[PROP_MAXLEN] = g_param_spec_uint("maxlen",
      "Maximum Length",
      "Maximum length of the undo list.",
      1, MAXLEN_MAX,
      MAXLEN_MAX,
      G_PARAM_READABLE);

  properties[PROP_CAN_UNDO] = g_param_spec_boolean("can-undo",
      "Can Undo",
      "Are there any actions that can be undone?",
      FALSE,
      G_PARAM_READABLE);

  properties[PROP_CAN_REDO] = g_param_spec_boolean("can-redo",
      "Can Redo",
      "Are there any actions that can be redone?",
      FALSE,
      G_PARAM_READABLE);

  g_object_class_install_properties(base, N_PROPERTIES, properties);

  signals[SIGNAL_CHANGED] = g_signal_new("changed",
      G_TYPE_UNDO_LIST,
      G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET(GUndoListClass, changed),
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[SIGNAL_UNDONE] = g_signal_new("undone",
      G_TYPE_UNDO_LIST,
      G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET(GUndoListClass, undone),
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[SIGNAL_REDONE] = g_signal_new("redone",
      G_TYPE_UNDO_LIST,
      G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET(GUndoListClass, redone),
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[SIGNAL_GROUP_BEGIN] = g_signal_new("group-begin",
      G_TYPE_UNDO_LIST,
      G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET(GUndoListClass, group_begin),
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[SIGNAL_GROUP_END] = g_signal_new("group-end",
      G_TYPE_UNDO_LIST,
      G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET(GUndoListClass, group_end),
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[SIGNAL_GROUP_CANCEL] = g_signal_new("group-cancel",
      G_TYPE_UNDO_LIST,
      G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET(GUndoListClass, group_cancel),
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}

GUndoList *
g_undo_list_new_with_max_length(guint maxlen)
{
  GUndoList * ulist = G_UNDO_LIST(g_object_new(G_TYPE_UNDO_LIST, NULL));
  g_undo_list_set_max_length(ulist, maxlen);
  return ulist;
}

GUndoList *
g_undo_list_new(void)
{
  return g_undo_list_new_with_max_length(0);
}

/* ********************************************************************** */
/* Properties */
/* ********************************************************************** */

void
g_undo_list_set_max_length(GUndoList * ulist, guint maxlen)
{
  g_return_if_fail(G_TYPE_IS_UNDO_LIST(ulist));
  maxlen = maxlen ? maxlen : MAXLEN_MAX;
  g_assert(maxlen <= MAXLEN_MAX);
  ulist->maxlen = maxlen;
  free_old_actions(ulist, ulist->maxlen);
}

guint
g_undo_list_get_max_length(GUndoList * ulist)
{
  g_return_if_fail(G_TYPE_IS_UNDO_LIST(ulist));
  return ulist->maxlen;
}

gboolean
g_undo_list_can_undo(GUndoList * ulist)
{
  g_return_val_if_fail(G_TYPE_IS_UNDO_LIST(ulist), FALSE);
  return ulist->next_redo > 0;
}

gboolean
g_undo_list_can_redo(GUndoList * ulist)
{
  g_return_val_if_fail(G_TYPE_IS_UNDO_LIST(ulist), FALSE);
  return ulist->next_redo < ulist->actions->len;
}

/* ********************************************************************** */
/* Clear */
/* ********************************************************************** */

void
g_undo_list_clear(GUndoList * ulist)
{
  gboolean could_undo, could_redo, was_empty;

  g_return_if_fail(G_TYPE_IS_UNDO_LIST(ulist));
  g_return_if_fail(ulist->group == NULL);

  could_undo = g_undo_list_can_undo(ulist);
  could_redo = g_undo_list_can_redo(ulist);
  was_empty = ulist->actions->len == 0;

  g_array_set_size(ulist->actions, 0);
  ulist->next_redo = 0;

  if (could_undo) NOTIFY(ulist, PROP_CAN_UNDO);
  if (could_redo) NOTIFY(ulist, PROP_CAN_REDO);
  if (!was_empty) EMIT(ulist, SIGNAL_CHANGED);
}

/* ********************************************************************** */
/* Add Action */
/* ********************************************************************** */

void
g_undo_list_add_action(GUndoList * ulist, GUndoActionType * type, gpointer data)
{
  UndoAction action;
  gboolean could_undo, could_redo;
  g_return_if_fail(G_TYPE_IS_UNDO_LIST(ulist));

  could_undo = g_undo_list_can_undo(ulist);
  could_redo = g_undo_list_can_redo(ulist);
  if (ulist->group) {
    g_undo_list_add_action(ulist->group, type, data);
  } else {
    if (ulist->next_redo < ulist->actions->len)
      g_array_remove_range(ulist->actions, ulist->next_redo,
                           ulist->actions->len - ulist->next_redo);
    free_old_actions(ulist, ulist->maxlen - 1);

    action.type = type;
    action.data = data;
    g_array_append_val(ulist->actions, action);
    ulist->next_redo++;
  }
  if (!could_undo) NOTIFY(ulist, PROP_CAN_UNDO);
  if (could_redo)  NOTIFY(ulist, PROP_CAN_REDO);
  EMIT(ulist, SIGNAL_CHANGED);
}

/* ********************************************************************** */
/* Undo / Redo */
/* ********************************************************************** */

gboolean
g_undo_list_undo(GUndoList * ulist)
{
  UndoAction * action;
  gboolean could_redo, can_undo;
  g_return_val_if_fail(G_TYPE_IS_UNDO_LIST(ulist), FALSE);

  if (ulist->group) return g_undo_list_undo(ulist->group);
  if (!g_undo_list_can_undo(ulist)) return FALSE;

  could_redo = g_undo_list_can_redo(ulist);

  ulist->next_redo--;
  action = &g_array_index(ulist->actions, UndoAction, ulist->next_redo);
  action->type->undo(action->data);

  can_undo = g_undo_list_can_undo(ulist);
  if (!can_undo)   NOTIFY(ulist, PROP_CAN_UNDO);
  if (!could_redo) NOTIFY(ulist, PROP_CAN_REDO);
  EMIT(ulist, SIGNAL_UNDONE);
  return TRUE;
}

gboolean
g_undo_list_redo(GUndoList * ulist)
{
  UndoAction * action;
  gboolean could_undo, can_redo;
  g_return_val_if_fail(G_TYPE_IS_UNDO_LIST(ulist), FALSE);

  if (ulist->group) return g_undo_list_redo(ulist->group);
  if (!g_undo_list_can_redo(ulist)) return FALSE;

  could_undo = g_undo_list_can_undo(ulist);

  action = &g_array_index(ulist->actions, UndoAction, ulist->next_redo);
  ulist->next_redo++;
  action->type->redo(action->data);

  can_redo = g_undo_list_can_redo(ulist);
  if (!could_undo) NOTIFY(ulist, PROP_CAN_UNDO);
  if (!can_redo)   NOTIFY(ulist, PROP_CAN_REDO);
  EMIT(ulist, SIGNAL_REDONE);
  return TRUE;
}

/* ********************************************************************** */
/* Action Groups */
/* ********************************************************************** */

static void
group_undo(GUndoList * ulist)
{
  g_return_if_fail(G_TYPE_IS_UNDO_LIST(ulist));
  while (g_undo_list_can_undo(ulist)) g_undo_list_undo(ulist);
}

static void
group_redo(GUndoList * ulist)
{
  g_return_if_fail(G_TYPE_IS_UNDO_LIST(ulist));
  while (g_undo_list_can_redo(ulist)) g_undo_list_redo(ulist);
}

static GUndoActionType g_undo_action_group = {
  (GUndoActionCallback)group_undo,
  (GUndoActionCallback)group_redo,
  (GUndoActionCallback)g_object_unref
};

void
g_undo_list_begin_group(GUndoList * ulist)
{
  g_return_if_fail(G_TYPE_IS_UNDO_LIST(ulist));
  if (ulist->group) {
    g_undo_list_begin_group(ulist->group);
  } else {
    ulist->group = g_undo_list_new();
  }
  EMIT(ulist, SIGNAL_GROUP_BEGIN);
}

void
g_undo_list_end_group(GUndoList * ulist)
{
  g_return_if_fail(G_TYPE_IS_UNDO_LIST(ulist));
  g_return_if_fail(ulist->group != NULL);

  if (ulist->group->group) {
    g_undo_list_end_group(ulist->group);
  } else {
    GUndoList * group = ulist->group;
    ulist->group = NULL;

    if (group->actions->len > 0) {
      g_undo_list_add_action(ulist, &g_undo_action_group, group);
    } else {
      g_object_unref(G_OBJECT(group));
    }
  }
  EMIT(ulist, SIGNAL_GROUP_END);
}

void
g_undo_list_cancel_group(GUndoList * ulist)
{
  g_return_if_fail(G_TYPE_IS_UNDO_LIST(ulist));
  g_return_if_fail(ulist->group != NULL);

  if (ulist->group->group) {
    g_undo_list_cancel_group(ulist->group);
  } else {
    GUndoList * group = ulist->group;
    ulist->group = NULL;
    g_object_unref(G_OBJECT(group));
  }
  EMIT(ulist, SIGNAL_GROUP_CANCEL);
}

/* ********************************************************************** */
/* Querying */
/* ********************************************************************** */

guint
g_undo_list_get_length(GUndoList * ulist)
{
  g_return_val_if_fail(G_TYPE_IS_UNDO_LIST(ulist), 0);
  return ulist->actions->len;
}

guint
g_undo_list_get_undo_length(GUndoList * ulist)
{
  g_return_val_if_fail(G_TYPE_IS_UNDO_LIST(ulist), 0);
  return ulist->next_redo;
}

guint
g_undo_list_get_redo_length(GUndoList * ulist)
{
  g_return_val_if_fail(G_TYPE_IS_UNDO_LIST(ulist), 0);
  return ulist->actions->len - ulist->next_redo;
}

gpointer
g_undo_list_get_action(GUndoList * ulist, guint index, GUndoActionType ** action)
{
  UndoAction * act;
  g_return_val_if_fail(G_TYPE_IS_UNDO_LIST(ulist), NULL);
  g_return_val_if_fail(index < g_undo_list_get_length(ulist), NULL);
  act = &g_array_index(ulist->actions, UndoAction, index);
  if (action) *action = (act->type == &g_undo_action_group) ? NULL : act->type;
  return act->data;
}

gpointer
g_undo_list_get_undo(GUndoList * ulist, guint index, GUndoActionType ** action)
{
  g_return_val_if_fail(index < g_undo_list_get_undo_length(ulist), NULL);
  return g_undo_list_get_action(ulist, ulist->next_redo - index - 1, action);
}

gpointer
g_undo_list_get_redo(GUndoList * ulist, guint index, GUndoActionType ** action)
{
  g_return_val_if_fail(index < g_undo_list_get_redo_length(ulist), NULL);
  return g_undo_list_get_action(ulist, ulist->next_redo + index, action);
}

/* ********************************************************************** */
/* GTK+ */
/* ********************************************************************** */

#ifdef G_UNDO_LIST_HAVE_GTK

static void
cb_set_undo_sensitivity(GObject * obj, GParamSpec * pspec, gpointer data)
{
  GUndoList * ulist = G_UNDO_LIST(obj);
  GtkWidget * widget = GTK_WIDGET(data);
  gtk_widget_set_sensitive(widget, g_undo_list_can_undo(ulist));
  pspec = pspec;
}

static void
cb_set_redo_sensitivity(GObject * obj, GParamSpec * pspec, gpointer data)
{
  GUndoList * ulist = G_UNDO_LIST(obj);
  GtkWidget * widget = GTK_WIDGET(data);
  gtk_widget_set_sensitive(widget, g_undo_list_can_redo(ulist));
  pspec = pspec;
}

void
g_undo_make_undo_sensitive(GtkWidget * widget, GUndoList * ulist)
{
  g_signal_connect(G_OBJECT(ulist),
      "notify::can-undo",
      G_CALLBACK(cb_set_undo_sensitivity),
      widget);
  cb_set_undo_sensitivity(G_OBJECT(ulist), NULL, widget);
}

void
g_undo_make_redo_sensitive(GtkWidget * widget, GUndoList * ulist)
{
  g_signal_connect(G_OBJECT(ulist),
      "notify::can-redo",
      G_CALLBACK(cb_set_redo_sensitivity),
      widget);
  cb_set_redo_sensitivity(G_OBJECT(ulist), NULL, widget);
}

#endif /* G_UNDO_LIST_HAVE_GTK */

/* ********************************************************************** */
/* ********************************************************************** */

#ifdef G_UNDO_LIST_TEST
#include <stdio.h>
#include <string.h>

#define LOG(x)    fprintf(stderr, "%s\n", x)

static void log_cb(GObject * obj, gpointer data)
{
  obj = obj;
  fprintf(stderr, "\t\t%s\n", (char*)data);
}

static int allocs = 0;
static GtkWidget * wredo = NULL;
static GtkWidget * wundo = NULL;
static gboolean canUndo = FALSE;
static gboolean canRedo = FALSE;
static void can_undo_cb(GObject * obj, GParamSpec * pspec, gpointer data)
{
  GUndoList * ulist = G_UNDO_LIST(obj);
  pspec = pspec;
  canUndo = g_undo_list_can_undo(ulist);
  fprintf(stderr, "\t\tcan undo %d\n", canUndo);
  if (canUndo && ulist->next_redo == 0)
    fprintf(stderr, "\t\t\tERROR %d (%u %u)\n", canUndo,
            ulist->next_redo, g_undo_list_get_length(ulist));
  g_assert(data);
  g_assert(data == &canUndo);
}
static void can_redo_cb(GObject * obj, GParamSpec * pspec, gpointer data)
{
  GUndoList * ulist = G_UNDO_LIST(obj);
  pspec = pspec;
  canRedo = g_undo_list_can_redo(ulist);
  fprintf(stderr, "\t\tcan redo %d\n", canRedo);
  if (canRedo && ulist->next_redo == g_undo_list_get_length(ulist))
    fprintf(stderr, "\t\t\tERROR %d (%u %u)\n", canRedo,
            ulist->next_redo, g_undo_list_get_length(ulist));
  g_assert(data);
  g_assert(data == &canRedo);
}

static int * undoData = NULL;
static int * redoData = NULL;
static int * freeData = NULL;
static void test_undo_cb(gpointer data) { undoData = data; }
static void test_redo_cb(gpointer data) { redoData = data; }
static void test_free_cb(gpointer data)
{
  g_assert(data);
  freeData = data;
  fprintf(stderr, "\t\tfree %d\n", *(int*)data);
  g_assert(allocs > 0);
  allocs--;
}
static GUndoActionType actions = { &test_undo_cb, &test_redo_cb, &test_free_cb };

#define do_undo(ulist,expect)  do_undo_redo(ulist, TRUE, expect)
#define do_redo(ulist,expect)  do_undo_redo(ulist, FALSE, expect)
static void
do_undo_redo(GUndoList * ulist, gboolean doUndo, int expect)
{
  undoData = NULL;
  redoData = NULL;
  freeData = NULL;
  if (doUndo) {
    gboolean undo = canUndo;
    gboolean rv = g_undo_list_undo(ulist);
    g_assert_cmpint(undo, ==, rv);
    g_assert(undoData != NULL);
    g_assert(redoData == NULL);
    g_assert(freeData == NULL);
    g_assert_cmpint(*undoData, ==, expect);
  } else {
    gboolean redo = canRedo;
    gboolean rv = g_undo_list_redo(ulist);
    g_assert_cmpint(redo, ==, rv);
    g_assert(undoData == NULL);
    g_assert(redoData != NULL);
    g_assert(freeData == NULL);
    g_assert_cmpint(*redoData, ==, expect);
  }
  {
    gboolean wundoSensitive = FALSE;
    gboolean wredoSensitive = FALSE;
    g_object_get(wundo, "sensitive", &wundoSensitive, NULL);
    g_object_get(wredo, "sensitive", &wredoSensitive, NULL);
    g_assert_cmpint(wundoSensitive, ==, g_undo_list_can_undo(ulist));
    g_assert_cmpint(wredoSensitive, ==, g_undo_list_can_redo(ulist));
  }
}
static void
do_add(GUndoList * ulist, int * data)
{
  fprintf(stderr, "\tadd %d\n", *data);
  g_undo_list_add_action(ulist, &actions, data);
  allocs++;
}

int
main(void)
{
  GUndoList * ulist = g_undo_list_new();
  int data0 = 10, data1 = 20, data2 = 30, data3 = 40, data4 = 50;

  LOG("#signals");
  g_signal_connect(ulist, "notify::can-undo", G_CALLBACK(can_undo_cb), &canUndo);
  g_signal_connect(ulist, "notify::can-redo", G_CALLBACK(can_redo_cb), &canRedo);
  g_signal_connect(ulist, "changed",      G_CALLBACK(log_cb), "changed");
  g_signal_connect(ulist, "undone",       G_CALLBACK(log_cb), "undone");
  g_signal_connect(ulist, "redone",       G_CALLBACK(log_cb), "redone");
  g_signal_connect(ulist, "group-begin",  G_CALLBACK(log_cb), "group begin");
  g_signal_connect(ulist, "group-end",    G_CALLBACK(log_cb), "group end");
  g_signal_connect(ulist, "group-cancel", G_CALLBACK(log_cb), "group cancel");

  LOG("#making sensitive");
  wredo = gtk_button_new();
  wundo = gtk_button_new();
  g_undo_make_undo_sensitive(wundo, ulist);
  g_undo_make_redo_sensitive(wredo, ulist);

  LOG("#testing");
  do_add(ulist, &data0);
  do_add(ulist, &data1);
  do_add(ulist, &data2);
  do_add(ulist, &data1);
  do_add(ulist, &data0);
  g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 5);

  g_assert(g_undo_list_can_undo(ulist));
  g_assert(!g_undo_list_can_redo(ulist));

  LOG("#undo 1");
  do_undo(ulist, data0);
  do_undo(ulist, data1);
  g_assert(g_undo_list_can_undo(ulist));
  g_assert(g_undo_list_can_redo(ulist));

  LOG("#undo 2");
  do_undo(ulist, data2);
  do_undo(ulist, data1);
  do_undo(ulist, data0);
  g_assert(!g_undo_list_can_undo(ulist));
  g_assert(g_undo_list_can_redo(ulist));

  LOG("#redo 1");
  do_redo(ulist, data0);
  do_redo(ulist, data1);
  g_assert(g_undo_list_can_undo(ulist));
  g_assert(g_undo_list_can_redo(ulist));

  LOG("#adding 2");
  do_add(ulist, &data3);
  g_assert(g_undo_list_can_undo(ulist));
  g_assert(!g_undo_list_can_redo(ulist));
  do_add(ulist, &data4);
  g_assert(g_undo_list_can_undo(ulist));
  g_assert(!g_undo_list_can_redo(ulist));
  g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 4);

  LOG("#undo 3");
  do_undo(ulist, data4);
  do_undo(ulist, data3);
  do_undo(ulist, data1);
  do_undo(ulist, data0);
  g_assert(!g_undo_list_can_undo(ulist));
  g_assert(g_undo_list_can_redo(ulist));

  LOG("#redo 2");
  do_redo(ulist, data0);
  do_redo(ulist, data1);
  do_redo(ulist, data3);
  g_assert(g_undo_list_can_undo(ulist));
  g_assert(g_undo_list_can_redo(ulist));

  LOG("#query 1");
  g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 4);
  g_assert_cmpuint(g_undo_list_get_undo_length(ulist), ==, 3);
  g_assert_cmpuint(g_undo_list_get_redo_length(ulist), ==, 1);
  do_undo(ulist, data3);

  LOG("#query 2");
  g_assert(g_undo_list_get_action(ulist, 0, NULL) == &data0);
  g_assert(g_undo_list_get_action(ulist, 1, NULL) == &data1);
  g_assert(g_undo_list_get_action(ulist, 2, NULL) == &data3);
  g_assert(g_undo_list_get_action(ulist, 3, NULL) == &data4);
  g_assert(g_undo_list_get_undo(ulist, 1, NULL) == &data0);
  g_assert(g_undo_list_get_undo(ulist, 0, NULL) == &data1);
  g_assert(g_undo_list_get_redo(ulist, 0, NULL) == &data3);
  g_assert(g_undo_list_get_redo(ulist, 1, NULL) == &data4);

  LOG("#clear");
  g_undo_list_clear(ulist);
  g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 0);
  g_assert_cmpint(allocs, ==, 0);

  LOG("#adding 3");
  do_add(ulist, &data0);

  LOG("#group empty");
  g_undo_list_begin_group(ulist);
  g_undo_list_end_group(ulist);
  g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 1);

  LOG("#group non empty");
  g_undo_list_begin_group(ulist);
  g_undo_list_begin_group(ulist);
  g_undo_list_begin_group(ulist);
  do_add(ulist, &data2);
  g_undo_list_end_group(ulist);
  g_undo_list_end_group(ulist);
  g_undo_list_end_group(ulist);
  g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 2);

  LOG("#group cancel");
  g_undo_list_begin_group(ulist);
  g_undo_list_begin_group(ulist);
  do_add(ulist, &data1);
  g_undo_list_cancel_group(ulist);
  g_undo_list_end_group(ulist);
  g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 2);

  LOG("#group undo/redo");
  do_undo(ulist, data2);
  do_redo(ulist, data2);

  LOG("maxlen");
  g_undo_list_set_max_length(ulist, 2);
  g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 2);
  g_undo_list_set_max_length(ulist, 8);
  do_add(ulist, &data3); g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 3);
  do_add(ulist, &data3); g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 4);
  do_add(ulist, &data3); g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 5);
  do_add(ulist, &data3); g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 6);
  do_add(ulist, &data3); g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 7);
  do_add(ulist, &data3); g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 8);
  do_add(ulist, &data3); g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 8);
  g_undo_list_set_max_length(ulist, 4);
  g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 4);
  do_add(ulist, &data2); g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 4);
  do_add(ulist, &data1); g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 4);
  do_add(ulist, &data0); g_assert_cmpuint(g_undo_list_get_length(ulist), ==, 4);
  g_assert(g_undo_list_get_action(ulist, 0, NULL) == &data3);
  g_assert(g_undo_list_get_action(ulist, 1, NULL) == &data2);
  g_assert(g_undo_list_get_action(ulist, 2, NULL) == &data1);
  g_assert(g_undo_list_get_action(ulist, 3, NULL) == &data0);

  LOG("#freeing");
  g_object_unref(G_OBJECT(ulist));
  g_assert_cmpint(allocs, ==, 0);
  LOG("done");
  return 0;
}

#endif
