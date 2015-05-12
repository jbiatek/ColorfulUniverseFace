#include <pebble.h>
#include "main.h"

#define DEBUG false
  
#define CIRCLES 256
#define RADIUS 10
  
#define TIME_HEIGHT 50
#define TIME_WIDTH 144
  
static Window *s_main_window; 
static TextLayer *s_time_layer;
static Layer *s_canvas_layer;
static unsigned int rand_seed;

static GColor random_color() {
  return (GColor8) { .argb = ((rand() % 0b00111111) + 0b11000000)  };
}

static GColor invert(GColor c) {
  return (GColor8) { .argb = (0b00111111 ^ c.argb) };
}

static uint16_t int_sqrt(uint16_t n) {
  uint16_t ret = n;
  uint16_t prev = 0;
  while (prev - ret != 0) {
    prev = ret;
    ret = (prev + (n / prev)) / 2;
  }
  return ret;
}

static void background_layer_draw(Layer *layer, GContext *ctx) {
  srand(rand_seed);
  GRect bounds = layer_get_bounds(layer);
//  graphics_context_set_stroke_width(ctx, 3);
 

  // Make a whole bunch of random color circles
  size_t i;
  for (i = 0; i < CIRCLES; i++) {
    GColor color = random_color();
    //GColor color = GColorLightGray;
    if (rand() % 2) {
      graphics_context_set_fill_color(ctx, color);
      uint16_t x = (rand() % bounds.size.w);
      //x = (x * x) / (bounds.size.w);
      uint16_t y = (rand() % bounds.size.h);
      //y = (y * y) / (bounds.size.h);
      uint16_t r = (rand() % RADIUS) + 1;
      x = (rand() % 10)+((RADIUS-r+1) * x / RADIUS);
      y = (rand() % 10)+((RADIUS-r+1) * y / RADIUS);
      graphics_fill_circle(ctx, GPoint(x, y), r);
    } else {
      graphics_context_set_stroke_color(ctx, color);
      uint16_t x = (rand() % bounds.size.w);
      uint16_t y = (rand() % bounds.size.h);
      uint16_t l = (rand() % RADIUS) + 3;
      graphics_draw_line(ctx, GPoint(x, y), GPoint(x-l, y-l));

    }
    
  }
  /*
  // Draw a white filled circle a radius of half the layer height
  graphics_context_set_fill_color(ctx, GColorWhite);
  const int16_t half_h = bounds.size.h / 2;
  const int16_t half_w = bounds.size.w / 2;
  graphics_fill_circle(ctx, GPoint(half_w, half_h), half_w);
  */
}

static void color_time_display() {
  srand(rand_seed);
  GColor rand_bg = random_color();
  GColor rand_fg = invert(rand_bg);
  text_layer_set_background_color(s_time_layer, rand_bg);
  text_layer_set_text_color(s_time_layer, rand_fg);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_set_background_color(window, GColorBlack);
  GRect window_bounds = layer_get_bounds(window_layer);

  // Create the background
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
  rand_seed = time(NULL);
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