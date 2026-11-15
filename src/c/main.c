#include <pebble.h>

// The phone sends at most this many issues per list and this many columns;
// see MAX_ISSUES and MAX_COLUMNS in index.js.
#define MAX_ISSUES 50
#define MAX_COLUMNS 12

#define HEADER_HEIGHT 26
#define COLUMN_ROW_HEIGHT 40
#define ISSUE_ROW_HEIGHT 62
#define STRIPE_WIDTH 6
#define ROW_PADDING 6
#define BANNER_HEIGHT 36
#define MARGIN 8
// The sprint's days left turn red at this many or fewer.
#define DAYS_LEFT_WARNING 2

// Jira's status categories, as index.js numbers them.
typedef enum {
  CategoryToDo = 0,
  CategoryInProgress = 1,
  CategoryDone = 2,
} IssueCategory;

// A column of the board. The name's size matches LIMITS in index.js.
typedef struct {
  char name[24];
  int issue_count;
  uint8_t category;
} Column;

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

// The sprint, as the phone last sent it.
static bool s_sprint_loaded;
static char s_sprint_name[32];
static int s_days_left;
static Column s_columns[MAX_COLUMNS];
// How many columns the board has, and how many of them have arrived.
static int s_column_total;
static int s_column_count;

// The open list: one column's issues.
static int s_list_column;
// How many issues the column holds; -1 until the phone says.
static int s_list_total = -1;
static Issue s_issues[MAX_ISSUES];
static int s_issue_count;

static Window *s_home_window;
static TextLayer *s_home_header_layer;
static TextLayer *s_sprint_name_layer;
static TextLayer *s_days_left_layer;
static MenuLayer *s_home_menu_layer;
static TextLayer *s_home_message_layer;
static char s_days_left_text[24];
static char s_home_message[128];

static Window *s_list_window;
static TextLayer *s_list_header_layer;
static MenuLayer *s_list_menu_layer;
static TextLayer *s_list_message_layer;
static char s_list_header_text[32];

static Window *s_issue_window;
static int s_issue_index;
static TextLayer *s_banner_layer;
static TextLayer *s_key_layer;
static TextLayer *s_summary_layer;
static TextLayer *s_details_layer;
static char s_details_text[80];

static GColor category_color(int category) {
  switch (category) {
    case CategoryInProgress: return GColorVividCerulean;
    case CategoryDone: return GColorIslamicGreen;
    default: return GColorLightGray;
  }
}

// A darker shade of the category color, for white text.
static GColor category_dark_color(int category) {
  switch (category) {
    case CategoryInProgress: return GColorCobaltBlue;
    case CategoryDone: return GColorIslamicGreen;
    default: return GColorDarkGray;
  }
}

static GColor stripe_color(const Issue *issue) {
  return issue->urgent ? GColorRed : category_color(issue->category);
}

static GColor banner_color(const Issue *issue) {
  return issue->urgent ? GColorDarkCandyAppleRed : category_dark_color(issue->category);
}

static TextLayer *add_text_layer(Layer *parent, GRect frame, const char *font_key) {
  TextLayer *layer = text_layer_create(frame);
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_font(layer, fonts_get_system_font(font_key));
  text_layer_set_overflow_mode(layer, GTextOverflowModeWordWrap);
  layer_add_child(parent, text_layer_get_layer(layer));
  return layer;
}

static TextLayer *add_header_layer(Layer *parent, GRect bounds) {
  TextLayer *layer = add_text_layer(parent, GRect(0, 0, bounds.size.w, HEADER_HEIGHT),
                                    FONT_KEY_GOTHIC_18_BOLD);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(layer, GTextOverflowModeTrailingEllipsis);
  return layer;
}

static TextLayer *add_message_layer(Layer *parent, GRect bounds) {
  GRect frame = GRect(MARGIN, HEADER_HEIGHT + 40, bounds.size.w - 2 * MARGIN,
                      bounds.size.h - HEADER_HEIGHT - 40);
  TextLayer *layer = add_text_layer(parent, frame, FONT_KEY_GOTHIC_24_BOLD);
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  return layer;
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

// ---------------------------------------------------------------------------
// List window: the sprint's issues in one category

static void list_window_update(void) {
  if (!window_stack_contains_window(s_list_window)) {
    return;
  }
  const char *name = s_columns[s_list_column].name;
  if (s_list_total < 0) {
    snprintf(s_list_header_text, sizeof(s_list_header_text), "%s", name);
  } else {
    snprintf(s_list_header_text, sizeof(s_list_header_text), "%s · %d", name,
             s_list_total);
  }
  text_layer_set_text(s_list_header_layer, s_list_header_text);

  bool has_issues = s_issue_count > 0;
  layer_set_hidden(menu_layer_get_layer(s_list_menu_layer), !has_issues);
  layer_set_hidden(text_layer_get_layer(s_list_message_layer), has_issues);
  text_layer_set_text(s_list_message_layer,
                      s_list_total == 0 ? "Nothing here." : "Loading...");
  menu_layer_reload_data(s_list_menu_layer);
}

static uint16_t list_get_num_rows(MenuLayer *menu_layer, uint16_t section, void *context) {
  return s_issue_count;
}

static int16_t list_get_cell_height(MenuLayer *menu_layer, MenuIndex *index, void *context) {
  return ISSUE_ROW_HEIGHT;
}

static void list_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index,
                          void *context) {
  const Issue *issue = &s_issues[index->row];
  GRect bounds = layer_get_bounds(cell_layer);
  bool highlighted = menu_cell_layer_is_highlighted(cell_layer);

  graphics_context_set_fill_color(ctx, stripe_color(issue));
  graphics_fill_rect(ctx, GRect(0, 0, STRIPE_WIDTH, bounds.size.h), 0, GCornerNone);

  int left = STRIPE_WIDTH + ROW_PADDING;
  int width = bounds.size.w - left - ROW_PADDING;
  GFont key_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont text_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);

  // The key on the left and the assignee on the right of the first line.
  graphics_context_set_text_color(ctx, highlighted ? GColorWhite : GColorBlack);
  GRect key_frame = GRect(left, -2, width, 22);
  graphics_draw_text(ctx, issue->key, key_font, key_frame, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  int key_width = graphics_text_layout_get_content_size(
      issue->key, key_font, key_frame, GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft).w + ROW_PADDING;
  graphics_context_set_text_color(ctx, highlighted ? GColorLightGray : GColorDarkGray);
  graphics_draw_text(ctx, issue->assignee[0] ? issue->assignee : "Unassigned", text_font,
                     GRect(left + key_width, -2, width - key_width, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  graphics_context_set_text_color(ctx, highlighted ? GColorWhite : GColorBlack);
  graphics_draw_text(ctx, issue->summary, text_font, GRect(left, 17, width, 42),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // A hairline between rows.
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(STRIPE_WIDTH, bounds.size.h - 1),
                     GPoint(bounds.size.w, bounds.size.h - 1));
}

static void list_select(MenuLayer *menu_layer, MenuIndex *index, void *context) {
  if (index->row >= s_issue_count) {
    return;
  }
  s_issue_index = index->row;
  window_stack_push(s_issue_window, true);
}

static void list_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_list_header_layer = add_header_layer(root, bounds);
  text_layer_set_background_color(s_list_header_layer,
                                  category_dark_color(s_columns[s_list_column].category));

  GRect body = GRect(0, HEADER_HEIGHT, bounds.size.w, bounds.size.h - HEADER_HEIGHT);
  s_list_menu_layer = menu_layer_create(body);
  menu_layer_set_callbacks(s_list_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = list_get_num_rows,
    .get_cell_height = list_get_cell_height,
    .draw_row = list_draw_row,
    .select_click = list_select,
  });
  menu_layer_set_highlight_colors(s_list_menu_layer, GColorOxfordBlue, GColorWhite);
  menu_layer_set_click_config_onto_window(s_list_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_list_menu_layer));

  s_list_message_layer = add_message_layer(root, bounds);
}

static void list_window_appear(Window *window) {
  list_window_update();
}

static void list_window_unload(Window *window) {
  text_layer_destroy(s_list_header_layer);
  menu_layer_destroy(s_list_menu_layer);
  text_layer_destroy(s_list_message_layer);
}

static void open_column(int column) {
  s_list_column = column;
  s_list_total = -1;
  s_issue_count = 0;
  window_stack_push(s_list_window, true);

  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_int32(iter, MESSAGE_KEY_RequestColumn, column);
    app_message_outbox_send();
  }
}

// ---------------------------------------------------------------------------
// Home window: the board's columns

static void home_window_update(void) {
  text_layer_set_text(s_sprint_name_layer, s_sprint_loaded ? s_sprint_name : "Sprint");

  if (s_days_left > 0) {
    snprintf(s_days_left_text, sizeof(s_days_left_text), "%dd left", s_days_left);
  } else {
    snprintf(s_days_left_text, sizeof(s_days_left_text), s_days_left == 0 ? "Today" : "Ended");
  }
  text_layer_set_text(s_days_left_layer, s_days_left_text);
  layer_set_hidden(text_layer_get_layer(s_days_left_layer), !s_sprint_loaded);

  // The header turns red near the end of the sprint.
  bool ending = s_sprint_loaded && s_days_left <= DAYS_LEFT_WARNING;
  GColor header_color = ending ? GColorDarkCandyAppleRed : GColorCobaltBlue;
  text_layer_set_background_color(s_home_header_layer, header_color);

  layer_set_hidden(menu_layer_get_layer(s_home_menu_layer), !s_sprint_loaded);
  layer_set_hidden(text_layer_get_layer(s_home_message_layer), s_sprint_loaded);
  text_layer_set_text(s_home_message_layer, s_home_message[0] ? s_home_message : "Loading...");
  menu_layer_reload_data(s_home_menu_layer);
}

static uint16_t home_get_num_rows(MenuLayer *menu_layer, uint16_t section, void *context) {
  return s_column_count;
}

static int16_t home_get_cell_height(MenuLayer *menu_layer, MenuIndex *index, void *context) {
  return COLUMN_ROW_HEIGHT;
}

static void home_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *index,
                          void *context) {
  const Column *column = &s_columns[index->row];
  GRect bounds = layer_get_bounds(cell_layer);

  graphics_context_set_fill_color(ctx, category_color(column->category));
  graphics_fill_rect(ctx, GRect(0, 0, STRIPE_WIDTH, bounds.size.h), 0, GCornerNone);

  int left = STRIPE_WIDTH + ROW_PADDING;
  int width = bounds.size.w - left - ROW_PADDING;
  int count_width = 36;
  graphics_context_set_text_color(ctx, menu_cell_layer_is_highlighted(cell_layer)
                                           ? GColorWhite
                                           : GColorBlack);
  graphics_draw_text(ctx, column->name, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(left, 3, width - count_width, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  char count[8];
  snprintf(count, sizeof(count), "%d", column->issue_count);
  graphics_draw_text(ctx, count, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(left, 0, width, 32), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);

  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(STRIPE_WIDTH, bounds.size.h - 1),
                     GPoint(bounds.size.w, bounds.size.h - 1));
}

static void home_select(MenuLayer *menu_layer, MenuIndex *index, void *context) {
  // The menu still takes clicks while it's hidden behind a message.
  if (s_sprint_loaded && index->row < s_column_count) {
    open_column(index->row);
  }
}

static void home_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // A colored bar, with the sprint name on its left and the days left on its right.
  s_home_header_layer = add_header_layer(root, bounds);

  int days_width = 60;
  s_sprint_name_layer = add_header_layer(root, bounds);
  layer_set_frame(text_layer_get_layer(s_sprint_name_layer),
                  GRect(MARGIN, 0, bounds.size.w - days_width - MARGIN, HEADER_HEIGHT));
  text_layer_set_background_color(s_sprint_name_layer, GColorClear);
  text_layer_set_text_alignment(s_sprint_name_layer, GTextAlignmentLeft);

  s_days_left_layer = add_header_layer(root, bounds);
  layer_set_frame(text_layer_get_layer(s_days_left_layer),
                  GRect(bounds.size.w - days_width, 0, days_width - MARGIN, HEADER_HEIGHT));
  text_layer_set_background_color(s_days_left_layer, GColorClear);
  text_layer_set_text_alignment(s_days_left_layer, GTextAlignmentRight);

  GRect body = GRect(0, HEADER_HEIGHT, bounds.size.w, bounds.size.h - HEADER_HEIGHT);
  s_home_menu_layer = menu_layer_create(body);
  menu_layer_set_callbacks(s_home_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = home_get_num_rows,
    .get_cell_height = home_get_cell_height,
    .draw_row = home_draw_row,
    .select_click = home_select,
  });
  menu_layer_set_highlight_colors(s_home_menu_layer, GColorOxfordBlue, GColorWhite);
  menu_layer_set_click_config_onto_window(s_home_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_home_menu_layer));

  s_home_message_layer = add_message_layer(root, bounds);

  home_window_update();
}

static void home_window_unload(Window *window) {
  text_layer_destroy(s_home_header_layer);
  text_layer_destroy(s_sprint_name_layer);
  text_layer_destroy(s_days_left_layer);
  menu_layer_destroy(s_home_menu_layer);
  text_layer_destroy(s_home_message_layer);
}

// ---------------------------------------------------------------------------
// Messages from the phone

// Back to the home window, as when the sprint reloads or fails.
static void close_lists(void) {
  window_stack_remove(s_issue_window, false);
  window_stack_remove(s_list_window, false);
}

static int find_int(DictionaryIterator *iter, uint32_t key) {
  Tuple *tuple = dict_find(iter, key);
  return tuple ? tuple->value->int32 : 0;
}

static void copy_string(char *dest, size_t size, DictionaryIterator *iter, uint32_t key) {
  Tuple *tuple = dict_find(iter, key);
  snprintf(dest, size, "%s", tuple ? tuple->value->cstring : "");
}

static void receive_sprint(DictionaryIterator *iter) {
  copy_string(s_sprint_name, sizeof(s_sprint_name), iter, MESSAGE_KEY_SprintName);
  s_days_left = find_int(iter, MESSAGE_KEY_SprintDaysLeft);
  s_column_total = find_int(iter, MESSAGE_KEY_ColumnTotal);
  s_column_count = 0;
  s_sprint_loaded = true;
  s_home_message[0] = '\0';
}

static void receive_column(DictionaryIterator *iter, int index) {
  if (index < 0 || index >= MAX_COLUMNS || index >= s_column_total) {
    return;
  }
  Column *column = &s_columns[index];
  copy_string(column->name, sizeof(column->name), iter, MESSAGE_KEY_ColumnName);
  column->issue_count = find_int(iter, MESSAGE_KEY_ColumnIssues);
  column->category = find_int(iter, MESSAGE_KEY_ColumnCategory);
  if (index >= s_column_count) {
    s_column_count = index + 1;
  }
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
  issue->category = find_int(iter, MESSAGE_KEY_IssueCategory);
  issue->urgent = find_int(iter, MESSAGE_KEY_IssueUrgent) != 0;

  if (index >= s_issue_count) {
    s_issue_count = index + 1;
  }
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *error = dict_find(iter, MESSAGE_KEY_Error);
  Tuple *sprint = dict_find(iter, MESSAGE_KEY_SprintName);
  Tuple *column = dict_find(iter, MESSAGE_KEY_ColumnIndex);
  Tuple *list = dict_find(iter, MESSAGE_KEY_ListColumn);
  Tuple *index = dict_find(iter, MESSAGE_KEY_IssueIndex);
  bool list_open = window_stack_contains_window(s_list_window);

  if (error) {
    snprintf(s_home_message, sizeof(s_home_message), "%s", error->value->cstring);
    s_sprint_loaded = false;
    close_lists();
  } else if (sprint) {
    // A reloaded sprint replaces any list taken from the old one.
    receive_sprint(iter);
    close_lists();
  } else if (column) {
    receive_column(iter, column->value->int32);
  } else if (list && list_open && list->value->int32 == s_list_column) {
    s_list_total = find_int(iter, MESSAGE_KEY_ListCount);
    s_issue_count = 0;
  } else if (index && list_open && s_list_total >= 0) {
    receive_issue(iter, index->value->int32);
  }
  home_window_update();
  list_window_update();
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
    .appear = list_window_appear,
    .unload = list_window_unload,
  });

  s_home_window = window_create();
  window_set_window_handlers(s_home_window, (WindowHandlers) {
    .load = home_window_load,
    .unload = home_window_unload,
  });
  window_stack_push(s_home_window, true);
}

static void deinit(void) {
  window_destroy(s_home_window);
  window_destroy(s_list_window);
  window_destroy(s_issue_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
