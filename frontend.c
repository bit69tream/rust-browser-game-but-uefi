#include <efi.h>
#include <efilib.h>
#include <elf.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "backend.h"

#define MOVE_DELTA 20
#define FPS 60.0

struct framebuffer {
    void* base;
    uint64_t size;
    uint32_t width, height, pixels_per_scan_line;
} global_framebuffer;

#define clamp(v, min, max) ((v) > (max) ? (max) : (v) < (min) ? (min) : (v))

void framebuffer_set_pixel (uint32_t x, uint32_t y, uint32_t color) {
    *(uint32_t *)((uint32_t *)(global_framebuffer.base) + x + (y * global_framebuffer.pixels_per_scan_line)) = color;
}

bool init_gop (EFI_SYSTEM_TABLE *table) {
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    if (EFI_ERROR (uefi_call_wrapper (table -> BootServices -> LocateProtocol, 3, &gop_guid, NULL, (void**)&gop))) {
        Print (L"error: can't locate GOP :/\n");
        return false;
    }

    global_framebuffer.base = (void*) gop -> Mode -> FrameBufferBase;
    global_framebuffer.size = gop -> Mode -> FrameBufferSize;
    global_framebuffer.width = gop -> Mode -> Info -> HorizontalResolution;
    global_framebuffer.height = gop -> Mode -> Info -> VerticalResolution;
    global_framebuffer.pixels_per_scan_line = gop -> Mode -> Info -> PixelsPerScanLine;

    /* clear whole framebuffer */
    for (uint32_t x = 0; x < global_framebuffer.width; x++) {
        for (uint32_t y = 0; y < global_framebuffer.height; y++) {
            framebuffer_set_pixel (x, y, 0);
        }
    }


    return true;
}

inline uint32_t abgr_to_argb (uint32_t color) {
    return (((color & 0xff000000))       |
            ((color & 0x00ff0000) >> 16) |
            ((color & 0x0000ff00))       |
            ((color & 0x000000ff) << 16));
}

EFI_STATUS efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *table) {
    InitializeLib (image, table);

    init ();
    init_gop (table);

    /* disable watchdog times, so game doesn't hang\reset after 5 minutes */
    table -> BootServices -> SetWatchdogTimer (0, 0, 0, NULL);

    const uint32_t game_display_width = get_display_width ();
    const uint32_t game_display_height = get_display_height ();
    const uint32_t *game_display = get_display ();

    if (global_framebuffer.width < game_display_width || global_framebuffer.height < game_display_height) {
        Print (L"error: this game requires at least %dx%d display\n", game_display_width, game_display_height);
        return EFI_ABORTED;
    }

    EFI_EVENT timer_event;
    EFI_EVENT wait_list [2];
    UINTN index;
    EFI_INPUT_KEY key;
    int32_t x = (int32_t) game_display_width / 2, y = (int32_t) game_display_height / 2;
    /* game display should be centerred on actual display */
    const uint32_t top_x = (global_framebuffer.width / 2) - (uint32_t) x, top_y = (global_framebuffer.height / 2) - (uint32_t) y;

    mouse_move(x, y);

    for (;;) {
        /* read key */
        {
            table -> BootServices -> CreateEvent (EFI_EVENT_TIMER, 0, NULL, NULL, &timer_event);
            table -> BootServices -> SetTimer (timer_event, TimerRelative, 10000000 / (int) FPS); /* wait for 1 / FPS seconds */

            wait_list [0] = table -> ConIn -> WaitForKey;
            wait_list [1] = timer_event;

            table -> BootServices -> WaitForEvent (2, wait_list, &index);
            table -> BootServices -> CloseEvent (timer_event);
            table -> ConIn -> ReadKeyStroke (table -> ConIn, &key);
        }

        /* process input */
        {
            switch (key.UnicodeChar) {
            case L'w':
            case L' ':
                mouse_click (x, y);
                break;
            case L'a':
                x -= MOVE_DELTA;
                x = clamp (x, 0, (int32_t) game_display_width);
                mouse_move(x, y);
                break;
            case L'd':
                x += MOVE_DELTA;
                x = clamp (x, 0, (int32_t) game_display_width);
                mouse_move(x, y);
                break;
            case L'p': toggle_pause (); break;
            default: break;
            }
        }

        /* render */
        {
            next_frame (1.0 / FPS);

            for (uint32_t i = top_x; i < game_display_width; i++) {
                for (uint32_t j = top_y; j < game_display_height; j++) {
                    framebuffer_set_pixel (i, j, abgr_to_argb (game_display [(j * game_display_width) + i]));
                }
            }
        }
    }

    return EFI_SUCCESS;
}
