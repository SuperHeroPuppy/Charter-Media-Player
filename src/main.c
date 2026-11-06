#include <gtk/gtk.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *library;
    GtkWidget *video;
    GPtrArray *files;
    char *library_path;
} CharterApp;

static void save_library(CharterApp *app) {
    GKeyFile *key = g_key_file_new();
    char **uris = g_new0(char *, app->files->len + 1);
    for (guint i = 0; i < app->files->len; i++)
        uris[i] = g_file_get_uri(g_ptr_array_index(app->files, i));
    g_key_file_set_string_list(key, "library", "uris",
                               (const char *const *)uris, app->files->len);
    char *text = g_key_file_to_data(key, NULL, NULL);
    char *directory = g_path_get_dirname(app->library_path);
    g_mkdir_with_parents(directory, 0700);
    g_file_set_contents(app->library_path, text, -1, NULL);
    g_free(directory);
    g_free(text);
    g_strfreev(uris);
    g_key_file_unref(key);
}

static gboolean add_file(CharterApp *app, GFile *file) {
    for (guint i = 0; i < app->files->len; i++) {
        if (g_file_equal(g_ptr_array_index(app->files, i), file)) return FALSE;
    }
    g_ptr_array_add(app->files, g_object_ref(file));
    char *name = g_file_get_basename(file);
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *label = gtk_label_new(name);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_margin_start(label, 12);
    gtk_widget_set_margin_end(label, 12);
    gtk_widget_set_margin_top(label, 10);
    gtk_widget_set_margin_bottom(label, 10);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
    gtk_list_box_append(GTK_LIST_BOX(app->library), row);
    g_free(name);
    return TRUE;
}

static void load_library(CharterApp *app) {
    GKeyFile *key = g_key_file_new();
    if (g_key_file_load_from_file(key, app->library_path, G_KEY_FILE_NONE, NULL)) {
        gsize count = 0;
        char **uris = g_key_file_get_string_list(key, "library", "uris", &count, NULL);
        for (gsize i = 0; i < count; i++) {
            GFile *file = g_file_new_for_uri(uris[i]);
            add_file(app, file);
            g_object_unref(file);
        }
        g_strfreev(uris);
    }
    g_key_file_unref(key);
}

static void on_row_selected(GtkListBox *list, GtkListBoxRow *row, gpointer data) {
    (void)list;
    CharterApp *app = data;
    if (!row) return;
    int index = gtk_list_box_row_get_index(row);
    if (index < 0 || (guint)index >= app->files->len) return;
    gtk_video_set_file(GTK_VIDEO(app->video), g_ptr_array_index(app->files, index));
    GtkMediaStream *stream = gtk_video_get_media_stream(GTK_VIDEO(app->video));
    if (stream) gtk_media_stream_play(stream);
}

static void on_chooser_response(GtkNativeDialog *dialog, int response, gpointer data) {
    CharterApp *app = data;
    if (response == GTK_RESPONSE_ACCEPT) {
        GListModel *files = gtk_file_chooser_get_files(GTK_FILE_CHOOSER(dialog));
        gboolean changed = FALSE;
        for (guint i = 0; i < g_list_model_get_n_items(files); i++) {
            GFile *file = g_list_model_get_item(files, i);
            changed |= add_file(app, file);
            g_object_unref(file);
        }
        if (changed) save_library(app);
        g_object_unref(files);
    }
    g_object_unref(dialog);
}

static void on_import_clicked(GtkButton *button, gpointer data) {
    (void)button;
    CharterApp *app = data;
    GtkFileChooserNative *dialog = gtk_file_chooser_native_new(
        "Import media", GTK_WINDOW(app->window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Import", "Cancel");
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    g_signal_connect(dialog, "response", G_CALLBACK(on_chooser_response), app);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}

static void free_app(gpointer data) {
    CharterApp *app = data;
    g_ptr_array_unref(app->files);
    g_free(app->library_path);
    g_free(app);
}

static void on_activate(GtkApplication *application, gpointer data) {
    (void)data;
    CharterApp *app = g_new0(CharterApp, 1);
    app->files = g_ptr_array_new_with_free_func(g_object_unref);
    app->library_path = g_build_filename(g_get_user_data_dir(),
                                         "charter-media-player", "library.ini", NULL);
    g_object_set_data_full(G_OBJECT(application), "charter-state", app, free_app);

    app->window = gtk_application_window_new(application);
    gtk_window_set_title(GTK_WINDOW(app->window), "Charter Media Player");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1100, 700);
    GtkWidget *header = gtk_header_bar_new();
    GtkWidget *title = gtk_label_new("Charter Media Player");
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), title);
    GtkWidget *import = gtk_button_new_with_label("Import media");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), import);
    gtk_window_set_titlebar(GTK_WINDOW(app->window), header);
    g_signal_connect(import, "clicked", G_CALLBACK(on_import_clicked), app);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(scroll, 260, -1);
    app->library = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(app->library), GTK_SELECTION_SINGLE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), app->library);
    gtk_paned_set_start_child(GTK_PANED(paned), scroll);
    app->video = gtk_video_new();
    gtk_video_set_autoplay(GTK_VIDEO(app->video), TRUE);
    gtk_widget_set_hexpand(app->video, TRUE);
    gtk_widget_set_vexpand(app->video, TRUE);
    gtk_paned_set_end_child(GTK_PANED(paned), app->video);
    gtk_window_set_child(GTK_WINDOW(app->window), paned);
    g_signal_connect(app->library, "row-selected", G_CALLBACK(on_row_selected), app);
    load_library(app);
    gtk_window_present(GTK_WINDOW(app->window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("network.supers.charter",
                                               G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int result = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return result;
}
