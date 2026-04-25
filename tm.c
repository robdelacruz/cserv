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
#include "uistuff.h"

typedef struct {
    GtkWidget *framebox;
    GtkWidget *menubar;
    GtkWidget *txtusername;
    GtkWidget *txtpassword;
    GtkWidget *loginbtn;
    GtkWidget *statusbar;
    String status;
} LoginUI;

typedef struct {
    GSourceFunc nextfunc;
} ConnectData;

void create_login_ui();
gboolean SF_connect_server(gpointer data);
gboolean IOF_connect_server1(GIOChannel *ch, GIOCondition cond, gpointer data);
gboolean SF_connect_server1_timeout(gpointer data);
gboolean SF_login(gpointer data);
gboolean IOF_login_out(GIOChannel *ch, GIOCondition cond, gpointer data);
gboolean IOF_login_in(GIOChannel *ch, GIOCondition cond, gpointer data);
void on_login_response(char *msgbytes, u16 len);

void CB_file_register(GtkWidget *w, gpointer data);
void CB_login_clicked(GtkWidget *w, gpointer data);

char *serverhost = "localhost";
char *serverport = "8000";
HostCtx hostctx;
int serverfd=-1;
GtkWidget *mainwin;
LoginUI loginui;
ConnectData connectdata;

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    if (argc > 1)
        serverhost = argv[1];
    if (argc > 2)
        serverport = argv[2];

    hostctx = HostCtxNew(-1);
    mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(mainwin), 400,600);
    gtk_window_set_position(GTK_WINDOW(mainwin), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(mainwin), "Messenger");

    create_login_ui();
    gtk_main();
    return 0;
}

void create_login_ui() {
    clear_controls(mainwin);

    gtk_window_set_title(GTK_WINDOW(mainwin), "Login");

    // Menu and statusbar
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *filemenu = gtk_menu_new();
    GtkWidget *filemi = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *registermi = gtk_menu_item_new_with_mnemonic("_Register New Account");
    GtkWidget *quitmi = gtk_menu_item_new_with_mnemonic("_Quit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(filemi), filemenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), registermi);
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
    set_widget_margins(vbox, 100, 100, 200, 200);
    gtk_box_pack_start(GTK_BOX(vbox), lbl1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), txtusername, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), txtpassword, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), loginbtn, FALSE, FALSE, 10);

    GtkWidget *framebox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(mainwin), framebox);
    gtk_box_pack_start(GTK_BOX(framebox), menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(framebox), vbox, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(framebox), statusbar, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(registermi), "activate", G_CALLBACK(CB_file_register), NULL);
    g_signal_connect(G_OBJECT(quitmi), "activate", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(mainwin, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    loginui.framebox = framebox;
    loginui.menubar = menubar;
    loginui.txtusername = txtusername;
    loginui.txtpassword = txtpassword;
    loginui.loginbtn = loginbtn;
    loginui.statusbar = statusbar;
    loginui.status = StringNew0();

    g_signal_connect(G_OBJECT(loginbtn), "clicked", G_CALLBACK(CB_login_clicked), NULL);

    set_statusbar(GTK_STATUSBAR(statusbar), "Connecting to %s...", serverhost);
    gtk_widget_show_all(mainwin);

    gtk_widget_set_sensitive(loginbtn, FALSE);
    gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Wait");

    connectdata.nextfunc = NULL;
    g_timeout_add(100, SF_connect_server, NULL);
}
void CB_file_register(GtkWidget *w, gpointer data) {
    clear_controls(mainwin);
}
void CB_login_clicked(GtkWidget *w, gpointer data) {
    gtk_widget_set_sensitive(loginui.loginbtn, FALSE);
    gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Wait");

    if (serverfd == -1) {
        set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Connecting to %s...", serverhost);
        connectdata.nextfunc = SF_login;
        g_idle_add(SF_connect_server, NULL);
    } else {
        g_idle_add(SF_login, NULL);
    }
}
gboolean SF_connect_server(gpointer data) {
    int z;
    int fd=-1;
    GIOChannel *ch=NULL;
    static guint timeout_sourceid = 0;
    static guint connect_sourceid = 0;

    struct addrinfo hints, *ai=NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo0(serverhost, serverport, &hints, &ai);
    if (z != 0) {
        set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Can't reach server %s", serverhost);
        gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
        gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
        goto ret;
    }
    fd = socket0(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
    if (fd == -1) {
        set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Can't create socket for %s", serverhost);
        gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
        gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
        goto ret;
    }
    int yes=1;
    z = setsockopt0(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    ch = g_io_channel_unix_new(fd);
    z = connect0(fd, ai->ai_addr, ai->ai_addrlen);
    if (z == -1 && errno == EINPROGRESS) {
        timeout_sourceid = g_timeout_add(2000, SF_connect_server1_timeout, &connect_sourceid);
        connect_sourceid = g_io_add_watch(ch, G_IO_OUT, IOF_connect_server1, &timeout_sourceid);
        goto ret;
    }
    if (z < 0) {
        set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Error connecting to %s", serverhost);
        gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
        gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
        goto ret;
    }

    // Connection success (without blocking)
    set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Connected to %s", serverhost);
    if (hostctx.fd != -1)
        shutdown(hostctx.fd, SHUT_RDWR);
    hostctx.fd = fd;
    serverfd = fd;
    g_io_channel_unref(ch);

    if (connectdata.nextfunc)
        g_idle_add(connectdata.nextfunc, NULL);

    gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
    gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");

ret:
    if (ai)
        freeaddrinfo(ai);
    return G_SOURCE_REMOVE;
}
gboolean SF_connect_server1_timeout(gpointer data) {
    // Cancel connect handler
    guint connect_sourceid = *((guint *)data);
    g_source_remove(connect_sourceid);

    set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Timeout connecting to %s", serverhost);

    gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
    gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
    return G_SOURCE_REMOVE;
}
gboolean IOF_connect_server1(GIOChannel *ch, GIOCondition cond, gpointer data) {
//    g_usleep(2000000);

    // Cancel timeout handler
    guint timeout_sourceid = *((guint *)data);
    g_source_remove(timeout_sourceid);

    int fd = g_io_channel_unix_get_fd(ch);

    int err=0;
    socklen_t errlen = sizeof(err);
    int zz = getsockopt0(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
    if (zz != 0) {
        fprintf(stderr, "nonblocking connect() error: getsockopt() failed\n");
        set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Error connecting to %s", serverhost);
        shutdown(fd, SHUT_RDWR);
        goto ret;
    } else if (err != 0) {
        fprintf(stderr, "nonblocking connect() error: %s\n", strerror(err));
        set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Error connecting to %s", serverhost);
        shutdown(fd, SHUT_RDWR);
        goto ret;
    }

    // Connection success
    set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Connected to %s", serverhost);
    hostctx.fd = fd;
    serverfd = fd;

    if (connectdata.nextfunc)
        g_idle_add(connectdata.nextfunc, NULL);

ret:
    gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
    gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
    g_io_channel_unref(ch);
    return G_SOURCE_REMOVE;
}

gboolean SF_login(gpointer data) {
    set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Sending login...");
    GIOChannel *ch=NULL;

    ch = g_io_channel_unix_new(serverfd);
    g_io_add_watch(ch, G_IO_IN, IOF_login_in, NULL);

    char *alias = (char *) gtk_entry_get_text(GTK_ENTRY(loginui.txtusername));
    char *password = (char *) gtk_entry_get_text(GTK_ENTRY(loginui.txtpassword));
    u8 msgno = LOGINMSG;
    NetPackLen(&hostctx.writebuf, "%b%s%s", msgno, alias, password);
    int z = NetSend(serverfd, &hostctx.writebuf);
    if (z == 1) {
        ch = g_io_channel_unix_new(serverfd);
        g_io_add_watch(ch, G_IO_OUT, IOF_login_out, NULL);
        return G_SOURCE_REMOVE;
    }
    if (z < 0) {
        set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Network error during login");
        BufferClear(&hostctx.writebuf);
        gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
        gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
        return G_SOURCE_REMOVE;
    }
    // All bytes sent
    set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Waiting for response");
    BufferClear(&hostctx.writebuf);
    gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
    gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
    return G_SOURCE_REMOVE;
}
gboolean IOF_login_out(GIOChannel *ch, GIOCondition cond, gpointer data) {
    int z = NetSend(serverfd, &hostctx.writebuf);
    if (z == 1)
        return G_SOURCE_CONTINUE;
    if (z < 0) {
        set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Network send error during login");
        BufferClear(&hostctx.writebuf);
        gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
        gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
        return G_SOURCE_REMOVE;
    }
    // All bytes sent
    set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Waiting for response");
    BufferClear(&hostctx.writebuf);
    gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
    gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
    return G_SOURCE_REMOVE;
}
gboolean IOF_login_in(GIOChannel *ch, GIOCondition cond, gpointer data) {
    int read_eof = 0;
    int z = NetRecv(serverfd, &hostctx.readbuf);
    if (z < 0) {
        set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Network receive error during login");
        BufferClear(&hostctx.readbuf);
        gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
        gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
        return G_SOURCE_REMOVE;
    }
    if (z == 0)
        read_eof = 1;

    Buffer *readbuf = &hostctx.readbuf;
    while (1) {
        if (hostctx.msglen == 0) {
            if (readbuf->len >= sizeof(u16)) {
                u16 *bs = (u16 *) readbuf->bs;
                hostctx.msglen = ntohs(*bs);
                if (hostctx.msglen == 0) {
                    read_eof = 1;
                    break;
                }
                BufferShift(readbuf, sizeof(u16));
                continue;
            }
            break;
        } else {
            // Read msg body (msglen bytes)
            if (readbuf->len >= hostctx.msglen) {
                on_login_response(readbuf->bs, hostctx.msglen);
                BufferShift(readbuf, hostctx.msglen);
                hostctx.msglen = 0;
                continue;
            }
            break;
        }
    }
//    if (read_eof) {
//        shutdown(serverfd, SHUT_RD);
//        if (hostctx.writebuf.len == 0)
//            shutdown(serverfd, SHUT_WR);
//    }
    if (read_eof)
        return G_SOURCE_REMOVE;
    return G_SOURCE_CONTINUE;
}
void on_login_response(char *msgbytes, u16 len) {
    set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Received login response");
    u8 msgno = MSGNO(msgbytes);
    printf("on_login_response() msgno: %d\n", msgno);
    if (msgno == 0)
        return;

    // Skip over msgno (first byte)
    msgbytes++;
    len--;

    if (msgno == LOGINRESPMSG) {
        String tok = StringNew0();
        i8 retno;
        String errorstr = StringNew0();

        NetUnpack(msgbytes, len, "%s%b%s", &tok, &retno, &errorstr);
        printf("** LOGINRESPMSG tok: '%.*s' retno: %d errorstr: '%.*s' **\n", tok.len, tok.bs, retno, errorstr.len, errorstr.bs);

        StringFree(&tok);
        StringFree(&errorstr);
    }
}

