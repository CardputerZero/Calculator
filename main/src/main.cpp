#include "lvgl/lvgl.h"
#include "calculator_app.h"
#include "calculator_keymap.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <time.h>
#endif

#if LV_USE_SDL
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"
#include <SDL2/SDL_render.h>
#endif

static volatile int g_app_quit_requested = 0;
static volatile int g_app_quit_enabled = 0;
static lv_indev_t *g_keyboard_indev = NULL;

extern "C" lv_indev_t *app_get_keyboard_indev(void)
{
    return g_keyboard_indev;
}

extern "C" void app_request_quit(void)
{
    if (g_app_quit_enabled) {
        g_app_quit_requested = 1;
    }
}

extern "C" int app_should_quit(void)
{
    return g_app_quit_requested;
}

static const char *getenv_default(const char *name, const char *fallback)
{
    return getenv(name) ? getenv(name) : fallback;
}

static int find_st7789v_fbdev(char *path, size_t size)
{
    FILE *fp = fopen("/proc/fb", "r");
    if (fp == NULL) {
        return -1;
    }

    char line[256];
    int fb_number = -1;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "fb_st7789v") != NULL &&
            sscanf(line, "%d", &fb_number) == 1 && fb_number >= 0) {
            break;
        }
        fb_number = -1;
    }
    fclose(fp);

    if (fb_number < 0) {
        return -1;
    }
    snprintf(path, size, "/dev/fb%d", fb_number);
    return 0;
}

#if LV_USE_EVDEV

constexpr uint16_t kKeySym = KEY_RIGHTALT;
constexpr uint32_t kHelpKey = 0x10001;
constexpr uint32_t kClearAllKey = 'C';
constexpr int kLongPressMs = 1000;
constexpr int kPollTimeoutMs = 50;

static long monotonic_ms()
{
    struct timespec now {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000L + now.tv_nsec / 1000000L;
}

struct queued_key {
    uint32_t key;
    int pressed;
    STAILQ_ENTRY(queued_key) entries;
};

STAILQ_HEAD(key_queue_t, queued_key);
static struct key_queue_t g_key_queue = STAILQ_HEAD_INITIALIZER(g_key_queue);
static pthread_mutex_t g_key_mutex = PTHREAD_MUTEX_INITIALIZER;

// The TCA8418 keymap emits a distinct keycode for each blue Sym symbol. The
// symbol layer is already resolved by the keyboard driver, so do not require a
// separate KEY_RIGHTALT-style modifier event here.
static uint32_t symbol_from_device_keymap(uint16_t code)
{
    uint32_t symbol = 0;
    return calculator::tca8418_keycode_lookup(code, &symbol) ? symbol : 0;
}

#ifndef I2C_SLAVE_FORCE
#define I2C_SLAVE_FORCE 0x0706
#endif

static bool legacy_i2c_fn_held()
{
    int fd = open("/dev/i2c-1", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    bool held = false;
    if (ioctl(fd, I2C_SLAVE_FORCE, 0x4f) >= 0) {
        static const uint8_t fn_register = 0xbe;
        uint8_t value = 0;
        if (write(fd, &fn_register, sizeof(fn_register)) == sizeof(fn_register) &&
            read(fd, &value, sizeof(value)) == sizeof(value)) {
            held = value == 0x03;
        }
    }
    close(fd);
    return held;
}

static uint32_t evdev_key_to_lv_key(uint16_t code, bool fn_active)
{
    if (const uint32_t symbol = symbol_from_device_keymap(code)) {
        return symbol;
    }

    // Some firmware paths emit the logical Help key directly; others expose the
    // orange Fn layer as Fn + H. Support both encodings from the keymap.
    if (code == KEY_HELP || (code == KEY_H && (fn_active || legacy_i2c_fn_held()))) {
        return kHelpKey;
    }

    switch (code) {
    case KEY_0: case KEY_KP0: return '0';
    case KEY_1: case KEY_KP1: return '1';
    case KEY_2: case KEY_KP2: return '2';
    case KEY_3: case KEY_KP3: return '3';
    case KEY_4: case KEY_KP4: return '4';
    case KEY_5: case KEY_KP5: return '5';
    case KEY_6: case KEY_KP6: return '6';
    case KEY_7: case KEY_KP7: return '7';
    case KEY_8: case KEY_KP8: return '8';
    case KEY_9: case KEY_KP9: return '9';
    case KEY_DOT: case KEY_KPDOT: return '.';
    case KEY_EQUAL:
#ifdef KEY_KPEQUAL
    case KEY_KPEQUAL:
#endif
        return '=';
    case KEY_MINUS: case KEY_KPMINUS: return '-';
    case KEY_SLASH: case KEY_KPSLASH: return '/';
    case KEY_KPASTERISK: return '*';
    case KEY_KPPLUS: return '+';
    case KEY_ENTER: case KEY_KPENTER: return LV_KEY_ENTER;
    case KEY_HOME: return LV_KEY_HOME;
    case KEY_F: return 'f';
    case KEY_X: return 'x';
    case KEY_UP: return LV_KEY_UP;
    case KEY_DOWN: return LV_KEY_DOWN;
    case KEY_LEFT: return LV_KEY_LEFT;
    case KEY_RIGHT: return LV_KEY_RIGHT;
    case KEY_BACKSPACE: case KEY_DELETE: return LV_KEY_BACKSPACE;
    case KEY_SPACE: return LV_KEY_ENTER;
    default: return 0;
    }
}

static void enqueue_key(uint32_t key, int pressed)
{
    if (key == 0) {
        return;
    }

    queued_key *item = static_cast<queued_key *>(malloc(sizeof(*item)));
    if (item == NULL) {
        return;
    }
    item->key = key;
    item->pressed = pressed;

    pthread_mutex_lock(&g_key_mutex);
    STAILQ_INSERT_TAIL(&g_key_queue, item, entries);
    pthread_mutex_unlock(&g_key_mutex);
}

static void *keyboard_read_thread(void *argument)
{
    const char *device = argument ? static_cast<const char *>(argument)
                                  : "/dev/input/by-path/platform-3f804000.i2c-event";
    int fd = open(device, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "Failed to open keyboard %s: %s\n", device, strerror(errno));
        return NULL;
    }

    bool delete_long_clear_sent = false;
    bool fn_active = false;
    bool esc_pressed = false;
    bool backspace_pressed = false;
    long esc_pressed_at_ms = 0;
    long backspace_pressed_at_ms = 0;

    while (!app_should_quit()) {
        struct pollfd descriptor {};
        descriptor.fd = fd;
        descriptor.events = POLLIN;
        const bool timing_press = esc_pressed || backspace_pressed;
        const int ready = poll(&descriptor, 1, timing_press ? kPollTimeoutMs : -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (ready == 0) {
            if (esc_pressed && monotonic_ms() - esc_pressed_at_ms >= kLongPressMs) {
                esc_pressed = false;
                app_request_quit();
            }
            if (backspace_pressed && monotonic_ms() - backspace_pressed_at_ms >= kLongPressMs) {
                delete_long_clear_sent = true;
                enqueue_key(kClearAllKey, 1);
            }
            continue;
        }

        struct input_event event {};
        ssize_t got = read(fd, &event, sizeof(event));
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (got != sizeof(event) || event.type != EV_KEY) {
            continue;
        }

        switch (event.code) {
        case KEY_FN:
            fn_active = event.value != 0;
            continue;
        case kKeySym:
            // Accepted for compatibility, but the TCA8418 symbol layer does not
            // require us to track this modifier before decoding its keycodes.
            continue;
        case KEY_ESC:
            if (event.value == 1) {
                esc_pressed = true;
                esc_pressed_at_ms = monotonic_ms();
            } else if (event.value == 0) {
                if (esc_pressed && monotonic_ms() - esc_pressed_at_ms < kLongPressMs) {
                    enqueue_key(LV_KEY_ESC, 1);
                }
                esc_pressed = false;
            }
            continue;
        case KEY_BACKSPACE:
        case KEY_DELETE:
            // Fn+Backspace is the physical Delete legend, but this display-only
            // calculator has no caret; both delete backward by one character.
            if (event.value == 0) {
                backspace_pressed = false;
                backspace_pressed_at_ms = 0;
                delete_long_clear_sent = false;
                continue;
            }
            if (event.value == 1) {
                delete_long_clear_sent = false;
                backspace_pressed = true;
                backspace_pressed_at_ms = monotonic_ms();
                enqueue_key(LV_KEY_BACKSPACE, 1);
                continue;
            }
            if (!delete_long_clear_sent) {
                enqueue_key(LV_KEY_BACKSPACE, 1);
            }
            continue;
        default:
            break;
        }

        if (event.value == 0) {
            continue;
        }
        enqueue_key(evdev_key_to_lv_key(event.code, fn_active), 1);
    }

    close(fd);
    return NULL;
}

static void keypad_read_cb(lv_indev_t *, lv_indev_data_t *data)
{
    data->state = LV_INDEV_STATE_RELEASED;
    data->continue_reading = false;

    pthread_mutex_lock(&g_key_mutex);
    if (!STAILQ_EMPTY(&g_key_queue)) {
        queued_key *item = STAILQ_FIRST(&g_key_queue);
        STAILQ_REMOVE_HEAD(&g_key_queue, entries);
        data->key = item->key;
        data->state = item->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        data->continue_reading = !STAILQ_EMPTY(&g_key_queue);
        free(item);
    }
    pthread_mutex_unlock(&g_key_mutex);
}

static void linux_indev_init(void)
{
    const char *keyboard_device = getenv_default("LV_LINUX_KEYBOARD_DEVICE",
                                                 "/dev/input/by-path/platform-3f804000.i2c-event");
    calculator::load_tca8418_keymap(calculator::tca8418_keymap_path());
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, keyboard_read_thread,
                       const_cast<char *>(keyboard_device)) == 0) {
        pthread_detach(thread_id);
    }

    g_keyboard_indev = lv_indev_create();
    lv_indev_set_type(g_keyboard_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(g_keyboard_indev, keypad_read_cb);
}
#elif LV_USE_SDL
static void linux_indev_init(void)
{
    lv_sdl_mouse_create();
    g_keyboard_indev = lv_sdl_keyboard_create();
}
#else
static void linux_indev_init(void)
{
}
#endif

#if LV_USE_LINUX_FBDEV
static void linux_display_init(void)
{
    char fbdev[64] = {};
    const char *device = getenv_default("LV_LINUX_FBDEV_DEVICE", NULL);
    if (device == NULL && find_st7789v_fbdev(fbdev, sizeof(fbdev)) == 0) {
        device = fbdev;
    }
    if (device == NULL && access("/dev/fb_lcd", W_OK) == 0) {
        device = "/dev/fb_lcd";
    }
    if (device == NULL && access("/dev/fb0", W_OK) == 0) {
        device = "/dev/fb0";
    }
    printf("Using framebuffer device: %s\n", device ? device : "unknown");
    if (device == NULL) {
        return;
    }

    lv_display_t *display = lv_linux_fbdev_create();
    if (display == NULL) {
        fprintf(stderr, "Failed to create fbdev display\n");
        return;
    }
    lv_linux_fbdev_set_file(display, device);
}
#elif LV_USE_SDL
static void linux_display_init(void)
{
    const int width = atoi(getenv("LV_SDL_VIDEO_WIDTH") ? getenv("LV_SDL_VIDEO_WIDTH") : "320");
    const int height = atoi(getenv("LV_SDL_VIDEO_HEIGHT") ? getenv("LV_SDL_VIDEO_HEIGHT") : "170");
    lv_display_t *display = lv_sdl_window_create(width, height);
    lv_sdl_window_set_title(display, "Calculator");
}
#elif LV_USE_LINUX_DRM
static void linux_display_init(void)
{
    const char *device = getenv_default("LV_LINUX_DRM_CARD", "/dev/dri/card0");
    lv_display_t *display = lv_linux_drm_create();
    lv_linux_drm_set_file(display, device, -1);
}
#else
#error "No supported CardputerZero display backend is enabled"
#endif

int main(void)
{
    lv_init();
    linux_display_init();
    linux_indev_init();

    if (lv_display_get_default() == NULL) {
        fprintf(stderr, "No usable framebuffer display\n");
        return 1;
    }

    lv_obj_t *screen = lv_obj_create(NULL);
    calculator_ui_build(screen);
    if (getenv("CALCULATOR_SHOW_HELP") != NULL) {
        calculator_ui_show_help_for_debug();
    }
    lv_screen_load(screen);

    printf("Calculator started\n");
    int startup_ticks = 0;
    while (!app_should_quit()) {
        uint32_t idle_time = lv_timer_handler();
        if (!g_app_quit_enabled && ++startup_ticks >= 5) {
            g_app_quit_enabled = 1;
        }
        usleep((idle_time ? idle_time : 1) * 1000);
    }
    printf("Calculator stopped\n");
    return 0;
}
