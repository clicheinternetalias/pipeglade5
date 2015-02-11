/* PipeGlade
 * Do Unixy things with GTK+.
 */
#define VERSION "5.2.3"

#define _POSIX_C_SOURCE 200809L
#include "pipeglade.h"

#include <fcntl.h> /* open */
#include <sys/stat.h> /* stat, mkfifo */
#include <unistd.h> /* close, unlink */
#include <stdio.h> /* FILE */
#include <stdlib.h> /* atexit */
#include <time.h> /* nanosleep */
#include <pthread.h>
#include <locale.h>

#ifndef HAVE_G_ASPRINTF
#include <stdarg.h>
int
g_asprintf(char ** pp, const char * fmt, ...)
{
  int rv;
  va_list ap;
  va_start(ap, fmt);
  rv = g_vasprintf(pp, fmt, ap);
  va_end(ap);
  return rv;
}
#endif

#ifndef HAVE_G_STRING_FREE_ALL
void
g_string_free_all(GString * string)
{
  g_string_free(string, TRUE);
}
#endif

/* ********************************************************************** */
/* Errors */
/* ********************************************************************** */

static const char * errmsgs[] = {
  "okay",
  "invalid argument", /* PG_ERROR_BADARG */
  "wrong number of arguments", /* PG_ERROR_ARGC */
  "index out of bounds", /* PG_ERROR_BOUNDS */
  "unexpected end of line", /* PG_ERROR_EOL */
  "unknown name", /* PG_ERROR_BADNAME */
  "missing name or action", /* PG_ERROR_BADCMD */
  "unknown command", /* PG_ERROR_UNKOWNCMD */
  "missing top-level window named \'window\'" /* PG_ERROR_MISSINGWINDOW */
};

static void
emit_error(pg_error_t err, const char * msg)
{
  fprintf(stderr, "error: %s in \"\"\"%s\"\"\"\n",
          errmsgs[err], msg);
}

/* ********************************************************************** */
/* Primitives */
/* ********************************************************************** */

static void
wait(long int amt)
{
  struct timespec tmspc = { 0, amt * 1000 };
  nanosleep(&tmspc, NULL);
}

/*
 * Create a fifo if necessary, and open it.  Give up if the file
 * exists but is not a fifo
 */
static FILE *
pg_fifo(const char * name, const char * mode)
{
  FILE * fp = NULL;
  int fd = -1;

  if (name) {
    struct stat sb;
    memset(&sb, 0, sizeof(sb));
    stat(name, &sb);
    if (!S_ISFIFO(sb.st_mode)) {
      if (mkfifo(name, 0666) != 0) goto error;
    }
  }
  switch (mode[0]) {
    case 'r': {
      if (!name) { fp = stdin; break; }
      if ((fd = open(name, O_RDWR | O_NONBLOCK)) < 0) goto error;
      if (!(fp = fdopen(fd, "r"))) goto error;
      break;
    }
    case 'w': {
      if (!name) { fp = stdout; break; }
      /* fopen blocks if no reader */
      if ((fd = open(name, O_RDONLY | O_NONBLOCK)) < 0) goto error;
      if (!(fp = fopen(name, "w"))) goto error;
      close(fd); fd = -1;
      break;
    }
  }
  setvbuf(fp, NULL, _IOLBF, 0); /* make line-buffered */
  return fp;
error:
  if (fd >= 0) close(fd);
  return NULL;
}

/* Append a line from a file or fifo.
 */
static void
pg_file_getline(GString * s, FILE * fp)
{
  for (;;) {
    int c = getc(fp);
    if (c == EOF) {
      wait(1000); /* wait 1/10th of a second */
      continue;
    }
    if (c == '\n') break;
    g_string_append_c(s, (char)c);
  }
}

/* ********************************************************************** */
/* Globals */
/* ********************************************************************** */

static FILE * pg_input;
static FILE * pg_output;
static gchar * pg_in_fname;
static gchar * pg_out_fname;
int pg_syntax_esc = '\\';
int pg_syntax_open = '\"';
int pg_syntax_close = '\"';
static gboolean pg_suppress_events;

/* ********************************************************************** */
/* Send Output */
/* ********************************************************************** */

static void
send_string(const char * s)
{
  /* use quotes if we got them; otherwise, escape all whitespace */
  if (pg_syntax_open) putc(pg_syntax_open, pg_output);
  while (*s) {
    if (*s == pg_syntax_esc) {
      putc(pg_syntax_esc, pg_output);
      putc(pg_syntax_esc, pg_output);
    } else if (*s == '\n') {
      putc(pg_syntax_esc, pg_output);
      putc('n', pg_output);
    } else {
      if (!pg_syntax_open && g_ascii_isspace(*s)) putc(pg_syntax_esc, pg_output);
      putc(*s, pg_output);
    }
    ++s;
  }
  if (pg_syntax_close) putc(pg_syntax_close, pg_output);
}

/*
 * Send GUI feedback to global stream "pg_output". The message format is
 * "<origin> <name> <'data' ...>". We're being patient with
 * receivers which may intermittently close their end of the fifo, and
 * make a couple of retries if an error occurs.
 */
void
send_msg(GObject * obj, const char * name, size_t argc, char ** argv, gboolean force)
{
  size_t i;
  long int nsec;

  if (pg_suppress_events && !force) return;

  for (nsec = 100; nsec < 100000; nsec *= 10) {
    fputs(gtk_buildable_get_name(GTK_BUILDABLE(obj)), pg_output);
    putc(' ', pg_output);
    fputs(name, pg_output);
    for (i = 0; i < argc; ++i) {
      putc(' ', pg_output);
      if (argv[i]) send_string(argv[i]);
    }
    putc('\n', pg_output);
    if (ferror(pg_output)) {
      fputs("error sending message; retrying\n", stderr);
      clearerr(pg_output);
      wait(nsec);
      putc('\n', pg_output);
    } else {
      break;
    }
  }
}

pg_error_t
send_func(GObject * obj, const char * name, void * ctxt, int (*getval)(GObject *, char **, void *), gboolean force)
{
  GPtrArray * pa = g_ptr_array_new_with_free_func((GDestroyNotify)g_free);
  do {
    char * str = NULL;
    int dofree = (*getval)(obj, &str, ctxt);
    if (!dofree) str = g_strdup(str);
    if (!str) break;
    g_ptr_array_add(pa, str);
  } while (ctxt); /* if ctxt, we may have more than one arg to send */
  send_msg(obj, name, pa->len, (char**)pa->pdata, force);
  g_ptr_array_unref(pa);
  return PG_OKAY;
}

pg_error_t
send_response(GObject * obj, int id, void * ctxt, int (*getval)(GObject *, char **, void *), gboolean force)
{
  char * name = NULL;
  int dofree = value_dialog_response(&name, id);
  send_func(obj, name, ctxt, getval, force);
  if (dofree) g_free(name);
  return PG_OKAY;
}

/* ********************************************************************** */
/* Receive Input */
/* ********************************************************************** */

struct pg_ui_data {
  GtkBuilder * builder;
  GString * msg;
  gboolean completed;
};

/* Execute a command. Set ud->completed = true if done.
 * Runs once per command inside gtk_main_loop()
 */
static gboolean
execute_message(void * p) /* gdk_threads_add_timeout callback */
{
  struct pg_ui_data * ud = p;
  pg_error_t err;
  pg_suppress_events = TRUE;
  err = cmd_execute(ud->builder, ud->msg->str);
  pg_suppress_events = FALSE;
  if (err != PG_OKAY) emit_error(err, ud->msg->str);
  ud->completed = TRUE;
  return G_SOURCE_REMOVE;
}

/* Read lines from global stream "pg_input" and perform the appropriate actions.
 */
static void *
receive_message(void * builder) /* pthread_create callback */
{
  for (;;) {
    GString * str = g_string_new(NULL);
    struct pg_ui_data ud = { builder, str, FALSE };

    pthread_cleanup_push((void(*)(void *))g_string_free_all, ud.msg);
    pthread_testcancel();

    g_string_truncate(ud.msg, 0);
    pg_file_getline(ud.msg, pg_input);

    if (!cmd_iscomment(ud.msg->str)) {
      ud.completed = FALSE;
      pthread_testcancel();
      gdk_threads_add_timeout(1, &execute_message, &ud);
      while (!ud.completed) {
        wait(100); /* every 1/100th of a second */
      }
    }
    pthread_cleanup_pop(1);
  }
  return NULL;
}

/* ********************************************************************** */
/* Main */
/* ********************************************************************** */

static gboolean
pg_opt_escape(const gchar * option_name, const gchar * value, gpointer data, GError ** gerr)
{
  option_name = option_name;
  data = data;
  if (!*value || g_ascii_isspace(*value)) {
    g_set_error(gerr, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "Escape character must be a non-whitespace character.");
    return FALSE;
  }
  pg_syntax_esc = (int)(unsigned char)*value;
  return TRUE;
}

static gboolean
pg_opt_quotes(const gchar * option_name, const gchar * value, gpointer data, GError ** gerr)
{
  option_name = option_name;
  data = data;
  gerr = gerr;
  pg_syntax_close = pg_syntax_open = (int)(unsigned char)value[0];
  if (value[0] && value[1]) pg_syntax_close = (int)(unsigned char)value[1];
  return TRUE;
}

static void
cleanup(void)
{
  if (pg_input && pg_input != stdin) {
    fclose(pg_input); pg_input = NULL;
    unlink(pg_in_fname); g_free(pg_in_fname); pg_in_fname = NULL;
  }
  if (pg_output && pg_output != stdout) {
    fclose(pg_output); pg_output = NULL;
    unlink(pg_out_fname); g_free(pg_out_fname); pg_out_fname = NULL;
  }
}

int
main(int argc, char ** argv)
{
  GtkBuilder * builder;
  pthread_t receiver;
  GObject * window = NULL;
  GError * gerr = NULL;

  gchar ** ui_filenames = NULL;
  gboolean version = FALSE;

  GOptionEntry entries[] = {
    { "infifo",  'i', 0, G_OPTION_ARG_FILENAME, &pg_in_fname,
                         "Input FIFO filename (default: stdin)", "fname" },
    { "outfifo", 'o', 0, G_OPTION_ARG_FILENAME, &pg_out_fname,
                         "Output FIFO filename (default: stdout)", "fname" },
    { "escape",  'e', 0, G_OPTION_ARG_CALLBACK, &pg_opt_escape, /* FIXME: obj/func ptr conversion */
                         "Escape character in messages (default: \"\\\\\")", "char" },
    { "string",  's', 0, G_OPTION_ARG_CALLBACK, &pg_opt_quotes, /* FIXME: obj/func ptr conversion */
                         "Quote characters in messages (default: \"\\\"\\\"\")", "two-chars" },
    { "version", 'V', 0, G_OPTION_ARG_NONE,     &version, "Show versions", NULL },
    { "",       '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &ui_filenames,
                         "Glade UI files", "file1.ui..." },
    { NULL,     '\0', 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
  };
  GOptionContext * goptc;

  setlocale(LC_ALL, "");

  goptc = g_option_context_new("- control a Glade UI from pipes");
  g_option_context_add_main_entries(goptc, entries, NULL);
  g_option_context_add_group(goptc, gtk_get_option_group(TRUE));
  if (!g_option_context_parse(goptc, &argc, &argv, &gerr)) {
    fprintf(stderr, "%s\n", gerr->message);
    return 1;
  }
  if (version) {
    fprintf(stdout, VERSION "\nGTK+ v%d.%d.%d (running v%d.%d.%d)\n",
            GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
            gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());
    return 0;
  }

  /* build UI */
  if (!ui_filenames) {
    fputs("missing glade .ui file(s)\n", stderr);
    return 1;
  }
  gtk_init(&argc, &argv); /* QUESTION: didn't we already parse gtk opts above? */
  builder = gtk_builder_new();
  {
    gchar ** s = ui_filenames;
    while (*s) {
      if (gtk_builder_add_from_file(builder, *s, &gerr) == 0) {
        fprintf(stderr, "%s\n", gerr->message);
        return 1;
      }
      ++s;
    }
  }
  g_strfreev(ui_filenames); ui_filenames = NULL;

  /* establish I/O */
  atexit(&cleanup);
  if (!(pg_input = pg_fifo(pg_in_fname, "r")) || !(pg_output = pg_fifo(pg_out_fname, "w"))) {
    fputs("failed to open fifo", stderr);
    return 1;
  }
  pthread_create(&receiver, NULL, &receive_message, (void*)builder);
  gtk_builder_connect_signals(builder, builder);
  {
    pg_error_t err = connect_signals(builder, &window);
    if (err) {
      fputs(errmsgs[err], stderr);
      return 1;
    }
  }

  /* Do the thing! */
  gtk_widget_show(GTK_WIDGET(window));
  gtk_main();

  /* shut down I/O */
  pthread_cancel(receiver);
  pthread_join(receiver, NULL);
  return 0;
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
