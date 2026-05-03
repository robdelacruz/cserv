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
    GtkWidget *mainwin;
    GtkWidget *menubar;
    GtkWidget *framebox;
    GtkWidget *contentbox;
    GtkWidget *btnbox;
    gboolean btnbox_isenabled;
    GtkWidget *statusbar;
    String statusbar_text;
    GtkWidget *login_menuitem;
    GtkWidget *logout_menuitem;
    GtkWidget *register_menuitem;

    GtkWidget *login_username_entry;
    GtkWidget *login_password_entry;
    GtkWidget *login_remember_me_check;
    GtkWidget *login_loginbtn;

    GtkWidget *register_username_entry;
    GtkWidget *register_password_entry;
    GtkWidget *register_registerbtn;

    GtkWidget *contacts_list;

    GtkWidget *main_contacts_listbox;
    WindowID active_win;
} UILedger;

typedef struct {
    String username;
    String tok;
    gboolean is_loggedin;
} Session;

void create_main_frame();
void create_login_ui();
void create_register_ui();
void create_contacts_ui();

static gboolean SF_update_loginui(gpointer data);
static void update_connect_fail_ui();

static gpointer TF_connect(gpointer data);

void CB_login_menuitem(GtkWidget *w, gpointer data);
void CB_logout_menuitem(GtkWidget *w, gpointer data);
void CB_register_menuitem(GtkWidget *w, gpointer data);

void CB_login_clicked(GtkWidget *w, gpointer data);
void CB_register_clicked(GtkWidget *w, gpointer data);
gpointer TF_login(gpointer data);

static gpointer TF_refresh_contacts(gpointer data);

void on_recv_msg(HostCtx *hostctx, char *msgbytes, u16 len, fd_set *writefds, int *maxfd);
void on_read_eof(HostCtx *hostctx);
void on_server_close(HostCtx *hostctx);

gboolean SF_on_login_response(gpointer data);

char *serverhost = "localhost";
char *serverport = "8000";
HostCtx hostctx;
UILedger ui;
Session session;

fd_set readfds, writefds;
int maxfd=0;

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    if (argc > 1)
        serverhost = argv[1];
    if (argc > 2)
        serverport = argv[2];

    hostctx = HostCtxNew(-1);

    session.username = StringNew0();
    session.tok = StringNew0();
    session.is_loggedin = FALSE;

    create_main_frame();

    FILE *f = fopen("cserv_session.txt", "r");
    if (f != NULL) {
        char buf[512];
        fgets(buf, sizeof(buf), f);
        StringAssign(&session.tok, buf);
        fclose(f);
    }

    if (session.tok.len == 0) {
        create_login_ui();
    } else {
        create_contacts_ui();
    }

    g_thread_new("TF_connect", TF_connect, NULL);

    gtk_main();
    return 0;
}

void create_main_frame() {
    // Main window
    GtkWidget *mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    //gtk_window_set_default_size(GTK_WINDOW(mainwin), 400,600);
    //gtk_window_set_default_size(GTK_WINDOW(mainwin), 200,200);
    gtk_window_set_default_size(GTK_WINDOW(mainwin), 275,425);
    gtk_window_set_position(GTK_WINDOW(mainwin), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(mainwin), "Messenger");

    // Menu and statusbar
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *filemenu = gtk_menu_new();
    GtkWidget *filemi = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *login_menuitem = gtk_menu_item_new_with_mnemonic("_Login");
    GtkWidget *logout_menuitem = gtk_menu_item_new_with_mnemonic("Log_out");
    GtkWidget *register_menuitem = gtk_menu_item_new_with_mnemonic("_Register");
    GtkWidget *quitmi = gtk_menu_item_new_with_mnemonic("_Quit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(filemi), filemenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), login_menuitem);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), register_menuitem);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), logout_menuitem);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), quitmi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), filemi);
    GtkWidget *statusbar = gtk_statusbar_new();

    // Outer framebox and inner contentbox
    // Menus, content, and statusbar goes into framebox
    // UI widgets goes into contentbox
    GtkWidget *contentbox = gtk_vbox_new(FALSE, 0);
    GtkWidget *framebox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(mainwin), framebox);
    gtk_box_pack_start(GTK_BOX(framebox), menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(framebox), contentbox, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(framebox), statusbar, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(login_menuitem), "activate", G_CALLBACK(CB_login_menuitem), NULL);
    g_signal_connect(G_OBJECT(logout_menuitem), "activate", G_CALLBACK(CB_logout_menuitem), NULL);
    g_signal_connect(G_OBJECT(register_menuitem), "activate", G_CALLBACK(CB_register_menuitem), NULL);
    g_signal_connect(G_OBJECT(quitmi), "activate", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(mainwin, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    ui.mainwin = mainwin;
    ui.framebox = framebox;
    ui.menubar = menubar;
    ui.contentbox = contentbox;
    ui.statusbar = statusbar;
    ui.statusbar_text = StringNew0();
    ui.active_win = NONE;
    ui.btnbox = NULL;
    ui.login_menuitem = login_menuitem;
    ui.logout_menuitem = logout_menuitem;
    ui.register_menuitem = register_menuitem;
}

void create_login_ui() {
    clear_controls(ui.contentbox);

    gtk_window_set_title(GTK_WINDOW(ui.mainwin), "Login");

    // Login controls
    GtkWidget *lbl1 = create_label1("Username");
    GtkWidget *txtusername = gtk_entry_new();
    GtkWidget *lbl2 = create_label1("Password");
    GtkWidget *txtpassword = gtk_entry_new();
    GtkWidget *remember_me_check = gtk_check_button_new_with_mnemonic("_Remember me");
    GtkWidget *btnbox = gtk_vbox_new(FALSE, 0);
    GtkWidget *loginbtn = create_center_button("Login");

    set_widget_margins(ui.contentbox, 50, 50, 100, 100);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), lbl1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), txtusername, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), lbl2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), txtpassword, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), remember_me_check, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(btnbox), loginbtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), btnbox, FALSE, FALSE, 10);

    ui.active_win = LOGINWIN;
    ui.btnbox = btnbox;
    ui.login_username_entry = txtusername;
    ui.login_password_entry = txtpassword;
    ui.login_remember_me_check = remember_me_check;
    ui.login_loginbtn = loginbtn;
    ui.btnbox_isenabled = TRUE;

    gtk_widget_show_all(ui.mainwin);
    gtk_widget_hide(ui.login_menuitem);
    gtk_widget_hide(ui.logout_menuitem);
    gtk_widget_show(ui.register_menuitem);

    g_signal_connect(G_OBJECT(loginbtn), "clicked", G_CALLBACK(CB_login_clicked), NULL);
}

void create_register_ui() {
    clear_controls(ui.contentbox);

    gtk_window_set_title(GTK_WINDOW(ui.mainwin), "Register");

    // Register controls
    GtkWidget *lbl1 = create_label1("Username");
    GtkWidget *txtusername = gtk_entry_new();
    GtkWidget *lbl2 = create_label1("Password");
    GtkWidget *txtpassword = gtk_entry_new();
    GtkWidget *lbl3 = create_label1("Re-enter password");
    GtkWidget *txtpassword2 = gtk_entry_new();
    GtkWidget *btnbox = gtk_vbox_new(FALSE, 0);
    GtkWidget *registerbtn = create_center_button("Register");

    //set_widget_margins(ui.contentbox, 100, 100, 200, 200);
    set_widget_margins(ui.contentbox, 50, 50, 75, 75);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), lbl1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), txtusername, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), lbl2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), txtpassword, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), lbl3, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), txtpassword2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btnbox), registerbtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), btnbox, FALSE, FALSE, 10);

    ui.active_win = REGISTERWIN;
    ui.btnbox = btnbox;
    ui.register_username_entry = txtusername;
    ui.register_password_entry = txtpassword;
    ui.register_registerbtn = registerbtn;
    ui.btnbox_isenabled = TRUE;

    gtk_widget_show_all(ui.mainwin);
    gtk_widget_show(ui.login_menuitem);
    gtk_widget_hide(ui.logout_menuitem);
    gtk_widget_hide(ui.register_menuitem);

    g_signal_connect(G_OBJECT(registerbtn), "clicked", G_CALLBACK(CB_register_clicked), NULL);
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
    String s = StringNew0();

    clear_controls(ui.contentbox);

    gtk_window_set_title(GTK_WINDOW(ui.mainwin), "Contacts");

    StringAssign(&session.username, "robtwister");
    StringAssignFormat(&s, "<b><big>%s</big></b>", session.username.bs);
    GtkWidget *lbluser = create_label1("");
    gtk_label_set_markup(GTK_LABEL(lbluser), s.bs);

    GtkWidget *frame = gtk_frame_new("");
    GtkWidget *list = gtk_list_box_new();
    gtk_container_add(GTK_CONTAINER(frame), list);
    set_widget_margins(frame, 2,2, 2,2);

    set_widget_margins(ui.contentbox, 5, 5, 5, 5);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), lbluser, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(ui.contentbox), frame, TRUE, TRUE, 0);

    // abcuser, buddy123, hey_snoopy
    GtkWidget *lbl = create_contact_label("⚪ <span style=\"italic\" foreground=\"darkgrey\">abcuser</span>");
    gtk_container_add(GTK_CONTAINER(list), lbl);
    lbl = create_contact_label("🟢 buddy123");
    gtk_container_add(GTK_CONTAINER(list), lbl);
    lbl = create_contact_label("🟢 hey_snoopy");
    gtk_container_add(GTK_CONTAINER(list), lbl);

    ui.active_win = CONTACTSWIN;
    ui.btnbox = NULL;
    ui.contacts_list = list;

    gtk_widget_show_all(ui.mainwin);
    gtk_widget_hide(ui.login_menuitem);
    gtk_widget_show(ui.logout_menuitem);
    gtk_widget_hide(ui.register_menuitem);

    StringFree(&s);

    g_thread_new("TF_refresh_contacts", TF_refresh_contacts, NULL);
}

void CB_register_clicked(GtkWidget *w, gpointer data) {
}


void CB_login_menuitem(GtkWidget *w, gpointer data) {
    create_login_ui();
}
void CB_logout_menuitem(GtkWidget *w, gpointer data) {
    create_login_ui();
}
void CB_register_menuitem(GtkWidget *w, gpointer data) {
    create_register_ui();
}

void CB_login_clicked(GtkWidget *w, gpointer data) {
    g_thread_new("TF_login", TF_login, NULL);
}
static gboolean SF_update_loginui(gpointer data) {
    set_statusbar_message(GTK_STATUSBAR(ui.statusbar), 0, ui.statusbar_text.bs);
    if (ui.btnbox)
        gtk_widget_set_sensitive(ui.btnbox, ui.btnbox_isenabled);

    return G_SOURCE_REMOVE;
}
static void update_connect_fail_ui() {
    StringAssignFormat(&ui.statusbar_text, "Can't connect to '%s'", serverhost);
    ui.btnbox_isenabled = TRUE;
    g_idle_add(SF_update_loginui, NULL);
}

static gpointer TF_connect(gpointer data) {
    printf("TF_connect()\n");
    int z;

    struct addrinfo hints, *ai=NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    StringAssignFormat(&ui.statusbar_text, "Connecting to %s...'", serverhost);
    ui.btnbox_isenabled = FALSE;
    g_idle_add(SF_update_loginui, NULL);

    // getaddrinfo() will block if an unreachable serverhost (Ex. 'abcdomain') is given.
    z = getaddrinfo0(serverhost, serverport, &hints, &ai);
    if (z != 0) {
        StringAssignFormat(&ui.statusbar_text, "Can't reach server '%s'", serverhost);
        ui.btnbox_isenabled = TRUE;
        g_idle_add(SF_update_loginui, NULL);

        freeaddrinfo(ai);
        return NULL;
    }
    int fd = socket0(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol);
    if (fd == -1) {
        StringAssignFormat(&ui.statusbar_text, "Can't create socket for '%s'", serverhost);
        ui.btnbox_isenabled = TRUE;
        g_idle_add(SF_update_loginui, NULL);

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
                StringAssignFormat(&ui.statusbar_text, "Timeout connecting to '%s'", serverhost);
                ui.btnbox_isenabled = TRUE;
                g_idle_add(SF_update_loginui, NULL);

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
    StringAssignFormat(&ui.statusbar_text, "Connected to %s", serverhost);
    ui.btnbox_isenabled = TRUE;
    g_idle_add(SF_update_loginui, NULL);

    if (hostctx.fd != -1) {
        shutdown(hostctx.fd, SHUT_RDWR);
        close(hostctx.fd);
    }
    hostctx.fd = fd;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(fd, &readfds);
    maxfd = fd;

    GThreadFunc nextfunc = (GThreadFunc) data;
    if (nextfunc)
        g_thread_new("TF_connect_nextfunc", nextfunc, NULL);

    fd_set readfds0, writefds0;
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
        TF_connect(TF_login);
        return NULL;
    }

    StringAssignFormat(&ui.statusbar_text, "Logging in...");
    ui.btnbox_isenabled = FALSE;
    g_idle_add(SF_update_loginui, NULL);

    char *username = (char *) gtk_entry_get_text(GTK_ENTRY(ui.login_username_entry));
    char *password = (char *) gtk_entry_get_text(GTK_ENTRY(ui.login_password_entry));
    u8 msgno = LOGINUSER_REQUEST;
    NetPackLen(&hostctx.writebuf, "%b%s%s", msgno, username, password);
    int z = NetSend2(hostctx.fd, &hostctx.writebuf, &writefds, &maxfd);

    StringAssignFormat(&ui.statusbar_text, "Waiting for response...");
    ui.btnbox_isenabled = FALSE;
    g_idle_add(SF_update_loginui, NULL);

    return NULL;
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

void on_read_eof(HostCtx *hostctx) {
    printf("on_read_eof()\n");
}

void on_server_close(HostCtx *hostctx) {
    printf("on_server_close()\n");
}

gboolean SF_on_login_response(gpointer data) {
    LoginUserResponse *resp = data;

    StringAssign(&session.tok, resp->tok.bs);

    if (resp->retno == 0) {
        set_statusbar(GTK_STATUSBAR(ui.statusbar), "Logged on");

        gboolean is_remember_me = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui.login_remember_me_check));
        if (is_remember_me) {
            char *username = (char *) gtk_entry_get_text(GTK_ENTRY(ui.login_username_entry));
            char *password = (char *) gtk_entry_get_text(GTK_ENTRY(ui.login_password_entry));

            while (1) {
                FILE *f = fopen("cserv_session.txt", "w");
                if (f == NULL) break;
                fprintf(f, "%s", session.tok.bs);
                fclose(f);
                break;
            }
        } else {
            remove("cserv_session.txt");
        }

        create_contacts_ui();
    } else {
        GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(ui.mainwin), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", resp->errortext.bs);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);

        gtk_widget_set_sensitive(ui.btnbox, TRUE);
    }

    StringFree(&resp->tok);
    StringFree(&resp->errortext);
    free(resp);

    return G_SOURCE_REMOVE;
}

static gpointer TF_refresh_contacts(gpointer data) {
    return NULL;
}
