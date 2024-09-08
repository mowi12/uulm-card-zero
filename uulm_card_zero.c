#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG "UUlmCardZero"

// Change this to BACKLIGHT_AUTO if you don't want the backlight to be continuously on.
#define BACKLIGHT_ON 1

typedef enum {
    UUlmCardZeroSubmenuIndexStart,
    UUlmCardZeroSubmenuIndexAbout,
} UUlmCardZeroSubmenuIndex;

typedef enum {
    UUlmCardZeroViewSubmenu,
    UUlmCardZeroViewStart,
    UUlmCardZeroViewAbout,
} UUlmCardZeroView;

typedef enum {
    UUlmCardZeroEventIdRedrawScreen = 0,
    UUlmCardZeroEventIdOkPressed = 1,
} UUlmCardZeroEventId;

typedef struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    View* view_start;
    Widget* widget_about;

    FuriTimer* timer;
} UUlmCardZeroApp;

typedef struct {
} UUlmCardZeroModel;

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to exit the application.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t uulm_card_zero_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to navigate to the submenu.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t uulm_card_zero_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return UUlmCardZeroViewSubmenu;
}

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - UUlmCardZeroApp object.
 * @param      index     The UUlmCardZeroSubmenuIndex item that was clicked.
*/
static void uulm_card_zero_submenu_callback(void* context, uint32_t index) {
    UUlmCardZeroApp* app = (UUlmCardZeroApp*)context;
    switch(index) {
    case UUlmCardZeroSubmenuIndexStart:
        view_dispatcher_switch_to_view(app->view_dispatcher, UUlmCardZeroViewStart);
        break;
    case UUlmCardZeroSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, UUlmCardZeroViewAbout);
        break;
    default:
        break;
    }
}

/**
 * @brief      Callback for drawing the game screen.
 * @details    This function is called when the screen needs to be redrawn.
 * @param      canvas  The canvas to draw on.
*/
static void uulm_card_zero_view_start_draw_callback(Canvas* canvas, void* _model) {
    UNUSED(_model);

    FuriString* xstr = furi_string_alloc();
    furi_string_printf(xstr, "OK = play tone");
    canvas_draw_str(canvas, 40, 35, furi_string_get_cstr(xstr));
    furi_string_printf(xstr, "random: %u", (uint8_t)(furi_hal_random_get() % 256));
    canvas_draw_str(canvas, 40, 45, furi_string_get_cstr(xstr));
    furi_string_free(xstr);
}

/**
 * @brief      Callback for timer elapsed.
 * @details    This function is called when the timer is elapsed.  We use this to queue a redraw event.
 * @param      context  The context - UUlmCardZeroApp object.
*/
static void uulm_card_zero_view_start_timer_callback(void* context) {
    UUlmCardZeroApp* app = (UUlmCardZeroApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, UUlmCardZeroEventIdRedrawScreen);
}

/**
 * @brief      Callback when the user starts the game screen.
 * @details    This function is called when the user enters the game screen.  We start a timer to
 *           redraw the screen periodically (so the random number is refreshed).
 * @param      context  The context - UUlmCardZeroApp object.
*/
static void uulm_card_zero_view_start_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(5000);
    UUlmCardZeroApp* app = (UUlmCardZeroApp*)context;
    furi_assert(app->timer == NULL);
    app->timer =
        furi_timer_alloc(uulm_card_zero_view_start_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer, period);
}

/**
 * @brief      Callback when the user exits the game screen.
 * @details    This function is called when the user exits the game screen.  We stop the timer.
 * @param      context  The context - UUlmCardZeroApp object.
*/
static void uulm_card_zero_view_start_exit_callback(void* context) {
    UUlmCardZeroApp* app = (UUlmCardZeroApp*)context;
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    app->timer = NULL;
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - UUlmCardZeroEventId value.
 * @param      context  The context - UUlmCardZeroApp object.
*/
static bool uulm_card_zero_view_start_custom_event_callback(uint32_t event, void* context) {
    UUlmCardZeroApp* app = (UUlmCardZeroApp*)context;
    switch(event) {
    case UUlmCardZeroEventIdRedrawScreen:
        // Redraw screen by passing true to last parameter of with_view_model.
        {
            bool redraw = true;
            with_view_model(
                app->view_start, UUlmCardZeroModel * _model, { UNUSED(_model); }, redraw);
            return true;
        }
    case UUlmCardZeroEventIdOkPressed:
        // Process the OK button.  We play a tone based on the x coordinate.
        if(furi_hal_speaker_acquire(500)) {
            bool redraw = false;
            with_view_model(
                app->view_start, UUlmCardZeroModel * _model, { UNUSED(_model); }, redraw);
            furi_hal_speaker_start(4000, 1.0);
            furi_delay_ms(100);
            furi_hal_speaker_stop();
            furi_hal_speaker_release();
        }
        return true;
    default:
        return false;
    }
}

/**
 * @brief      Callback for game screen input.
 * @details    This function is called when the user presses a button while on the game screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - UUlmCardZeroApp object.
 * @return     true if the event was handled, false otherwise.
*/
static bool uulm_card_zero_view_start_input_callback(InputEvent* event, void* context) {
    UUlmCardZeroApp* app = (UUlmCardZeroApp*)context;
    if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            // We choose to send a custom event when user presses OK button.  uulm_card_zero_custom_event_callback will
            // handle our UUlmCardZeroEventIdOkPressed event.  We could have just put the code from
            // uulm_card_zero_custom_event_callback here, it's a matter of preference.
            view_dispatcher_send_custom_event(app->view_dispatcher, UUlmCardZeroEventIdOkPressed);
            return true;
        }
    }

    return false;
}

/**
 * @brief      Allocate the uulm_card_zero application.
 * @details    This function allocates the uulm_card_zero application resources.
 * @return     UUlmCardZeroApp object.
*/
static UUlmCardZeroApp* uulm_card_zero_app_alloc() {
    UUlmCardZeroApp* app = (UUlmCardZeroApp*)malloc(sizeof(UUlmCardZeroApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Start", UUlmCardZeroSubmenuIndexStart, uulm_card_zero_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", UUlmCardZeroSubmenuIndexAbout, uulm_card_zero_submenu_callback, app);
    view_set_previous_callback(
        submenu_get_view(app->submenu), uulm_card_zero_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, UUlmCardZeroViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, UUlmCardZeroViewSubmenu);

    app->view_start = view_alloc();
    view_set_draw_callback(app->view_start, uulm_card_zero_view_start_draw_callback);
    view_set_input_callback(app->view_start, uulm_card_zero_view_start_input_callback);
    view_set_previous_callback(app->view_start, uulm_card_zero_navigation_submenu_callback);
    view_set_enter_callback(app->view_start, uulm_card_zero_view_start_enter_callback);
    view_set_exit_callback(app->view_start, uulm_card_zero_view_start_exit_callback);
    view_set_context(app->view_start, app);
    view_set_custom_callback(app->view_start, uulm_card_zero_view_start_custom_event_callback);
    view_dispatcher_add_view(app->view_dispatcher, UUlmCardZeroViewStart, app->view_start);

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "UUlm Card Zero\n                 ---\nWhat happens if i dont end the line manualy??\n\nauthor: @mowi12\nhttps://github.com/mowi12");
    view_set_previous_callback(
        widget_get_view(app->widget_about), uulm_card_zero_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, UUlmCardZeroViewAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
#endif

    return app;
}

/**
 * @brief      Free the uulm_card_zero application.
 * @details    This function frees the uulm_card_zero application resources.
 * @param      app  The uulm_card_zero application object.
*/
static void uulm_card_zero_app_free(UUlmCardZeroApp* app) {
#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, UUlmCardZeroViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, UUlmCardZeroViewStart);
    view_free(app->view_start);
    view_dispatcher_remove_view(app->view_dispatcher, UUlmCardZeroViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

/**
 * @brief      Main function for uulm_card_zero application.
 * @details    This function is the entry point for the uulm_card_zero application.  It should be defined in
 *           application.fam as the entry_point setting.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t main_uulm_card_zero_app(void* _p) {
    UNUSED(_p);

    UUlmCardZeroApp* app = uulm_card_zero_app_alloc();
    view_dispatcher_run(app->view_dispatcher);

    uulm_card_zero_app_free(app);
    return 0;
}
