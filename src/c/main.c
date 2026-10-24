#include <pebble.h>

static Window *s_window;
static TextLayer *s_title_layer;
static TextLayer *s_body_layer;

// Holds the text the body layer shows, which must outlive the inbox callback.
static char s_body_text[128];

static void set_body(const char *text) {
  snprintf(s_body_text, sizeof(s_body_text), "%s", text);
  text_layer_set_text(s_body_layer, s_body_text);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *name = dict_find(iter, MESSAGE_KEY_DisplayName);
  Tuple *error = dict_find(iter, MESSAGE_KEY_Error);

  if (name) {
    set_body(name->value->cstring);
    text_layer_set_text_color(s_body_layer, GColorBlack);
  } else if (error) {
    set_body(error->value->cstring);
    text_layer_set_text_color(s_body_layer, GColorDarkCandyAppleRed);
  }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_title_layer = text_layer_create(GRect(0, 0, bounds.size.w, 40));
  text_layer_set_background_color(s_title_layer, GColorCobaltBlue);
  text_layer_set_text_color(s_title_layer, GColorWhite);
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_text(s_title_layer, "Jira");
  layer_add_child(window_layer, text_layer_get_layer(s_title_layer));

  s_body_layer = text_layer_create(GRect(8, 70, bounds.size.w - 16, bounds.size.h - 78));
  text_layer_set_font(s_body_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_body_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_body_layer, GTextOverflowModeWordWrap);
  set_body("Connecting...");
  layer_add_child(window_layer, text_layer_get_layer(s_body_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_body_layer);
}

static void init(void) {
  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_open(256, 64);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
