#ifndef UISTUFF_H
#define UISTUFF_H

#include <gtk/gtk.h>

GtkWidget *create_label1(char *caption);
GtkWidget *create_label2(char *caption);
GtkWidget *create_center_button(char *caption);
void set_widget_margins(GtkWidget *w, int left, int right, int top, int bottom);
void set_statusbar_message(GtkStatusbar *statusbar, guint ctxid, char *msg);
void set_statusbar(GtkStatusbar *statusbar, const char *fmt, ...);
gpointer new_data_args(int n, ...);
void clear_controls(GtkWidget *parent);

#endif
