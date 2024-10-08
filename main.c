#include <libretro.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "emu.h"
#include "font.h"
#include "hw/beeper.h"
#include "hw/system.h"
#include "hw/vram.h"
#include "input.h"
#include "screen.h"

static f8_system_t lr_system;

static u16 screen_buffer[128 * 64];
static char system_dir[1024];

/* libretro video options */
static u8 (*lr_video_draw)(u8 *vram, u16 *buffer);
static u16 lr_video_width;
static u16 lr_video_height;
static float lr_video_aspect;

/* libretro callbacks */
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static struct retro_led_interface led_cb = { NULL };
static retro_log_printf_t log_cb;

static struct retro_microphone *mic = NULL;
static struct retro_microphone_interface mic_cb;
static bool mic_enabled = false;
static unsigned mic_frequency = PF_SOUND_FREQUENCY;
static int16_t mic_samples[PF_SOUND_FREQUENCY / 60];

static struct retro_rumble_interface rumble;
static retro_video_refresh_t video_cb;

/* Initialize the microphone, read in the input rate, if initialization
 * succeeds and rate is valid, return true. */
bool init_mic(void)
{
  retro_microphone_params_t params = { PF_SOUND_FREQUENCY };

  mic_cb.interface_version = RETRO_MICROPHONE_INTERFACE_VERSION;
  if (environ_cb(RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE, &mic_cb))
  {
    retro_microphone_t *new_mic = NULL;

    mic = mic_cb.open_mic(&params);
    if (mic && mic_cb.get_params(mic, &params))
    {
      mic_frequency = params.rate;
      return (mic_frequency > 0 &&
              mic_frequency <= PF_SOUND_FREQUENCY &&
              mic_cb.set_mic_state(mic, true));
    }
  }

  return false;
}

void stop_mic(void)
{
  if (mic)
    mic_cb.set_mic_state(mic, false);
}

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
  set_input_button(0, INPUT_TIME, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L) ? 1 : 0);
  set_input_button(0, INPUT_MODE, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT) ? 1 : 0);
  set_input_button(0, INPUT_HOLD, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R) ? 1 : 0);
  set_input_button(0, INPUT_START, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START) ? 1 : 0);

  set_input_button(4, INPUT_RIGHT, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) ? 1 : 0);
  set_input_button(4, INPUT_LEFT, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) ? 1 : 0);
  set_input_button(4, INPUT_BACK, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) ? 1 : 0);
  set_input_button(4, INPUT_FORWARD, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) ? 1 : 0);
  set_input_button(4, INPUT_ROTATE_CCW, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y) ? 1 : 0);
  set_input_button(4, INPUT_ROTATE_CW, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0);
  set_input_button(4, INPUT_PULL, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) ? 1 : 0);
  set_input_button(4, INPUT_PUSH, input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0);

  set_input_button(1, INPUT_RIGHT, input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) ? 1 : 0);
  set_input_button(1, INPUT_LEFT, input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) ? 1 : 0);
  set_input_button(1, INPUT_BACK, input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) ? 1 : 0);
  set_input_button(1, INPUT_FORWARD, input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) ? 1 : 0);
  set_input_button(1, INPUT_ROTATE_CCW, input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y) ? 1 : 0);
  set_input_button(1, INPUT_ROTATE_CW, input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0);
  set_input_button(1, INPUT_PULL, input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) ? 1 : 0);
  set_input_button(1, INPUT_PUSH, input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0);

  /* Read microphone data for the TV Powww option */
  if (mic_enabled && mic && mic_cb.get_mic_state(mic))
  {
    unsigned read = mic_cb.read_mic(mic, mic_samples, mic_frequency / 60);
    unsigned entropy = 0;
    int i;

    for (i = 0; i < read; i++)
      entropy += mic_samples[i] > 0 ? mic_samples[i] : -mic_samples[i];
    entropy /= read;
    set_input_button(4, INPUT_PUSH, entropy > 32767 / 4);
  }
}

bool load_system_file(char *filename, void *rom_data, u16 rom_size)
{
  char rom_filename[4096];

  snprintf(rom_filename, 4096, "%s%c%s", system_dir, '/', filename);
  rom_filename[sizeof(rom_filename) - 1] = '\0';
  if (filestream_exists(rom_filename))
  {
    RFILE *file = filestream_open(rom_filename, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (file)
    {
      filestream_read(file, rom_data, rom_size);
      filestream_close(file);
    }
    if (log_cb)
      log_cb(RETRO_LOG_INFO, "[Press F]: Loaded %s from system_dir (0x%08X)\n", filename, rom_size);
  }
  else
  {
    if (log_cb)
      log_cb(RETRO_LOG_ERROR, "[Press F]: Couldn't find %s in system_dir\n", filename);
    return false;
  }

  return true;
}

void set_variables(void)
{
  struct retro_variable var = { 0 };
  f8_settings_t settings = { 0, 0, 0, 0, 0 };

  var.key = "press_f_screen_size";

  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !strcmp(var.value, "extended"))
  {
    lr_video_draw  = draw_frame_rgb565_full;
    lr_video_height = VRAM_HEIGHT;
    lr_video_width  = VRAM_WIDTH;
    lr_video_aspect = VRAM_WIDTH / VRAM_HEIGHT;
  }
  else
  {
    lr_video_draw  = draw_frame_rgb565;
    lr_video_height = SCREEN_HEIGHT;
    lr_video_width  = SCREEN_WIDTH;
    lr_video_aspect = (float)SCREEN_WIDTH / SCREEN_HEIGHT;
  }

  var.key = "press_f_skip_verification";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !strcmp(var.value, "enabled"))
    settings.cf_skip_cartridge_verification = true;
  else
    settings.cf_skip_cartridge_verification = false;

  //var.key = "press_f_cpu_clock";
  //if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    lr_system.settings.f3850_clock_speed = F8_CLOCK_CHANNEL_F_NTSC; //@todo

  var.key = "press_f_font";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    if (string_is_equal(var.value, "cute"))
      font_load(&lr_system, FONT_CUTE);
    else if (string_is_equal(var.value, "skinny"))
      font_load(&lr_system, FONT_SKINNY);
    else
      font_reset(&lr_system);
  }

  var.key = "press_f_tv_powww";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    if (string_is_equal(var.value, "enabled"))
    {
      settings.cf_tv_powww = true;
      mic_enabled = init_mic();
    }
    else
    {
      settings.cf_tv_powww = false;
      stop_mic();
      mic_enabled = false;
    }
  }

  var.key = "press_f_schach_led";
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    if (string_is_equal(var.value, "enabled"))
      environ_cb(RETRO_ENVIRONMENT_GET_LED_INTERFACE, &led_cb);
  }

  f8_settings_apply_default(&lr_system);
}

/* libretro API */

void retro_init(void)
{
  char *dir = NULL;

  memset(&lr_system, 0, sizeof(f8_system_t));
  pressf_init(&lr_system);
  f8_system_init(&lr_system, &pf_systems[0]);

  if (!environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_cb))
    log_cb = NULL;
  if (!environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) || !dir)
  {
    if (log_cb)
      log_cb(RETRO_LOG_ERROR, "[PRESS F]: Couldn't find system dir\n");
  }
  else
  {
    snprintf(system_dir, sizeof(system_dir), "%s", dir);
    load_system_file("sl31253.rom", lr_system.f8devices[1].data, 1024);
    load_system_file("sl31254.rom", lr_system.f8devices[2].data, 1024);
  }
  set_variables();
}

void retro_reset(void)
{
  pressf_reset(&lr_system);
}

bool retro_load_game(const struct retro_game_info *info)
{
  if (info && info->data && info->size)
  {
    for (unsigned i = 0; i < info->size; i += 0x0400)
    {
      u8 temp[0x0400];

      memcpy(temp, &((u8*)info->data)[i], sizeof(temp));
      f8_write(&lr_system, 0x0800 + i, temp, sizeof(temp));
    }

    return true;
  }
  else
    return true;
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
  /* Have the core options been changed? Re-init video if so */
  u8 settings_changed = FALSE;

  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &settings_changed) && settings_changed)
  {
    struct retro_system_av_info info;

    set_variables();
    retro_get_system_av_info(&info);
    environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info);
    force_draw_frame();
  }

  handle_input();
  pressf_run(&lr_system);

  /* Update audio and video */
  for (int i = 0; i < lr_system.f8device_count; i++)
  {
    f8_device_t* device = &lr_system.f8devices[i];

    if (device->type == F8_DEVICE_BEEPER)
    {
      f8_beeper_t *beeper = (f8_beeper_t*)device->device;

      audio_batch_cb(beeper->samples, PF_SOUND_SAMPLES);
    }
    else if (device->type == F8_DEVICE_MK4027)
    {
      vram_t *vram = (vram_t*)device->device;

      lr_video_draw(vram->data, screen_buffer);
      video_cb(screen_buffer, lr_video_width, lr_video_height,
               lr_video_width * 2);
    }
  }

  /* Update miscellaneous hardware */
  if (led_cb.set_led_state)
  {
    f8_byte led_state;

    if (f8_read(&lr_system, &led_state, 0x3800, sizeof(led_state)))
      led_cb.set_led_state(0, led_state.s);
  }
}

void retro_get_system_info(struct retro_system_info *info)
{
  memset(info, 0, sizeof(*info));
  info->library_name = "Press F";
  info->library_version = "GIT_VERSION";
  info->need_fullpath = false;
  info->valid_extensions = "chf|bin";
  info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
  memset(info, 0, sizeof(*info));
  info->geometry.base_width = lr_video_width;
  info->geometry.base_height = lr_video_height;
  info->geometry.max_width = lr_video_width;
  info->geometry.max_height = lr_video_height;
  info->geometry.aspect_ratio = lr_video_aspect;
  info->timing.fps = 60;
  info->timing.sample_rate = PF_SOUND_FREQUENCY;
}

void retro_deinit(void)
{
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
  static const struct retro_variable vars[] = {
    { "press_f_screen_size", "Screen size; normal|extended" },
    { "press_f_skip_verification", "Skip cartridge verification; disabled|enabled" },
    { "press_f_cpu_clock", "CPU clock speed; 1|2|3" },
    { "press_f_font", "Font; fairchild|cute|skinny" },
    { "press_f_tv_powww", "TV POWWW!; disabled|enabled" },
    { "press_f_schach_led", "Scach LED; disabled|enabled" },
    { NULL, NULL },
  };
  static const struct retro_controller_description port[] = {
    { "Hand-Controller", RETRO_DEVICE_JOYPAD },
    /* { "Keyboard", RETRO_DEVICE_JOYPAD }, not yet implemented */
    { NULL, 0 },
  };
  static const struct retro_controller_info ports[] = {
    { port, 2 },
    { port, 2 },
    { NULL, 0 },
  };
  struct retro_input_descriptor desc[] = {
    /* Console buttons */
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "TIME (1)" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "MODE (2)" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "HOLD (3)" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "START (4)" },

    /* Left controller */
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Backward" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Forward" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Rotate Left" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Rotate Right" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Pull Up" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Plunge Down" },

    /* Right controller */
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Backward" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Forward" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Rotate Left" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Rotate Right" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Pull Up" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Plunge Down" },

    { 0 },
  };
  enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;
  bool support_no_game = true;

  environ_cb = cb;
  cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
  cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
  cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565);
  cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &support_no_game);
  cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
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
  return 0;
}

bool retro_serialize(void *data, size_t size)
{
  return false;
}

bool retro_unserialize(const void *data, size_t size)
{
  return false;
}

void *retro_get_memory_data(unsigned type)
{
  return NULL;
}

size_t retro_get_memory_size(unsigned type)
{
  return 0;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned a, bool b, const char *c)
{
}
