#include <pebble.h>

#define COLORS       PBL_IF_COLOR_ELSE(true, false)
#define ANTIALIASING true

#define HAND_MARGIN  15
#define FINAL_RADIUS 80
#define TIME_VAL_RADIUS 15

#define ANIMATION_DURATION 500
#define ANIMATION_DELAY    600

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;
static char s_time_as_string[3];

static GPoint s_center;
static Time s_last_time, s_anim_time;
static int s_radius = 0, s_anim_hours_60 = 0;
static bool s_animating = false;
static GColor inner_cricle_color, outer_circle_color, stroke_color;


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
  s_last_time.hours = tick_time->tm_hour;

  if(s_last_time.minutes != tick_time->tm_min) { 
    s_last_time.minutes = tick_time->tm_min;
  }

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static int hours_to_minutes(int hours_out_of_12) {
  return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void draw_number_proportional_to_value(GContext *ctx, int number, int value, GPoint position){
  snprintf(s_time_as_string, 3, "%02d", number);
  GFont font;
  int y_offset = 0;
  if (value > 0 && value <= 20) {
    font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    y_offset = 2;
  }
  else if (value > 20 && value <= 26) {
    font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
    y_offset = 3;
  }
  else if (value > 26 && value <= 30) {
    font = fonts_get_system_font(FONT_KEY_GOTHIC_24);
    y_offset = 4;
  }
  else {
    font = fonts_get_system_font(FONT_KEY_GOTHIC_28);
    y_offset = 5;
  }    
    
  GSize s = graphics_text_layout_get_content_size_with_attributes(s_time_as_string, font, 
                     GRect(0, 0, 30, 14), GTextOverflowModeFill, 
                     GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_time_as_string, font, 
                     GRect(position.x-(s.w/2), position.y-(s.h/2)-y_offset,
                     s.w, s.h), GTextOverflowModeFill, 
                     GTextAlignmentCenter, NULL);
}

static void update_proc(Layer *layer, GContext *ctx) {
  // Color background?
  GRect bounds = layer_get_bounds(layer);
//  if(COLORS) {
    inner_cricle_color = GColorWhite;
    outer_circle_color = GColorWhite;
    stroke_color = GColorBlack;

//  } else {
    graphics_context_set_fill_color(ctx, inner_cricle_color);
    graphics_context_set_stroke_color(ctx, stroke_color);
    graphics_context_set_text_color(ctx, stroke_color); 
// }
  graphics_context_set_antialiased(ctx, ANTIALIASING);
  
  int outer_radius = (s_radius-HAND_MARGIN>0) ? s_radius-HAND_MARGIN : 0;
  int inner_radius = (s_radius - (3.5 * HAND_MARGIN)>0) ? s_radius - (3.5 * HAND_MARGIN) : 0;
  int small_stroke_width = 3;
  int big_stroke_width = (outer_radius - inner_radius) - small_stroke_width;
  
  // Draw outline
  graphics_context_set_stroke_width(ctx,  small_stroke_width);
  graphics_draw_circle(ctx, s_center, outer_radius);
  graphics_context_set_fill_color(ctx, inner_cricle_color);
  graphics_draw_circle(ctx, s_center, inner_radius);
  
    
  // Don't use current time while animating
  Time mode_time = (s_animating) ? s_anim_time : s_last_time;

  // Adjust for minutes through the hour
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  float hour_angle;
  int hours_smoothness;
  if(s_animating) {
    // Hours out of 60 for smoothness
    hours_smoothness = 60;
  } else {
    hours_smoothness = 12;
  }
  hour_angle = TRIG_MAX_ANGLE * mode_time.hours / hours_smoothness;
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

  // Plot hands
  GPoint minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - (3.5 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - (3.5 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
  };

  // Draw hands with positive length only
  if(s_radius > 4 * HAND_MARGIN) {

    graphics_fill_circle(ctx, minute_hand, TIME_VAL_RADIUS);
    graphics_draw_circle(ctx, minute_hand, TIME_VAL_RADIUS);
    
    graphics_fill_circle(ctx, hour_hand, TIME_VAL_RADIUS);
    graphics_draw_circle(ctx, hour_hand, TIME_VAL_RADIUS);
    
    graphics_context_set_stroke_color(ctx, inner_cricle_color);  
    graphics_context_set_stroke_width(ctx, big_stroke_width);
    graphics_draw_circle(ctx, s_center, outer_radius - (big_stroke_width/2) - small_stroke_width/2.0);
    graphics_context_set_stroke_width(ctx, small_stroke_width);
    
    draw_number_proportional_to_value(ctx, mode_time.minutes, 2*TIME_VAL_RADIUS, minute_hand);
    draw_number_proportional_to_value(ctx, mode_time.hours, 2*TIME_VAL_RADIUS, hour_hand);

  }

}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);

  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
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
