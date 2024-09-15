#include <stdio.h>
#include <libretro.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#define LOW_FOOTPRINT 0

#include <cpu.h>
#include <hal.h>
#include <tamalib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#include <images/8.h>
#include <images/16.h>
#include <images/32.h>
#include <images/64.h>

#define TAMALR_AUDIO_FREQUENCY 44100
#define TAMALR_AUDIO_PERIOD (1.0f / TAMALR_AUDIO_FREQUENCY)
#define TAMALR_AUDIO_SAMPLES 44100 / 30

static int16_t audio_wavetable[9][TAMALR_AUDIO_SAMPLES];
static bool_t audio_wavetables_compiled = false;

typedef struct tamalr_t
{
  bool_t audio_playing;
  unsigned audio_sine_pos;
  uint32_t audio_frequency;
  int16_t *audio_samples;
  
  bool_t video_buffer[LCD_HEIGHT][LCD_WIDTH];
  uint16_t video_screen[256 * 256];
  bool_t video_icons[ICON_NUM];

  u12_t rom[12288 / 2];

  unsigned video_scale;
  
  hal_t hal;
} tamalr_t;

static tamalr_t tamalr;

void* tamalr_malloc(u32_t size)
{
  return malloc(size);
}

void tamalr_free(void* ptr)
{
  free(ptr);
}

void tamalr_halt(void)
{
}

bool_t tamalr_is_log_enabled(log_level_t level)
{
  return 0;
}

static const char *log_level(log_level_t level)
{
  switch (level)
  {
  case LOG_ERROR:
    return "ERROR";
  case LOG_INFO:
    return "INFO ";
  case LOG_MEMORY:
    return "MEM  ";
  case LOG_CPU:
    return "CPU  ";
  default:
    return "???  ";
  }
}

void tamalr_log(log_level_t level, char *buff, ...)
{
/*
  va_list args;
  va_start(args, buff);
  printf("[TAMALIB %s]: ", log_level(level));
  vprintf(buff, args);
  printf("\n");
  va_end(args);
*/
}

void tamalr_sleep_until(timestamp_t ts)
{
  //struct timespec sleep_time = { ts / 1000000, (ts % 1000000) * 1000 };
  //nanosleep(&sleep_time, NULL);
}

timestamp_t tamalr_get_timestamp(void)
{
  struct timespec current_time;

  clock_gettime(CLOCK_MONOTONIC, &current_time);
  timestamp_t ts = (timestamp_t)(current_time.tv_sec * 1000000LL + current_time.tv_nsec / 1000LL);

  return ts;
}

void tamalr_update_screen(void)
{
  for (unsigned y = 0; y < LCD_HEIGHT; y++)
  {
    for (unsigned x = 0; x < LCD_WIDTH; x++)
    {
      uint16_t color = tamalr.video_buffer[y][x] ? 0x0000 : 0xFFFF;

      for (unsigned sy = 0; sy < tamalr.video_scale; sy++)
      {
        for (unsigned sx = 0; sx < tamalr.video_scale; sx++)
        {
          tamalr.video_screen[(tamalr.video_scale * LCD_WIDTH) *
                              (tamalr.video_scale * (y + 8) + sy) +
                              (tamalr.video_scale * x + sx)] = color;
        }
      }
    }
  }

  const unsigned char** images;

  switch (tamalr.video_scale)
  {
  case 1:
    images = images_8;
    break;
  case 2:
    images = images_16;
    break;
  case 4:
    images = images_32;
    break;
  case 8:
    images = images_64;
    break;
  default:
    return;
  }

  for (unsigned i = 0; i < ICON_NUM; i++)
  {
    unsigned x = (i % 4) * 8;
    unsigned y = i >= 4 ? 24 * tamalr.video_scale : 0;

    /* Calculate base address in framebuffer where the image is copied */
    unsigned short *framebuffer_ptr = (unsigned short*)&tamalr.video_screen[
      (tamalr.video_scale * LCD_WIDTH * y) + (tamalr.video_scale * x)
    ];

    for (int h = 0; h < 8 * tamalr.video_scale; h++)
    {
      if (tamalr.video_icons[i])
      {
        memcpy(&framebuffer_ptr[h * tamalr.video_scale * LCD_WIDTH],
               &images[i][h * 16 * tamalr.video_scale],
               8 * tamalr.video_scale * 2);
      }
      else
      {
        memset(&framebuffer_ptr[h * tamalr.video_scale * LCD_WIDTH], 0,
               8 * tamalr.video_scale * 2);
      }
    }
  }
}

void tamalr_set_lcd_matrix(u8_t x, u8_t y, bool_t val)
{
  if (x < LCD_WIDTH && y < LCD_HEIGHT)
    tamalr.video_buffer[y][x] = val;
}

void tamalr_set_lcd_icon(u8_t icon, bool_t val)
{
  if (icon < ICON_NUM)
    tamalr.video_icons[icon] = val;
}

/* todo */
void tamalr_set_frequency(u32_t freq)
{
  tamalr.audio_frequency = freq;
  switch (freq)
  {
  case 40960:
    tamalr.audio_samples = audio_wavetable[0];
    break;
  case 32768:
    tamalr.audio_samples = audio_wavetable[1];
    break;
  case 27307:
    tamalr.audio_samples = audio_wavetable[2];
    break;
  case 23406:
    tamalr.audio_samples = audio_wavetable[3];
    break;
  case 20480:
    tamalr.audio_samples = audio_wavetable[4];
    break;
  case 16384:
    tamalr.audio_samples = audio_wavetable[5];
    break;
  case 13653:
    tamalr.audio_samples = audio_wavetable[6];
    break;
  case 11703:
    tamalr.audio_samples = audio_wavetable[7];
    break;
  default:
    tamalr.audio_samples = audio_wavetable[4];
  }
}

void tamalr_play_frequency(bool_t en)
{
  tamalr.audio_playing = en;
}

/* Stubbed, as this is not called from step mode */
int tamalr_handler(void)
{
  return 0;
}

/* Initialize the global HAL structure with the stubbed functions */
void init_tamalr_hal(void)
{
  tamalr.hal.malloc = tamalr_malloc;
  tamalr.hal.free = tamalr_free;
  tamalr.hal.halt = tamalr_halt;
  tamalr.hal.is_log_enabled = tamalr_is_log_enabled;
  tamalr.hal.log = tamalr_log;
  tamalr.hal.sleep_until = tamalr_sleep_until;
  tamalr.hal.get_timestamp = tamalr_get_timestamp;
  tamalr.hal.update_screen = tamalr_update_screen;
  tamalr.hal.set_lcd_matrix = tamalr_set_lcd_matrix;
  tamalr.hal.set_lcd_icon = tamalr_set_lcd_icon;
  tamalr.hal.set_frequency = tamalr_set_frequency;
  tamalr.hal.play_frequency = tamalr_play_frequency;
  tamalr.hal.handler = tamalr_handler;
}

/* libretro callbacks */
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_log_printf_t log_cb;
static struct retro_rumble_interface rumble;
static retro_video_refresh_t video_cb;

void display_message(const char *msg)
{
  char *str = (char*)calloc(4096, sizeof(char));
  struct retro_message rmsg;

  snprintf(str, 4096, "%s", msg);
  rmsg.frames = 300;
  rmsg.msg = str;
  environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &rmsg);
}

void handle_input(void)
{
  input_poll_cb();
  tamalib_set_button(BTN_LEFT, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y));
  tamalib_set_button(BTN_RIGHT, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A));
  tamalib_set_button(BTN_MIDDLE, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B));
}

/* libretro API */

#define PF_PI 3.141592653589793115997963468544185161590576171875f

/* Largest Taylor series factorial a float can hold */
#define PF_TERMS 32

float pf_factorial(u8_t x)
{
  float result = x;

  for (x--; x > 1; x--)
    result *= x;

  return result;
}

float pf_power(float x, u8_t power)
{
  float result = x;

  for (; power > 1; power--)
    result *= x;

  return result;
}

float pf_wave(float x, u8_t cosine)
{
  float next, result;
  u8_t positive = true;
  u8_t i;

  while (x > 2 * PF_PI)
    x -= 2 * PF_PI;
  result = cosine ? 1 : x;
  for (i = cosine ? 2 : 3; i <= PF_TERMS; i += 2)
  {
    positive ^= true;
    next = pf_power(x, i) / pf_factorial(i);
    result = positive ? result + next : result - next;
  }

  return result;
}

void retro_init(void)
{
  memset(&tamalr, 0, sizeof(tamalr));
  tamalr.video_scale = 8;

  if (!environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_cb))
    log_cb = NULL;

  if (!audio_wavetables_compiled)
  {
    int i;
  
    for (i = 0; i < TAMALR_AUDIO_SAMPLES; i++)
    {
      audio_wavetable[0][i] = pf_wave((2 * PF_PI * 4096.0 * (float)i * TAMALR_AUDIO_PERIOD), false);
      audio_wavetable[1][i] = pf_wave((2 * PF_PI * 3276.8 * (float)i * TAMALR_AUDIO_PERIOD), false);
      audio_wavetable[2][i] = pf_wave((2 * PF_PI * 2730.7 * (float)i * TAMALR_AUDIO_PERIOD), false);
      audio_wavetable[3][i] = pf_wave((2 * PF_PI * 2340.6 * (float)i * TAMALR_AUDIO_PERIOD), false);
      audio_wavetable[4][i] = pf_wave((2 * PF_PI * 2048.0 * (float)i * TAMALR_AUDIO_PERIOD), false);
      audio_wavetable[5][i] = pf_wave((2 * PF_PI * 1638.4 * (float)i * TAMALR_AUDIO_PERIOD), false);
      audio_wavetable[6][i] = pf_wave((2 * PF_PI * 1365.3 * (float)i * TAMALR_AUDIO_PERIOD), false);
      audio_wavetable[7][i] = pf_wave((2 * PF_PI * 1170.3 * (float)i * TAMALR_AUDIO_PERIOD), false);
      audio_wavetable[8][i] = 0;
    }
    audio_wavetables_compiled = true;
  }
}

void retro_reset(void)
{
  tamalib_reset();
}

bool retro_load_game(const struct retro_game_info *info)
{
  if (info && info->data && info->size)
  {
    u8_t buf[2];
    int i;

    /* Bitshift the ROM into the correct format */
    for (i = 0; i < info->size; i += 2)
    {
      memcpy(buf, &((u8_t*)(info->data))[i], 2);
      tamalr.rom[i / 2] = buf[1] | ((buf[0] & 0xF) << 8);
    }  
    init_tamalr_hal();
    tamalib_register_hal(&tamalr.hal);

    return tamalib_init(tamalr.rom, NULL, 1000000) == 0;
  }
  else
    return false;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num_info)
{
  return false;
}

void retro_unload_game(void)
{
}

void retro_run(void)
{
  handle_input();

  for (int i = 0; i < 200; i++)
  {
    tamalib_set_exec_mode(EXEC_MODE_RUN);
    tamalib_step();
  }

  audio_batch_cb(tamalr.audio_playing ? tamalr.audio_samples : audio_wavetable[8], TAMALR_AUDIO_SAMPLES);

  tamalr.hal.update_screen();
  video_cb(tamalr.video_screen,
           LCD_WIDTH * tamalr.video_scale,
           LCD_WIDTH * tamalr.video_scale,
           LCD_WIDTH * tamalr.video_scale * 2);
}

void retro_get_system_info(struct retro_system_info *info)
{
  memset(info, 0, sizeof(*info));
  info->library_name     = "TamaLIBretro";
  info->library_version  = GIT_VERSION;
  info->need_fullpath    = false;
  info->valid_extensions = "rom|b|bin";
  info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
  memset(info, 0, sizeof(*info));
  info->geometry.base_width   = LCD_WIDTH * 8;
  info->geometry.base_height  = LCD_WIDTH * 8;
  info->geometry.max_width    = LCD_WIDTH * 8;
  info->geometry.max_height   = LCD_WIDTH * 8;
  info->geometry.aspect_ratio = 1.0;
  info->timing.fps            = 30;
  info->timing.sample_rate    = 44100;
}

void retro_deinit(void)
{
  tamalib_release();
}

unsigned retro_get_region(void)
{
  return RETRO_REGION_NTSC;
}

unsigned retro_api_version(void)
{
  return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
}

void retro_set_environment(retro_environment_t cb)
{
  static const struct retro_variable vars[] = 
  {
    { NULL, NULL },
  };
  static const struct retro_controller_description port[] =
  {
    { "Tamagotchi", RETRO_DEVICE_JOYPAD },
    { 0 },
  };
  static const struct retro_controller_info ports[] =
  {
    { port, 1 },
    { NULL, 0 },
  };
  struct retro_input_descriptor desc[] =
  {
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "A (Select)" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B (Execute)" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "C (Cancel)" },

    { 0 },
  };
  enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;
  bool support_no_game = false;
 
  environ_cb = cb;
  cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
  cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO,   (void*)ports);
  cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,      &rgb565);
  cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME,   &support_no_game);
  cb(RETRO_ENVIRONMENT_SET_VARIABLES,         (void*)vars);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
  audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
  audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
  input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
  input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
  video_cb = cb;
}

size_t retro_serialize_size(void)
{
  return /* pc                   */ sizeof(u13_t) + 
         /* x                    */ sizeof(u12_t) + 
         /* y                    */ sizeof(u12_t) + 
         /* a                    */ sizeof(u4_t) +
         /* b                    */ sizeof(u4_t) +
         /* np                   */ sizeof(u5_t) +
         /* sp                   */ sizeof(u8_t) +
         /* flags                */ sizeof(u4_t) +
         /* tick_counter         */ sizeof(u32_t) +
         /* clk_timer_timestamp  */ sizeof(u32_t) +
         /* prog_timer_timestamp */ sizeof(u32_t) +
         /* prog_timer_enabled   */ sizeof(bool_t) +
         /* prog_timer_data      */ sizeof(u8_t) +
         /* prog_timer_rld       */ sizeof(u8_t) +
         /* call_depth           */ sizeof(u32_t) +
         /* interrupts           */ sizeof(interrupt_t) +
         /* memory               */ MEM_BUFFER_SIZE;
}

static bool tamalr_serialize(void *data, size_t *offset, const void *field_data, size_t field_size)
{
  memcpy((uint8_t*)data + *offset, field_data, field_size);
  *offset += field_size;

  return true;
}

bool retro_serialize(void *data, size_t size)
{
  state_t *state = cpu_get_state();
  size_t offset = 0;

  if (!state || size < retro_serialize_size())
    return false;

  if (!tamalr_serialize(data, &offset, &state->pc, sizeof(u13_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->x, sizeof(u12_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->y, sizeof(u12_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->a, sizeof(u4_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->b, sizeof(u4_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->np, sizeof(u5_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->sp, sizeof(u8_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->flags, sizeof(u4_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->tick_counter, sizeof(u32_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->clk_timer_timestamp, sizeof(u32_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->prog_timer_timestamp, sizeof(u32_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->prog_timer_enabled, sizeof(bool_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->prog_timer_data, sizeof(u8_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->prog_timer_rld, sizeof(u8_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->call_depth, sizeof(u32_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->interrupts, sizeof(interrupt_t))) return false;
  if (!tamalr_serialize(data, &offset, &state->memory, MEM_BUFFER_SIZE)) return false;

  return true;
}

static bool tamalr_unserialize(const void *data, size_t *offset, void *field_data, size_t field_size)
{
  memcpy(field_data, (const uint8_t*)data + *offset, field_size);
  *offset += field_size;

  return true;
}

bool retro_unserialize(const void *data, size_t size)
{
  state_t *state = cpu_get_state();
  size_t offset = 0;

  if (!state || size < retro_serialize_size())
    return false;

  if (!tamalr_unserialize(data, &offset, &state->pc, sizeof(u13_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->x, sizeof(u12_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->y, sizeof(u12_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->a, sizeof(u4_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->b, sizeof(u4_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->np, sizeof(u5_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->sp, sizeof(u8_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->flags, sizeof(u4_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->tick_counter, sizeof(u32_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->clk_timer_timestamp, sizeof(u32_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->prog_timer_timestamp, sizeof(u32_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->prog_timer_enabled, sizeof(bool_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->prog_timer_data, sizeof(u8_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->prog_timer_rld, sizeof(u8_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->call_depth, sizeof(u32_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->interrupts, sizeof(interrupt_t))) return false;
  if (!tamalr_unserialize(data, &offset, &state->memory, MEM_BUFFER_SIZE)) return false;

  return true;
}

void *retro_get_memory_data(unsigned type)
{
  const state_t *state = cpu_get_state();

  if (state && type == RETRO_MEMORY_SYSTEM_RAM)
    return state->memory;
  else
    return NULL;
}

size_t retro_get_memory_size(unsigned type)
{
  if (type == RETRO_MEMORY_SYSTEM_RAM)
    return MEMORY_SIZE;
  else
    return 0;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned a, bool b, const char *c)
{
}
