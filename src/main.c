#include <pebble.h>
#include "main.h"

#define DEBUG false
  
#define CIRCLES 512
#define RADIUS 13
  
#define TIME_HEIGHT 50
#define TIME_WIDTH 144
#define ANIMATION_DURATION 1000
  
static Window *s_main_window; 
static TextLayer *s_time_layer;
static Layer *s_canvas_layer;
static AnimationProgress s_animation_counter;

static unsigned int s_rand_seed;

#ifdef PBL_COLOR
static GColor random_color() {
  return (GColor8) { .argb = ((rand() % 0b00111111) + 0b11000000)  };
}

static GColor invert(GColor c) {
  return (GColor8) { .argb = (0b00111111 ^ c.argb) };
}
#endif

/*static uint16_t int_sqrt(uint16_t n) {
  uint16_t ret = n;
  uint16_t prev = 0;
  while (prev - ret != 0) {
    prev = ret;
    ret = (prev + (n / prev)) / 2;
  }
  return ret;
}*/

static void background_layer_draw(Layer *layer, GContext *ctx) {
  // Reset our random to what it (possibly) was before. 
  srand(s_rand_seed);
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), 0, GCornerNone);
  
  int16_t width = bounds.size.w * 3/2;
  int16_t height = bounds.size.h * 3/2;

  // Make a whole bunch of random color circles and lines
  size_t i;
  for (i = 0; i < CIRCLES; i++) {
    #ifdef PBL_COLOR
    GColor color = random_color();
    #else
    GColor color = GColorWhite;
    #endif
    //GColor color = GColorLightGray;
    if (rand() % 2) {
      graphics_context_set_fill_color(ctx, color);
      int16_t x = (rand() % width);
      //x = (x * x) / (bounds.size.w);
      int16_t y = (rand() % height);
      //y = (y * y) / (bounds.size.h);
      int16_t r = (rand() % RADIUS) + 1;
      // (some random bumping left/down) + move it right and up
      // proportionally to its size. 
      x = (rand() % 10)+((RADIUS-(2*r)+1) * x / RADIUS);
      y = (rand() % 10)+((RADIUS-(2*r)+1) * y / RADIUS);
      // Animation: 
      x = x * s_animation_counter / ANIMATION_NORMALIZED_MAX;
      y = y * s_animation_counter / ANIMATION_NORMALIZED_MAX;
      graphics_fill_circle(ctx, GPoint(x, y), r);
      #ifdef PBL_BW
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_circle(ctx, GPoint(x, y), r-1);
      #endif
    } else {
      graphics_context_set_stroke_color(ctx, color);
      int16_t x = (rand() % width);
      int16_t y = (rand() % height);
      int16_t l = (rand() % RADIUS) + 3;
      // Animation:
      int16_t shift = (x > y ? x : y) + l;
      x = x + (-shift * (ANIMATION_NORMALIZED_MAX - s_animation_counter) / ANIMATION_NORMALIZED_MAX);
      y = y + (-shift * (ANIMATION_NORMALIZED_MAX - s_animation_counter) / ANIMATION_NORMALIZED_MAX);
      //x = x * s_animation_counter / ANIMATION_NORMALIZED_MAX;
      //y = y * s_animation_counter / ANIMATION_NORMALIZED_MAX;

      graphics_draw_line(ctx, GPoint(x, y), GPoint(x-x/l, y-y/l));
    }
    
  }
}

static void color_time_display() {
  #ifdef PBL_COLOR
  srand(s_rand_seed);
  GColor rand_bg = random_color();
  GColor rand_fg = invert(rand_bg);
  text_layer_set_background_color(s_time_layer, rand_bg);
  text_layer_set_text_color(s_time_layer, rand_fg);
  #else
  text_layer_set_background_color(s_time_layer, GColorBlack);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  #endif
}




static void canvas_anim_update(Animation *animation, const AnimationProgress progress) {
  s_animation_counter = progress;
  layer_mark_dirty(s_canvas_layer);
}

static const AnimationImplementation canvas_anim_impl = {
  .update = canvas_anim_update
};

static void create_animation() { 
  // This will be destroyed when it finishes apparently
  static struct Animation *s_animation;
  
  // Except on APLITE
  #ifdef PBL_PLATFORM_APLITE
  if (s_animation) animation_destroy(s_animation);    
  #endif

  s_animation = animation_create();
  animation_set_duration(s_animation, ANIMATION_DURATION);
  animation_set_curve(s_animation, AnimationCurveEaseOut);
  animation_set_implementation(s_animation, &canvas_anim_impl);

  animation_schedule(s_animation);
}



static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_set_background_color(window, GColorBlack);
  GRect window_bounds = layer_get_bounds(window_layer);

  // Create the background canvas
  s_canvas_layer = layer_create(GRect(0, 0, window_bounds.size.w, window_bounds.size.h-TIME_HEIGHT));
  layer_set_update_proc(s_canvas_layer, background_layer_draw);
  layer_add_child(window_layer, s_canvas_layer);
  
  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(0, window_bounds.size.h - TIME_HEIGHT, TIME_WIDTH, TIME_HEIGHT));
  color_time_display();
  text_layer_set_text(s_time_layer, "--:--");
  
  // Improve the layout to be more like a watchface
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
}

static void main_window_unload(Window *window) {
  // Destroy canvas
  layer_destroy(s_canvas_layer);
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
}

static void update_time() {
  s_rand_seed = time(NULL);
  s_animation_counter = ANIMATION_NORMALIZED_MIN;
  color_time_display();
  
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[] = "--:--";

  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    // Use 24 hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    // Use 12 hour format
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);
  // Show the animation
  create_animation();
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Register with TickTimerService
  if (DEBUG) {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  }
}

static void deinit() {
    // Destroy Window
    window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}