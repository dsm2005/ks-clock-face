#include <pebble.h>

#define COLORS       PBL_IF_COLOR_ELSE(true, false)
#define ANTIALIASING true

#define STROKE_WIDTH 3 /* originally 4 */
#define HAND_MARGIN  25
#define FINAL_RADIUS 100

#define ANIMATION_DURATION 500
#define ANIMATION_DELAY    600

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;

static BitmapLayer *s_background_layers[2];
static GBitmap *s_background_bitmaps[2];
static int s_current_background_layer_index;

static GPoint s_center;
static Time s_last_time, s_anim_time;
static int s_radius = 0;
static bool s_animating = false;

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if(handlers) {
    animation_set_handlers(anim, (AnimationHandlers) {
      .started = animation_started,
      .stopped = animation_stopped
    }, NULL);
  }
  animation_schedule(anim);
}

/************************************ UI **************************************/

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  s_last_time.hours = tick_time->tm_hour % 12;
  s_last_time.minutes = tick_time->tm_min;

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static int hours_to_minutes(int hours_out_of_12) {
  return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void update_background_proc(Layer *layer, GContext *ctx) {
  /* s_background_layer
  uint8_t background_index = s_last_time.minutes % 2;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "entering update_background_proc");
  s_background_bitmap = s_background_bitmaps[background_index];
  APP_LOG(APP_LOG_LEVEL_DEBUG, "set background index to %d", background_index);
  bitmap_layer_set_bitmap((BitmapLayer) layer, s_background_bitmap);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "leaving update_background_proc"); */
}

static void update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_antialiased(ctx, ANTIALIASING);
  
  int new_background_layer_index = s_last_time.minutes % 2; // fixme should be hours, but need to test quicker
  if (new_background_layer_index != s_current_background_layer_index) {
    layer_set_hidden(bitmap_layer_get_layer(s_background_layers[1]), !((bool) new_background_layer_index));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "background_layer_index change from %d to %d", s_current_background_layer_index, new_background_layer_index);
    bool layer1_hidden = layer_get_hidden(bitmap_layer_get_layer(s_background_layers[1]));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "layer1_hidden now %s", layer1_hidden ? "true" : "false");
    s_current_background_layer_index = new_background_layer_index;
  }

  // Don't use current time while animating
  Time mode_time = (s_animating) ? s_anim_time : s_last_time;

  // Adjust for minutes through the hour
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  float hour_angle;
  if(s_animating) {
    // Hours out of 60 for smoothness
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
  } else {
    hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  }
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

  // Plot hands
  GPoint minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
  };

  // Draw hands with positive length only
  if(s_radius > 2 * HAND_MARGIN) {
    graphics_context_set_stroke_color(ctx, GColorOxfordBlue);
    graphics_context_set_stroke_width(ctx, STROKE_WIDTH+1);
    graphics_draw_line(ctx, s_center, hour_hand);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, STROKE_WIDTH);
    graphics_draw_line(ctx, s_center, hour_hand);
  }
  if(s_radius > HAND_MARGIN) {
    graphics_context_set_stroke_color(ctx, GColorOxfordBlue);
    graphics_context_set_stroke_width(ctx, STROKE_WIDTH+1);
    graphics_draw_line(ctx, s_center, minute_hand);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, STROKE_WIDTH);
    graphics_draw_line(ctx, s_center, minute_hand);
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);
  
  s_background_bitmaps[0] = gbitmap_create_with_resource(RESOURCE_ID_SCRIPT_CAL_LOGO);
  s_background_layers[0] = bitmap_layer_create(window_bounds);
  bitmap_layer_set_bitmap(s_background_layers[0], s_background_bitmaps[0]);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layers[0]));
  
  s_background_bitmaps[1] = gbitmap_create_with_resource(RESOURCE_ID_BEAR_LOGO);
  s_background_layers[1] = bitmap_layer_create(window_bounds);
  bitmap_layer_set_bitmap(s_background_layers[1], s_background_bitmaps[1]);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layers[1]));
  s_current_background_layer_index = 1;

  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
  gbitmap_destroy(s_background_bitmaps[0]);
  gbitmap_destroy(s_background_bitmaps[1]);
  layer_destroy(bitmap_layer_get_layer(s_background_layers[0]));
  layer_destroy(bitmap_layer_get_layer(s_background_layers[1]));
  layer_destroy(s_canvas_layer);
}

/*********************************** App **************************************/

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
  s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);

  layer_mark_dirty(s_canvas_layer);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
  s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
  s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);

  layer_mark_dirty(s_canvas_layer);
}

static void init() {
  srand(time(NULL));

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  tick_handler(time_now, MINUTE_UNIT);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Prepare animations
  AnimationImplementation radius_impl = {
    .update = radius_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);

  AnimationImplementation hands_impl = {
    .update = hands_update
  };
  animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
}

static void deinit() {
  window_destroy(s_main_window);  
}

int main() {
  init();
  app_event_loop();
  deinit();
}
