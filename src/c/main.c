#include <pebble.h>

// The phone sends at most this many issues; see MAX_ISSUES in index.js.
#define MAX_ISSUES 25

#define HEADER_HEIGHT 26
#define ROW_HEIGHT 46
#define STRIPE_WIDTH 6
#define ROW_PADDING 6
#define BANNER_HEIGHT 36
#define MARGIN 8

// Jira's status categories, as index.js numbers them.
typedef enum {
  CategoryToDo = 0,
  CategoryInProgress = 1,
  CategoryDone = 2,
} IssueCategory;

// String sizes match LIMITS in index.js.
typedef struct {
  char key[16];
  char summary[160];
  char status[32];
  char priority[16];
  char assignee[32];
  uint8_t category;
  bool urgent;
} Issue;

static Issue s_issues[MAX_ISSUES];
static int s_issue_count;
// How many issues the phone said it would send; -1 until the queue arrives.
static int s_expected_count = -1;

static Window *s_list_window;
static TextLayer *s_header_layer;
static MenuLayer *s_menu_layer;
static TextLayer *s_message_layer;
static char s_header_text[32];
static char s_message_text[128];

static Window *s_issue_window;
static int s_issue_index;
static TextLayer *s_banner_layer;
static TextLayer *s_key_layer;
static TextLayer *s_summary_layer;
static TextLayer *s_details_layer;
static char s_details_text[80];

static GColor stripe_color(const Issue *issue) {
  if (issue->urgent) {
    return GColorRed;
  }
  switch (issue->category) {
    case CategoryInProgress: return GColorVividCerulean;
    case CategoryDone: return GColorIslamicGreen;
    default: return GColorLightGray;
  }
}

// A darker shade of the stripe color, for white text on the issue banner.
static GColor banner_color(const Issue *issue) {
  if (issue->urgent) {
    return GColorDarkCandyAppleRed;
  }
  switch (issue->category) {
    case CategoryInProgress: return GColorCobaltBlue;
    case CategoryDone: return GColorIslamicGreen;
    default: return GColorDarkGray;
  }
}

// ---------------------------------------------------------------------------
// Issue window

static void issue_window_update(void) {
  const Issue *issue = &s_issues[s_issue_index];
  Layer *root = window_get_root_layer(s_issue_window);
  GRect bounds = layer_get_bounds(root);
  int width = bounds.size.w - 2 * MARGIN;

  text_layer_set_background_color(s_banner_layer, banner_color(issue));
  text_layer_set_text(s_banner_layer, issue->status);
  text_layer_set_text(s_key_layer, issue->key);
  text_layer_set_text(s_summary_layer, issue->summary);

  // Size the summary to its text so the details sit right below it.
  int summary_top = BANNER_HEIGHT + 28;
  layer_set_frame(text_layer_get_layer(s_summary_layer),
                  GRect(MARGIN, summary_top, width, bounds.size.h));
  GSize summary_size = text_layer_get_content_size(s_summary_layer);
  int details_top = summary_top + summary_size.h + 8;
  layer_set_frame(text_layer_get_layer(s_summary_layer),
                  GRect(MARGIN, summary_top, width, summary_size.h + 4));

  snprintf(s_details_text, sizeof(s_details_text), "Priority: %s\n%s",
           issue->priority[0] ? issue->priority : "None",
           issue->assignee[0] ? issue->assignee : "Unassigned");
  text_layer_set_text(s_details_layer, s_details_text);
  layer_set_frame(text_layer_get_layer(s_details_layer),
                  GRect(MARGIN, details_top, width, bounds.size.h - details_top));
}

static TextLayer *add_text_layer(Layer *parent, GRect frame, const char *font_key) {
  TextLayer *layer = text_layer_create(frame);
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_font(layer, fonts_get_system_font(font_key));
  text_layer_set_overflow_mode(layer, GTextOverflowModeWordWrap);
  layer_add_child(parent, text_layer_get_layer(layer));
  return layer;
}

static void issue_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int width = bounds.size.w - 2 * MARGIN;

  s_banner_layer = add_text_layer(root, GRect(0, 0, bounds.size.w, BANNER_HEIGHT),
                                  FONT_KEY_GOTHIC_24_BOLD);
  text_layer_set_text_color(s_banner_layer, GColorWhite);
  text_layer_set_text_alignment(s_banner_layer, GTextAlignmentCenter);

  s_key_layer = add_text_layer(root, GRect(MARGIN, BANNER_HEIGHT + 2, width, 26),
                               FONT_KEY_GOTHIC_18_BOLD);
  text_layer_set_text_color(s_key_layer, GColorDarkGray);

  s_summary_layer = add_text_layer(root, GRect(MARGIN, BANNER_HEIGHT + 28, width, 0),
                                   FONT_KEY_GOTHIC_24_BOLD);
  s_details_layer = add_text_layer(root, GRect(MARGIN, 0, width, 0), FONT_KEY_GOTHIC_18);
  text_layer_set_text_color(s_details_layer, GColorDarkGray);

  issue_window_update();
}

static void issue_window_unload(Window *window) {
  text_layer_destroy(s_banner_layer);
  text_layer_destroy(s_key_layer);
  text_layer_destroy(s_summary_layer);
  text_layer_destroy(s_details_layer);
}

static void show_issue(int index) {
  s_issue_index = index;
  window_stack_push(s_issue_window, true);
}

// ---------------------------------------------------------------------------
// List window

static void update_list(void) {
  if (s_expected_count < 0) {
    snprintf(s_header_text, sizeof(s_header_text), "My queue");
  } else {
    snprintf(s_header_text, sizeof(s_header_text), "My queue · %d", s_expected_count);
  }
  text_layer_set_text(s_header_layer, s_header_text);

  bool has_issues = s_issue_count > 0;
  if (!has_issues && s_message_text[0] == '\0') {
    snprintf(s_message_text, sizeof(s_message_text), "%s",
             s_expected_count == 0 ? "Nothing assigned to you." : "Loading...");
  }
  layer_set_hidden(menu_layer_get_layer(s_menu_layer), !has_issues);
  layer_set_hidden(text_layer_get_layer(s_message_layer), has_issues);
  text_layer_set_text(s_message_layer, s_message_text);
  menu_layer_reload_data(s_menu_layer);
}

static uint16_t menu_get_num_rows(MenuLayer *menu_layer, uint16_t section, void *context) {
  return s_issue_count;
}

static int16_t menu_get_cell_height(MenuLayer *menu_layer, MenuIndex *index, void *context) {
  return ROW_HEIGHT;
}

static void menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index,
                          void *context) {
  const Issue *issue = &s_issues[index->row];
  GRect bounds = layer_get_bounds(cell_layer);
  bool highlighted = menu_cell_layer_is_highlighted(cell_layer);

  graphics_context_set_fill_color(ctx, stripe_color(issue));
  graphics_fill_rect(ctx, GRect(0, 0, STRIPE_WIDTH, bounds.size.h), 0, GCornerNone);

  int left = STRIPE_WIDTH + ROW_PADDING;
  int width = bounds.size.w - left - ROW_PADDING;
  graphics_context_set_text_color(ctx, highlighted ? GColorWhite : GColorBlack);
  graphics_draw_text(ctx, issue->key, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(left, -1, width, 16), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, issue->summary, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(left, 13, width, 32), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  // A hairline between rows.
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(STRIPE_WIDTH, bounds.size.h - 1),
                     GPoint(bounds.size.w, bounds.size.h - 1));
}

static void menu_select(MenuLayer *menu_layer, MenuIndex *index, void *context) {
  show_issue(index->row);
}

static void list_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_header_layer = text_layer_create(GRect(0, 0, bounds.size.w, HEADER_HEIGHT));
  text_layer_set_background_color(s_header_layer, GColorCobaltBlue);
  text_layer_set_text_color(s_header_layer, GColorWhite);
  text_layer_set_font(s_header_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_header_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_header_layer));

  GRect body = GRect(0, HEADER_HEIGHT, bounds.size.w, bounds.size.h - HEADER_HEIGHT);
  s_menu_layer = menu_layer_create(body);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = menu_get_num_rows,
    .get_cell_height = menu_get_cell_height,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
  });
  menu_layer_set_highlight_colors(s_menu_layer, GColorOxfordBlue, GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));

  s_message_layer = text_layer_create(grect_inset(body, GEdgeInsets(40, MARGIN, 0)));
  text_layer_set_font(s_message_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_message_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_message_layer, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(s_message_layer));

  update_list();
}

static void list_window_unload(Window *window) {
  text_layer_destroy(s_header_layer);
  menu_layer_destroy(s_menu_layer);
  text_layer_destroy(s_message_layer);
}

// ---------------------------------------------------------------------------
// Messages from the phone

static void copy_string(char *dest, size_t size, DictionaryIterator *iter, uint32_t key) {
  Tuple *tuple = dict_find(iter, key);
  snprintf(dest, size, "%s", tuple ? tuple->value->cstring : "");
}

static void receive_issue(DictionaryIterator *iter, int index) {
  if (index < 0 || index >= MAX_ISSUES) {
    return;
  }
  Issue *issue = &s_issues[index];
  copy_string(issue->key, sizeof(issue->key), iter, MESSAGE_KEY_IssueKey);
  copy_string(issue->summary, sizeof(issue->summary), iter, MESSAGE_KEY_IssueSummary);
  copy_string(issue->status, sizeof(issue->status), iter, MESSAGE_KEY_IssueStatus);
  copy_string(issue->priority, sizeof(issue->priority), iter, MESSAGE_KEY_IssuePriority);
  copy_string(issue->assignee, sizeof(issue->assignee), iter, MESSAGE_KEY_IssueAssignee);
  Tuple *category = dict_find(iter, MESSAGE_KEY_IssueCategory);
  issue->category = category ? category->value->int32 : CategoryToDo;
  Tuple *urgent = dict_find(iter, MESSAGE_KEY_IssueUrgent);
  issue->urgent = urgent && urgent->value->int32 != 0;

  if (index >= s_issue_count) {
    s_issue_count = index + 1;
  }
  if (window_stack_contains_window(s_issue_window) && index == s_issue_index) {
    issue_window_update();
  }
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *error = dict_find(iter, MESSAGE_KEY_Error);
  Tuple *count = dict_find(iter, MESSAGE_KEY_QueueCount);
  Tuple *index = dict_find(iter, MESSAGE_KEY_IssueIndex);

  if (error) {
    snprintf(s_message_text, sizeof(s_message_text), "%s", error->value->cstring);
    s_issue_count = 0;
    s_expected_count = -1;
    window_stack_remove(s_issue_window, false);
  } else if (count) {
    // A fresh queue replaces the old one.
    s_message_text[0] = '\0';
    s_issue_count = 0;
    s_expected_count = count->value->int32;
    window_stack_remove(s_issue_window, false);
  } else if (index) {
    receive_issue(iter, index->value->int32);
  }
  update_list();
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void init(void) {
  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_open(512, 64);

  s_issue_window = window_create();
  window_set_window_handlers(s_issue_window, (WindowHandlers) {
    .load = issue_window_load,
    .unload = issue_window_unload,
  });

  s_list_window = window_create();
  window_set_window_handlers(s_list_window, (WindowHandlers) {
    .load = list_window_load,
    .unload = list_window_unload,
  });
  window_stack_push(s_list_window, true);
}

static void deinit(void) {
  window_destroy(s_list_window);
  window_destroy(s_issue_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
