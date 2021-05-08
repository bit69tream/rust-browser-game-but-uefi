#ifndef GAME_H_
#define GAME_H_

#include <stdint.h>

void init(void);
size_t get_display_width(void);
size_t get_display_height(void);
uint32_t *get_display(void);
void next_frame(float dt);
void mouse_move(int32_t x, int32_t y);
void mouse_click(int32_t x, int32_t y);
void toggle_pause(void);

#endif // GAME_H_
