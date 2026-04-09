#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clib.h"
#include "cnet.h"
#include "msg.h"

GtkWidget *create_label1(char *caption) {
    GtkWidget *lbl = gtk_label_new(caption);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_valign(lbl, GTK_ALIGN_END);
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

gboolean idlefunc(gpointer userdata) {
    return TRUE;
}

gboolean on_server_read(GIOChannel *ch, GIOCondition iocond, void *data);
gboolean on_server_write(GIOChannel *ch, GIOCondition iocond, void *data);

void create_login_ui(HostCtx *hostctx, GIOChannel *serverch);
void login_clicked(GtkWidget *w, void **data);

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    char *serverhost = "localhost";
    char *serverport = "8000";
    if (argc > 1)
        serverhost = argv[1];
    if (argc > 2)
        serverport = argv[2];

    int backlog = 50;
    struct sockaddr sa;
    int serverfd = OpenConnectSocket(serverhost, serverport, backlog, &sa);
    if (serverfd == -1)
        exit(1);
    printf("main() serverfd: %d\n", serverfd);

    String ipaddr = StringNew("");
    GetTextIPAddress(&sa, &ipaddr);
    printf("Connected to %.*s port %s...\n", ipaddr.len, ipaddr.bs, serverport);
    StringFree(ipaddr);

    HostCtx hostctx = HostCtxNew(serverfd);

    GIOChannel *serverch = g_io_channel_unix_new(serverfd);
    g_io_add_watch(serverch, G_IO_IN, on_server_read, &hostctx);

    create_login_ui(&hostctx, serverch);
    gtk_main();
    return 0;
}

gboolean on_server_read(GIOChannel *ch, GIOCondition iocond, void *data) {
    HostCtx *hostctx = data;
    return TRUE;
}
gboolean on_server_write(GIOChannel *ch, GIOCondition iocond, void *data) {
    HostCtx *hostctx = data;
    int serverfd = g_io_channel_unix_get_fd(ch);

    int z = NetSend(serverfd, &hostctx->writebuf);
    // If not able to send all the bytes, keep this event handler alive 
    // to resume sending bytes.
    if (z != 0)
        return TRUE;

    // All bytes sent, remove this event handler.
    return FALSE;
}

void create_login_ui(HostCtx *hostctx, GIOChannel *serverch) {
    GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w), "Messenger");
    gtk_window_set_default_size(GTK_WINDOW(w), 230,300);
    gtk_window_set_position(GTK_WINDOW(w), GTK_WIN_POS_CENTER);

    // Window menu
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *filemenu = gtk_menu_new();
    GtkWidget *filemi = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *quitmi = gtk_menu_item_new_with_mnemonic("_Quit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(filemi), filemenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), quitmi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), filemi);

    // Login controls
    GtkWidget *lbl1 = create_label1("Username");
    GtkWidget *txtusername = gtk_entry_new();
    GtkWidget *lbl2 = create_label1("Password");
    GtkWidget *txtpassword = gtk_entry_new();
    GtkWidget *loginbtn = create_center_button("Login");

    GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
    set_widget_margins(vbox, 25, 25, 25, 25);
    gtk_box_pack_start(GTK_BOX(vbox), lbl1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), txtusername, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), txtpassword, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), loginbtn, FALSE, FALSE, 10);

    GtkWidget *framebox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(w), framebox);
    gtk_box_pack_start(GTK_BOX(framebox), menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(framebox), vbox, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(quitmi), "activate", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(w, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    static void *data[4];
    data[0] = serverch;
    data[1] = hostctx;
    data[2] = txtusername;
    data[3] = txtpassword;
    g_signal_connect(G_OBJECT(loginbtn), "clicked", G_CALLBACK(login_clicked), data);

    gtk_widget_show_all(w);
}
void login_clicked(GtkWidget *w, void **data) {
    GIOChannel *serverch = data[0];
    HostCtx *hostctx = data[1];
    GtkEntry *txtusername = data[2];
    GtkEntry *txtpassword = data[3];
    printf("login_clicked() serverfd: %d\n", hostctx->fd);

    char *alias = (char *) gtk_entry_get_text(GTK_ENTRY(txtusername));
    char *password = (char *) gtk_entry_get_text(GTK_ENTRY(txtpassword));
    LoginMsg loginmsg = {LOGINMSG, StringNew(alias), StringNew(password)};
    MsgPack(&loginmsg, &hostctx->writebuf);
    int z = NetSend(hostctx->fd, &hostctx->writebuf);
    // If not able to send all the bytes, start event handler to resume
    // sending bytes.
    if (z != 0)
        g_io_add_watch(serverch, G_IO_OUT, on_server_write, hostctx);
}


