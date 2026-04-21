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
    GtkWidget *win;
    GtkWidget *menubar;
    GtkWidget *txtusername;
    GtkWidget *txtpassword;
    GtkWidget *loginbtn;
    GtkWidget *statusbar;
    gboolean enabled;
    String status;
} LoginUI;

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

static GtkWidget *create_login_ui(char *serverhost, char *serverport, HostCtx *hostctx);

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    HostCtx hostctx = HostCtxNew(-1);
    char *serverhost = "localhost";
    char *serverport = "8000";
    if (argc > 1)
        serverhost = argv[1];
    if (argc > 2)
        serverport = argv[2];

    GtkWidget *loginwin = create_login_ui(serverhost, serverport, &hostctx);
    gtk_main();
    return 0;
}

static gpointer connect_server(gpointer data);
static void open_server_socket(char *serverhost, char *serverport, HostCtx *hostctx, LoginUI *loginui);
void login_clicked(GtkWidget *w, LoginUI *loginui);
void send_login(LoginUI *loginui);

static GtkWidget *create_login_ui(char *serverhost, char *serverport, HostCtx *hostctx) {
    GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w), "Messenger");
    gtk_window_set_default_size(GTK_WINDOW(w), 230,300);
    gtk_window_set_position(GTK_WINDOW(w), GTK_WIN_POS_CENTER);

    // Menu and statusbar
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *filemenu = gtk_menu_new();
    GtkWidget *filemi = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *quitmi = gtk_menu_item_new_with_mnemonic("_Quit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(filemi), filemenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), quitmi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), filemi);
    GtkWidget *statusbar = gtk_statusbar_new();

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
    gtk_box_pack_end(GTK_BOX(framebox), statusbar, FALSE, FALSE, 0);

    static LoginUI loginui;
    loginui.win = w;
    loginui.menubar = menubar;
    loginui.txtusername = txtusername;
    loginui.txtpassword = txtpassword;
    loginui.loginbtn = loginbtn;
    loginui.statusbar = statusbar;
    loginui.enabled = TRUE;
    loginui.status = StringNew("Not Connected");

    g_signal_connect(G_OBJECT(quitmi), "activate", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(w, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(loginbtn), "clicked", G_CALLBACK(login_clicked), &loginui);

    set_statusbar(GTK_STATUSBAR(statusbar), loginui.status.bs);
    gtk_widget_show_all(w);

    g_thread_new("threadlogin", connect_server, new_data_args(4, serverhost, serverport, hostctx, &loginui));
    return w;
}
void enable_loginui(LoginUI *loginui, gboolean f) {
    gtk_widget_set_sensitive(loginui->txtusername, f);
    gtk_widget_set_sensitive(loginui->txtpassword, f);
    gtk_widget_set_sensitive(loginui->loginbtn, f);
}
void login_clicked(GtkWidget *w, LoginUI *loginui) {
}
static gboolean update_statusbar(gpointer data) {
    void **args = data;

    GtkStatusbar *sb = args[0];
    char *s = args[1];
    set_statusbar_message(GTK_STATUSBAR(sb), 0, s);

    free(args);
    return G_SOURCE_REMOVE;
}
static gboolean enable_widget(gpointer data) {
    void **args = data;

    GtkWidget *w = args[0];
    gboolean f = GPOINTER_TO_INT(args[1]);
    gtk_widget_set_sensitive(w, f);

    free(args);
    return G_SOURCE_REMOVE;
}
static gpointer connect_server(gpointer data) {
    void **args = data;

    char *serverhost = args[0];
    char *serverport = args[1];
    HostCtx *hostctx = args[2];
    LoginUI *loginui = args[3];
    open_server_socket(serverhost, serverport, hostctx, loginui);

    free(args);
    return NULL;
}
static void open_server_socket(char *serverhost, char *serverport, HostCtx *hostctx, LoginUI *loginui) {
    int z;
    static String status = {0};

    StringAssignFormat(&status, "Connecting to %s...", serverhost);
    g_idle_add(update_statusbar, new_data_args(2, loginui->statusbar, status.bs));

    g_idle_add(enable_widget, new_data_args(2, loginui->loginbtn, FALSE));

    struct addrinfo hints, *ai=NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo0(serverhost, serverport, &hints, &ai);
    if (z != 0) {
        StringAssignFormat(&status, "Can't reach server '%s'", serverhost);
        g_idle_add(update_statusbar, new_data_args(2, loginui->statusbar, status.bs));
        goto ret;
    }
    int fd = socket0(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
    if (fd == -1) {
        StringAssignFormat(&status, "Can't create socket for '%s'", serverhost);
        g_idle_add(update_statusbar, new_data_args(2, loginui->statusbar, status.bs));
        goto ret;
    }
    int yes=1;
    z = setsockopt0(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    z = connect0(fd, ai->ai_addr, ai->ai_addrlen);
    if (z == -1 && errno == EINPROGRESS) {
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(fd, &writefds);

        while (1) {
            struct timeval timeout = {2, 0}; // timeout in 2 seconds
            int zz = select(fd+1, NULL, &writefds, NULL, &timeout);
            if (zz == 0) {
                // Handle timeout
                StringAssignFormat(&status, "Timeout connecting to '%s'", serverhost);
                g_idle_add(update_statusbar, new_data_args(2, loginui->statusbar, status.bs));

                shutdown(fd, SHUT_RDWR);
                goto ret;
            }
            if (zz == -1 && errno == EINTR)
                continue;
            if (zz == -1) {
                fprintf(stderr, "select(): %s\n", strerror(errno));
                goto connect_fail;
            }
            assert(zz > 0);
            break;
        }
        assert(FD_ISSET(fd, &writefds));

        int err=0;
        socklen_t errlen = sizeof(err);
        int zz = getsockopt0(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
        if (zz != 0) {
            fprintf(stderr, "nonblocking connect() error: getsockopt() failed\n");
            goto connect_fail;
        } else if (err != 0) {
            fprintf(stderr, "nonblocking connect() error: %s\n", strerror(err));
            goto connect_fail;
        } else {
            // Connection success
            goto connected;
        }
        goto ret;
    }
    if (z < 0)
        goto connect_fail;

connect_fail:
    StringAssignFormat(&status, "Can't connect to '%s'", serverhost);
    g_idle_add(update_statusbar, new_data_args(2, loginui->statusbar, status.bs));
    shutdown(fd, SHUT_RDWR);
    goto ret;

connected:
    if (hostctx->fd != -1) {
        shutdown(hostctx->fd, SHUT_RDWR);
        hostctx->fd = -1;
    }
    hostctx->fd = fd;
    StringAssignFormat(&status, "Connected to %s", serverhost);
    g_idle_add(update_statusbar, new_data_args(2, loginui->statusbar, status.bs));

ret:
    g_idle_add(enable_widget, new_data_args(2, loginui->loginbtn, TRUE));

    if (ai)
        freeaddrinfo(ai);
}
void send_login(LoginUI *loginui) {
    printf("send_login()\n");
}

