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
    GtkWidget *bodybox;
    GtkWidget *txtusername;
    GtkWidget *txtpassword;
    GtkWidget *loginbtn;
    GtkWidget *statusbar;
    String status;
    gboolean loginbtn_active;
} LoginUI;

void create_login_ui();

static gboolean SF_update_loginui(gpointer data);
static void update_ui();
static void update_connect_fail_ui();

static gpointer TF_connect(gpointer data);
void select_wait(int fd);

void CB_file_register(GtkWidget *w, gpointer data);
void CB_login_clicked(GtkWidget *w, gpointer data);
gpointer TF_login(gpointer data);
gboolean SF_send_login(gpointer data);

void on_recv_msg(HostCtx *hostctx, char *msgbytes, u16 len, fd_set *writefds, int *maxfd);
void on_read_eof(HostCtx *hostctx);
void on_server_close(HostCtx *hostctx);

gboolean SF_on_login_response(gpointer data);

char *serverhost = "localhost";
char *serverport = "8000";
HostCtx hostctx;
GtkWidget *mainwin;
LoginUI loginui;
String logintok = {0};

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    if (argc > 1)
        serverhost = argv[1];
    if (argc > 2)
        serverport = argv[2];

    hostctx = HostCtxNew(-1);
    logintok = StringNew0();

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

    GtkWidget *bodybox = gtk_vbox_new(FALSE, 5);
    set_widget_margins(bodybox, 100, 100, 200, 200);
    gtk_box_pack_start(GTK_BOX(bodybox), lbl1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bodybox), txtusername, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bodybox), lbl2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bodybox), txtpassword, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bodybox), loginbtn, FALSE, FALSE, 10);
    gtk_widget_set_sensitive(loginbtn, FALSE);

    GtkWidget *framebox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(mainwin), framebox);
    gtk_box_pack_start(GTK_BOX(framebox), menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(framebox), bodybox, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(framebox), statusbar, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(registermi), "activate", G_CALLBACK(CB_file_register), NULL);
    g_signal_connect(G_OBJECT(quitmi), "activate", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(mainwin, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    loginui.framebox = framebox;
    loginui.menubar = menubar;
    loginui.bodybox = bodybox;
    loginui.txtusername = txtusername;
    loginui.txtpassword = txtpassword;
    loginui.loginbtn = loginbtn;
    loginui.statusbar = statusbar;
    loginui.status = StringNew0();
    loginui.loginbtn_active = FALSE;

    set_statusbar(GTK_STATUSBAR(statusbar), "Connecting to %s...", serverhost);
    gtk_button_set_label(GTK_BUTTON(loginbtn), "Wait");
    gtk_widget_set_sensitive(loginbtn, FALSE);

    gtk_widget_show_all(mainwin);

    g_signal_connect(G_OBJECT(loginbtn), "clicked", G_CALLBACK(CB_login_clicked), NULL);
    g_thread_new("TF_connect", TF_connect, NULL);
}
void CB_file_register(GtkWidget *w, gpointer data) {
    clear_controls(mainwin);
}
void CB_login_clicked(GtkWidget *w, gpointer data) {
    gtk_widget_set_sensitive(loginui.loginbtn, FALSE);

    g_thread_new("TF_login", TF_login, NULL);
}
static gboolean SF_update_loginui(gpointer data) {
    set_statusbar_message(GTK_STATUSBAR(loginui.statusbar), 0, loginui.status.bs);
    gtk_widget_set_sensitive(loginui.loginbtn, loginui.loginbtn_active);
    if (loginui.loginbtn_active)
        gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
    else
        gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Wait");

    return G_SOURCE_REMOVE;
}
static void update_ui() {
    g_idle_add(SF_update_loginui, NULL);
}
static void update_connect_fail_ui() {
    StringAssignFormat(&loginui.status, "Can't connect to '%s'", serverhost);
    loginui.loginbtn_active = TRUE;
    update_ui();
}

static gpointer TF_connect(gpointer data) {
    printf("TF_connect()\n");
    int z;

    struct addrinfo hints, *ai=NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // getaddrinfo() will block if an unreachable serverhost (Ex. 'abcdomain') is given.
    z = getaddrinfo0(serverhost, serverport, &hints, &ai);
    if (z != 0) {
        StringAssignFormat(&loginui.status, "Can't reach server '%s'", serverhost);
        loginui.loginbtn_active = TRUE;
        update_ui();

        freeaddrinfo(ai);
        return NULL;
    }
    int fd = socket0(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
    if (fd == -1) {
        StringAssignFormat(&loginui.status, "Can't create socket for '%s'", serverhost);
        loginui.loginbtn_active = TRUE;
        update_ui();

        freeaddrinfo(ai);
        return NULL;
    }
    int yes=1;
    setsockopt0(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    z = connect0(fd, ai->ai_addr, ai->ai_addrlen);
    freeaddrinfo(ai);
    if (z == 0)
        goto connected;
    if (z < 0 && errno != EINPROGRESS) {
        update_connect_fail_ui();
        shutdown(fd, SHUT_RDWR);
        close(fd);
        return NULL;
    }
    if (z == -1 && errno == EINPROGRESS) {
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(fd, &writefds);

        while (1) {
//            g_usleep(2000000);
            struct timeval timeout = {2, 0}; // timeout in 2 seconds
            int zz = select(fd+1, NULL, &writefds, NULL, &timeout);
            if (zz == 0) {
                // Handle timeout
                StringAssignFormat(&loginui.status, "Timeout connecting to '%s'", serverhost);
                loginui.loginbtn_active = TRUE;
                update_ui();
                shutdown(fd, SHUT_RDWR);
                close(fd);
                return NULL;
            }
            if (zz == -1 && errno == EINTR)
                continue;
            if (zz == -1) {
                fprintf(stderr, "select(): %s\n", strerror(errno));
                update_connect_fail_ui();
                shutdown(fd, SHUT_RDWR);
                close(fd);
                return NULL;
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
            update_connect_fail_ui();
            shutdown(fd, SHUT_RDWR);
            close(fd);
            return NULL;
        } else if (err != 0) {
            fprintf(stderr, "nonblocking connect() error: %s\n", strerror(err));
            update_connect_fail_ui();
            shutdown(fd, SHUT_RDWR);
            close(fd);
            return NULL;
        }
    }

connected:
    // Socket connected
    StringAssignFormat(&loginui.status, "Connected to %s", serverhost);
    loginui.loginbtn_active = TRUE;
    update_ui();

    if (hostctx.fd != -1) {
        shutdown(hostctx.fd, SHUT_RDWR);
        close(hostctx.fd);
    }
    hostctx.fd = fd;

    GThreadFunc nextfunc = (GThreadFunc) data;
    if (nextfunc)
        g_thread_new("TF_connect_nextfunc", nextfunc, NULL);

    select_wait(fd);

    // If control reached here, it means server socket was closed.
    hostctx.fd = -1;
    return NULL;
}

gboolean SF_show_connect_error(gpointer data) {
    GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Error connecting to %s", serverhost);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    return G_SOURCE_REMOVE;
}

gpointer TF_login(gpointer data) {
    if (hostctx.fd == -1) {
        StringAssignFormat(&loginui.status, "Connecting to %s...", serverhost);
        loginui.loginbtn_active = FALSE;
        update_ui();
        TF_connect(TF_login);
        return NULL;
    }

    StringAssignFormat(&loginui.status, "Logging in...");
    loginui.loginbtn_active = FALSE;
    update_ui();

    g_idle_add(SF_send_login, NULL);
    return NULL;
}

gboolean SF_send_login(gpointer data) {
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

    StringAssignFormat(&loginui.status, "Waiting for response...");
    loginui.loginbtn_active = FALSE;
    update_ui();

    return G_SOURCE_REMOVE;
}

void select_wait(int fd) {
    int z;
    fd_set readfds, writefds;
    fd_set readfds0, writefds0;
    int maxfd=0;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(fd, &readfds);
    maxfd = fd;

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
        if (FD_ISSET(fd, &readfds0)) {
            if (NetRecv(fd, &hostctx.readbuf) == 0)
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
                        on_recv_msg(&hostctx, readbuf->bs, hostctx.msglen, &writefds, &maxfd);
                        BufferShift(readbuf, hostctx.msglen);
                        hostctx.msglen = 0;
                        continue;
                    }
                    break;
                }
            }
            if (read_eof) {
                on_read_eof(&hostctx);
                FD_CLR(fd, &readfds);
                shutdown(fd, SHUT_RD);
                hostctx.shut_rd = 1;

                // Close serverfd if no remaining reads and writes.
                if (hostctx.writebuf.len == 0) {
                    FD_CLR(fd, &writefds);
                    shutdown(fd, SHUT_WR);
                    goto ret;
                }
            }
        }
        if (FD_ISSET(fd, &writefds0)) {
            z = NetSend2(fd, &hostctx.writebuf, &writefds, &maxfd);

            // Close serverfd if no remaining reads and writes.
            if (z == 0 && hostctx.shut_rd) {
                shutdown(fd, SHUT_WR);
                goto ret;
            }
        }
    }

ret:
    close(fd);
    on_server_close(&hostctx);
}

void on_recv_msg(HostCtx *hostctx, char *msgbytes, u16 len, fd_set *writefds, int *maxfd) {
    int z;
    u8 msgno = MSGNO(msgbytes);
    printf("on_recv_msg() msgno: %d\n", msgno);
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
        LoginResponse *resp = malloc(sizeof(LoginResponse));
        resp->msgno = msgno;
        resp->tok = tok;
        resp->retno = retno;
        resp->errorstr = errorstr;

        g_idle_add(SF_on_login_response, resp);
    }
}

void on_read_eof(HostCtx *hostctx) {
    printf("on_read_eof()\n");
}

void on_server_close(HostCtx *hostctx) {
    printf("on_server_close()\n");
}

gboolean SF_on_login_response(gpointer data) {
    LoginResponse *resp = data;

    logintok = StringDup(resp->tok);

    if (resp->retno == 0) {
        set_statusbar(GTK_STATUSBAR(loginui.statusbar), "Logged on");
        clear_controls(loginui.bodybox);
    } else {
        GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(mainwin), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", resp->errorstr.bs);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);

        gtk_button_set_label(GTK_BUTTON(loginui.loginbtn), "Login");
        gtk_widget_set_sensitive(loginui.loginbtn, TRUE);
    }

    return G_SOURCE_REMOVE;
}

