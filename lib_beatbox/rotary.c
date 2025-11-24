/*
 * Rotary Encoder Module
 * * Handles the rotary encoder input using libgpiod.
 * This module runs a thread that waits for GPIO edge events (interrupts).
 * * Functionality:
 * 1. Push Button (SW): Cycles through Beat Modes (None -> Rock -> Custom).
 * 2. Rotation (DT/CLK): Increases or decreases the BPM (Tempo).
 */

#include "rotary.h"
#include "beatGenerator.h"
#include <gpiod.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

// --- Configuration Constants ---

#define GPIO_CHIP_DEVICE "/dev/gpiochip2"

// GPIO Line Offsets (Pin numbers on the specific chip)
#define LINE_SW 13  // Push button
#define LINE_B  11  // Rotary B (DT)
#define LINE_A   8  // Rotary A (CLK)

#define TEMPO_INCREMENT 1 // How much to change BPM per tick

// --- Internal State ---

static struct gpiod_chip *chip = NULL;
static struct gpiod_line_request *req = NULL;
static pthread_t thr;
static atomic_int running = 0;

// --- Thread Function ---

static void* rotaryLoop(void *arg)
{
    (void)arg;
    
    // 1. Open the GPIO chip
    chip = gpiod_chip_open(GPIO_CHIP_DEVICE);
    if (!chip) {
        fprintf(stderr, "Rotary: Failed to open chip %s.\n", GPIO_CHIP_DEVICE);
        return NULL;
    }

    // 2. Configure Lines
    struct gpiod_line_settings *ls = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(ls, GPIOD_LINE_EDGE_BOTH); // Interrupt on both rise and fall
    gpiod_line_settings_set_bias(ls, GPIOD_LINE_BIAS_PULL_UP);        // Enable internal pull-ups

    struct gpiod_line_config *lc = gpiod_line_config_new();
    unsigned int offsets[3] = { LINE_SW, LINE_A, LINE_B };
    
    gpiod_line_config_add_line_settings(lc, offsets, 3, ls); 

    struct gpiod_request_config *rc = gpiod_request_config_new();
    gpiod_request_config_set_consumer(rc, "beatbox_rotary");

    // Request the lines from the kernel
    req = gpiod_chip_request_lines(chip, rc, lc);

    // Free config structures now that request is made
    gpiod_request_config_free(rc);
    gpiod_line_config_free(lc);
    gpiod_line_settings_free(ls);

    if (!req) {
        fprintf(stderr, "Rotary: Failed to request lines.\n");
        gpiod_chip_close(chip);
        return NULL;
    }

    running = 1;
    
    // 3. Read Initial State
    int a = gpiod_line_request_get_value(req, LINE_A);
    int b = gpiod_line_request_get_value(req, LINE_B);
    int lastSw = gpiod_line_request_get_value(req, LINE_SW);
    
    // Combine A and B into a 2-bit state (0-3)
    int lastState = (a << 1) | b;

    // Buffer for reading events
    struct gpiod_edge_event_buffer *buf = gpiod_edge_event_buffer_new(16);

    // 4. Event Loop
    while (running) {
        // Wait for an event (timeout 1 sec to allow checking 'running' flag)
        int ret = gpiod_line_request_wait_edge_events(req, 1000000000LL); 
        if (ret <= 0) continue; // Timeout or error, loop back

        // Read the events from the buffer
        int num = gpiod_line_request_read_edge_events(req, buf, 16);
        
        int total_delta = 0;

        for (int i = 0; i < num; i++) {
            struct gpiod_edge_event *ev = gpiod_edge_event_buffer_get_event(buf, i);
            unsigned int off = gpiod_edge_event_get_line_offset(ev);
            int eventType = gpiod_edge_event_get_event_type(ev); 

            // --- Handle Push Button (SW) ---
            if (off == LINE_SW) {
                // Determine logic level (Rising Edge = Release if Pull-up used inverted? check hardware)
                // Assuming standard Pull-Up: Pressed = 0, Released = 1.
                // We usually trigger on the 'press' (Falling edge) or 'release' (Rising edge).
                // Code assumes Active Low logic where 1->0 is press.
                
                int currentSw = (eventType == GPIOD_EDGE_EVENT_RISING_EDGE) ? 1 : 0; 
                
                // Detect Button Release (0 -> 1 transition) or Press depending on logic
                // This logic detects a transition from High to Low (Press)
                if (lastSw == 1 && currentSw == 0) {
                    BeatMode m = BeatGenerator_getMode();
                    m = (m + 1) % 3; // Cycle 0 -> 1 -> 2 -> 0
                    BeatGenerator_setMode(m);
                    printf("Rotary: Mode cycled to %d\n", m);
                }
                lastSw = currentSw;
            } 
            // --- Handle Rotation (A / B) ---
            else {
                // Update local state based on which line changed
                if (off == LINE_A) a = (eventType == GPIOD_EDGE_EVENT_RISING_EDGE) ? 1 : 0;
                else if (off == LINE_B) b = (eventType == GPIOD_EDGE_EVENT_RISING_EDGE) ? 1 : 0;

                int currentState = (a << 1) | b;
                int delta = 0;

                // --- Quadrature Logic ---
                // We only count a "tick" when the encoder returns to the detent/rest position (00).
                // By looking at the *previous* state before it hit 00, we know the direction.
                if (currentState == 0x00) {
                    if (lastState == 0x02) delta = +1; // Clockwise final step
                    else if (lastState == 0x01) delta = -1; // Counter-Clockwise final step
                } 
                
                // Note: Full quadrature decoding tracks every state transition (00->01->11->10).
                // This simplified logic works well for dented knobs where it settles at 00.

                lastState = currentState;
                if (delta != 0) total_delta += delta;
            }
        }
        
        // Apply Tempo Change
        if (total_delta != 0) {
            int currentTempo = BeatGenerator_getTempo();
            int newTempo = currentTempo + (total_delta * TEMPO_INCREMENT); 
            
            // Set the tempo (BeatGenerator will clamp it safely)
            BeatGenerator_setTempo(newTempo);
            
            // Read back the clamped value for display
            int actualTempo = BeatGenerator_getTempo();
            printf("Rotary: Tempo changed to %d\n", actualTempo);
        }
    }

    // Cleanup
    gpiod_edge_event_buffer_free(buf);
    if (req) { gpiod_line_request_release(req); req = NULL; }
    if (chip) { gpiod_chip_close(chip); chip = NULL; }
    return NULL;
}

// --- Public API ---

void Rotary_init(void) {
    pthread_create(&thr, NULL, rotaryLoop, NULL);
}

void Rotary_cleanup(void) {
    atomic_store(&running, 0);
    pthread_join(thr, NULL);
}