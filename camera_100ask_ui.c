#include "camera_100ask_ui.h"
#include "camera_100ask_dev.h"
#include <pthread.h>
#include <unistd.h>
#include <errno.h>   // 定义了 errno
#include <string.h>  // 定义了 strerror
#include <signal.h>
#include <sys/types.h>

#define PATH_FILE_NAME_LEN 256
#define BLINK_TIME         200 /*ms*/
#define BOOT_TIME          1500

typedef struct{
	uint8_t * name;            // 蛇身
} photo_file_t;

// 视频操作结构体
typedef struct {
    photo_file_t * node;
    lv_obj_t * btn;
} video_op_t;

static bool g_is_recording = false; // 全局录像状态标志
static bool g_long_pressed_triggered = false; // 新增：记录本次动作是否触发过长按
static uint8_t g_dir_path[PATH_FILE_NAME_LEN];
static lv_img_dsc_t * img_dsc;
static photo_file_t * g_node_ll;
static lv_ll_t photo_file_ll;
static lv_obj_t * g_obj_blink;
static lv_obj_t * g_slider_label_setting;
static lv_obj_t * g_img_photo_browser;
static lv_obj_t * g_btn_photo_delete; 
static lv_obj_t * g_btn_photo_browser_pre;
static lv_obj_t * g_btn_photo_browser_next;

static lv_obj_t * g_btn_open_photo_browser;
static lv_obj_t * g_btn_open_video_browser;

/* 录像状态容器（默认隐藏） */
static lv_obj_t * g_cont_rec_info;
/* 红色指示灯 */
static lv_obj_t * g_rec_led;
/* 录制时间文字 */
static lv_obj_t * g_label_rec_time;
// 1. 在文件顶部定义静态全局指针，初始为 NULL
static lv_timer_t * g_rec_timer = NULL; 
static int g_rec_seconds = 0;

extern pthread_mutex_t g_camera_100ask_mutex; // 互斥锁，用于多线程同步
static void lv_100ask_boot_animation(uint32_t boot_time);
static void camera_startup_timer(lv_timer_t * timer);
static void camera_work_timer(lv_timer_t * timer);
static void blink_timer(lv_timer_t * timer);
static void recording_timer(lv_timer_t * timer);//视频录制定时器

static void btn_capture_event_handler(lv_event_t * e);
static void btn_setting_event_handler(lv_event_t * e);
static void slider_setting_event_cb(lv_event_t * e);

static void btn_photo_browser_event_handler(lv_event_t * e);
static void btn_open_photo_browser_event_handler(lv_event_t * e);
static void btn_photo_delete_event_handler(lv_event_t * e);
static void update_photo_browser_ui_state(void); // 删除后立即刷新按钮状态

static bool is_end_with(const char * str1, const char * str2);

/* 视频浏览器相关变量 */
static lv_obj_t * g_cont_video_browser;   // 视频浏览器主容器
static lv_obj_t * g_list_video;          // 使用列表组件展示视频文件
static lv_ll_t video_file_ll;            // 视频文件链表
static photo_file_t * g_video_node_cur;  // 当前选中的视频节点（借用 photo_file_t 结构体即可）

static void btn_open_video_browser_event_handler(lv_event_t * e);
static void video_item_event_handler(lv_event_t * e);
static void video_mbox_event_handler(lv_event_t * e);

//初始化ui
void camera_100ask_ui_init(void)
{
    lv_100ask_boot_animation(BOOT_TIME);

    lv_obj_t * cont = lv_obj_create(lv_scr_act());//创建基础对象，作为所有UI元素的父对象
    lv_obj_set_style_radius(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_y(cont, 0);

    lv_obj_fade_in(cont, 0, BOOT_TIME);

    img_dsc = lv_mem_alloc(sizeof(lv_img_dsc_t));
    lv_memset_00(img_dsc, sizeof(lv_img_dsc_t)); 

    lv_obj_t * img = lv_img_create(cont);//创建图片对象，继承自基础对象
    //LVGL 本身不直接解码 JPEG/PNG 等格式（除非集成第三方库），而是依赖 预处理后的像素格式。
    lv_img_set_antialias(img, true);//开启抗锯齿
    lv_obj_center(img);//居中
    lv_timer_t * timer = lv_timer_create(camera_startup_timer, BOOT_TIME, img);//创建定时器，定时器创建后1500ms调用函数camera_startup_timer
    lv_timer_set_repeat_count(timer, 1);//设置重复次数

    /*Blinking effect*/
    g_obj_blink = lv_obj_create(cont);//创建空白对象，继承自基础对象
    lv_obj_set_style_border_width(g_obj_blink, 0, 0);
    lv_obj_set_style_pad_all(g_obj_blink, 0, 0);
    lv_obj_set_style_radius(g_obj_blink, 0, 0);
    lv_obj_set_size(g_obj_blink, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(g_obj_blink, lv_color_hex(0x000000), 0);
    lv_obj_add_flag(g_obj_blink, LV_OBJ_FLAG_HIDDEN);//添加隐藏标志


    /*btn_capture*/
    static lv_style_t style;
    lv_style_init(&style);

    lv_style_set_radius(&style, LV_RADIUS_CIRCLE);

    lv_style_set_bg_opa(&style, LV_OPA_100);
    lv_style_set_bg_color(&style, lv_color_hex(0xffffff));

    lv_style_set_border_opa(&style, LV_OPA_40);
    lv_style_set_border_width(&style, 2);
    lv_style_set_border_color(&style, lv_color_hex(0x000000));

    lv_style_set_outline_opa(&style, LV_OPA_COVER);
    lv_style_set_outline_color(&style, lv_color_hex(0x000000));

    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_pad_all(&style, 10);

    /*Init the pressed style*///按下样式
    static lv_style_t style_pr;
    lv_style_init(&style_pr);

    /*Ad a large outline when pressed*/
    lv_style_set_outline_width(&style_pr, 15);
    lv_style_set_outline_opa(&style_pr, LV_OPA_TRANSP);

    lv_style_set_translate_y(&style_pr, 5);
    //lv_style_set_shadow_ofs_y(&style_pr, 3);
    lv_style_set_bg_color(&style_pr, lv_color_hex(0xffffff));
    lv_style_set_bg_grad_color(&style_pr, lv_palette_main(LV_PALETTE_GREEN));

    /*Add a transition to the the outline*/
    //让按钮等控件在按下（pressed）状态时，其 轮廓（outline）的宽度和不透明度 能够平滑变化，而不是突变。
    static lv_style_transition_dsc_t trans;
    static lv_style_prop_t props[] = {LV_STYLE_OUTLINE_WIDTH, LV_STYLE_OUTLINE_OPA, 0};
    //列出需要应用过渡效果的样式属性：
    // LV_STYLE_OUTLINE_WIDTH：轮廓线宽度
    // LV_STYLE_OUTLINE_OPA：轮廓线不透明度（opacity）
    // 在较新版本的 LVGL（v8.3+）中，官方推荐使用 LV_STYLE_PROP_INV 而不是 0
    lv_style_transition_dsc_init(&trans, props, lv_anim_path_linear, 300, 0, NULL);//采用线性插值匀速变化，过度时间300ms
    lv_style_set_transition(&style_pr, &trans);
    // 将这个过渡描述符绑定到 style_pr 样式上,当应用 style_pr 时，这些属性会有平滑的过渡效果。

    lv_obj_t * cont_capture = lv_obj_create(cont);//创建一个容器对象，并设置其大小为100x100，继承自基础对象
    lv_obj_set_size(cont_capture, 100, 100);
    lv_obj_set_align(cont_capture, LV_ALIGN_BOTTOM_MID);
    lv_obj_clear_flag(cont_capture, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(cont_capture, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(cont_capture, 0, 0);

    lv_obj_t * btn_capture = lv_btn_create(cont_capture);//创建按钮对象，父对象为上面的容器
    lv_obj_set_size(btn_capture, 75, 75);
    lv_obj_set_align(btn_capture, LV_ALIGN_CENTER);

    lv_obj_add_style(btn_capture, &style, 0);
    lv_obj_add_style(btn_capture, &style_pr, LV_STATE_PRESSED);
    // 按钮响应事件
    lv_obj_add_event_cb(btn_capture, btn_capture_event_handler, LV_EVENT_ALL, NULL);

    /*camera setting*/
    lv_obj_t * btn_setting = lv_btn_create(cont);//创建按钮对象，父对象为cont
    lv_obj_set_style_radius(btn_setting, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(btn_setting, 50, 50);
    lv_obj_align_to(btn_setting, cont_capture, LV_ALIGN_OUT_RIGHT_MID, (LV_VER_RES / 4), 0);

    lv_obj_t * label_setting = lv_label_create(btn_setting);//创建标签对象，父对象为btn_setting
    lv_obj_set_style_text_font(label_setting, &lv_font_montserrat_28, 0);
    lv_label_set_text(label_setting, LV_SYMBOL_SETTINGS);//设置标签的文本为LV_SYMBOL_SETTINGS
    lv_obj_set_align(label_setting, LV_ALIGN_CENTER);

    // slider setting
    lv_obj_t * slider_setting = lv_slider_create(cont);//创建滑块对象，父对象为cont
    lv_slider_set_mode(slider_setting, LV_SLIDER_MODE_SYMMETRICAL);
    lv_slider_set_range(slider_setting, -255, 255);
    lv_obj_add_flag(slider_setting, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align_to(slider_setting, btn_setting, LV_ALIGN_OUT_TOP_MID, 0, -10);

    /*Create a label below the slider*/
    g_slider_label_setting = lv_label_create(cont);//创建标签对象，父对象为cont
    lv_label_set_text(g_slider_label_setting, "camera brightness:0");
    lv_obj_add_flag(g_slider_label_setting, LV_OBJ_FLAG_HIDDEN);

    lv_obj_align_to(g_slider_label_setting, slider_setting, LV_ALIGN_OUT_TOP_MID, 0, -10);
    //设置按钮点击事件、滑块值改变事件的回调函数
    lv_obj_add_event_cb(btn_setting, btn_setting_event_handler, LV_EVENT_CLICKED, slider_setting);
    lv_obj_add_event_cb(slider_setting, slider_setting_event_cb, LV_EVENT_VALUE_CHANGED, g_slider_label_setting);

    /*Photo Browser*/
    g_btn_open_photo_browser = lv_btn_create(cont);//创建图片浏览器按钮对象，父对象为cont
    lv_obj_set_style_radius(g_btn_open_photo_browser, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(g_btn_open_photo_browser, 50, 50);
    lv_obj_align_to(g_btn_open_photo_browser, cont_capture, LV_ALIGN_OUT_LEFT_MID, -(LV_VER_RES / 4), 0);

    lv_obj_t * label_photo_browser = lv_label_create(g_btn_open_photo_browser);//创建标签对象，父对象为g_btn_open_photo_browser
    lv_obj_set_style_text_font(label_photo_browser, &lv_font_montserrat_28, 0);
    lv_label_set_text(label_photo_browser, LV_SYMBOL_IMAGE);
    lv_obj_set_align(label_photo_browser, LV_ALIGN_CENTER);

    lv_obj_t * cont_photo_browser = lv_obj_create(cont);//创建一个容器对象，父对象为cont
    lv_obj_set_size(cont_photo_browser, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(cont_photo_browser, 0, 0);
    lv_obj_set_style_radius(cont_photo_browser, 0, 0);
    lv_obj_set_align(cont_photo_browser, LV_ALIGN_CENTER);
    lv_obj_add_flag(cont_photo_browser, LV_OBJ_FLAG_HIDDEN);

    g_img_photo_browser = lv_img_create(cont_photo_browser);//创建图片对象,用于显示要浏览的照片，父对象为cont_photo_browser
    //lv_img_set_src(g_img_photo_browser, "//mnt/100ask-picture-20230803-105727.bmp");
    //lv_img_set_src(g_img_photo_browser, "//mnt/12345.bmp");
    //lv_img_set_src(g_img_photo_browser, "//mnt/test.png");
    lv_obj_set_align(g_img_photo_browser, LV_ALIGN_CENTER);

    g_btn_photo_browser_pre = lv_btn_create(cont_photo_browser);//创建按钮对象，浏览上一张，父对象为cont_photo_browser
    lv_obj_set_style_radius(g_btn_photo_browser_pre, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(g_btn_photo_browser_pre, 50, 50);
    lv_obj_align(g_btn_photo_browser_pre, LV_ALIGN_LEFT_MID, 0, 0);
    /* 设置禁用状态 (LV_STATE_DISABLED) 下的背景颜色为灰色 */
    lv_obj_set_style_bg_color(g_btn_photo_browser_pre, lv_palette_main(LV_PALETTE_GREY), LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(g_btn_photo_browser_pre, LV_OPA_50, LV_STATE_DISABLED); // 增加半透明感

    lv_obj_t * label_photo_browser_pre = lv_label_create(g_btn_photo_browser_pre);
    lv_obj_set_style_text_font(label_photo_browser_pre, &lv_font_montserrat_28, 0);
    lv_label_set_text(label_photo_browser_pre, LV_SYMBOL_LEFT);
    lv_obj_set_align(label_photo_browser_pre, LV_ALIGN_CENTER);

    g_btn_photo_browser_next = lv_btn_create(cont_photo_browser);//创建按钮对象，浏览下一张，父对象为cont_photo_browser
    lv_obj_set_style_radius(g_btn_photo_browser_next, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(g_btn_photo_browser_next, 50, 50);
    lv_obj_align(g_btn_photo_browser_next, LV_ALIGN_RIGHT_MID, 0, 0);
    /* 设置禁用状态 (LV_STATE_DISABLED) 下的背景颜色为灰色 */
    lv_obj_set_style_bg_color(g_btn_photo_browser_next, lv_palette_main(LV_PALETTE_GREY), LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(g_btn_photo_browser_next, LV_OPA_50, LV_STATE_DISABLED); // 增加半透明感

    lv_obj_t * label_photo_browser_next = lv_label_create(g_btn_photo_browser_next);
    lv_obj_set_style_text_font(label_photo_browser_next, &lv_font_montserrat_28, 0);
    lv_label_set_text(label_photo_browser_next, LV_SYMBOL_RIGHT);
    lv_obj_set_align(label_photo_browser_next, LV_ALIGN_CENTER);

    /* 删除按钮 (Delete Button) */
    g_btn_photo_delete = lv_btn_create(cont_photo_browser);
    lv_obj_set_style_radius(g_btn_photo_delete, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(g_btn_photo_delete, 50, 50);
    /* 设置禁用状态 (LV_STATE_DISABLED) 下的背景颜色为灰色 */
    lv_obj_set_style_bg_color(g_btn_photo_delete, lv_palette_main(LV_PALETTE_GREY), LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(g_btn_photo_delete, LV_OPA_50, LV_STATE_DISABLED); // 增加半透明感
    // 将删除按钮放在底部中间或右上角，这里演示放在底部中间
    lv_obj_align(g_btn_photo_delete, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(g_btn_photo_delete, lv_palette_main(LV_PALETTE_RED), 0); // 设置为红色提示危险操作
    lv_obj_set_style_bg_color(g_btn_photo_delete, lv_palette_main(LV_PALETTE_GREY), LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(g_btn_photo_delete, LV_OPA_50, LV_STATE_DISABLED);

    lv_obj_t * label_photo_delete = lv_label_create(g_btn_photo_delete);
    lv_obj_set_style_text_font(label_photo_delete, &lv_font_montserrat_28, 0);
    lv_label_set_text(label_photo_delete, LV_SYMBOL_TRASH);
    lv_obj_set_align(label_photo_delete, LV_ALIGN_CENTER); 

    // 为图片浏览器按钮及其子按钮添加点击事件回调函数
    lv_obj_add_event_cb(g_btn_photo_browser_pre, btn_photo_browser_event_handler, LV_EVENT_CLICKED, label_photo_browser_pre);
    lv_obj_add_event_cb(g_btn_photo_browser_next, btn_photo_browser_event_handler, LV_EVENT_CLICKED, label_photo_browser_next);
    lv_obj_add_event_cb(g_btn_open_photo_browser, btn_open_photo_browser_event_handler, LV_EVENT_CLICKED, cont_photo_browser);

        // 绑定删除事件回调
    lv_obj_add_event_cb(g_btn_photo_delete, btn_photo_delete_event_handler, LV_EVENT_CLICKED, NULL);

    /* 录像状态容器（默认隐藏） */
    g_cont_rec_info = lv_obj_create(cont);
    lv_obj_set_size(g_cont_rec_info, 120, 40);
    lv_obj_align(g_cont_rec_info, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_opa(g_cont_rec_info, LV_OPA_50, 0); // 半透明背景
    lv_obj_set_style_radius(g_cont_rec_info, 10, 0);
    lv_obj_add_flag(g_cont_rec_info, LV_OBJ_FLAG_HIDDEN); // 初始隐藏

    /* 正在录像红色指示灯 */
    g_rec_led = lv_led_create(g_cont_rec_info);
    lv_obj_set_size(g_rec_led, 15, 15);
    lv_obj_align(g_rec_led, LV_ALIGN_LEFT_MID, 5, 0);
    lv_led_set_color(g_rec_led, lv_palette_main(LV_PALETTE_RED));
    lv_led_on(g_rec_led);

    /* 录制时间文字 */
    g_label_rec_time = lv_label_create(g_cont_rec_info);
    lv_label_set_text(g_label_rec_time, "00:00");
    lv_obj_align(g_label_rec_time, LV_ALIGN_RIGHT_MID, -5, 0);

    /* --- 核心修复：初始化末尾强制同步状态 --- */
    // 因为刚启动时肯定没有扫描目录，g_node_ll 默认为 NULL
    // 调用此函数会立刻把按钮设为禁用（灰色），避免渲染时的红光闪现
    update_photo_browser_ui_state();

    /* Video Browser Button (在主界面上) */
    g_btn_open_video_browser = lv_btn_create(cont);
    lv_obj_set_style_radius(g_btn_open_video_browser, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(g_btn_open_video_browser, 50, 50);
    // 位置放在图片浏览器旁边
    lv_obj_align_to(g_btn_open_video_browser, btn_capture, LV_ALIGN_OUT_LEFT_MID, -(LV_VER_RES/8), 0);

    lv_obj_t * label_video_icon = lv_label_create(g_btn_open_video_browser);
    lv_obj_set_style_text_font(label_video_icon, &lv_font_montserrat_28, 0);
    lv_label_set_text(label_video_icon, LV_SYMBOL_VIDEO);
    lv_obj_set_align(label_video_icon, LV_ALIGN_CENTER);

    /* 视频浏览器全屏容器 */
    g_cont_video_browser = lv_obj_create(cont);
    lv_obj_set_size(g_cont_video_browser, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_cont_video_browser, lv_color_hex(0x333333), 0); // 深色背景区分
    lv_obj_add_flag(g_cont_video_browser, LV_OBJ_FLAG_HIDDEN);

    /* 视频列表 */
    g_list_video = lv_list_create(g_cont_video_browser);
    lv_obj_set_size(g_list_video, LV_PCT(70), LV_PCT(90));
    lv_obj_center(g_list_video);
    // 绑定事件
    lv_obj_add_event_cb(g_btn_open_video_browser, btn_open_video_browser_event_handler, LV_EVENT_CLICKED, NULL);
}

static void lv_100ask_boot_animation(uint32_t boot_time)
{   //启动动画。
    //创建居中的logo图片和标题标签，并为logo图片设置缩放动画效果。
    LV_IMG_DECLARE(img_lv_100ask_demo_logo);
    lv_obj_t * logo = lv_img_create(lv_scr_act());
    lv_img_set_src(logo, &img_lv_100ask_demo_logo);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, 0);
 
    /*Animate in the content after the intro time*/
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_path_cb(&a, lv_anim_path_bounce);   //弹跳效果
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);//回弹效果
    lv_anim_set_var(&a, logo);
    lv_anim_set_time(&a, boot_time);//启动过程耗时
    lv_anim_set_delay(&a, 0);
    lv_anim_set_values(&a, 1, LV_IMG_ZOOM_NONE);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t) lv_img_set_zoom);//缩放图片
	lv_anim_set_ready_cb(&a, lv_obj_del_anim_ready_cb);//删除动画
    lv_anim_start(&a);

    /* Create an intro from a label */
    lv_obj_t * title = lv_label_create(lv_scr_act());
    //lv_label_set_text(title, "100ASK LVGL DEMO\nhttps://www.100ask.net\nhttp:/lvgl.100ask.net");
	lv_label_set_text(title, "100ASK LVGL DEMO");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, LV_STATE_DEFAULT); // Please enable LV_FONT_MONTSERRAT_22 in lv_conf.h
    lv_obj_set_style_text_line_space(title, 8, LV_STATE_DEFAULT);
    lv_obj_align_to(title, logo, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    //动画淡出
    lv_obj_fade_out(title, 0, boot_time);
    lv_obj_fade_out(logo, 0, boot_time);
}


static void camera_startup_timer(lv_timer_t * timer)
{
    lv_obj_t * img = (lv_obj_t *)timer->user_data;
    lv_timer_create(camera_work_timer, 0, img);
}

static void camera_work_timer(lv_timer_t * timer)
{
    /*Use the user_data*/
    lv_obj_t * img = (lv_obj_t *)timer->user_data;
    
    /*Do something with LVGL*/
    pthread_mutex_lock(&g_camera_100ask_mutex);// 加锁，防止与摄像头采集线程冲突
    PT_VideoBuf VideoBufCur = camera_100ask_dev_get_video_buf_cur();
    PT_PixelDatas ptSmallPic = &VideoBufCur->tPixelDatas;
    img_dsc->data = ptSmallPic->aucPixelDatas;
    img_dsc->data_size = (VideoBufCur->tPixelDatas.iBpp / 8) * VideoBufCur->tPixelDatas.iWidth * VideoBufCur->tPixelDatas.iHeight;

    img_dsc->header.w = VideoBufCur->tPixelDatas.iWidth;
    img_dsc->header.h = VideoBufCur->tPixelDatas.iHeight;
    img_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
    lv_img_set_src(img, img_dsc);

    pthread_mutex_unlock(&g_camera_100ask_mutex); // 解锁
  
}

static void blink_timer(lv_timer_t * timer)
{
    lv_obj_add_flag(g_obj_blink, LV_OBJ_FLAG_HIDDEN);
}


static void btn_capture_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn_capture = lv_event_get_target(e);

    if (code == LV_EVENT_PRESSED) {
        // 每次按下时，重置长按触发标志
        g_long_pressed_triggered = false;
    }
    else if (code == LV_EVENT_LONG_PRESSED) {
        if (!g_is_recording) {
            g_is_recording = true;
            g_long_pressed_triggered = true; // 标记：这次按下已经触发了长按
            
            start_video_recording(); 

            lv_obj_set_style_bg_color(btn_capture, lv_palette_main(LV_PALETTE_RED), 0);
            LV_LOG_USER("Video recording started...");
        }
    }
    else if (code == LV_EVENT_CLICKED) {
        // 【关键保护】：如果是长按刚结束后的松手点击，直接跳过，不执行任何操作
        if (g_long_pressed_triggered) {
            g_long_pressed_triggered = false; // 重置
            return; 
        }

        if (g_is_recording) {
            // 录像中 -> 真正的第二次点击 -> 停止
            g_is_recording = false;
            stop_video_recording(); 
            
            lv_obj_set_style_bg_color(btn_capture, lv_color_hex(0xffffff), 0); 
            LV_LOG_USER("Video recording stopped.");
        } else {
            // 未录像 -> 短按 -> 拍照
            camera_100ask_dev_set_opt(CAMERA_100ASK_OPT_TAKE_PHOTOS);
            lv_obj_clear_flag(g_obj_blink, LV_OBJ_FLAG_HIDDEN);

            lv_timer_t * timer = lv_timer_create(blink_timer, BLINK_TIME, NULL);
            lv_timer_set_repeat_count(timer, 1);
            LV_LOG_USER("Photo taken.");
        }
    }
}


static void btn_setting_event_handler(lv_event_t * e)
{//点击设置按钮，显示或隐藏滑块
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * slider_setting = lv_event_get_user_data(e);

    if(code == LV_EVENT_CLICKED) {
        if(lv_obj_has_flag(slider_setting, LV_OBJ_FLAG_HIDDEN))
        {
            lv_obj_clear_flag(slider_setting, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_slider_label_setting, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(slider_setting, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_slider_label_setting, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void slider_setting_event_cb(lv_event_t * e)
{// 滑杆值改变亮度
    lv_obj_t * slider_setting = lv_event_get_target(e);
    lv_obj_t * slider_label_setting  = lv_event_get_user_data(e);

    int slider_value = (int)lv_slider_get_value(slider_setting);

    char buf[32];
    lv_snprintf((char *)buf, sizeof(buf), "camera brightness: %d", slider_value);
    lv_label_set_text(slider_label_setting, buf);
    //lv_obj_align_to(slider_label_setting, slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    camera_100ask_dev_set_brightness(slider_value);
    camera_100ask_dev_set_opt(CAMERA_100ASK_OPT_UPDATE_BRIGHTNESS);
}


static void btn_open_photo_browser_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    // 获取按钮上的 Label
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    lv_obj_t * cont_photo_browse = lv_event_get_user_data(e);

    char file_path_name[PATH_FILE_NAME_LEN];

    if(code == LV_EVENT_CLICKED) {
        lv_obj_move_foreground(btn);
        if(lv_obj_has_flag(cont_photo_browse, LV_OBJ_FLAG_HIDDEN))
        {
            _lv_ll_init(&photo_file_ll, sizeof(photo_file_t));

            lv_snprintf((char *)g_dir_path, sizeof(g_dir_path), "/%s", getcwd(NULL, 0));

            lv_fs_dir_t dir;
            lv_fs_res_t res;
            res = lv_fs_dir_open(&dir, (const char *)g_dir_path);
            if(res != LV_FS_RES_OK) {
                LV_LOG_USER("Open dir error %d!", res);
                return;
            }

            char fn[PATH_FILE_NAME_LEN];
            photo_file_t * node_ll;
            while(1) {
                res = lv_fs_dir_read(&dir, fn);
                if(res != LV_FS_RES_OK) {
                    LV_LOG_USER("Driver, file or directory is not exists %d!", res);
                    break;
                }

                if(strlen(fn) == 0) {
                    LV_LOG_USER("Not more files to read!");
                    break;
                }

                if ((is_end_with(fn, ".png") == true)  || (is_end_with(fn, ".PNG") == true)  ||\
                    (is_end_with(fn , ".jpg") == true) || (is_end_with(fn , ".JPG") == true) ||\
                    (is_end_with(fn , ".sjpg") == true) || (is_end_with(fn , ".SJPG") == true) ||\
                    (is_end_with(fn , ".bmp") == true) || (is_end_with(fn , ".BMP") == true))
                {
                    // 排除掉占位图本身，避免它出现在可翻页列表中（可选）
                    if (strcmp(fn, "no_photo.bmp") == 0) continue;

                    node_ll = _lv_ll_ins_tail(&photo_file_ll);
                    node_ll->name = lv_mem_alloc(strlen(fn) + 1); // +1 为了存放停止符
                    strcpy((char *)(node_ll->name), (const char *)fn);
                    LV_LOG_USER("%s", node_ll->name);
                }
            }

            lv_fs_dir_close(&dir);

            g_node_ll = _lv_ll_get_tail(&photo_file_ll);

            // --- 修改部分开始 ---
            if (g_node_ll == NULL) 
            {
                // 如果链表为空，显示占位图
                // 请确保 no_photo.bmp 存放在程序运行的当前目录下
                lv_snprintf(file_path_name, sizeof(file_path_name), "S:%s/no_photo.bmp", g_dir_path);
                // 注意：如果你的 LVGL 文件系统驱动前缀不是 "S:"，请根据实际情况修改（例如 "P:" 或直接路径）
                lv_img_set_src(g_img_photo_browser, file_path_name);
                LV_LOG_USER("No photos found, showing placeholder: %s", file_path_name);
            }
            else 
            {
                // 如果有照片，正常拼接路径并显示
                lv_snprintf(file_path_name, sizeof(file_path_name), "S:%s/%s", g_dir_path, (char *)g_node_ll->name);
                lv_img_set_src(g_img_photo_browser, file_path_name);
            }
            update_photo_browser_ui_state(); // 删除后立即刷新按钮状态
            // --- 修改部分结束 ---

            lv_obj_clear_flag(cont_photo_browse, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_btn_open_video_browser, LV_OBJ_FLAG_HIDDEN);
            // 将图标切换为“home”，暗示点击可回到拍摄模式
            lv_label_set_text(label, LV_SYMBOL_HOME);
        }
        else 
        {
            // 退出预览，清理链表
            photo_file_t * node_ll = _lv_ll_get_head(&photo_file_ll);
            while(node_ll != NULL)
            {
                lv_mem_free(node_ll->name);
                node_ll = _lv_ll_get_next(&photo_file_ll, node_ll);
            }
            _lv_ll_clear(&photo_file_ll);
            g_node_ll = NULL;

            lv_obj_add_flag(cont_photo_browse, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_btn_open_video_browser, LV_OBJ_FLAG_HIDDEN);
            // 恢复为“相册”图标，暗示点击可查看图片
            lv_label_set_text(label, LV_SYMBOL_IMAGE);
        }
    }
}

static void btn_photo_browser_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * label = lv_event_get_user_data(e);
    photo_file_t * tmp_node_ll;
    char file_path_name[PATH_FILE_NAME_LEN];

    /* --- 核心修复：增加空指针保护 --- */
    if(g_node_ll == NULL) {
        LV_LOG_USER("No photos in list, ignoring navigation.");
        return; 
    }

    if(code == LV_EVENT_CLICKED) {
        if((strcmp(LV_SYMBOL_LEFT, lv_label_get_text(label)) == 0))
        {
            tmp_node_ll = _lv_ll_get_prev(&photo_file_ll, g_node_ll);
            if(tmp_node_ll != NULL)
            {
                g_node_ll = tmp_node_ll;
                lv_snprintf((char *)file_path_name, sizeof(file_path_name), "S:%s/%s", g_dir_path, (char *)g_node_ll->name);
                lv_img_set_src(g_img_photo_browser, file_path_name);
                LV_LOG_USER("Open %s", g_node_ll->name);
            }
        }
        else if((strcmp(LV_SYMBOL_RIGHT, lv_label_get_text(label)) == 0))
        {
            tmp_node_ll = _lv_ll_get_next(&photo_file_ll, g_node_ll);
            if(tmp_node_ll != NULL)
            {
                g_node_ll = tmp_node_ll;
                lv_snprintf((char *)file_path_name, sizeof(file_path_name), "S:%s/%s", g_dir_path, (char *)g_node_ll->name);
                lv_img_set_src(g_img_photo_browser, file_path_name);
                LV_LOG_USER("Open %s", g_node_ll->name);
            }
        }
    }
}

/**
 * @brief 照片删除按钮的事件回调函数
 * @param e 事件对象
 */
static void btn_photo_delete_event_handler(lv_event_t * e)
{
    // 获取当前触发事件的删除按钮对象
    lv_obj_t * btn_photo_delete = lv_event_get_target(e);

    // 【安全检查】：如果当前照片节点指针为空（即没有照片可删），直接返回，防止后续空指针访问
    if (g_node_ll == NULL) 
    {
        lv_obj_add_state(btn_photo_delete, LV_STATE_DISABLED);
        return;
    }

    char file_path_name[PATH_FILE_NAME_LEN];

    // 1. 构建当前要删除的文件路径
    // 注意：S: 是 LVGL 内部识别的文件系统驱动盘符前缀
    lv_snprintf(file_path_name, sizeof(file_path_name), "S:%s/%s", g_dir_path, (char *)g_node_ll->name);

    // 2. 执行物理删除（Linux 系统调用）
    // file_path_name + 2 是为了去掉 "S:" 前缀，还原为 Linux 系统识别的真实路径（如 /root/xxx.bmp）
    if (unlink(file_path_name + 2) == 0) { 
        LV_LOG_USER("Deleted file: %s", file_path_name);
        
        // 3. 寻找下一个要显示的图片节点
        // 优先获取下一张照片
        photo_file_t * next_node = _lv_ll_get_next(&photo_file_ll, g_node_ll);
        if (next_node == NULL) {
            // 如果没有下一张了，尝试获取上一张（即删除的是最后一张的情况）
            next_node = _lv_ll_get_prev(&photo_file_ll, g_node_ll);
        }

        // 4. 彻底释放当前被删除节点的内存
        lv_mem_free(g_node_ll->name);        // 释放存放在节点内的文件名字符串内存
        _lv_ll_remove(&photo_file_ll, g_node_ll); // 从 LVGL 链表中移除该节点项
        lv_mem_free(g_node_ll);              // 释放节点本身占用的内存

        // 5. 更新全局当前节点指针
        g_node_ll = next_node;

        // 6. UI 刷新逻辑
        if (g_node_ll != NULL) {
            // 情况 A：链表中还有其他照片，显示更新后的当前照片
            lv_snprintf(file_path_name, sizeof(file_path_name), "S:%s/%s", g_dir_path, (char *)g_node_ll->name);
            lv_img_set_src(g_img_photo_browser, file_path_name);
        } 
        else {
            // 情况 B：照片已全部删光
            LV_LOG_USER("No more photos. Showing placeholder.");
            
            // 切换显示预设的“无照片”占位图，防止界面报错或显示空白
            lv_img_set_src(g_img_photo_browser, "S:/root/no_photo.bmp"); 
            
        }
        update_photo_browser_ui_state(); // 删除后立即刷新按钮状态
    } else {
        // 物理删除失败（如文件权限问题或文件已被手动删除）
        LV_LOG_USER("Unlink failed!");
    }
}

static void btn_open_video_browser_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    if(code != LV_EVENT_CLICKED) return;
    lv_obj_move_foreground(btn);
    if(lv_obj_has_flag(g_cont_video_browser, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(g_btn_open_photo_browser, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clean(g_list_video); // 清空列表
        _lv_ll_init(&video_file_ll, sizeof(photo_file_t));

        // 核心修复：直接指定 /mnt 目录，不要使用 getcwd()
        const char * video_dir = "/mnt"; 

        lv_fs_dir_t dir;
        // 注意：LVGL 的文件系统通常需要盘符前缀，如 "S:/mnt"
        if(lv_fs_dir_open(&dir, "S:/mnt") == LV_FS_RES_OK) {
            char fn[256];
            while(lv_fs_dir_read(&dir, fn) == LV_FS_RES_OK && strlen(fn) > 0) {
                // 检查是否为视频文件
                if(is_end_with(fn, ".avi") || is_end_with(fn, ".AVI")) {
                    photo_file_t * node = _lv_ll_ins_tail(&video_file_ll);
                    node->name = lv_mem_alloc(strlen(fn) + 1);
                    strcpy((char *)node->name, fn);

                    // 添加到列表 UI
                    lv_obj_t * btn = lv_list_add_btn(g_list_video, LV_SYMBOL_VIDEO, fn);
                    // 绑定点击事件（播放或删除）
                    lv_obj_add_event_cb(btn, video_item_event_handler, LV_EVENT_CLICKED, node);
                }
            }
            lv_fs_dir_close(&dir);
        }
        lv_obj_clear_flag(g_cont_video_browser, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label, LV_SYMBOL_HOME);
    }
    else{
        // 1. 隐藏视频浏览器容器
        lv_obj_add_flag(g_cont_video_browser, LV_OBJ_FLAG_HIDDEN);
        
        // 2. 清理视频链表内存 (参考图片浏览器的清理逻辑)
        photo_file_t * node = _lv_ll_get_head(&video_file_ll);
        while(node != NULL) {
            lv_mem_free(node->name);
            node = _lv_ll_get_next(&video_file_ll, node);
        }
        _lv_ll_clear(&video_file_ll);
        lv_label_set_text(label, LV_SYMBOL_VIDEO);
        // 3. 恢复主菜单按钮的显示 (比如拍照按钮、图片浏览器入口)
        lv_obj_clear_flag(g_btn_open_photo_browser, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_photo_browser_ui_state(void)
{
    if (!g_btn_photo_delete || !g_btn_photo_browser_pre || !g_btn_photo_browser_next) return;
    // 如果这些按钮还没被创建（防止初始化顺序问题），直接返回
    if (g_node_ll == NULL) {
        lv_obj_add_state(g_btn_photo_delete, LV_STATE_DISABLED);
        lv_obj_add_state(g_btn_photo_browser_pre, LV_STATE_DISABLED);
        lv_obj_add_state(g_btn_photo_browser_next, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(g_btn_photo_delete, LV_STATE_DISABLED);
        lv_obj_clear_state(g_btn_photo_browser_pre, LV_STATE_DISABLED);
        lv_obj_clear_state(g_btn_photo_browser_next, LV_STATE_DISABLED);
    }
}


static void recording_timer(lv_timer_t * timer)
{
    g_rec_seconds++;
    // 更新时间显示 (格式化为 mm:ss)
    lv_label_set_text_fmt(g_label_rec_time, "%02d:%02d", g_rec_seconds / 60, g_rec_seconds % 60);
    
    // 指示灯闪烁效果
    // 直接切换 LED 状态（亮 -> 灭 / 灭 -> 亮）
    // 这样不需要你自己写 if-else 判断亮度，减少逻辑出错
    if (g_rec_led) {
        if(lv_led_get_brightness(g_rec_led) > 127) lv_led_off(g_rec_led);
        else lv_led_on(g_rec_led);
    }
}

// 3. 开始录像的函数
void start_video_recording(void)
{
    // ... 执行底层开启录像的逻辑 ...
    camera_100ask_dev_start_video();
    g_rec_seconds = 0; // 重置秒数
    // 2. 【核心修复】：在显示 UI 之前，强制将 Label 文字设为 00:00
    // 这样容器一显示，用户看到的就是 00:00，而不是上次残留的数字
    lv_label_set_text(g_label_rec_time, "00:00");
    // 3. 确保 LED 初始状态是亮的
    lv_led_on(g_rec_led);
    lv_obj_clear_flag(g_cont_rec_info, LV_OBJ_FLAG_HIDDEN); // 显示计时容器
    
    // 【核心操作】：只有在此时才创建定时器
    if(g_rec_timer == NULL) {
        g_rec_timer = lv_timer_create(recording_timer, 1000, NULL);
    }
}

// 4. 停止录像的函数
void stop_video_recording(void)
{
    // ... 执行底层停止录像、保存文件的逻辑 ...
    camera_100ask_dev_stop_video();
    lv_obj_add_flag(g_cont_rec_info, LV_OBJ_FLAG_HIDDEN); // 隐藏计时容器
    
    // 【核心操作】：录像结束，立即销毁定时器并清空指针
    if(g_rec_timer != NULL) {
        lv_timer_del(g_rec_timer);
        g_rec_timer = NULL; // 这一步至关重要！
    }
}


static bool is_end_with(const char * str1, const char * str2)
{// 判断字符串str1是否以str2结尾
    if(str1 == NULL || str2 == NULL)
        return false;
    
    uint16_t len1 = strlen(str1);
    uint16_t len2 = strlen(str2);
    if((len1 < len2) || (len1 == 0 || len2 == 0))
        return false;
    
    while(len2 >= 1)
    {
        if(str2[len2 - 1] != str1[len1 - 1])
            return false;

        len2--;
        len1--;
    }

    return true;
}

static void video_item_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    photo_file_t * node = lv_event_get_user_data(e);

    if(code == LV_EVENT_CLICKED) {
        static const char * btns[] = {"Delete", "Play", ""};
        lv_obj_t * mbox = lv_msgbox_create(NULL, "Video Management", (const char *)node->name, btns, true);
        lv_obj_center(mbox);
        
        // 动态分配一个小内存来存储这两个指针
        video_op_t * op = lv_mem_alloc(sizeof(video_op_t));
        op->node = node;
        op->btn = btn;

        lv_obj_set_user_data(mbox, op); 
        lv_obj_add_event_cb(mbox, video_mbox_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

static pid_t g_play_pid = -1; // 存储播放进程的 PID
static bool g_is_paused = false;
static lv_obj_t * g_play_ctrl_cont = NULL; // 播放控制层容器

static void btn_exit_video_event_handler(lv_event_t * e);
// 暂停/恢复播放 (GStreamer 进程通常难通过信号实现精确暂停，这里用 SIGSTOP/SIGCONT 模拟)
static void btn_pause_video_event_handler(lv_event_t * e);

static void video_mbox_event_handler(lv_event_t * e)
{
    lv_obj_t * mbox = lv_event_get_current_target(e);
    video_op_t * op = lv_obj_get_user_data(mbox); // 获取刚才存入的结构体
    const char * btn_txt = lv_msgbox_get_active_btn_text(mbox);

    if (btn_txt == NULL) return;
    char file_path[256];
    sprintf(file_path, "/mnt/%s", (char *)op->node->name);
    if (strcmp(btn_txt, "Delete") == 0) {
        if (unlink(file_path) == 0) {
            LV_LOG_USER("Deleted video file: %s", file_path);
            
            // 1. 直接删除对应的列表按钮，UI 会自动刷新布局
            if(op->btn) lv_obj_del(op->btn);

            // 2. 清理内存链表数据
            lv_mem_free(op->node->name);
            _lv_ll_remove(&video_file_ll, op->node);
            lv_mem_free(op->node);
            
            // 注意：这里删除了物理删除后的隐藏逻辑，实现了“无感刷新”
        } else {
            LV_LOG_USER("Failed to delete file: %s, error: %s", file_path, strerror(errno));
        }
    }
    
    else if(strcmp(btn_txt, "Play") == 0)
    {
        char cmd[512];
        sprintf(file_path, "/mnt/%s", (char *)op->node->name);

        // 1. 停止预览并清理屏幕
        camera_100ask_dev_preview_control(0);
        usleep(10000); // 给硬件一点缓冲时间
        
        // 在 video_mbox_event_handler 中修改
        sprintf(cmd, "gst-launch-1.0 filesrc location=%s ! decodebin name=dec ! queue ! videoconvert ! fbdevsink dec. ! queue ! audioconvert ! audioresample ! alsasink device=hw:0,0", file_path);
        LV_LOG_USER("playing...");
        system(cmd);
        
        usleep(10000); // 给硬件一点缓冲时间
        camera_100ask_dev_preview_control(1);
    }
    // 记得释放刚才动态分配的 op 结构体内存
    lv_mem_free(op);
    lv_msgbox_close(mbox);
}
