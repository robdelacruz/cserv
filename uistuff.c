#include <gtk/gtk.h>

#include "uistuff.h"

GtkWidget *create_label1(char *caption) {
    GtkWidget *lbl = gtk_label_new(caption);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_valign(lbl, GTK_ALIGN_END);
    return lbl;
}
GtkWidget *create_label2(char *caption) {
    GtkWidget *lbl = gtk_label_new(caption);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
    return lbl;
}

GtkWidget *create_center_button(char *caption) {
    GtkWidget *btn = gtk_button_new_with_label(caption);
    gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
    return btn;
}

void set_widget_margins(GtkWidget *w, int left, int right, int top, int bottom) {
    gtk_widget_set_margin_start(w, left);
    gtk_widget_set_margin_end(w, right);
    gtk_widget_set_margin_top(w, top);
    gtk_widget_set_margin_bottom(w, bottom);
}

void set_statusbar_message(GtkStatusbar *statusbar, guint ctxid, char *msg) {
    static guint _ctxid=0;
    if (_ctxid == 0)
        _ctxid = gtk_statusbar_get_context_id(statusbar, "status0");
    if (ctxid == 0)
        ctxid = _ctxid;

    gtk_statusbar_remove_all(statusbar, ctxid);
    gtk_statusbar_push(statusbar, ctxid, msg);
}

void set_statusbar(GtkStatusbar *statusbar, const char *fmt, ...) {
    static char status[512];
    va_list args;

    va_start(args, fmt);
    vsnprintf(status, sizeof(status), fmt, args);
    va_end(args);

    set_statusbar_message(statusbar, 0, status);
}

gpointer new_data_args(int n, ...) {
    gpointer *data = malloc(sizeof(gpointer)*n);
    va_list args;

    va_start(args, n);
    for (int i=0; i < n; i++)
        data[i] = va_arg(args, gpointer);
    va_end(args);

    return data;
}

void clear_controls(GtkWidget *parent) {
    gtk_container_foreach(GTK_CONTAINER(parent), (GtkCallback) gtk_widget_destroy, NULL);
}

