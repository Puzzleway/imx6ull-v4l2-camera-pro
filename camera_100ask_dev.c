#include "camera_100ask_dev.h"
#include "convert_to_bmp_file.h"

#include <time.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>    
#include <sys/ioctl.h> 
#include <sys/time.h>   // 必须包含，用于 gettimeofday
#include <unistd.h>    
#include <errno.h>
#include <alsa/asoundlib.h>

#include "avilib.h" 

static T_VideoDevice tVideoDevice;
static PT_VideoConvert ptVideoConvert;
static int iPixelFormatOfVideo;
static int iPixelFormatOfDisp;

static PT_VideoBuf ptVideoBufCur;
static T_VideoBuf tVideoBuf;
static T_VideoBuf tConvertBuf;
static T_VideoBuf tZoomBuf;    
static T_VideoBuf tFrameBuf;

static int iLcdWidth;
static int iLcdHeigt;
static int iLcdBpp;

/* 视频录制控制变量 */
static avi_t *g_ptAviHandle = NULL;
static int g_is_recording = 0;
static int g_brightness_value = 0;

/* 帧率统计变量 */
static int g_total_frames = 0;
static struct timeval g_rec_start, g_rec_end;

static camera_100ask_opt_t g_camera_opt = CAMERA_100ASK_OPT_NONE;

pthread_mutex_t g_camera_100ask_mutex; 
pthread_mutex_t g_avi_write_mutex; // 文件写入锁

static void *thread_camera_work(void *args);
static void camera_set_brightness(int value);
static void *thread_audio_record(void *args);
//视频播放
static int g_preview_enabled = 1; 



int camera_100ask_dev_init(char * dev)
{
    int iError;

    pthread_mutex_init(&g_camera_100ask_mutex, NULL);
    pthread_mutex_init(&g_avi_write_mutex, NULL);

    /* 初始化显示设备 */
    DisplayInit();
    SelectAndInitDefaultDispDev("fb");
    GetDispResolution(&iLcdWidth, &iLcdHeigt, &iLcdBpp);
    GetVideoBufForDisplay(&tFrameBuf);
    iPixelFormatOfDisp = tFrameBuf.iPixelFormat;
    
    VideoInit();

    iError = VideoDeviceInit(dev, &tVideoDevice);
    if (iError)
    {
        DBG_PRINTF("VideoDeviceInit for %s error!\n", dev);
        return -1;
    }

    /* 强制设置格式为 MJPEG (前提是 v4l2.c 已修改支持优先选择 MJPEG) */
    iPixelFormatOfVideo = V4L2_PIX_FMT_MJPEG; 

    VideoConvertInit();
    ptVideoConvert = GetVideoConvertForFormats(iPixelFormatOfVideo, iPixelFormatOfDisp);
    if (NULL == ptVideoConvert)
    {
        DBG_PRINTF("can not support this format convert\n");
        return -1;
    }

    /* 启动摄像头 */
    iError = tVideoDevice.ptOPr->StartDevice(&tVideoDevice);
    if (iError)
    {
        DBG_PRINTF("StartDevice for %s error!\n", dev);
        return -1;
    }

    memset(&tVideoBuf, 0, sizeof(tVideoBuf));
    memset(&tConvertBuf, 0, sizeof(tConvertBuf));
    tConvertBuf.iPixelFormat     = iPixelFormatOfDisp;
    tConvertBuf.tPixelDatas.iBpp = iLcdBpp;
    /* 在 camera_100ask_dev_init 函数内 */
    // 预先分配足够大的空间 (800*480*4 字节足以容纳各种格式)
    tZoomBuf.tPixelDatas.aucPixelDatas = malloc(iLcdWidth * iLcdHeigt * 4);
    if (NULL == tZoomBuf.tPixelDatas.aucPixelDatas) {
        DBG_PRINTF("Critical: Failed to pre-allocate ZoomBuffer\n");
        return -1;
    }
    memset(&tZoomBuf, 0, sizeof(tZoomBuf));

    pthread_t thread;
    pthread_create(&thread, NULL, thread_camera_work, NULL);

    return 0;
}

/* 其他控制函数保持不变... */
void camera_100ask_dev_set_opt(camera_100ask_opt_t opt) { g_camera_opt = opt; }
void camera_100ask_dev_set_brightness(int value) { g_brightness_value = value; }
PT_VideoBuf camera_100ask_dev_get_video_buf_cur(void) { return ptVideoBufCur; }

void camera_100ask_dev_start_video(void) { camera_100ask_dev_set_opt(CAMERA_100ASK_OPT_START_VIDEO); }
void camera_100ask_dev_stop_video(void) { camera_100ask_dev_set_opt(CAMERA_100ASK_OPT_STOP_VIDEO); }

void camera_100ask_dev_preview_control(int enable) {g_preview_enabled = enable;}
static void camera_set_brightness(int value)
{
    PT_VideoDevice ptVideoDevice = &tVideoDevice;

    struct v4l2_queryctrl   qctrl;
    memset(&qctrl, 0, sizeof(qctrl));
    qctrl.id = V4L2_CID_BRIGHTNESS; // V4L2_CID_BASE+0;
    if (0 != ioctl(ptVideoDevice->iFd, VIDIOC_QUERYCTRL, &qctrl))
    {
        printf("can not query brightness\n");
        return;
    }

    //printf("current value=%d, brightness min = %d, max = %d\n", value, qctrl.minimum, qctrl.maximum);
        
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_BRIGHTNESS; // V4L2_CID_BASE+0;
    ioctl(ptVideoDevice->iFd, VIDIOC_G_CTRL, &ctl);

    ctl.value = value;

    if (ctl.value > qctrl.maximum)
        ctl.value = qctrl.maximum;
    if (ctl.value < qctrl.minimum)
        ctl.value = qctrl.minimum;
    
    ioctl(ptVideoDevice->iFd, VIDIOC_S_CTRL, &ctl);

}

static void *thread_camera_work(void *args)
{
    int iError;
    float k;
    time_t timep;
    struct tm *p;
    char time_buffer [64];

    struct timeval last_time, now_time;
    // 记录开始时间
    int spend_time;
    int interval = 50000; // 目标 20fps
    while(1)
    {
        gettimeofday(&last_time, NULL); 
        /* 1. 获取一帧数据 */
        iError = tVideoDevice.ptOPr->GetFrame(&tVideoDevice, &tVideoBuf);
        if (iError)
        {
            DBG_PRINTF("GetFrame for error!\n");
            continue;
        }

        /* 2. 核心修复：先初始化当前数据指针 */
        pthread_mutex_lock(&g_camera_100ask_mutex);
        ptVideoBufCur = &tVideoBuf; 
        pthread_mutex_unlock(&g_camera_100ask_mutex);

        /* 2. 录像逻辑 */
        if (g_is_recording && g_ptAviHandle)
        {
            pthread_mutex_lock(&g_avi_write_mutex);
            AVI_write_frame(g_ptAviHandle, (char *)tVideoBuf.tPixelDatas.aucPixelDatas, tVideoBuf.tPixelDatas.iTotalBytes);
            pthread_mutex_unlock(&g_avi_write_mutex);
            g_total_frames++; // 累加录制帧数
        }

        /* 3. 预览显示优化 */
        if (g_preview_enabled) 
        {//正在通过系统调用播放视频，相机不刷新
            pthread_mutex_lock(&g_camera_100ask_mutex);

            if (iPixelFormatOfVideo != iPixelFormatOfDisp)
            {
                iError = ptVideoConvert->Convert(&tVideoBuf, &tConvertBuf);
                if (!iError) ptVideoBufCur = &tConvertBuf;
            }

            if (ptVideoBufCur->tPixelDatas.iWidth > iLcdWidth || ptVideoBufCur->tPixelDatas.iHeight > iLcdHeigt)
            {
                k = (float)ptVideoBufCur->tPixelDatas.iHeight / ptVideoBufCur->tPixelDatas.iWidth;
                tZoomBuf.tPixelDatas.iWidth  = iLcdWidth;
                tZoomBuf.tPixelDatas.iHeight = iLcdWidth * k;
                if ( tZoomBuf.tPixelDatas.iHeight > iLcdHeigt)
                {
                    tZoomBuf.tPixelDatas.iWidth  = iLcdHeigt / k;
                    tZoomBuf.tPixelDatas.iHeight = iLcdHeigt;
                }
                tZoomBuf.tPixelDatas.iBpp        = iLcdBpp;
                tZoomBuf.tPixelDatas.iLineBytes  = tZoomBuf.tPixelDatas.iWidth * tZoomBuf.tPixelDatas.iBpp / 8;
                tZoomBuf.tPixelDatas.iTotalBytes = tZoomBuf.tPixelDatas.iLineBytes * tZoomBuf.tPixelDatas.iHeight;
                
                PicZoom(&ptVideoBufCur->tPixelDatas, &tZoomBuf.tPixelDatas);
                ptVideoBufCur = &tZoomBuf;
            }
            
            /* 无论是否显示，必须归还缓冲区 */
            //tVideoDevice.ptOPr->PutFrame(&tVideoDevice, &tVideoBuf);

            pthread_mutex_unlock(&g_camera_100ask_mutex);
        }
        /* 4. 响应 UI 操作 */
        switch (g_camera_opt)
        {
            case CAMERA_100ASK_OPT_START_VIDEO:
                if (!g_is_recording) {
                    time(&timep);
                    p = gmtime(&timep);
                    strftime(time_buffer, sizeof(time_buffer), "/mnt/video-%Y%m%d-%H%M%S.avi", p);
                    g_ptAviHandle = AVI_open_output_file(time_buffer);
                    if (g_ptAviHandle) {
                        /* 设置视频与音频参数 */
                        AVI_set_video(g_ptAviHandle, tVideoDevice.iWidth, tVideoDevice.iHeight, 20, "MJPG");
                        AVI_set_audio(g_ptAviHandle, AUDIO_CHANNELS, AUDIO_RATE, AUDIO_SAMPLE_BIT, 0x0001);
                        
                        g_total_frames = 0;
                        gettimeofday(&g_rec_start, NULL);
                        g_is_recording = 1;

                        pthread_t audio_thread;
                        pthread_create(&audio_thread, NULL, thread_audio_record, NULL);
                        pthread_detach(audio_thread);
                        printf("Start recording: %s\n", time_buffer);
                    }
                }
                g_camera_opt = CAMERA_100ASK_OPT_NONE;
                break;

            case CAMERA_100ASK_OPT_STOP_VIDEO:
                if (g_is_recording) {
                    g_is_recording = 0; 
                    usleep(200000); // 确保音频线程完成最后一次写入
                    pthread_mutex_lock(&g_avi_write_mutex);
                    if (g_ptAviHandle) {
                        AVI_close(g_ptAviHandle);
                        g_ptAviHandle = NULL;
                        gettimeofday(&g_rec_end, NULL);
                        double seconds = (g_rec_end.tv_sec - g_rec_start.tv_sec) + 
                                        (g_rec_end.tv_usec - g_rec_start.tv_usec) / 1000000.0;
                        printf("Video saved. Actual FPS: %.2f\n", g_total_frames / seconds);
                    }
                    pthread_mutex_unlock(&g_avi_write_mutex);
                }
                g_camera_opt = CAMERA_100ASK_OPT_NONE;
                break;

            case CAMERA_100ASK_OPT_UPDATE_BRIGHTNESS:
                camera_set_brightness(g_brightness_value);
                g_camera_opt = CAMERA_100ASK_OPT_NONE;
                break;

            case CAMERA_100ASK_OPT_TAKE_PHOTOS:
                time (&timep);
                p=gmtime(&timep);
                strftime (time_buffer, sizeof(time_buffer),"100ask-picture-%Y%m%d-%H%M%S.bmp",p);
                printf("photos name: %s\n", time_buffer);
                CvtRgb2BMPFileFrmFrameBuffer(ptVideoBufCur->tPixelDatas.aucPixelDatas, ptVideoBufCur->tPixelDatas.iWidth, ptVideoBufCur->tPixelDatas.iHeight, ptVideoBufCur->tPixelDatas.iBpp, time_buffer);
                g_camera_opt = CAMERA_100ASK_OPT_NONE;
                break;

            default: break;
        }
        /* 6. 核心修复：无论是否开启预览，处理完后必须归还原始驱动缓冲区 */
        tVideoDevice.ptOPr->PutFrame(&tVideoDevice, &tVideoBuf);

        gettimeofday(&now_time, NULL);

        spend_time = (now_time.tv_sec - last_time.tv_sec) * 1000000 + (now_time.tv_usec - last_time.tv_usec);

        if ((spend_time) < interval)
            usleep(interval - spend_time); // 强制补齐到 50ms (20fps)

    }
    return NULL;
}

/* 音频采集线程 */
static void *thread_audio_record(void *args) {
    snd_pcm_t *pcm_handle;
    unsigned int rate = AUDIO_RATE; 
    char *audio_buffer = malloc(AUDIO_CHUNK_SIZE);

    if (snd_pcm_open(&pcm_handle, "hw:1,0", SND_PCM_STREAM_CAPTURE, 0) < 0) return NULL;

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, AUDIO_CHANNELS);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);
    snd_pcm_hw_params(pcm_handle, params);

    while (g_is_recording) {
        int frames = snd_pcm_readi(pcm_handle, audio_buffer, AUDIO_CHUNK_SIZE / 2);
        if (frames < 0) {
            snd_pcm_prepare(pcm_handle);
            continue;
        }
        if (g_ptAviHandle && g_is_recording) {
            pthread_mutex_lock(&g_avi_write_mutex);
            AVI_write_audio(g_ptAviHandle, audio_buffer, frames * 2);
            pthread_mutex_unlock(&g_avi_write_mutex);
        }
    }
    snd_pcm_close(pcm_handle);
    free(audio_buffer);
    return NULL;
}