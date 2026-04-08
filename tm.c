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

typedef struct {
    int serverfd;
    GIOChannel *serverch;
    HostCtx hostctx;
    GtkWidget *txtusername;
    GtkWidget *txtpassword;
} UICtx;

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

void create_login_ui(UICtx *uictx);
gboolean on_serverfd_event(GIOChannel *ch, GIOCondition iocond, gpointer data);

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

    GIOChannel *serverch = g_io_channel_unix_new(serverfd);
    g_io_add_watch(serverch, G_IO_IN|G_IO_ERR|G_IO_HUP, on_serverfd_event, NULL);

    HostCtx hostctx = HostCtxNew(serverfd);

    UICtx uictx;
    uictx.serverfd = serverfd;
    uictx.serverch = serverch;
    uictx.hostctx = hostctx;

    create_login_ui(&uictx);
    gtk_main();
    return 0;
}

gboolean on_serverfd_event(GIOChannel *ch, GIOCondition iocond, gpointer data) {
    int fd = g_io_channel_unix_get_fd(ch);
    if (iocond == G_IO_IN)
        printf("server fd %d: data available for read\n", fd);
    else if (iocond == G_IO_OUT)
        printf("server fd %d: data can be sent\n", fd);
    else if (iocond == G_IO_HUP)
        printf("server fd %d: hung up\n", fd);
    else if (iocond == G_IO_ERR)
        printf("server fd %d: error\n", fd);
    return TRUE;
}

void login_clicked(GtkWidget *w, UICtx *uictx) {
    printf("login_clicked() serverfd: %d\n", uictx->serverfd);
    printf("txtusername: '%s'\n", gtk_entry_get_text(GTK_ENTRY(uictx->txtusername)));
    printf("txtpassword: '%s'\n", gtk_entry_get_text(GTK_ENTRY(uictx->txtpassword)));
}

void create_login_ui(UICtx *uictx) {
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

    uictx->txtusername = txtusername;
    uictx->txtpassword = txtpassword;
    g_signal_connect(G_OBJECT(loginbtn), "clicked", G_CALLBACK(login_clicked), uictx);

    gtk_widget_show_all(w);
}

