#ifndef HISTORY_H
#define HISTORY_H

void history_add(const char *command);
void history_display(void);
void history_cleanup(void);

#endif