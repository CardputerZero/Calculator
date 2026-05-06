#include "lvgl/lvgl.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui/ui.h"
#include "calculator_app.h"
#include <cstring>

#if LV_USE_SDL
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"
#endif

#if LV_USE_EVDEV
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/queue.h>
#endif

static lv_indev_t *g_keyboard_indev = NULL;
static volatile int g_app_quit_requested = 0;
static volatile int g_app_quit_enabled = 0;

static const char *getenv_default(const char *name, const char *dflt)
{
    return getenv(name) ?: dflt;
}

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

int get_st7789v_fbdev(char *dev_path, size_t buf_size)
{
    if (dev_path == NULL || buf_size == 0)
    {
        return -1;
    }

    FILE *fp = fopen("/proc/fb", "r");
    if (fp == NULL)
    {
        perror("Failed to open /proc/fb");
        return -1;
    }

    char line[256];
    int fb_num = -1;

    /* 逐行读取，查找包含 fb_st7789v 的行，格式如：0 fb_st7789v */
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (strstr(line, "fb_st7789v") != NULL)
        {
            if (sscanf(line, "%d", &fb_num) == 1)
            {
                break;
            }
        }
    }

    fclose(fp);

    if (fb_num < 0)
    {
        fprintf(stderr, "fb_st7789v not found in /proc/fb\n");
        return -1;
    }

    snprintf(dev_path, buf_size, "/dev/fb%d", fb_num);
    return 0;
}


#if LV_USE_EVDEV

struct queued_key {
    uint32_t key;
    int pressed;
    STAILQ_ENTRY(queued_key) entries;
};

STAILQ_HEAD(key_queue_t, queued_key);
static struct key_queue_t g_key_queue = STAILQ_HEAD_INITIALIZER(g_key_queue);
static pthread_mutex_t g_key_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t evdev_key_to_lv_key(uint16_t code)
{
    switch (code) {
    case KEY_UP:
        return LV_KEY_UP;
    case KEY_DOWN:
        return LV_KEY_DOWN;
    case KEY_RIGHT:
        return LV_KEY_RIGHT;
    case KEY_LEFT:
        return LV_KEY_LEFT;
    case KEY_ENTER:
    case KEY_KPENTER:
        return LV_KEY_ENTER;
    case KEY_ESC:
    case KEY_HOME:
        return LV_KEY_ESC;
    case KEY_BACKSPACE:
        return LV_KEY_BACKSPACE;
    case KEY_DELETE:
        return LV_KEY_DEL;
    case KEY_PAGEUP:
        return LV_KEY_PREV;
    case KEY_PAGEDOWN:
        return LV_KEY_NEXT;
    case KEY_SPACE:
        return ' ';
    case KEY_COMMA:
        return ',';
    case KEY_0:
    case KEY_KP0:
        return '0';
    case KEY_1:
    case KEY_KP1:
        return '1';
    case KEY_2:
    case KEY_KP2:
        return '2';
    case KEY_3:
    case KEY_KP3:
        return '3';
    case KEY_4:
    case KEY_KP4:
        return '4';
    case KEY_5:
    case KEY_KP5:
        return '5';
    case KEY_6:
    case KEY_KP6:
        return '6';
    case KEY_7:
    case KEY_KP7:
        return '7';
    case KEY_8:
    case KEY_KP8:
        return '8';
    case KEY_9:
    case KEY_KP9:
        return '9';
    case KEY_DOT:
    case KEY_KPDOT:
        return '.';
    case KEY_EQUAL:
#ifdef KEY_KPEQUAL
    case KEY_KPEQUAL:
#endif
        return '=';
    case KEY_MINUS:
    case KEY_KPMINUS:
        return '-';
    case KEY_SLASH:
    case KEY_KPSLASH:
        return '/';
    case KEY_KPASTERISK:
        return '*';
    case KEY_KPPLUS:
        return '+';
    case 183:
        return '!';
    case 187:
        return '%';
    case 188:
        return '^';
    case 190:
        return '*';
    case 191:
        return '(';
    case 192:
        return ')';
    case 195:
        return '+';
    case 196:
        return '-';
    case 197:
        return '/';
    case 209:
        return '=';
    case 231:
        return ',';
    case 232:
        return '.';
    default:
        return 0;
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

static void *keyboard_read_thread(void *arg)
{
    const char *device = arg ? static_cast<const char *>(arg)
                             : "/dev/input/by-path/platform-3f804000.i2c-event";
    int fd = open(device, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "Failed to open keyboard %s: %s\n", device, strerror(errno));
        return NULL;
    }

    while (!app_should_quit()) {
        struct input_event event {};
        ssize_t got = read(fd, &event, sizeof(event));
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (got != sizeof(event) || event.type != EV_KEY || event.value == 0) {
            continue;
        }
        enqueue_key(evdev_key_to_lv_key(event.code), 1);
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

static void lv_linux_indev_init(void)
{
    const char *mouse_device = getenv_default("LV_LINUX_MOUSE_DEVICE", NULL);
    const char *keyboard_device = getenv_default("LV_LINUX_KEYBOARD_DEVICE", "/dev/input/by-path/platform-3f804000.i2c-event");

    lv_indev_t *touch = NULL;
    if (mouse_device)
        touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, mouse_device);

    if (keyboard_device) {
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, keyboard_read_thread, const_cast<char *>(keyboard_device));
        pthread_detach(thread_id);

        g_keyboard_indev = lv_indev_create();
        lv_indev_set_type(g_keyboard_indev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(g_keyboard_indev, keypad_read_cb);
    }
}
#endif

#if LV_USE_LINUX_FBDEV
static void lv_linux_disp_init(void)
{
    // export LV_LINUX_FBDEV_DEVICE="/dev/fb$(grep 'fb_st7789v' /proc/fb | awk '{print $1}')"
    const char *device = NULL;
    char fbdev[64] = {0};
    device = getenv_default("LV_LINUX_FBDEV_DEVICE", NULL);
    if ((device == NULL) && (get_st7789v_fbdev(fbdev, sizeof(fbdev)) == 0))
    {
        device = fbdev;
    }
    printf("Using framebuffer device: %s\n", device);
    lv_display_t *disp = lv_linux_fbdev_create();
    if (disp == NULL)
    {
        printf("Failed to create fbdev display!\n");
        return;
    }

    lv_linux_fbdev_set_file(disp, device);

    // 打印获取到的分辨率
    lv_coord_t w = lv_display_get_horizontal_resolution(disp);
    lv_coord_t h = lv_display_get_vertical_resolution(disp);
    printf("Framebuffer resolution: %dx%d\n", w, h);
}
#if !LV_USE_EVDEV && !LV_USE_LIBINPUT
static void lv_linux_indev_init(void)
{
}
#endif

#elif LV_USE_LINUX_DRM
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_DRM_CARD", "/dev/dri/card0");
    lv_display_t *disp = lv_linux_drm_create();

    lv_linux_drm_set_file(disp, device, -1);
}
#elif LV_USE_SDL
static void lv_linux_disp_init(void)
{
    const int width = atoi(getenv("LV_SDL_VIDEO_WIDTH") ?: "320");
    const int height = atoi(getenv("LV_SDL_VIDEO_HEIGHT") ?: "170");

    lv_display_t *disp = lv_sdl_window_create(width, height);
    lv_sdl_window_set_title(disp, "Calculator");
}

static void lv_linux_indev_init(void)
{
    lv_sdl_mouse_create();
    g_keyboard_indev = lv_sdl_keyboard_create();
}

#else
#error Unsupported configuration
#endif

int main(void)
{

    lv_init();

    /*Linux display device init*/
    lv_linux_disp_init();

    lv_linux_indev_init();
    /*Create a Demo*/
    // lv_demo_widgets();
    // lv_demo_widgets_start_slideshow();
    // lv_demo_music();

    ui_init();
    // lv_demo_widgets(); // 用LVGL自带demo测试
    /*Handle LVGL tasks*/
    printf("Entering main loop...\n");
    int startup_ticks = 0;
    while (!app_should_quit())
    {
        uint32_t idle_time = lv_timer_handler();
        if (!g_app_quit_enabled && ++startup_ticks >= 5) {
            g_app_quit_enabled = 1;
        }
        usleep((idle_time ? idle_time : 1) * 1000);
    }

    return 0;
}
