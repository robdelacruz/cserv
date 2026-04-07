#include <gtk/gtk.h>

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w), "Messenger");
    gtk_window_set_default_size(GTK_WINDOW(w), 230,300);
    gtk_window_set_position(GTK_WINDOW(w), GTK_WIN_POS_CENTER);

    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(w), vbox);

    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *filemenu = gtk_menu_new();
    GtkWidget *filemi = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *quitmi = gtk_menu_item_new_with_mnemonic("_Quit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(filemi), filemenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), quitmi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), filemi);

    GtkWidget *tbl = gtk_table_new(3, 2, TRUE);
    gtk_table_set_row_spacings(GTK_TABLE(tbl), 5);
    gtk_table_set_col_spacings(GTK_TABLE(tbl), 0);
    GtkWidget *username_lbl = gtk_label_new("Username");
    gtk_widget_set_halign(username_lbl, GTK_ALIGN_START);
    gtk_widget_set_valign(username_lbl, GTK_ALIGN_END);
    GtkWidget *username_entry = gtk_entry_new();
    GtkWidget *loginbtn = gtk_button_new_with_label("Login");
    gtk_widget_set_halign(loginbtn, GTK_ALIGN_CENTER);
    gtk_table_attach_defaults(GTK_TABLE(tbl), username_lbl, 0,1, 0,1);
    gtk_table_attach_defaults(GTK_TABLE(tbl), username_entry, 0,2, 1,2);
    gtk_table_attach_defaults(GTK_TABLE(tbl), loginbtn, 0,2, 2,3);

    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
    GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), tbl, FALSE, FALSE, 50);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 30);

    g_signal_connect(G_OBJECT(quitmi), "activate", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(w, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(w);

    gtk_main();
    return 0;
}

