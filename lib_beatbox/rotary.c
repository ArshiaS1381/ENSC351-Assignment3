#include "rotary.h"
#include "beatGenerator.h"
#include <gpiod.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

// --- Configuration (Only the momentary switch is wired) ---
#define ROT_CHIP_DEV "/dev/gpiochip2"
#define ROT_LINE_SW 12 // Adjust to your actual GPIO line

static struct gpiod_chip *chip = NULL;
static struct gpiod_line_request *req = NULL;
static pthread_t thr;
static atomic_int running = 0;

static void* rotaryLoop(void *arg)
{
    (void)arg;
    chip = gpiod_chip_open(ROT_CHIP_DEV);
    if (!chip) {
        fprintf(stderr, "Rotary: Failed to open chip. Rotary switch disabled.\n");
        return NULL;
    }

    // Configure the line for input, both-edge detection, and pull-up bias
    struct gpiod_line_settings *ls = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(ls, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_bias(ls, GPIOD_LINE_BIAS_PULL_UP);

    struct gpiod_line_config *lc = gpiod_line_config_new();
    unsigned int offsets[1] = { ROT_LINE_SW };
    gpiod_line_config_add_line_settings(lc, offsets, 1, ls); 

    struct gpiod_request_config *rc = gpiod_request_config_new();
    gpiod_request_config_set_consumer(rc, "beatbox_rotary_switch");

    req = gpiod_chip_request_lines(chip, rc, lc);

    gpiod_request_config_free(rc);
    gpiod_line_config_free(lc);
    gpiod_line_settings_free(ls);

    if (!req) {
        fprintf(stderr, "Rotary: Failed to request line %d. Rotary switch disabled.\n", ROT_LINE_SW);
        gpiod_chip_close(chip);
        return NULL;
    }

    running = 1;
    
    // Read initial state for switch debouncing
    int lastSw = gpiod_line_request_get_value(req, ROT_LINE_SW);

    struct gpiod_edge_event_buffer *buf = gpiod_edge_event_buffer_new(16);

    while (running) {
        // Wait for events (1s timeout)
        int ret = gpiod_line_request_wait_edge_events(req, 1000000000LL); 
        if (ret < 0) break; 
        if (ret == 0) continue;

        int num = gpiod_line_request_read_edge_events(req, buf, 16);
        for (int i = 0; i < num; i++) {
            struct gpiod_edge_event *ev = gpiod_edge_event_buffer_get_event(buf, i);
            unsigned int off = gpiod_edge_event_get_line_offset(ev);

            if (off == ROT_LINE_SW) {
                int eventType = gpiod_edge_event_get_event_type(ev);
                // 0 is active (pressed), 1 is inactive (released)
                int currentSw = (eventType == GPIOD_EDGE_EVENT_RISING_EDGE) ? 1 : 0; 

                // Detect Press (Falling Edge: 1 -> 0)
                if (lastSw == 1 && currentSw == 0) {
                    BeatMode m = BeatGenerator_getMode();
                    // Cycle mode: NONE (0) -> ROCK (1) -> CUSTOM (2) -> NONE (0)
                    m = (m + 1) % 3; 
                    BeatGenerator_setMode(m);
                    printf("Rotary: Mode cycled to %d\n", m);
                }
                lastSw = currentSw;
            } 
        }
    }

    gpiod_edge_event_buffer_free(buf);
    if (req) { gpiod_line_request_release(req); req = NULL; }
    if (chip) { gpiod_chip_close(chip); chip = NULL; }
    return NULL;
}

void Rotary_init(void) {
    pthread_create(&thr, NULL, rotaryLoop, NULL);
}

void Rotary_cleanup(void) {
    atomic_store(&running, 0);
    pthread_join(thr, NULL);
}