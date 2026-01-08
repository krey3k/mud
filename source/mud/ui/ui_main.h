#pragma once

typedef struct sapp_event sapp_event;

void UI_Init();
void UI_Shutdown();
void UI_Frame();

void UI_HandleEvent(const sapp_event* event);
