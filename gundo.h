/**
 * GUndo: Multilevel Undo/Redo for GLib / GTK+
 * Modified 2014 by the other anonymous
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
#ifndef __UNDO_H__
#define __UNDO_H__ 1

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/** GObject -> GUndoList
 *
 * A GUndoList is a GObject that manages a list of actions that
 * can be undone and redone.
 *
 * GUndoList objects provide the following signals:
 *
 *   "changed"
 *     Fired when the undo list has been modified. This is emitted only
 *     after adding an action to the list, and after removing an action
 *     from the list.
 *   "undone"
 *     Fired when an action is undone.
 *   "redone"
 *     Fired when an action is redone.
 *   "group-begin"
 *     Fired when an action group is begun.
 *   "group-end"
 *     Fired when an action group is completed.
 *   "group-cancel"
 *     Fired when an action group is cancelled.
 *
 * GUndoList objects have the following properties:
 *
 *   "maxlen"
 *     guint: The maximum number of actions to keep in the list.
 *   "can-undo"
 *     gboolean: TRUE if the list has actions that can be undone.
 *   "can-redo"
 *     gboolean: TRUE if the list has actions that can be redone.
 */
typedef struct _GUndoList GUndoList;

/** The GObject class of GUndoList objects.
 */
typedef struct _GUndoListClass GUndoListClass;
struct _GUndoListClass {
  GObjectClass base;
  void (*changed)(GUndoList *);
  void (*undone)(GUndoList *);
  void (*redone)(GUndoList *);
  void (*group_begin)(GUndoList *);
  void (*group_end)(GUndoList *);
  void (*group_cancel)(GUndoList *);
};

/** The type of function called to undo or redo an action or free data
 * associated with an action.
 *
 * @param data Data about the action.
 * @see GUndoActionType
 */
typedef void (*GUndoActionCallback)(gpointer data);

/** An GUndoActionType defines the operations that can be applied to an undo
 * action that has been added to a GUndoList. All operations are of
 * type \ref GUndoActionCallback . That is, they take a pointer to some data
 * about the action and do not return a value.
 *
 * @see g_undo_list_add_action
 */
typedef struct _GUndoActionType {
  GUndoActionCallback undo; /**< Function called to undo the action. */
  GUndoActionCallback redo; /**< Function called to redo the action. */
  GUndoActionCallback free; /**< Function called to free the action data. May be NULL. */
} GUndoActionType;

#define G_TYPE_UNDO_LIST                  (g_undo_list_get_type())
#define G_UNDO_LIST(object)               (G_TYPE_CHECK_INSTANCE_CAST((object), G_TYPE_UNDO_LIST, GUndoList))
#define G_UNDO_LIST_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), G_TYPE_UNDO_LIST, GUndoListClass))
#define G_TYPE_IS_UNDO_LIST(object)       (G_TYPE_CHECK_INSTANCE_TYPE((object), G_TYPE_UNDO_LIST))
#define G_TYPE_IS_UNDO_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), G_TYPE_UNDO_LIST))
#define G_UNDO_LIST_GET_CLASS(object)     (G_TYPE_INSTANCE_GET_CLASS((object), G_TYPE_UNDO_LIST, GUndoListClass))

/** Returns the GObject type for a \ref GUndoList .
 *
 * @return The GObject type.
 */
extern GType g_undo_list_get_type(void);

/** Creates a new \ref GUndoList .
 *
 * The \ref GUndoList is destroyed by a call to g_object_unref.
 *
 * @return A new \ref GUndoList object.
 */
extern GUndoList * g_undo_list_new(void) G_GNUC_WARN_UNUSED_RESULT;

/** Creates a new \ref GUndoList with a limited number of actions.
 *
 * @param maxlen The maximum number of actions.
 * @return A new \ref GUndoList object.
 * @see g_undo_list_new
 */
extern GUndoList * g_undo_list_new_with_max_length(guint maxlen) G_GNUC_WARN_UNUSED_RESULT;

/** Frees all actions in the undo list.
 *
 * @pre No group is being constructed.
 * @param ulist The undo list.
 */
extern void g_undo_list_clear(GUndoList * ulist);

/** Adds an action to the end of the \ref GUndoList .
 *
 * The action is added to the most-recently begun group, if any. Redo
 * information for previous groups is not destroyed until the group
 * is completed by calling \ref g_undo_list_end_group .
 *
 * @note Only a pointer to the \ref GUndoActionType is kept in the list.
 *
 * @param ulist The undo list to which to add an action.
 * @param type The type of the action.
 * @param data Data about the action. The data will be destroyed by
 *             the "free" callback of the \ref GUndoActionType .
*/
extern void g_undo_list_add_action(GUndoList * ulist,
                                   GUndoActionType * type,
                                   gpointer data);

/** Undoes the most recent action.
 *
 * If any groups are in progress, only the most recent action
 * of the current group will be undone and the "undone" signal
 * will not be fired.
 *
 * @pre No group is being constructed.
 * @param ulist The undo list.
 * @return TRUE if an action was undone, FALSE otherwise.
 */
extern gboolean g_undo_list_undo(GUndoList * ulist);

/** Redoes the previously undone action.
 *
 * If any groups are in progress, only the most recent action
 * of the current group will be redone and the "redone" signal
 * will not be fired.
 *
 * @pre No group is being constructed.
 * @param ulist The undo list.
 * @return TRUE if an action was redone, FALSE otherwise.
 */
extern gboolean g_undo_list_redo(GUndoList * ulist);

/** Begins a new group of actions.
 *
 * Groups can be nested.
 *
 * Actions in a group are treated as a single action: one call to
 * undo or redo will undo or redo all of the actions in
 * the group. This is useful if a single action by the user causes
 * the program to issue multiple internal actions.
 *
 * Normally, when actions are added to the list, the actions which can be
 * redone will be destroyed. This destruction is postponed until the group
 * is ended with a call to \ref g_undo_list_end_group . If
 * \ref g_undo_list_cancel_group is called instead, the redo actions
 * of the parent groups will not be destroyed.
 *
 * When a group is undone or redone, the "undone" or "redone"
 * signal will only be issued once.
 *
 * @param ulist The undo list.
 * @see g_undo_list_end_group
 * @see g_undo_list_cancel_group
 */
extern void g_undo_list_begin_group(GUndoList * ulist);

/** Ends a group, adding it to the end of the undo list or its parent group.
 *
 * @param ulist The undo list.
 * @see g_undo_list_begin_group
 * @see g_undo_list_cancel_group
 */
extern void g_undo_list_end_group(GUndoList * ulist);

/** Cancels the construction of a group, destroying all of its actions.
 *
 * @param ulist The undo list.
 * @see g_undo_list_begin_group
 * @see g_undo_list_end_group
 */
extern void g_undo_list_cancel_group(GUndoList * ulist);

/** Sets the maximum number of actions allowed in the undo list.
 *
 * When the limit is reached, the oldest actions will be destroyed
 * to make room for the new ones.
 *
 * This value does not affect the length of action groups, which are
 * always unlimited.
 *
 * @param ulist The undo list.
 * @param maxlen The maximum number of actions.
 * @see g_undo_list_get_max_length
 */
extern void g_undo_list_set_max_length(GUndoList * ulist, guint maxlen);

/** Returns the maximum number of actions allowed in the undo list.
 *
 * @param ulist The undo list.
 * @return The maximum number of actions.
 * @see g_undo_list_set_max_length
 */
extern guint g_undo_list_get_max_length(GUndoList * ulist);

/** Returns whether the undo list contains any actions that can be undone.
 *
 * @param ulist The undo list.
 * @return TRUE if there are actions that can be undone, FALSE otherwise.
 */
extern gboolean g_undo_list_can_undo(GUndoList * ulist);

/** Returns whether the undo list contains any actions that can be redone.
 *
 * @param ulist The undo list.
 * @return TRUE if there are actions that can be redone, FALSE otherwise.
 */
extern gboolean g_undo_list_can_redo(GUndoList * ulist);

/** Returns the total number of actions in the list.
 *
 * @param ulist The undo list.
 * @return The total number of actions in the list.
 */
extern guint g_undo_list_get_length(GUndoList * ulist);

/** Returns the number of undo-able actions in the list.
 *
 * @param ulist The undo list.
 * @return The number of undo-able actions in the list.
 */
extern guint g_undo_list_get_undo_length(GUndoList * ulist);

/** Returns the number of redo-able actions in the list.
 *
 * @param ulist The undo list.
 * @return The number of redo-able actions in the list.
 */
extern guint g_undo_list_get_redo_length(GUndoList * ulist);

/** Get the information for an action by index.
 *
 * Actions at lower indexes are older than the actions at higher indexes.
 *
 * Action groups will be returned as a \ref GUndoList for the data
 * pointer and NULL for the \ref GUndoActionType .
 *
 * @note The indexes of actions within the list may change.
 * The only way to maintain a "saved" indicator is to place it
 * inside the action's data.
 *
 * @param ulist The undo list.
 * @param index The index of the action.
 * @param[out] action Pointer to return action information. May be NULL.
 * @return The user data for the action.
 */
extern gpointer g_undo_list_get_action(GUndoList * ulist, guint index,
                                       GUndoActionType ** action);

/** Get the information for the specified undo action.
 *
 * The index is relative to the current action. Index 0 returns the
 * action that will be undone next, index 1 returns the action after
 * that, and so on.
 *
 * @param ulist The undo list.
 * @param index The index of the action.
 * @param[out] action Pointer to return the action type in. May be NULL.
 * @return The user data for the action.
 */
extern gpointer g_undo_list_get_undo(GUndoList * ulist, guint index,
                                     GUndoActionType ** action);

/** Get the information for the specified redo action.
 *
 * The index is relative to the current action. Index 0 returns the
 * action that will be redone next, index 1 returns the action after
 * that, and so on.
 *
 * @param ulist The undo list.
 * @param index The index of the action.
 * @param[out] action Pointer to return the action type in. May be NULL.
 * @return The user data for the action.
 */
extern gpointer g_undo_list_get_redo(GUndoList * ulist, guint index,
                                     GUndoActionType ** action);

#ifdef G_UNDO_LIST_HAVE_GTK
#include <gtk/gtk.h>

/** Makes a widget sensitive to the current undo state of a \rer GUndoList .
 *
 * A widget that is undo-sensitive will only be sensitive
 * when it is possible to undo an action on its associated
 * \ref GUndoList .
 *
 * @param widget The widget to make undo-sensitive.
 * @param ulist The undo list that the widget should be sensitive to.
 */
extern void g_undo_make_undo_sensitive(GtkWidget * widget, GUndoList * ulist);

/** Makes a widget sensitive to the current redo state of a GUndoList.
 *
 * A widget that is redo-sensitive will only be sensitive
 * when it is possible to redo an action on its associated
 * \ref GUndoList .
 *
 * @param widget The widget to make redo-sensitive.
 * @param ulist The undo list that the widget should be sensitive to.
 */
extern void g_undo_make_redo_sensitive(GtkWidget * widget, GUndoList * ulist);

#endif /* G_UNDO_LIST_HAVE_GTK */

G_END_DECLS

#endif /* __UNDO_H__ */
