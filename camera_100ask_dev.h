#ifndef CAMERA_100ASK_DEV_H
#define CAMERA_100ASK_DEV_H

#include <config.h>
#include <disp_manager.h>
#include <video_manager.h>
#include <convert_manager.h>
#include <render.h>

/* 音频参数设置 */
#define AUDIO_CHANNELS    1        // 单声道
#define AUDIO_RATE        16000    // 16K 采样率
#define AUDIO_SAMPLE_BIT  16       // 16位深度
#define AUDIO_CHUNK_SIZE  1600     // 每次读取的字节数

typedef enum {
    CAMERA_100ASK_OPT_NONE = 0,
    CAMERA_100ASK_OPT_UPDATE_BRIGHTNESS,
    CAMERA_100ASK_OPT_TAKE_PHOTOS,
    CAMERA_100ASK_OPT_TAKE_VIDEO,
    CAMERA_100ASK_OPT_START_VIDEO,
    CAMERA_100ASK_OPT_STOP_VIDEO,
} camera_100ask_opt_t;

int camera_100ask_dev_init(char * dev);

void camera_100ask_dev_set_brightness(int value);

void camera_100ask_dev_set_opt(camera_100ask_opt_t opt);

PT_VideoBuf camera_100ask_dev_get_video_buf_cur(void);


void camera_100ask_dev_start_video(void);

void camera_100ask_dev_stop_video(void);

void camera_100ask_dev_preview_control(int enable);

#endif /*CAMERA_100ASK_DEV_H*/
