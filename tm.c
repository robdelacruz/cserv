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

void create_login_ui();
gpointer TF_connect_server(gpointer data);

void CB_file_register(GtkWidget *w, gpointer data);
void CB_login_clicked(GtkWidget *w, gpointer data);
gpointer TF_login(gpointer data);

char *serverhost = "localhost";
char *serverport = "8000";
HostCtx hostctx;
GtkWidget *mainwin;
LoginUI loginui;

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
    gtk_widget_set_sensitive(loginbtn, FALSE);

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

    set_statusbar(GTK_STATUSBAR(statusbar), "Not connected");
    gtk_widget_show_all(mainwin);

    g_thread_new("TF_connect_server", TF_connect_server, new_data_args(2, statusbar, loginbtn));
}
void CB_file_register(GtkWidget *w, gpointer data) {
    clear_controls(mainwin);
}
void CB_login_clicked(GtkWidget *w, gpointer data) {
    gtk_widget_set_sensitive(loginui.loginbtn, FALSE);

    g_thread_new("TF_login", TF_login, NULL);
}
gboolean SF_update_statusbar(gpointer data) {
    set_statusbar_message(GTK_STATUSBAR(loginui.statusbar), 0, loginui.status.bs);

    return G_SOURCE_REMOVE;
}
gboolean SF_enable_loginbtn(gpointer data) {
    gtk_widget_set_sensitive(loginui.loginbtn, TRUE);

    return G_SOURCE_REMOVE;
}
gboolean SF_disable_loginbtn(gpointer data) {
    gtk_widget_set_sensitive(loginui.loginbtn, FALSE);

    return G_SOURCE_REMOVE;
}
gpointer TF_connect_server(gpointer data) {
    int z;

    StringAssignFormat(&loginui.status, "Connecting to %s...", serverhost);
    g_idle_add(SF_update_statusbar, NULL);

    struct addrinfo hints, *ai=NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo0(serverhost, serverport, &hints, &ai);
    if (z != 0) {
        StringAssignFormat(&loginui.status, "Can't reach server '%s'", serverhost);
        g_idle_add(SF_update_statusbar, NULL);
        goto ret;
    }
    int fd = socket0(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
    if (fd == -1) {
        StringAssignFormat(&loginui.status, "Can't create socket for '%s'", serverhost);
        g_idle_add(SF_update_statusbar, NULL);
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
                StringAssignFormat(&loginui.status, "Timeout connecting to '%s'", serverhost);
                g_idle_add(SF_update_statusbar, NULL);

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
    StringAssignFormat(&loginui.status, "Can't connect to '%s'", serverhost);
    g_idle_add(SF_update_statusbar, NULL);
    shutdown(fd, SHUT_RDWR);
    goto ret;

connected:
    if (hostctx.fd != -1)
        shutdown(hostctx.fd, SHUT_RDWR);
    hostctx.fd = fd;
    StringAssignFormat(&loginui.status, "Connected to %s", serverhost);
    g_idle_add(SF_update_statusbar, NULL);

ret:
    g_idle_add(SF_enable_loginbtn, NULL);

    if (ai)
        freeaddrinfo(ai);

    return NULL;
}

gboolean SF_show_connect_error(gpointer data) {
    GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Error connecting to %s", serverhost);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    return G_SOURCE_REMOVE;
}

gpointer TF_login(gpointer data) {
    if (hostctx.fd == -1)
        TF_connect_server(NULL);

    if (hostctx.fd == -1) {
        g_idle_add(SF_show_connect_error, NULL);
        return NULL;
    }

    fd_set readfds, writefds;
    fd_set readfds0, writefds0;
    int maxfd=0;
    int z;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(hostctx.fd, &readfds);
    maxfd = hostctx.fd;

    char *alias = (char *) gtk_entry_get_text(GTK_ENTRY(loginui.txtusername));
    char *password = (char *) gtk_entry_get_text(GTK_ENTRY(loginui.txtpassword));
    u8 msgno = LOGINMSG;
    NetPackLen(&hostctx.writebuf, "%b%s%s", msgno, alias, password);
    z = NetSend2(hostctx.fd, &hostctx.writebuf, &writefds, &maxfd);

    return NULL;
}

gpointer TF_select(gpointer data) {
    if (hostctx.fd == -1)
        TF_connect_server(NULL);

    if (hostctx.fd == -1) {
        g_idle_add(SF_show_connect_error, NULL);
        return NULL;
    }

    int z;
    fd_set readfds, writefds;
    fd_set readfds0, writefds0;
    int maxfd=0;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(hostctx.fd, &readfds);
    maxfd = hostctx.fd;

    while (1) {
        readfds0 = readfds;
        writefds0 = writefds;
        z = select(maxfd+1, &readfds0, &writefds0, NULL, NULL);
        if (z == 0) // timeout
            continue;
        if (z == -1 && errno == EINTR)
            continue;
        if (z == -1) {
            fprintf(stderr, "select(): %s\n", strerror(errno));
            break;
        }

        int read_eof = 0;
        if (FD_ISSET(hostctx.fd, &readfds0)) {
            if (NetRecv(hostctx.fd, &hostctx.readbuf) == 0)
                read_eof = 1;

            // Each message is a 16bit msglen value followed by msglen sequence of bytes.
            // A msglen of 0 means no more bytes remaining in the stream.

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
//                        on_host_recv_msg(&hostctx, readbuf->bs, hostctx.msglen, &writefds, &maxfd);
                        BufferShift(readbuf, hostctx.msglen);
                        hostctx.msglen = 0;
                        continue;
                    }
                    break;
                }
            }
            if (read_eof) {
//                on_host_eof(&hostctx);
                FD_CLR(hostctx.fd, &readfds);
                shutdown(hostctx.fd, SHUT_RD);
                hostctx.shut_rd = 1;

                // Close serverfd if no remaining reads and writes.
                if (hostctx.writebuf.len == 0) {
                    FD_CLR(hostctx.fd, &writefds);
                    shutdown(hostctx.fd, SHUT_WR);
                    break;
                }
            }
        }
        if (FD_ISSET(hostctx.fd, &writefds0)) {
            z = NetSend2(hostctx.fd, &hostctx.writebuf, &writefds, &maxfd);

            // Close serverfd if no remaining reads and writes.
            if (z == 0 && hostctx.shut_rd) {
                shutdown(hostctx.fd, SHUT_WR);
                break;
            }
        }
    }

    return NULL;
}

