#include <pebble.h>

#define MAX_SHORTCUTS 8
#define NAME_MAX_LEN 25
#define SETTINGS_KEY 1

// Icon resource IDs in order matching package.json media array
static const uint32_t ICON_RESOURCES[] = {
  RESOURCE_ID_ICON_GENERIC,
  RESOURCE_ID_ICON_UNLOCK,
  RESOURCE_ID_ICON_LOCK,
  RESOURCE_ID_ICON_LIGHT,
  RESOURCE_ID_ICON_GARAGE,
  RESOURCE_ID_ICON_SCENE,
  RESOURCE_ID_ICON_POWER,
  RESOURCE_ID_ICON_HOME,
};
#define NUM_ICON_TYPES (int)(sizeof(ICON_RESOURCES) / sizeof(ICON_RESOURCES[0]))

// --- Persisted settings ---

typedef struct {
  int count;
  char names[MAX_SHORTCUTS][NAME_MAX_LEN];
  uint8_t icons[MAX_SHORTCUTS];
} __attribute__((__packed__)) Settings;

static Settings s_settings;

// --- UI elements ---
static Window *s_main_window;
static MenuLayer *s_menu_layer;
static GBitmap *s_icons[NUM_ICON_TYPES];
static TextLayer *s_empty_text;

// Result dialog
static Window *s_result_window;
static TextLayer *s_result_label;
static Layer *s_result_bg_layer;
static Layer *s_result_icon_layer;
static GBitmap *s_result_icon_bitmap;
static Animation *s_result_anim;
static AppTimer *s_dismiss_timer;
static char s_result_buffer[64];
static bool s_result_is_success;

#define DIALOG_MARGIN 10
#define RESULT_DISMISS_MS 2000

// --- Persistence ---

static void load_settings(void) {
  s_settings.count = 0;
  memset(s_settings.names, 0, sizeof(s_settings.names));
  memset(s_settings.icons, 0, sizeof(s_settings.icons));
  persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

static void save_settings(void) {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

// ============================================================
// Result dialog (animated, colored)
// ============================================================

static void result_bg_update(Layer *layer, GContext *ctx) {
  GColor bg = PBL_IF_COLOR_ELSE(
    s_result_is_success ? GColorGreen : GColorRed,
    GColorWhite
  );
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, 0);
}

static void result_icon_update(Layer *layer, GContext *ctx) {
  if (!s_result_icon_bitmap) return;
  GRect bounds = layer_get_bounds(layer);
  GRect bmp = gbitmap_get_bounds(s_result_icon_bitmap);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, s_result_icon_bitmap,
    (GRect){.origin = bounds.origin, .size = bmp.size});
}

static void result_anim_stopped(Animation *animation, bool finished, void *ctx) {
  s_result_anim = NULL;
}

static void result_dismiss_callback(void *data) {
  s_dismiss_timer = NULL;
  window_stack_remove(s_result_window, true);
}

static void result_window_appear(Window *window) {
  if (s_result_anim) animation_unschedule(s_result_anim);

  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  GRect bmp_bounds = s_result_icon_bitmap ? gbitmap_get_bounds(s_result_icon_bitmap) : GRectZero;
  Layer *label_layer = text_layer_get_layer(s_result_label);

  GRect bg_start = layer_get_frame(s_result_bg_layer);
  GRect bg_finish = bounds;
  Animation *bg_anim = (Animation *)property_animation_create_layer_frame(
    s_result_bg_layer, &bg_start, &bg_finish);

  // Center icon + text vertically (icon height + gap + ~30px text)
  int content_height = bmp_bounds.size.h + 5 + 30;
  int top_offset = (bounds.size.h - content_height) / 2;

  GRect icon_start = layer_get_frame(s_result_icon_layer);
  GRect icon_finish = grect_inset(bounds, (GEdgeInsets){
    .top = top_offset,
    .left = (bounds.size.w - bmp_bounds.size.w) / 2
  });
  Animation *icon_anim = (Animation *)property_animation_create_layer_frame(
    s_result_icon_layer, &icon_start, &icon_finish);

  GRect label_start = layer_get_frame(label_layer);
  GRect label_finish = grect_inset(bounds, (GEdgeInsets){
    .top = top_offset + bmp_bounds.size.h + 5,
    .left = DIALOG_MARGIN, .right = DIALOG_MARGIN
  });
  Animation *label_anim = (Animation *)property_animation_create_layer_frame(
    label_layer, &label_start, &label_finish);

  s_result_anim = animation_spawn_create(bg_anim, icon_anim, label_anim, NULL);
  animation_set_handlers(s_result_anim, (AnimationHandlers){
    .stopped = result_anim_stopped
  }, NULL);
  animation_set_delay(s_result_anim, 200);
  animation_schedule(s_result_anim);

  if (s_dismiss_timer) app_timer_cancel(s_dismiss_timer);
  s_dismiss_timer = app_timer_register(RESULT_DISMISS_MS, result_dismiss_callback, NULL);
}

static void result_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_result_bg_layer = layer_create(GRect(0, bounds.size.h, bounds.size.w, bounds.size.h));
  layer_set_update_proc(s_result_bg_layer, result_bg_update);
  layer_add_child(root, s_result_bg_layer);

  s_result_icon_bitmap = gbitmap_create_with_resource(
    s_result_is_success ? RESOURCE_ID_ICON_CONFIRM : RESOURCE_ID_ICON_WARNING);
  GRect bmp_bounds = gbitmap_get_bounds(s_result_icon_bitmap);

  s_result_icon_layer = layer_create(GRect(
    (bounds.size.w - bmp_bounds.size.w) / 2, bounds.size.h + DIALOG_MARGIN,
    bmp_bounds.size.w, bmp_bounds.size.h));
  layer_set_update_proc(s_result_icon_layer, result_icon_update);
  layer_add_child(root, s_result_icon_layer);

  s_result_label = text_layer_create(GRect(
    DIALOG_MARGIN, bounds.size.h + DIALOG_MARGIN + bmp_bounds.size.h,
    bounds.size.w - 2 * DIALOG_MARGIN, bounds.size.h));
  text_layer_set_text(s_result_label, s_result_buffer);
  text_layer_set_background_color(s_result_label, GColorClear);
  text_layer_set_text_color(s_result_label, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  text_layer_set_text_alignment(s_result_label, GTextAlignmentCenter);
  text_layer_set_font(s_result_label, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(root, text_layer_get_layer(s_result_label));
}

static void result_window_unload(Window *window) {
  if (s_dismiss_timer) { app_timer_cancel(s_dismiss_timer); s_dismiss_timer = NULL; }
  layer_destroy(s_result_bg_layer);
  layer_destroy(s_result_icon_layer);
  text_layer_destroy(s_result_label);
  gbitmap_destroy(s_result_icon_bitmap);
  s_result_icon_bitmap = NULL;
  window_destroy(s_result_window);
  s_result_window = NULL;
}

static void show_result(bool success, const char *text) {
  s_result_is_success = success;
  snprintf(s_result_buffer, sizeof(s_result_buffer), "%s", text);

  if (s_result_window) window_stack_remove(s_result_window, false);

  s_result_window = window_create();
  window_set_background_color(s_result_window, GColorBlack);
  window_set_window_handlers(s_result_window, (WindowHandlers){
    .load = result_window_load,
    .unload = result_window_unload,
    .appear = result_window_appear,
  });
  window_stack_push(s_result_window, true);
}

// ============================================================
// AppMessage handlers
// ============================================================

static void rebuild_menu(void);

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  // Check if this is a config update (has ShortcutCount)
  Tuple *count_t = dict_find(iter, MESSAGE_KEY_ShortcutCount);
  if (count_t) {
    s_settings.count = count_t->value->int32;
    if (s_settings.count > MAX_SHORTCUTS) s_settings.count = MAX_SHORTCUTS;

    for (int i = 0; i < MAX_SHORTCUTS; i++) {
      Tuple *name_t = dict_find(iter, MESSAGE_KEY_Name0 + i);
      if (name_t && name_t->length > 0) {
        strncpy(s_settings.names[i], name_t->value->cstring, NAME_MAX_LEN - 1);
        s_settings.names[i][NAME_MAX_LEN - 1] = '\0';
      } else {
        s_settings.names[i][0] = '\0';
      }

      Tuple *icon_t = dict_find(iter, MESSAGE_KEY_Icon0 + i);
      if (icon_t) {
        s_settings.icons[i] = (uint8_t)icon_t->value->int32;
        if (s_settings.icons[i] >= NUM_ICON_TYPES) s_settings.icons[i] = 0;
      } else {
        s_settings.icons[i] = 0;
      }
    }

    save_settings();
    rebuild_menu();
    return;
  }

  // Otherwise it's a result from an HTTP call
  Tuple *code_t = dict_find(iter, MESSAGE_KEY_ResultCode);
  Tuple *text_t = dict_find(iter, MESSAGE_KEY_ResultText);
  if (code_t && text_t) {
    int code = code_t->value->int32;
    if (code == 200) {
      vibes_short_pulse();
      show_result(true, "Done!");
    } else {
      vibes_double_pulse();
      show_result(false, text_t->value->cstring);
    }
  }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

static void outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", reason);
  show_result(false, "Send failed");
}

// ============================================================
// Send shortcut request to JS
// ============================================================

static void send_shortcut(int index) {
  DictionaryIterator *out;
  AppMessageResult result = app_message_outbox_begin(&out);
  if (result != APP_MSG_OK) {
    show_result(false, "Error");
    return;
  }

  dict_write_int32(out, MESSAGE_KEY_ShortcutIndex, index);
  dict_write_end(out);

  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    show_result(false, "Error");
    return;
  }

  show_result(true, "Sending...");
}

// ============================================================
// MenuLayer callbacks
// ============================================================

static uint16_t get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *ctx) {
  return s_settings.count > 0 ? s_settings.count : 0;
}

static int16_t get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *ctx) {
  return PBL_IF_ROUND_ELSE(
    menu_layer_is_index_selected(menu_layer, cell_index) ? 68 : 40,
    44
  );
}

static void draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  int idx = cell_index->row;
  if (idx < 0 || idx >= s_settings.count) return;

  uint8_t icon_idx = s_settings.icons[idx];
  if (icon_idx >= NUM_ICON_TYPES) icon_idx = 0;

  menu_cell_basic_draw(ctx, cell_layer, s_settings.names[idx], NULL, s_icons[icon_idx]);
}

static void select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *ctx) {
  int idx = cell_index->row;
  if (idx < 0 || idx >= s_settings.count) return;
  send_shortcut(idx);
}

// ============================================================
// Menu rebuild
// ============================================================

static void rebuild_menu(void) {
  if (!s_menu_layer) return;

  if (s_settings.count == 0) {
    layer_set_hidden(menu_layer_get_layer(s_menu_layer), true);
    layer_set_hidden(text_layer_get_layer(s_empty_text), false);
  } else {
    layer_set_hidden(menu_layer_get_layer(s_menu_layer), false);
    layer_set_hidden(text_layer_get_layer(s_empty_text), true);
    menu_layer_reload_data(s_menu_layer);
  }
}

// ============================================================
// Main window
// ============================================================

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Load all icon types
  for (int i = 0; i < NUM_ICON_TYPES; i++) {
    s_icons[i] = gbitmap_create_with_resource(ICON_RESOURCES[i]);
  }

  // Empty state text
  s_empty_text = text_layer_create(GRect(10, bounds.size.h / 2 - 30, bounds.size.w - 20, 60));
  text_layer_set_text(s_empty_text, "Open Settings\non phone\nto configure");
  text_layer_set_text_alignment(s_empty_text, GTextAlignmentCenter);
  text_layer_set_font(s_empty_text, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(root, text_layer_get_layer(s_empty_text));

  // Menu
  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows_callback,
    .get_cell_height = get_cell_height_callback,
    .draw_row = draw_row_callback,
    .select_click = select_callback,
  });
  menu_layer_set_click_config_onto_window(s_menu_layer, window);

#ifdef PBL_COLOR
  menu_layer_set_highlight_colors(s_menu_layer, GColorCobaltBlue, GColorWhite);
#endif

  layer_add_child(root, menu_layer_get_layer(s_menu_layer));

  rebuild_menu();
}

static void main_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;
  text_layer_destroy(s_empty_text);
  for (int i = 0; i < NUM_ICON_TYPES; i++) {
    if (s_icons[i]) { gbitmap_destroy(s_icons[i]); s_icons[i] = NULL; }
  }
}

// ============================================================
// App lifecycle
// ============================================================

static void init(void) {
  load_settings();

  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_register_outbox_failed(outbox_failed_handler);
  app_message_open(1024, 256);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers){
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
