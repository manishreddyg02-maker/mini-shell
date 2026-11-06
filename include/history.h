#ifndef HISTORY_H
#define HISTORY_H

void history_load(void);
void history_add(const char *command);
void history_display(void);
void history_cleanup(void);

const char *history_get_last(void);
const char *history_get_command(int number);

#endif