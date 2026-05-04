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

typedef enum {
    NONE=0,
    LOGINWIN=1,
    REGISTERWIN=2,
    CONTACTSWIN=3
} WindowID;

typedef struct {
    WindowID active_win;
    GtkWidget *mainwin;
    GtkWidget *statusbar;
    String statusbar_text;
    gboolean isenabled;

    GtkWidget *login_txtusername;
    GtkWidget *login_txtpassword;
    GtkWidget *login_chkremember;

    GtkWidget *register_txtusername;
    GtkWidget *register_txtpassword;
    GtkWidget *register_txtpassword2;

    GtkWidget *contacts_list;
} UIState;

typedef struct {
    String username;
    String tok;
    gboolean is_loggedin;
} Session;

void create_login_ui();
void create_register_ui();
void create_contacts_ui();

static gboolean SF_update_window(gpointer data);
static void update_connect_fail_ui();

static gpointer TF_connect(gpointer data);

void CB_loginmi(GtkWidget *w, gpointer data);
void CB_logoutmi(GtkWidget *w, gpointer data);
void CB_registermi(GtkWidget *w, gpointer data);

void CB_login_clicked(GtkWidget *w, gpointer data);
void CB_register_clicked(GtkWidget *w, gpointer data);
gpointer TF_login(gpointer data);

static gpointer TF_refresh_contacts(gpointer data);

void on_recv_msg(char *msgbytes, u16 len);
void on_read_eof();
void on_server_close();

gboolean SF_on_login_response(gpointer data);

char *G_serverhost = "localhost";
char *G_serverport = "8000";
GtkWidget *G_mainwin;
UIState G_ui;
Session G_session;
HostCtx G_hostctx;

fd_set G_readfds, G_writefds;
int G_maxfd=0;

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    if (argc > 1)
        G_serverhost = argv[1];
    if (argc > 2)
        G_serverport = argv[2];

    G_hostctx = HostCtxNew(-1);

    G_session.username = StringNew0();
    G_session.tok = StringNew0();
    G_session.is_loggedin = FALSE;

    // Main window
    G_mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(G_mainwin), 275,425);
    gtk_window_set_position(GTK_WINDOW(G_mainwin), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(G_mainwin), "RobChat");
    g_signal_connect(G_mainwin, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    FILE *f = fopen("cserv_session.txt", "r");
    if (f != NULL) {
        char buf[512];
        fgets(buf, sizeof(buf), f);
        StringAssign(&G_session.tok, buf);
        fclose(f);
    }

    if (G_session.tok.len == 0) {
        create_login_ui();
    } else {
        create_contacts_ui();
    }

    g_thread_new("TF_connect", TF_connect, NULL);

    gtk_main();
    return 0;
}

void create_login_ui() {
    clear_controls(G_mainwin);

    // Menu and statusbar
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *filemenu = gtk_menu_new();
    GtkWidget *filemi = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *registermi = gtk_menu_item_new_with_mnemonic("_Register");
    GtkWidget *quitmi = gtk_menu_item_new_with_mnemonic("_Quit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(filemi), filemenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), registermi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), filemi);
    GtkWidget *statusbar = gtk_statusbar_new();

    // Login controls
    GtkWidget *lbl1 = create_label1("Username");
    GtkWidget *txtusername = gtk_entry_new();
    GtkWidget *lbl2 = create_label1("Password");
    GtkWidget *txtpassword = gtk_entry_new();
    GtkWidget *chkremember = gtk_check_button_new_with_mnemonic("_Remember me");
    GtkWidget *btnbox = gtk_vbox_new(FALSE, 0);
    GtkWidget *loginbtn = create_center_button("Login");

    GtkWidget *formbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), lbl1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), txtusername, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), lbl2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), txtpassword, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), chkremember, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(btnbox), loginbtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), btnbox, FALSE, FALSE, 10);
    gtk_widget_set_halign(formbox, GTK_ALIGN_CENTER);

    GtkWidget *contentbox = gtk_vbox_new(FALSE, 0);
    gtk_box_set_center_widget(GTK_BOX(contentbox), formbox);

    GtkWidget *framebox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(framebox), menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(framebox), contentbox, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(framebox), statusbar, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(G_mainwin), framebox);

    G_ui.statusbar = statusbar;
    G_ui.isenabled = TRUE;

    G_ui.active_win = LOGINWIN;
    G_ui.login_txtusername = txtusername;
    G_ui.login_txtpassword = txtpassword;
    G_ui.login_chkremember = chkremember;

    g_signal_connect(G_OBJECT(quitmi), "activate", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(registermi), "activate", G_CALLBACK(CB_registermi), NULL);
    g_signal_connect(G_OBJECT(loginbtn), "clicked", G_CALLBACK(CB_login_clicked), NULL);

    gtk_widget_set_sensitive(G_mainwin, TRUE);
    gtk_widget_show_all(G_mainwin);
}

void create_register_ui() {
    clear_controls(G_mainwin);

    // Menu and statusbar
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *filemenu = gtk_menu_new();
    GtkWidget *filemi = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *loginmi = gtk_menu_item_new_with_mnemonic("_Login");
    GtkWidget *quitmi = gtk_menu_item_new_with_mnemonic("_Quit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(filemi), filemenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), loginmi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), filemi);
    GtkWidget *statusbar = gtk_statusbar_new();

    // Register controls
    GtkWidget *lbl1 = create_label1("Username");
    GtkWidget *txtusername = gtk_entry_new();
    GtkWidget *lbl2 = create_label1("Password");
    GtkWidget *txtpassword = gtk_entry_new();
    GtkWidget *lbl3 = create_label1("Re-enter password");
    GtkWidget *txtpassword2 = gtk_entry_new();
    GtkWidget *btnbox = gtk_vbox_new(FALSE, 0);
    GtkWidget *registerbtn = create_center_button("Register");

    GtkWidget *formbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), lbl1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), txtusername, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), lbl2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), txtpassword, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), lbl3, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), txtpassword2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btnbox), registerbtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(formbox), btnbox, FALSE, FALSE, 10);
    gtk_widget_set_halign(formbox, GTK_ALIGN_CENTER);

    GtkWidget *contentbox = gtk_vbox_new(FALSE, 0);
    gtk_box_set_center_widget(GTK_BOX(contentbox), formbox);

    GtkWidget *framebox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(framebox), menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(framebox), contentbox, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(framebox), statusbar, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(G_mainwin), framebox);

    G_ui.statusbar = statusbar;
    G_ui.isenabled = TRUE;

    G_ui.active_win = REGISTERWIN;
    G_ui.register_txtusername = txtusername;
    G_ui.register_txtpassword = txtpassword;
    G_ui.register_txtpassword2 = txtpassword2;

    g_signal_connect(G_OBJECT(quitmi), "activate", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(loginmi), "activate", G_CALLBACK(CB_loginmi), NULL);
    g_signal_connect(G_OBJECT(registerbtn), "clicked", G_CALLBACK(CB_register_clicked), NULL);

    gtk_widget_set_sensitive(G_mainwin, TRUE);
    gtk_widget_show_all(G_mainwin);
}

GtkWidget *create_contact_label(char *username) {
    String s = StringNew0();
    StringAssignFormat(&s, "%s", username);

    GtkWidget *lbl = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(lbl), s.bs);

    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
    set_widget_margins(lbl, 8,8, 5,5);

    StringFree(&s);
    return lbl;
}

void create_contacts_ui() {
    clear_controls(G_mainwin);

    // Menu and statusbar
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *filemenu = gtk_menu_new();
    GtkWidget *filemi = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *logoutmi = gtk_menu_item_new_with_mnemonic("_Logout");
    GtkWidget *quitmi = gtk_menu_item_new_with_mnemonic("_Quit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(filemi), filemenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), logoutmi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), filemi);
    GtkWidget *statusbar = gtk_statusbar_new();

    // Contacts controls
    StringAssign(&G_session.username, "robtwister");
    String s = StringNew0();
    StringAssignFormat(&s, "<b><big>%s</big></b>", G_session.username.bs);
    GtkWidget *lbluser = create_label1("");
    gtk_label_set_markup(GTK_LABEL(lbluser), s.bs);
    StringFree(&s);

    GtkWidget *list = gtk_list_box_new();
    GtkWidget *frame = gtk_frame_new("");
    gtk_container_add(GTK_CONTAINER(frame), list);
    set_widget_margins(frame, 2,2, 2,2);

    GtkWidget *contentbox = gtk_vbox_new(FALSE, 0);
    set_widget_margins(contentbox, 5, 5, 5, 5);
    gtk_box_pack_start(GTK_BOX(contentbox), lbluser, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(contentbox), frame, TRUE, TRUE, 0);

    // abcuser, buddy123, hey_snoopy
    GtkWidget *lbl = create_contact_label("⚪ <span style=\"italic\" foreground=\"darkgrey\">abcuser</span>");
    gtk_container_add(GTK_CONTAINER(list), lbl);
    lbl = create_contact_label("🟢 buddy123");
    gtk_container_add(GTK_CONTAINER(list), lbl);
    lbl = create_contact_label("🟢 hey_snoopy");
    gtk_container_add(GTK_CONTAINER(list), lbl);

    GtkWidget *framebox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(framebox), menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(framebox), contentbox, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(framebox), statusbar, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(G_mainwin), framebox);

    G_ui.statusbar = statusbar;
    G_ui.isenabled = TRUE;

    G_ui.active_win = CONTACTSWIN;
    G_ui.contacts_list = list;

    StringFree(&s);

    g_signal_connect(G_OBJECT(quitmi), "activate", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(logoutmi), "activate", G_CALLBACK(CB_logoutmi), NULL);

    g_thread_new("TF_refresh_contacts", TF_refresh_contacts, NULL);

    gtk_widget_set_sensitive(G_mainwin, TRUE);
    gtk_widget_show_all(G_mainwin);
}

void CB_register_clicked(GtkWidget *w, gpointer data) {
}


void CB_loginmi(GtkWidget *w, gpointer data) {
    create_login_ui();
}
void CB_logoutmi(GtkWidget *w, gpointer data) {
    remove("cserv_session.txt");
    create_login_ui();
}
void CB_registermi(GtkWidget *w, gpointer data) {
    create_register_ui();
}

void CB_login_clicked(GtkWidget *w, gpointer data) {
    g_thread_new("TF_login", TF_login, NULL);
}
static gboolean SF_update_window(gpointer data) {
    set_statusbar_message(GTK_STATUSBAR(G_ui.statusbar), 0, G_ui.statusbar_text.bs);
    gtk_widget_set_sensitive(G_mainwin, G_ui.isenabled);

    return G_SOURCE_REMOVE;
}
static void update_connect_fail_ui() {
    StringAssignFormat(&G_ui.statusbar_text, "Can't connect to '%s'", G_serverhost);
    G_ui.isenabled = TRUE;
    g_idle_add(SF_update_window, NULL);
}

static gpointer TF_connect(gpointer data) {
    printf("TF_connect()\n");
    int z;

    struct addrinfo hints, *ai=NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    StringAssignFormat(&G_ui.statusbar_text, "Connecting to %s...'", G_serverhost);
    G_ui.isenabled = FALSE;
    g_idle_add(SF_update_window, NULL);

    // getaddrinfo() will block if an unreachable serverhost (Ex. 'abcdomain') is given.
    z = getaddrinfo0(G_serverhost, G_serverport, &hints, &ai);
    if (z != 0) {
        StringAssignFormat(&G_ui.statusbar_text, "Can't reach server '%s'", G_serverhost);
        G_ui.isenabled = TRUE;
        g_idle_add(SF_update_window, NULL);

        freeaddrinfo(ai);
        return NULL;
    }
    int fd = socket0(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
    if (fd == -1) {
        StringAssignFormat(&G_ui.statusbar_text, "Can't create socket for '%s'", G_serverhost);
        G_ui.isenabled = TRUE;
        g_idle_add(SF_update_window, NULL);

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
                StringAssignFormat(&G_ui.statusbar_text, "Timeout connecting to '%s'", G_serverhost);
                G_ui.isenabled = TRUE;
                g_idle_add(SF_update_window, NULL);

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
    StringAssignFormat(&G_ui.statusbar_text, "Connected to %s", G_serverhost);
    G_ui.isenabled = TRUE;
    g_idle_add(SF_update_window, NULL);

    if (G_hostctx.fd != -1) {
        shutdown(G_hostctx.fd, SHUT_RDWR);
        close(G_hostctx.fd);
    }
    G_hostctx.fd = fd;

    FD_ZERO(&G_readfds);
    FD_ZERO(&G_writefds);
    FD_SET(fd, &G_readfds);
    G_maxfd = fd;

    GThreadFunc nextfunc = (GThreadFunc) data;
    if (nextfunc)
        g_thread_new("TF_connect_nextfunc", nextfunc, NULL);

    fd_set readfds0, writefds0;
    while (1) {
        readfds0 = G_readfds;
        writefds0 = G_writefds;
        z = select(G_maxfd+1, &readfds0, &writefds0, NULL, NULL);
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
            if (NetRecv(fd, &G_hostctx.readbuf) == 0)
                read_eof = 1;

            // Each message is a 16bit msglen value followed by msglen sequence of bytes.
            // A msglen of 0 means no more bytes remaining in the stream.

            Buffer *readbuf = &G_hostctx.readbuf;
            while (1) {
                if (G_hostctx.msglen == 0) {
                    if (readbuf->len >= sizeof(u16)) {
                        u16 *bs = (u16 *) readbuf->bs;
                        G_hostctx.msglen = ntohs(*bs);
                        if (G_hostctx.msglen == 0) {
                            read_eof = 1;
                            break;
                        }
                        BufferShift(readbuf, sizeof(u16));
                        continue;
                    }
                    break;
                } else {
                    // Read msg body (msglen bytes)
                    if (readbuf->len >= G_hostctx.msglen) {
                        on_recv_msg(readbuf->bs, G_hostctx.msglen);
                        BufferShift(readbuf, G_hostctx.msglen);
                        G_hostctx.msglen = 0;
                        continue;
                    }
                    break;
                }
            }
            if (read_eof) {
                on_read_eof();
                FD_CLR(fd, &G_readfds);
                shutdown(fd, SHUT_RD);
                G_hostctx.shut_rd = 1;

                // Close serverfd if no remaining reads and writes.
                if (G_hostctx.writebuf.len == 0) {
                    FD_CLR(fd, &G_writefds);
                    shutdown(fd, SHUT_WR);
                    goto ret;
                }
            }
        }
        if (FD_ISSET(fd, &writefds0)) {
            z = NetSend2(fd, &G_hostctx.writebuf, &G_writefds, &G_maxfd);

            // Close serverfd if no remaining reads and writes.
            if (z == 0 && G_hostctx.shut_rd) {
                shutdown(fd, SHUT_WR);
                goto ret;
            }
        }
    }
ret:
    close(fd);
    on_server_close();

    // If control reached here, it means server socket was closed.
    G_hostctx.fd = -1;
    return NULL;
}

gboolean SF_show_connect_error(gpointer data) {
    GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Error connecting to %s", G_serverhost);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    return G_SOURCE_REMOVE;
}

gpointer TF_login(gpointer data) {
    if (G_hostctx.fd == -1) {
        TF_connect(TF_login);
        return NULL;
    }

    StringAssignFormat(&G_ui.statusbar_text, "Logging in...");
    G_ui.isenabled = FALSE;
    g_idle_add(SF_update_window, NULL);

    char *username = (char *) gtk_entry_get_text(GTK_ENTRY(G_ui.login_txtusername));
    char *password = (char *) gtk_entry_get_text(GTK_ENTRY(G_ui.login_txtpassword));
    u8 msgno = LOGINUSER_REQUEST;
    NetPackLen(&G_hostctx.writebuf, "%b%s%s", msgno, username, password);
    int z = NetSend2(G_hostctx.fd, &G_hostctx.writebuf, &G_writefds, &G_maxfd);

    StringAssignFormat(&G_ui.statusbar_text, "Waiting for response...");
    G_ui.isenabled = FALSE;
    g_idle_add(SF_update_window, NULL);

    return NULL;
}

void on_recv_msg(char *msgbytes, u16 len) {
    int z;
    u8 msgno = MSGNO(msgbytes);
    if (msgno == 0)
        return;

    // Skip over msgno (first byte)
    msgbytes++;
    len--;

    if (msgno == LOGINUSER_RESPONSE) {
        String tok = StringNew0();
        i8 retno;
        String errortext = StringNew0();

        NetUnpack(msgbytes, len, "%s%b%s", &tok, &retno, &errortext);
        printf("** LOGINUSER_RESPONSE tok: '%.*s' retno: %d errortext: '%.*s' **\n", tok.len, tok.bs, retno, errortext.len, errortext.bs);
        LoginUserResponse *resp = malloc(sizeof(LoginUserResponse));
        resp->msgno = msgno;
        resp->tok = tok;
        resp->retno = retno;
        resp->errortext = errortext;

        g_idle_add(SF_on_login_response, resp);
    }
}

void on_read_eof() {
    printf("on_read_eof()\n");
}

void on_server_close() {
    printf("on_server_close()\n");
}

gboolean SF_on_login_response(gpointer data) {
    LoginUserResponse *resp = data;

    StringAssign(&G_session.tok, resp->tok.bs);

    if (resp->retno == 0) {
        set_statusbar(GTK_STATUSBAR(G_ui.statusbar), "Logged on");

        gboolean is_remember_me = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(G_ui.login_chkremember));
        if (is_remember_me) {
            char *username = (char *) gtk_entry_get_text(GTK_ENTRY(G_ui.login_txtusername));
            char *password = (char *) gtk_entry_get_text(GTK_ENTRY(G_ui.login_txtpassword));

            while (1) {
                FILE *f = fopen("cserv_session.txt", "w");
                if (f == NULL) break;
                fprintf(f, "%s", G_session.tok.bs);
                fclose(f);
                break;
            }
        } else {
            remove("cserv_session.txt");
        }

        create_contacts_ui();
    } else {
        GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(G_mainwin), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", resp->errortext.bs);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        gtk_widget_set_sensitive(G_mainwin, TRUE);
    }

    StringFree(&resp->tok);
    StringFree(&resp->errortext);
    free(resp);

    return G_SOURCE_REMOVE;
}

static gpointer TF_refresh_contacts(gpointer data) {
    return NULL;
}
