#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <liblgx.h>
#include "lgxsession.h"


LGXSession *session = nullptr;

void startStream() {
    session->startCapture();

    g_idle_add([](gpointer) -> gboolean {
        session->run();
        return true;
    }, session);
}

void stopStream() {
    g_idle_remove_by_data(session);
    session->stopCapture();
}

static void activate(GtkApplication *app, gpointer userData) {
    GtkWidget *window;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW (window), "LGX/LGX2 Userspace Driver");
    gtk_window_set_default_size(GTK_WINDOW (window), 960, 540);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *drawingArea = gtk_drawing_area_new();
    gtk_widget_set_hexpand(drawingArea, true);
    gtk_widget_set_vexpand(drawingArea, true);
    gtk_widget_set_halign(drawingArea, GTK_ALIGN_FILL);
    gtk_widget_set_valign(drawingArea, GTK_ALIGN_FILL);

    GtkWidget *start = gtk_button_new_with_label("Start");
    GtkWidget *stop = gtk_button_new_with_label("Stop");

    gtk_box_append(GTK_BOX(hbox), start);
    gtk_box_append(GTK_BOX(hbox), stop);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(box), drawingArea);

    gtk_window_set_child(GTK_WINDOW(window), box);

    gtk_widget_show(window);

    session = new LGXSession(drawingArea);

    g_signal_connect(G_OBJECT(start), "clicked", startStream, nullptr);
    g_signal_connect(G_OBJECT(stop), "clicked", stopStream, nullptr);
}

//
//int main(int argc, char **argv) {
//    gtk_init();
//    GtkWindow *window;
//    window = (GtkWindow *) gtk_window_new();
//    gtk_window_set_title(window, "LGX/LGX2 Userspace Driver");
//    gtk_window_set_default_size(GTK_WINDOW(window), 960, 540);
//

//    gtk_widget_show(GTK_WIDGET(window));
//
//    g_signal_connect(G_OBJECT(window), "destroy", gtk_main_quit, nullptr);
//
//    LGXSession session{drawingArea};
//
////    session.startCapture();
//
//    gtk_main();
//
////    session.stopCapture();
//
//    return 0;
//}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("chrisAJS.lgxuserspace", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION (app), argc, argv);
    g_object_unref(app);
    if (session != nullptr) {
        session->stopCapture();
        delete session;
    }

    return status;
}