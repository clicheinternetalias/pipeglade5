/* PipeGlade
 * Run a GUI separately, communicating via a FIFO.
 */
#ifndef PIPEGLADE_H_
#define PIPEGLADE_H_ 1

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#ifndef HAVE_G_ASPRINTF /* and it won't be defined because wtf */
extern int g_asprintf(char ** pp, const char * fmt, ...);
#endif
#ifndef HAVE_G_STRING_FREE_ALL /* seriously? two dtors but no full dtor? */
extern void g_string_free_all(GString * string);
#endif

/* Errors */

typedef enum pg_error_e {
  PG_OKAY = 0,
  PG_ERROR_BADARG,
  PG_ERROR_ARGC,
  PG_ERROR_BOUNDS,
  PG_ERROR_EOL,
  PG_ERROR_BADNAME,
  PG_ERROR_BADCMD,
  PG_ERROR_UNKOWNCMD,
  PG_ERROR_MISSINGWINDOW
} pg_error_t;

#define Q(EXPR)  do{ pg_error_t e_ = (EXPR); if (e_) return (e_); }while(0)
#define QG(EXPR) do{ err = (EXPR); if (err) goto error; }while(0)

/* Send Messages */

extern void send_msg(GObject * obj, const char * name, size_t argc, char ** argv, gboolean force);
#define send_command(O,FRC)      send_msg(G_OBJECT((O)), "command", 0, NULL, (FRC))
#define send_value(O,F,FRC)      send_func(G_OBJECT((O)), "value", NULL, (F), (FRC))
#define send_selection(O,F,FRC)  send_func(G_OBJECT((O)), "selection", NULL, (F), (FRC))
extern pg_error_t send_func(GObject * obj, const char * name, void * ctxt, int (*getval)(GObject *, char **, void *), gboolean force);
extern pg_error_t send_response(GObject * obj, int id, void * ctxt, int (*getval)(GObject *, char **, void *), gboolean force);

/* Connect Event Handlers */

extern pg_error_t connect_signals(GtkBuilder * builder, GObject ** window);

/* Parse/Execute Input */

extern int pg_syntax_esc;
extern int pg_syntax_open;
extern int pg_syntax_close;
extern int cmd_iscomment(const char * p);
extern pg_error_t cmd_execute(GtkBuilder * builder, const char * msg);

/* Convert Widget Values to Strings */

extern int value_none(GObject * obj, char ** val, void * ctxt);
extern int value_checkmenuitem(GObject * obj, char ** val, void * ctxt);
extern int value_colorbutton(GObject * obj, char ** val, void * ctxt);
extern int value_combobox(GObject * obj, char ** val, void * ctxt);
extern int value_comboboxtext(GObject * obj, char ** val, void * ctxt);
extern int value_dialog_response(char ** rv, int id);
extern int value_entry(GObject * obj, char ** val, void * ctxt);
extern int value_filechooserbutton(GObject * obj, char ** val, void * ctxt);
extern int value_filechooserdialog(GObject * obj, char ** val, void * ctxt);
extern int value_fontbutton(GObject * obj, char ** val, void * ctxt);
extern int value_iconview(GObject * obj, char ** val, void * ctxt);
extern int value_scale(GObject * obj, char ** val, void * ctxt);
extern int value_spinbutton(GObject * obj, char ** val, void * ctxt);
extern int value_switch(GObject * obj, char ** val, void * ctxt);
extern int value_textview(GObject * obj, char ** val, void * ctxt);
extern int value_textview_selection(GObject * obj, char ** val, void * ctxt);
extern int value_togglebutton(GObject * obj, char ** val, void * ctxt);
extern int value_toggletoolbutton(GObject * obj, char ** val, void * ctxt);
extern int value_treecell(GObject * obj, char ** val, void * ctxt);
extern void treeselection_cb(GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, void * ctxt);
struct value_treecell_s {
  GtkTreeModel * model;
  GtkTreeIter iter;
  gint col;
  gint maxcol;
};

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

#endif /* PIPEGLADE_H_ */
