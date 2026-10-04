/**
 * @file lvgl_port.h
 * @brief LVGL移植层 - 接口定义
 * @version 1.0.0
 * @date 2026-06-28
 */

#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include "display_driver.h"
#include "lvgl.h"

/* ========== 版本定义 ========== */
#define LVGL_PORT_VERSION "1.0.0"

/* ========== LVGL配置参数 ========== */
// 缓冲区配置
#define LVGL_BUFFER_SIZE    (display_get_width() * 10)  // 10行缓冲
#define LVGL_BUFFER_COUNT   2  // 双缓冲

// 任务配置
#define LVGL_TASK_PRIORITY  5
#define LVGL_TASK_STACK     4096
#define LVGL_TICK_PERIOD_MS 5  // LVGL心跳周期

/* ========== 锁函数类型定义 ========== */
typedef void (*lvgl_lock_fn_t)(void);
typedef void (*lvgl_unlock_fn_t)(void);

/* ========== 公共接口 ========== */

/**
 * @brief 初始化单个屏幕的LVGL显示
 * @param type 屏幕型号
 * @param pins 引脚配置（可选，NULL使用默认配置）
 * @param display_id 显示ID（用于多屏场景，0为主屏）
 * @return LVGL显示对象指针
 */
lv_display_t *lvgl_port_init_display(display_type_t type, const display_pin_config_t *pins, int display_id);

/**
 * @brief 初始化LVGL移植层（单屏场景）
 * @param type 屏幕型号
 * @param pins 引脚配置（可选，NULL使用默认配置）
 * @return ESP_OK成功，其他值失败
 */
esp_err_t lvgl_port_init(display_type_t type, const display_pin_config_t *pins);

/**
 * @brief 初始化多屏幕LVGL
 * @param types 屏幕型号数组
 * @param pins 引脚配置数组
 * @param count 屏幕数量
 * @return ESP_OK成功，其他值失败
 */
esp_err_t lvgl_port_init_multi(display_type_t *types, const display_pin_config_t **pins, int count);

/**
 * @brief 启动LVGL任务
 * @return ESP_OK成功，其他值失败
 */
esp_err_t lvgl_port_start(void);

/**
 * @brief 停止LVGL任务
 * @return ESP_OK成功，其他值失败
 */
esp_err_t lvgl_port_stop(void);

/**
 * @brief 获取LVGL显示对象（主屏幕）
 * @return LVGL显示对象指针
 */
lv_display_t *lvgl_port_get_display(void);

/**
 * @brief 获取指定屏幕的LVGL显示对象
 * @param display_id 显示ID
 * @return LVGL显示对象指针
 */
lv_display_t *lvgl_port_get_display_by_id(int display_id);

/**
 * @brief 获取已初始化的屏幕数量
 * @return 屏幕数量
 */
int lvgl_port_get_display_count(void);

/**
 * @brief LVGL flush回调函数
 * @param display LVGL显示对象
 * @param area 刷新区域
 * @param px_map 像素数据指针
 */
void lvgl_port_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map);

/**
 * @brief 获取LVGL版本
 * @return LVGL版本字符串
 */
const char *lvgl_port_get_version(void);

/**
 * @brief 注入锁函数（用于调度器）
 * @param lock_fn 锁函数
 * @param unlock_fn 解锁函数
 */
void lvgl_port_set_lock_functions(lvgl_lock_fn_t lock_fn, lvgl_unlock_fn_t unlock_fn);

/**
 * @brief 获取锁函数
 * @return 锁函数指针
 */
lvgl_lock_fn_t lvgl_port_get_lock_fn(void);

/**
 * @brief 获取解锁函数
 * @return 解锁函数指针
 */
lvgl_unlock_fn_t lvgl_port_get_unlock_fn(void);

#endif // LVGL_PORT_H
