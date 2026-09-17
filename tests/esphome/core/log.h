#pragma once
#include <cstring>
extern int valid_frames;
#define ESP_LOGD(tag, format, ...) do { if (std::strncmp(format,"Valid %s frame",14)==0) ++valid_frames; } while(0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGCONFIG(...) ((void)0)
#define LOG_SENSOR(...) ((void)0)
