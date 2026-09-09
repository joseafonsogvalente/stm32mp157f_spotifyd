#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>
#include <string.h>

#define MPRIS_PREFIX "org.mpris.MediaPlayer2.spotifyd"
#define MPRIS_PATH "/org/mpris/MediaPlayer2"
#define MPRIS_PLAYER_IFACE "org.mpris.MediaPlayer2.Player"
#define DBUS_PROPERTIES_IFACE "org.freedesktop.DBus.Properties"

#define ARTWORK_FILE "/tmp/spotify-artwork.jpg"

typedef enum {
    BUTTON_PREVIOUS,
    BUTTON_PLAY_PAUSE,
    BUTTON_NEXT
} MediaButtonType;

typedef struct {
    GtkWidget *title_label;
    GtkWidget *artist_label;
    GtkWidget *status_label;
    GtkWidget *progress;

    GtkWidget *play_button;

    GtkWidget *art_image;
    GtkWidget *art_placeholder;

    GDBusProxy *player;
    GDBusProxy *properties;

    gchar *service_name;
    gchar *art_url;

    gboolean playing;

    guint reconnect_delay;
} AppData;

typedef struct {
    MediaButtonType type;
    AppData *data;
} MediaButtonData;


/* =========================================================
 * MPRIS PROPERTY
 * ========================================================= */

static GVariant *get_mpris_property(
    AppData *data,
    const gchar *property
)
{
    if (!data->properties)
        return NULL;

    GError *error = NULL;

    GVariant *result =
        g_dbus_proxy_call_sync(
            data->properties,
            "Get",
            g_variant_new(
                "(ss)",
                MPRIS_PLAYER_IFACE,
                property
            ),
            G_DBUS_CALL_FLAGS_NONE,
            1000,
            NULL,
            &error
        );

    if (!result) {
        if (error)
            g_error_free(error);

        return NULL;
    }

    if (error) {
        g_error_free(error);
        g_variant_unref(result);
        return NULL;
    }

    GVariant *value = NULL;

    g_variant_get(
        result,
        "(v)",
        &value
    );

    g_variant_unref(result);

    return value;
}


/* =========================================================
 * STRING PROPERTY
 * ========================================================= */

static gchar *get_string_property(
    AppData *data,
    const gchar *property
)
{
    GVariant *value =
        get_mpris_property(
            data,
            property
        );

    if (!value)
        return NULL;

    gchar *result = NULL;

    if (
        g_variant_is_of_type(
            value,
            G_VARIANT_TYPE_STRING
        )
    ) {
        result =
            g_variant_dup_string(
                value,
                NULL
            );
    }

    g_variant_unref(value);

    return result;
}


/* =========================================================
 * CLEAR ARTWORK
 * ========================================================= */

static void clear_artwork(
    AppData *data
)
{
    if (data->art_image) {
        gtk_image_clear(
            GTK_IMAGE(data->art_image)
        );

        gtk_widget_hide(
            data->art_image
        );
    }

    if (data->art_placeholder)
        gtk_widget_show(
            data->art_placeholder
        );
}


/* =========================================================
 * RESET UI
 *
 * Used when spotifyd is no longer the active playback
 * device.
 * ========================================================= */

static void reset_ui(
    AppData *data
)
{
    gtk_label_set_text(
        GTK_LABEL(data->title_label),
        "Waiting for Spotify..."
    );

    gtk_label_set_text(
        GTK_LABEL(data->artist_label),
        ""
    );

    gtk_label_set_text(
        GTK_LABEL(data->status_label),
        "WAITING FOR SPOTIFY"
    );

    gtk_progress_bar_set_fraction(
        GTK_PROGRESS_BAR(data->progress),
        0.0
    );

    data->playing = FALSE;

    if (data->play_button)
        gtk_widget_queue_draw(
            data->play_button
        );

    clear_artwork(data);

    g_clear_pointer(
        &data->art_url,
        g_free
    );
}


/* =========================================================
 * LOAD ARTWORK
 * ========================================================= */

static gboolean load_artwork(
    AppData *data,
    const gchar *url
)
{
    if (!url || !url[0])
        return FALSE;

    gchar *output_file =
        g_strdup(ARTWORK_FILE);

    gchar *argv[] = {
        (gchar *)"curl",
        (gchar *)"-L",
        (gchar *)"--fail",
        (gchar *)"--silent",
        (gchar *)"--show-error",
        (gchar *)"--max-time",
        (gchar *)"5",
        (gchar *)"-o",
        output_file,
        (gchar *)url,
        NULL
    };

    GError *error = NULL;
    gint exit_status = -1;

    gboolean spawned =
        g_spawn_sync(
            NULL,
            argv,
            NULL,
            G_SPAWN_SEARCH_PATH,
            NULL,
            NULL,
            NULL,
            NULL,
            &exit_status,
            &error
        );

    g_free(output_file);

    if (!spawned) {
        if (error)
            g_error_free(error);

        return FALSE;
    }

    if (error) {
        g_error_free(error);
        return FALSE;
    }

    if (exit_status != 0)
        return FALSE;

    GError *pixbuf_error = NULL;

    GdkPixbuf *original =
        gdk_pixbuf_new_from_file(
            ARTWORK_FILE,
            &pixbuf_error
        );

    if (!original) {
        if (pixbuf_error)
            g_error_free(pixbuf_error);

        return FALSE;
    }

    gint width =
        gdk_pixbuf_get_width(
            original
        );

    gint height =
        gdk_pixbuf_get_height(
            original
        );

    gint square =
        MIN(width, height);

    gint x =
        (width - square) / 2;

    gint y =
        (height - square) / 2;

    GdkPixbuf *cropped =
        gdk_pixbuf_new_subpixbuf(
            original,
            x,
            y,
            square,
            square
        );

    if (!cropped) {
        g_object_unref(original);
        return FALSE;
    }

    GdkPixbuf *scaled =
        gdk_pixbuf_scale_simple(
            cropped,
            260,
            260,
            GDK_INTERP_BILINEAR
        );

    gboolean success =
        scaled != NULL;

    if (scaled) {

        gtk_image_set_from_pixbuf(
            GTK_IMAGE(data->art_image),
            scaled
        );

        gtk_widget_set_size_request(
            data->art_image,
            260,
            260
        );

        gtk_widget_show(
            data->art_image
        );

        gtk_widget_hide(
            data->art_placeholder
        );

        gtk_widget_queue_draw(
            data->art_image
        );

        g_object_unref(scaled);
    }

    g_object_unref(cropped);
    g_object_unref(original);

    return success;
}


/* =========================================================
 * UPDATE ARTWORK
 * ========================================================= */

static void update_artwork(
    AppData *data,
    GVariant *metadata
)
{
    if (!metadata)
        return;

    GVariant *art_value =
        g_variant_lookup_value(
            metadata,
            "mpris:artUrl",
            G_VARIANT_TYPE_STRING
        );

    if (!art_value) {

        clear_artwork(data);

        g_clear_pointer(
            &data->art_url,
            g_free
        );

        return;
    }

    const gchar *url =
        g_variant_get_string(
            art_value,
            NULL
        );

    if (
        data->art_url &&
        g_strcmp0(
            data->art_url,
            url
        ) == 0
    ) {
        g_variant_unref(art_value);
        return;
    }

    g_free(data->art_url);

    data->art_url =
        g_strdup(url);

    gboolean success =
        load_artwork(
            data,
            url
        );

    if (!success)
        clear_artwork(data);

    g_variant_unref(art_value);
}


/* =========================================================
 * UPDATE PROGRESS
 * ========================================================= */

static void update_progress(
    AppData *data
)
{
    if (!data->progress)
        return;

    GVariant *position_value =
        get_mpris_property(
            data,
            "Position"
        );

    GVariant *metadata =
        get_mpris_property(
            data,
            "Metadata"
        );

    if (!metadata) {

        if (position_value)
            g_variant_unref(position_value);

        return;
    }

    gint64 position = 0;

    if (
        position_value &&
        g_variant_is_of_type(
            position_value,
            G_VARIANT_TYPE_INT64
        )
    ) {
        position =
            g_variant_get_int64(
                position_value
            );
    }

    if (position_value)
        g_variant_unref(position_value);

    GVariant *length_value =
        g_variant_lookup_value(
            metadata,
            "mpris:length",
            G_VARIANT_TYPE_INT64
        );

    if (length_value) {

        gint64 length =
            g_variant_get_int64(
                length_value
            );

        if (length > 0) {

            gdouble fraction =
                (gdouble)position /
                (gdouble)length;

            if (fraction < 0.0)
                fraction = 0.0;

            if (fraction > 1.0)
                fraction = 1.0;

            gtk_progress_bar_set_fraction(
                GTK_PROGRESS_BAR(
                    data->progress
                ),
                fraction
            );
        }

        g_variant_unref(length_value);
    }

    g_variant_unref(metadata);
}


/* =========================================================
 * DRAW MEDIA BUTTON
 * ========================================================= */

static gboolean media_button_draw(
    GtkWidget *widget,
    cairo_t *cr,
    gpointer user_data
)
{
    MediaButtonData *button_data =
        user_data;

    MediaButtonType type =
        button_data->type;

    AppData *data =
        button_data->data;

    GtkAllocation allocation;

    gtk_widget_get_allocation(
        widget,
        &allocation
    );

    gdouble width =
        allocation.width;

    gdouble height =
        allocation.height;

    gdouble cx =
        width / 2.0;

    gdouble cy =
        height / 2.0;

    gboolean hovered =
        gtk_widget_get_state_flags(widget) &
        GTK_STATE_FLAG_PRELIGHT;

    gboolean active =
        gtk_widget_get_state_flags(widget) &
        GTK_STATE_FLAG_ACTIVE;


    /* -----------------------------------------------------
     * PLAY BUTTON
     * ----------------------------------------------------- */

    if (type == BUTTON_PLAY_PAUSE) {

        if (hovered) {

            cairo_set_source_rgb(
                cr,
                0.15,
                0.95,
                0.45
            );

        } else {

            cairo_set_source_rgb(
                cr,
                0.118,
                0.843,
                0.376
            );
        }

    } else {

        if (active) {

            cairo_set_source_rgb(
                cr,
                0.25,
                0.25,
                0.25
            );

        } else if (hovered) {

            cairo_set_source_rgb(
                cr,
                0.20,
                0.20,
                0.20
            );

        } else {

            cairo_set_source_rgb(
                cr,
                0.125,
                0.125,
                0.125
            );
        }
    }


    cairo_arc(
        cr,
        cx,
        cy,
        MIN(width, height) / 2.0,
        0,
        2 * G_PI
    );

    cairo_fill(cr);


    /* -----------------------------------------------------
     * ICON COLOR
     * ----------------------------------------------------- */

    if (type == BUTTON_PLAY_PAUSE) {

        cairo_set_source_rgb(
            cr,
            0.03,
            0.03,
            0.03
        );

    } else {

        cairo_set_source_rgb(
            cr,
            1.0,
            1.0,
            1.0
        );
    }


    /* -----------------------------------------------------
     * PLAY
     * ----------------------------------------------------- */

    if (
        type == BUTTON_PLAY_PAUSE &&
        !data->playing
    ) {

        gdouble size = 21.0;

        cairo_move_to(
            cr,
            cx - size * 0.30,
            cy - size * 0.50
        );

        cairo_line_to(
            cr,
            cx + size * 0.55,
            cy
        );

        cairo_line_to(
            cr,
            cx - size * 0.30,
            cy + size * 0.50
        );

        cairo_close_path(cr);

        cairo_fill(cr);

        return TRUE;
    }


    /* -----------------------------------------------------
     * PAUSE
     * ----------------------------------------------------- */

    if (
        type == BUTTON_PLAY_PAUSE &&
        data->playing
    ) {

        gdouble bar_width = 5.0;
        gdouble bar_height = 21.0;
        gdouble gap = 5.0;

        cairo_rectangle(
            cr,
            cx - gap / 2.0 - bar_width,
            cy - bar_height / 2.0,
            bar_width,
            bar_height
        );

        cairo_rectangle(
            cr,
            cx + gap / 2.0,
            cy - bar_height / 2.0,
            bar_width,
            bar_height
        );

        cairo_fill(cr);

        return TRUE;
    }


    /* -----------------------------------------------------
     * PREVIOUS
     * ----------------------------------------------------- */

    if (type == BUTTON_PREVIOUS) {

        gdouble size = 17.0;

        cairo_set_line_width(
            cr,
            3.0
        );

        cairo_move_to(
            cr,
            cx - 8,
            cy - size / 2.0
        );

        cairo_line_to(
            cr,
            cx - 8,
            cy + size / 2.0
        );

        cairo_stroke(cr);


        cairo_move_to(
            cr,
            cx - 4,
            cy
        );

        cairo_line_to(
            cr,
            cx + 5,
            cy - size / 2.0
        );

        cairo_line_to(
            cr,
            cx + 5,
            cy + size / 2.0
        );

        cairo_close_path(cr);

        cairo_fill(cr);


        cairo_move_to(
            cr,
            cx + 3,
            cy
        );

        cairo_line_to(
            cr,
            cx + 12,
            cy - size / 2.0
        );

        cairo_line_to(
            cr,
            cx + 12,
            cy + size / 2.0
        );

        cairo_close_path(cr);

        cairo_fill(cr);

        return TRUE;
    }


    /* -----------------------------------------------------
     * NEXT
     * ----------------------------------------------------- */

    if (type == BUTTON_NEXT) {

        gdouble size = 17.0;

        cairo_move_to(
            cr,
            cx - 12,
            cy - size / 2.0
        );

        cairo_line_to(
            cr,
            cx - 12,
            cy + size / 2.0
        );

        cairo_line_to(
            cr,
            cx - 3,
            cy
        );

        cairo_close_path(cr);

        cairo_fill(cr);


        cairo_move_to(
            cr,
            cx - 4,
            cy - size / 2.0
        );

        cairo_line_to(
            cr,
            cx - 4,
            cy + size / 2.0
        );

        cairo_line_to(
            cr,
            cx + 5,
            cy
        );

        cairo_close_path(cr);

        cairo_fill(cr);


        cairo_set_line_width(
            cr,
            3.0
        );

        cairo_move_to(
            cr,
            cx + 8,
            cy - size / 2.0
        );

        cairo_line_to(
            cr,
            cx + 8,
            cy + size / 2.0
        );

        cairo_stroke(cr);

        return TRUE;
    }

    return TRUE;
}


/* =========================================================
 * CREATE MEDIA BUTTON
 * ========================================================= */

static GtkWidget *create_media_button(
    MediaButtonType type,
    AppData *data,
    gint size
)
{
    GtkWidget *drawing_area =
        gtk_drawing_area_new();

    gtk_widget_set_size_request(
        drawing_area,
        size,
        size
    );

    gtk_widget_set_events(
        drawing_area,
        GDK_BUTTON_PRESS_MASK |
        GDK_ENTER_NOTIFY_MASK |
        GDK_LEAVE_NOTIFY_MASK
    );

    MediaButtonData *button_data =
        g_new0(
            MediaButtonData,
            1
        );

    button_data->type =
        type;

    button_data->data =
        data;

    g_signal_connect_data(
        drawing_area,
        "draw",
        G_CALLBACK(media_button_draw),
        button_data,
        (GClosureNotify)g_free,
        0
    );

    return drawing_area;
}


/* =========================================================
 * BUTTON PRESS
 * ========================================================= */

static gboolean media_button_press(
    GtkWidget *widget,
    GdkEventButton *event,
    gpointer user_data
)
{
    (void)widget;
    (void)event;

    MediaButtonData *button_data =
        user_data;

    AppData *data =
        button_data->data;

    if (!data->player)
        return TRUE;


    if (
        button_data->type ==
        BUTTON_PREVIOUS
    ) {

        g_dbus_proxy_call_sync(
            data->player,
            "Previous",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            1000,
            NULL,
            NULL
        );

    } else if (
        button_data->type ==
        BUTTON_PLAY_PAUSE
    ) {

        g_dbus_proxy_call_sync(
            data->player,
            "PlayPause",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            1000,
            NULL,
            NULL
        );

    } else if (
        button_data->type ==
        BUTTON_NEXT
    ) {

        g_dbus_proxy_call_sync(
            data->player,
            "Next",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            1000,
            NULL,
            NULL
        );
    }

    return TRUE;
}


/* =========================================================
 * UPDATE UI
 * ========================================================= */

static void update_ui(
    AppData *data
)
{
    if (!data->properties)
        return;


    /* -----------------------------------------------------
     * PLAYBACK STATUS FIRST
     *
     * This is important. When Spotify is moved to another
     * device, spotifyd can remain registered on D-Bus while
     * no longer being the active playback device.
     * ----------------------------------------------------- */

    gchar *status =
        get_string_property(
            data,
            "PlaybackStatus"
        );


    if (!status) {

        reset_ui(data);

        return;
    }


    /*
     * If spotifyd reports Stopped, return to the initial
     * waiting screen.
     */

    if (
        g_strcmp0(
            status,
            "Stopped"
        ) == 0
    ) {

        g_free(status);

        reset_ui(data);

        return;
    }


    data->playing =
        g_strcmp0(
            status,
            "Playing"
        ) == 0;


    if (
        g_strcmp0(
            status,
            "Playing"
        ) == 0
    ) {

        gtk_label_set_text(
            GTK_LABEL(data->status_label),
            "PLAYING"
        );

    } else if (
        g_strcmp0(
            status,
            "Paused"
        ) == 0
    ) {

        gtk_label_set_text(
            GTK_LABEL(data->status_label),
            "PAUSED"
        );

    } else {

        gtk_label_set_text(
            GTK_LABEL(data->status_label),
            "WAITING FOR SPOTIFY"
        );
    }


    g_free(status);


    /* -----------------------------------------------------
     * METADATA
     * ----------------------------------------------------- */

    GVariant *metadata =
        get_mpris_property(
            data,
            "Metadata"
        );


    if (!metadata) {

        reset_ui(data);

        return;
    }


    /* -----------------------------------------------------
     * TITLE
     * ----------------------------------------------------- */

    GVariant *title_value =
        g_variant_lookup_value(
            metadata,
            "xesam:title",
            G_VARIANT_TYPE_STRING
        );


    if (title_value) {

        const gchar *title =
            g_variant_get_string(
                title_value,
                NULL
            );

        gtk_label_set_text(
            GTK_LABEL(data->title_label),
            title
        );

        g_variant_unref(
            title_value
        );

    } else {

        gtk_label_set_text(
            GTK_LABEL(data->title_label),
            "Waiting for Spotify..."
        );
    }


    /* -----------------------------------------------------
     * ARTIST
     * ----------------------------------------------------- */

    GVariant *artist_value =
        g_variant_lookup_value(
            metadata,
            "xesam:artist",
            NULL
        );


    if (
        artist_value &&
        g_variant_is_of_type(
            artist_value,
            G_VARIANT_TYPE("as")
        )
    ) {

        GVariantIter iter;

        const gchar *artist_name = NULL;

        g_variant_iter_init(
            &iter,
            artist_value
        );

        if (
            g_variant_iter_next(
                &iter,
                "&s",
                &artist_name
            )
        ) {

            if (artist_name) {

                gtk_label_set_text(
                    GTK_LABEL(
                        data->artist_label
                    ),
                    artist_name
                );
            }
        }

    } else {

        gtk_label_set_text(
            GTK_LABEL(data->artist_label),
            ""
        );
    }


    if (artist_value)
        g_variant_unref(
            artist_value
        );


    /* -----------------------------------------------------
     * ARTWORK
     * ----------------------------------------------------- */

    update_artwork(
        data,
        metadata
    );


    g_variant_unref(
        metadata
    );


    /* -----------------------------------------------------
     * PROGRESS
     * ----------------------------------------------------- */

    update_progress(data);


    /* -----------------------------------------------------
     * PLAY BUTTON
     * ----------------------------------------------------- */

    if (data->play_button)
        gtk_widget_queue_draw(
            data->play_button
        );
}


/* =========================================================
 * FIND SPOTIFYD SERVICE
 * ========================================================= */

static gchar *find_spotifyd_service(
    GDBusConnection *connection
)
{
    GError *error = NULL;


    GDBusProxy *bus =
        g_dbus_proxy_new_sync(
            connection,
            G_DBUS_PROXY_FLAGS_NONE,
            NULL,
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            NULL,
            &error
        );


    if (!bus) {

        if (error)
            g_error_free(error);

        return NULL;
    }


    GVariant *result =
        g_dbus_proxy_call_sync(
            bus,
            "ListNames",
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            1000,
            NULL,
            &error
        );


    g_object_unref(bus);


    if (!result) {

        if (error)
            g_error_free(error);

        return NULL;
    }


    GVariantIter *iter;

    const gchar *name;

    gchar *found = NULL;


    g_variant_get(
        result,
        "(as)",
        &iter
    );


    while (
        g_variant_iter_next(
            iter,
            "&s",
            &name
        )
    ) {

        if (
            g_str_has_prefix(
                name,
                MPRIS_PREFIX
            )
        ) {

            found =
                g_strdup(name);

            break;
        }
    }


    g_variant_iter_free(iter);

    g_variant_unref(result);


    return found;
}


/* =========================================================
 * CONNECT MPRIS
 * ========================================================= */

static gboolean connect_mpris(
    AppData *data
)
{
    GError *error = NULL;


    GDBusConnection *connection =
        g_bus_get_sync(
            G_BUS_TYPE_SYSTEM,
            NULL,
            &error
        );


    if (!connection) {

        if (error) {

            g_warning(
                "Failed to connect to system D-Bus: %s",
                error->message
            );

            g_error_free(error);

            error = NULL;
        }

        return FALSE;
    }


    /* -----------------------------------------------------
     * FIND SPOTIFYD
     * ----------------------------------------------------- */

    gchar *service =
        find_spotifyd_service(
            connection
        );


    if (!service) {

        g_object_unref(connection);

        return FALSE;
    }


    /* -----------------------------------------------------
     * REMOVE OLD PROXIES
     * ----------------------------------------------------- */

    if (data->player) {

        g_object_unref(
            data->player
        );

        data->player = NULL;
    }


    if (data->properties) {

        g_object_unref(
            data->properties
        );

        data->properties = NULL;
    }


    g_clear_pointer(
        &data->service_name,
        g_free
    );


    data->service_name =
        service;


    /* -----------------------------------------------------
     * PLAYER PROXY
     * ----------------------------------------------------- */

    data->player =
        g_dbus_proxy_new_sync(
            connection,
            G_DBUS_PROXY_FLAGS_NONE,
            NULL,
            data->service_name,
            MPRIS_PATH,
            MPRIS_PLAYER_IFACE,
            NULL,
            &error
        );


    if (!data->player) {

        if (error) {

            g_warning(
                "Failed to create Spotify player proxy: %s",
                error->message
            );

            g_error_free(error);

            error = NULL;
        }

        g_object_unref(connection);

        return FALSE;
    }


    /* -----------------------------------------------------
     * PROPERTIES PROXY
     * ----------------------------------------------------- */

    data->properties =
        g_dbus_proxy_new_sync(
            connection,
            G_DBUS_PROXY_FLAGS_NONE,
            NULL,
            data->service_name,
            MPRIS_PATH,
            DBUS_PROPERTIES_IFACE,
            NULL,
            &error
        );


    if (!data->properties) {

        if (error) {

            g_warning(
                "Failed to create Spotify properties proxy: %s",
                error->message
            );

            g_error_free(error);

            error = NULL;
        }


        g_object_unref(
            data->player
        );

        data->player = NULL;


        g_object_unref(connection);

        return FALSE;
    }


    g_object_unref(connection);


    data->reconnect_delay = 1;


    gtk_label_set_text(
        GTK_LABEL(data->status_label),
        "CONNECTED"
    );


    return TRUE;
}


/* =========================================================
 * UPDATE TIMER
 * ========================================================= */

static gboolean update_timer(
    gpointer user_data
)
{
    AppData *data =
        user_data;


    if (
        !data->player ||
        !data->properties
    ) {

        if (connect_mpris(data)) {

            update_ui(data);

            data->reconnect_delay = 1;

        } else {

            if (data->reconnect_delay < 8)
                data->reconnect_delay *= 2;

            if (data->reconnect_delay > 8)
                data->reconnect_delay = 8;
        }


        return G_SOURCE_CONTINUE;
    }


    update_ui(data);


    return G_SOURCE_CONTINUE;
}


/* =========================================================
 * CSS
 * ========================================================= */

static void load_css(void)
{
    const gchar *css =
        "* {"
        "  font-family: Sans;"
        "}"

        "window {"
        "  background-color: #121212;"
        "}"

        ".title {"
        "  color: #ffffff;"
        "  font-size: 34px;"
        "  font-weight: 700;"
        "}"

        ".artist {"
        "  color: #b3b3b3;"
        "  font-size: 22px;"
        "}"

        ".status {"
        "  color: #1ed760;"
        "  font-size: 13px;"
        "  font-weight: 700;"
        "}"

        ".art {"
        "  background-color: #202020;"
        "  color: #555555;"
        "  font-size: 60px;"
        "}"

        "progressbar {"
        "  min-height: 6px;"
        "}"

        "progressbar trough {"
        "  background-color: #4a4a4a;"
        "  border: none;"
        "  border-radius: 4px;"
        "  min-height: 6px;"
        "}"

        "progressbar progress {"
        "  background-color: #1ed760;"
        "  border: none;"
        "  border-radius: 4px;"
        "  min-height: 6px;"
        "}";


    GtkCssProvider *provider =
        gtk_css_provider_new();


    gtk_css_provider_load_from_data(
        provider,
        css,
        -1,
        NULL
    );


    GdkScreen *screen =
        gdk_screen_get_default();


    if (screen) {

        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
    }


    g_object_unref(provider);
}


/* =========================================================
 * ACTIVATE
 * ========================================================= */

static void activate(
    GtkApplication *app,
    gpointer user_data
)
{
    (void)user_data;


    load_css();


    AppData *data =
        g_new0(
            AppData,
            1
        );


    data->reconnect_delay = 1;


    /* -----------------------------------------------------
     * WINDOW
     * ----------------------------------------------------- */

    GtkWidget *window =
        gtk_application_window_new(
            app
        );


    gtk_window_set_decorated(
        GTK_WINDOW(window),
        FALSE
    );


    gtk_window_set_default_size(
        GTK_WINDOW(window),
        820,
        500
    );


    gtk_window_set_resizable(
        GTK_WINDOW(window),
        FALSE
    );


    /* -----------------------------------------------------
     * MAIN BOX
     * ----------------------------------------------------- */

    GtkWidget *main_box =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            0
        );


    gtk_container_add(
        GTK_CONTAINER(window),
        main_box
    );


    /* -----------------------------------------------------
     * ARTWORK PANEL
     * ----------------------------------------------------- */

    GtkWidget *art_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0
        );


    gtk_widget_set_size_request(
        art_box,
        280,
        -1
    );


    gtk_box_pack_start(
        GTK_BOX(main_box),
        art_box,
        FALSE,
        FALSE,
        0
    );


    /* -----------------------------------------------------
     * ARTWORK CONTAINER
     * ----------------------------------------------------- */

    GtkWidget *art_overlay =
        gtk_overlay_new();


    gtk_widget_set_size_request(
        art_overlay,
        260,
        260
    );


    gtk_widget_set_halign(
        art_overlay,
        GTK_ALIGN_CENTER
    );


    gtk_widget_set_valign(
        art_overlay,
        GTK_ALIGN_CENTER
    );


    gtk_box_pack_start(
        GTK_BOX(art_box),
        art_overlay,
        TRUE,
        TRUE,
        0
    );


    /* -----------------------------------------------------
     * ARTWORK PLACEHOLDER
     * ----------------------------------------------------- */

    GtkWidget *placeholder =
        gtk_label_new(
            "♪"
        );


    data->art_placeholder =
        placeholder;


    gtk_style_context_add_class(
        gtk_widget_get_style_context(
            placeholder
        ),
        "art"
    );


    gtk_widget_set_size_request(
        placeholder,
        260,
        260
    );


    gtk_widget_set_halign(
        placeholder,
        GTK_ALIGN_CENTER
    );


    gtk_widget_set_valign(
        placeholder,
        GTK_ALIGN_CENTER
    );


    gtk_overlay_add_overlay(
        GTK_OVERLAY(art_overlay),
        placeholder
    );


    /* -----------------------------------------------------
     * ARTWORK IMAGE
     * ----------------------------------------------------- */

    GtkWidget *art_image =
        gtk_image_new();


    data->art_image =
        art_image;


    gtk_widget_set_size_request(
        art_image,
        260,
        260
    );


    gtk_widget_set_halign(
        art_image,
        GTK_ALIGN_CENTER
    );


    gtk_widget_set_valign(
        art_image,
        GTK_ALIGN_CENTER
    );


    gtk_overlay_add_overlay(
        GTK_OVERLAY(art_overlay),
        art_image
    );


    gtk_widget_show(
        placeholder
    );


    gtk_widget_hide(
        art_image
    );


    /* -----------------------------------------------------
     * RIGHT PANEL
     * ----------------------------------------------------- */

    GtkWidget *right_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0
        );


    gtk_widget_set_margin_start(
        right_box,
        30
    );


    gtk_widget_set_margin_end(
        right_box,
        30
    );


    gtk_widget_set_margin_top(
        right_box,
        40
    );


    gtk_widget_set_margin_bottom(
        right_box,
        35
    );


    gtk_box_pack_start(
        GTK_BOX(main_box),
        right_box,
        TRUE,
        TRUE,
        0
    );


    /* -----------------------------------------------------
     * TITLE
     * ----------------------------------------------------- */

    GtkWidget *title =
        gtk_label_new(
            "Waiting for Spotify..."
        );


    data->title_label =
        title;


    gtk_style_context_add_class(
        gtk_widget_get_style_context(
            title
        ),
        "title"
    );


    gtk_label_set_ellipsize(
        GTK_LABEL(title),
        PANGO_ELLIPSIZE_END
    );


    gtk_label_set_xalign(
        GTK_LABEL(title),
        0.0
    );


    gtk_box_pack_start(
        GTK_BOX(right_box),
        title,
        FALSE,
        FALSE,
        5
    );


    /* -----------------------------------------------------
     * ARTIST
     * ----------------------------------------------------- */

    GtkWidget *artist =
        gtk_label_new(
            ""
        );


    data->artist_label =
        artist;


    gtk_style_context_add_class(
        gtk_widget_get_style_context(
            artist
        ),
        "artist"
    );


    gtk_label_set_ellipsize(
        GTK_LABEL(artist),
        PANGO_ELLIPSIZE_END
    );


    gtk_label_set_xalign(
        GTK_LABEL(artist),
        0.0
    );


    gtk_box_pack_start(
        GTK_BOX(right_box),
        artist,
        FALSE,
        FALSE,
        0
    );


    /* -----------------------------------------------------
     * SPACER
     * ----------------------------------------------------- */

    GtkWidget *spacer =
        gtk_label_new(
            ""
        );


    gtk_box_pack_start(
        GTK_BOX(right_box),
        spacer,
        TRUE,
        TRUE,
        0
    );


    /* -----------------------------------------------------
     * STATUS
     * ----------------------------------------------------- */

    GtkWidget *status =
        gtk_label_new(
            "Connecting..."
        );


    data->status_label =
        status;


    gtk_style_context_add_class(
        gtk_widget_get_style_context(
            status
        ),
        "status"
    );


    gtk_label_set_xalign(
        GTK_LABEL(status),
        0.0
    );


    gtk_box_pack_start(
        GTK_BOX(right_box),
        status,
        FALSE,
        FALSE,
        4
    );


    /* -----------------------------------------------------
     * PROGRESS
     * ----------------------------------------------------- */

    GtkWidget *progress =
        gtk_progress_bar_new();


    data->progress =
        progress;


    gtk_progress_bar_set_fraction(
        GTK_PROGRESS_BAR(progress),
        0.0
    );


    gtk_widget_set_size_request(
        progress,
        -1,
        6
    );


    gtk_widget_set_margin_bottom(
        progress,
        15
    );


    gtk_box_pack_start(
        GTK_BOX(right_box),
        progress,
        FALSE,
        FALSE,
        0
    );


    /* -----------------------------------------------------
     * CONTROLS
     * ----------------------------------------------------- */

    GtkWidget *controls =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            24
        );


    gtk_widget_set_halign(
        controls,
        GTK_ALIGN_CENTER
    );


    gtk_widget_set_margin_bottom(
        controls,
        30
    );


    gtk_box_pack_start(
        GTK_BOX(right_box),
        controls,
        FALSE,
        FALSE,
        0
    );


    /* -----------------------------------------------------
     * PREVIOUS
     * ----------------------------------------------------- */

    GtkWidget *previous =
        create_media_button(
            BUTTON_PREVIOUS,
            data,
            64
        );


    MediaButtonData *previous_data =
        g_new0(
            MediaButtonData,
            1
        );


    previous_data->type =
        BUTTON_PREVIOUS;


    previous_data->data =
        data;


    g_signal_connect_data(
        previous,
        "button-press-event",
        G_CALLBACK(media_button_press),
        previous_data,
        (GClosureNotify)g_free,
        0
    );


    gtk_box_pack_start(
        GTK_BOX(controls),
        previous,
        FALSE,
        FALSE,
        0
    );


    /* -----------------------------------------------------
     * PLAY / PAUSE
     * ----------------------------------------------------- */

    GtkWidget *play =
        create_media_button(
            BUTTON_PLAY_PAUSE,
            data,
            82
        );


    data->play_button =
        play;


    MediaButtonData *play_data =
        g_new0(
            MediaButtonData,
            1
        );


    play_data->type =
        BUTTON_PLAY_PAUSE;


    play_data->data =
        data;


    g_signal_connect_data(
        play,
        "button-press-event",
        G_CALLBACK(media_button_press),
        play_data,
        (GClosureNotify)g_free,
        0
    );


    gtk_box_pack_start(
        GTK_BOX(controls),
        play,
        FALSE,
        FALSE,
        0
    );


    /* -----------------------------------------------------
     * NEXT
     * ----------------------------------------------------- */

    GtkWidget *next =
        create_media_button(
            BUTTON_NEXT,
            data,
            64
        );


    MediaButtonData *next_data =
        g_new0(
            MediaButtonData,
            1
        );


    next_data->type =
        BUTTON_NEXT;


    next_data->data =
        data;


    g_signal_connect_data(
        next,
        "button-press-event",
        G_CALLBACK(media_button_press),
        next_data,
        (GClosureNotify)g_free,
        0
    );


    gtk_box_pack_start(
        GTK_BOX(controls),
        next,
        FALSE,
        FALSE,
        0
    );


    /* -----------------------------------------------------
     * STORE DATA
     * ----------------------------------------------------- */

    g_object_set_data_full(
        G_OBJECT(window),
        "app-data",
        data,
        (GDestroyNotify)g_free
    );


    /* -----------------------------------------------------
     * SHOW WINDOW
     * ----------------------------------------------------- */

    gtk_widget_show_all(
        window
    );


    /*
     * Restore initial artwork state after show_all().
     */

    if (!data->art_url) {

        gtk_widget_hide(
            data->art_image
        );

        gtk_widget_show(
            data->art_placeholder
        );
    }


    /* -----------------------------------------------------
     * CONNECT MPRIS
     * ----------------------------------------------------- */

    if (connect_mpris(data)) {

        update_ui(data);

    } else {

        gtk_label_set_text(
            GTK_LABEL(data->status_label),
            "WAITING FOR SPOTIFY"
        );
    }


    /* -----------------------------------------------------
     * UPDATE EVERY SECOND
     * ----------------------------------------------------- */

    g_timeout_add_seconds(
        1,
        update_timer,
        data
    );
}


/* =========================================================
 * MAIN
 * ========================================================= */

int main(
    int argc,
    char **argv
)
{
    GtkApplication *app =
        gtk_application_new(
            "com.stm32.spotify",
            G_APPLICATION_DEFAULT_FLAGS
        );


    g_signal_connect(
        app,
        "activate",
        G_CALLBACK(activate),
        NULL
    );


    int status =
        g_application_run(
            G_APPLICATION(app),
            argc,
            argv
        );


    g_object_unref(app);


    return status;
}
