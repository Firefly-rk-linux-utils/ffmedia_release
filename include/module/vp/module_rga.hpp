/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-19 16:42:02
 * @Description: 图像处理组件，支持颜色格式转换、缩放、叠加等功能。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_RGA_HPP__
#define __MODULE_RGA_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"


class FFRga;

class ModuleRga : public ModuleMedia
{
private:
    shared_ptr<FFRga> rga;
    RgaRotate rotate;
    callback_handler blend_callback;
    void_object blend_callback_ctx;

public:
    enum RGA_SCHEDULER_CORE {
        SCHEDULER_DEFAULT = 0,
        SCHEDULER_RGA3_CORE0 = 1 << 0,
        SCHEDULER_RGA3_CORE1 = 1 << 1,
        SCHEDULER_RGA2_CORE0 = 1 << 2,
        SCHEDULER_RGA2_CORE1 = 1 << 3,
        SCHEDULER_RGA3_DEFAULT = SCHEDULER_RGA3_CORE0 | SCHEDULER_RGA3_CORE1,
        SCHEDULER_RGA2_DEFAULT = SCHEDULER_RGA2_CORE0
    };
    enum RGA_BLEND_MODE {
        BLEND_DISABLE = 0,
        BLEND_SRC,
        BLEND_DST,
        BLEND_SRC_OVER = 0x105,
        BLEND_DST_OVER = 0x501
    };

public:
    ModuleRga();
    ModuleRga(const ImagePara& output_para, RgaRotate rotate);
    /**
     * @description: ModuleRga 的构造函数。
     * @param {ImagePara&} input_para   输入处理的图像参数。
     * @param {ImagePara&} output_para  输出处理后的图像参数。
     * @param {RgaRotate} rotate        图像旋转。
     * @return {*}
     */
    ModuleRga(const ImagePara& input_para, const ImagePara& output_para, RgaRotate rotate);
    ~ModuleRga();

    /**
     * @description: 改变对象的输出图像参数。此调用应在对象停止时使用。
     * @param {ImagePara&} para     输出图像参数。
     * @return {int}                成功返回 0，失败返回负数。
     */
    int changeOutputPara(const ImagePara& para);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 设置输入图像参数函数。
     * @param {uint32_t} fmt        图像格式。
     * @param {uint32_t} x          图像使用区域的起点x坐标。
     * @param {uint32_t} y          图像使用区域的起点y坐标。
     * @param {uint32_t} w          图像使用区域的宽度。
     * @param {uint32_t} h          图像使用区域的高度。
     * @param {uint32_t} hstride    图像虚拟宽度。
     * @param {uint32_t} vstride    图像虚拟高度。
     * @return {*}
     */
    void setSrcPara(uint32_t fmt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t hstride, uint32_t vstride);

    /**
     * @description: 设置输出图像参数函数。
     * @param {uint32_t} fmt        图像格式。
     * @param {uint32_t} x          图像使用区域的起点x坐标。
     * @param {uint32_t} y          图像使用区域的起点y坐标。
     * @param {uint32_t} w          图像使用区域的宽度。
     * @param {uint32_t} h          图像使用区域的高度。
     * @param {uint32_t} hstride    图像虚拟宽度。
     * @param {uint32_t} vstride    图像虚拟高度。
     * @return {*}
     */
    void setDstPara(uint32_t fmt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t hstride, uint32_t vstride);

    /**
     * @description: 设置混合图像参数函数。
     * @param {uint32_t} fmt        图像格式。
     * @param {uint32_t} x          图像使用区域的起点x坐标。
     * @param {uint32_t} y          图像使用区域的起点y坐标。
     * @param {uint32_t} w          图像使用区域的宽度。
     * @param {uint32_t} h          图像使用区域的高度。
     * @param {uint32_t} hstride    图像虚拟宽度。
     * @param {uint32_t} vstride    图像虚拟高度。
     * @return {*}
     */
    void setPatPara(uint32_t fmt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t hstride, uint32_t vstride);
    /**
     * @description: 设置输入图像虚拟内存地址。
     * @param {void*} buf   图像内存地址。
     * @return {*}
     */
    void setSrcBuffer(void* buf);

    /**
     * @description: 设置输入图像内存描述符，零拷贝模式。
     * @param {int} fd  图像内存描述符。
     * @return {*}
     */
    void setSrcBuffer(int fd);

    /**
     * @description: 设置混合图像虚拟内存地址。
     * @param {void*} buf               图像内存地址。
     * @param {RGA_BLEND_MODE} mode     图像blend模式。
     * @return {*}
     */
    void setPatBuffer(void* buf, RGA_BLEND_MODE mode);

    /**
     * @description: 设置混合图像内存描述符，零拷贝模式。
     * @param {int} fd                  图像内存描述符。
     * @param {RGA_BLEND_MODE} mode     图像混合模式。
     * @return {*}
     */
    void setPatBuffer(int fd, RGA_BLEND_MODE mode);

    /**
     * @description: 设置混合回调函数，混合前将会调用该回调。使用图像混合功能，需要更新混合的图像，可在回调中设置混合图像内存。
     * @param {void_object_p} ctx           上下文。
     * @param {callback_handler} callback   混合回调函数指针。
     * @return {*}
     */
    void setBlendCallback(void_object_p ctx, callback_handler callback);

    /**
     * @description: 设置图像旋转模式
     * @param {RgaRotate} rotate
     * @return {*}
     */
    void setRotate(RgaRotate rotate);

    /**
     * @description: 设置调度核心。
     * @param {RGA_SCHEDULER_CORE} core
     * @return {*}
     */
    void setRgaSchedulerCore(RGA_SCHEDULER_CORE core);

    /**
     * @description: 申请对象使用的图像内存。可供exportUseMediaBuffer交换图像使用。
     * @param {BUFFER_TYPE} buffer_type     内存类型。
     * @return {*}
     */
    shared_ptr<MediaBuffer> newModuleMediaBuffer(VideoBuffer::BUFFER_TYPE buffer_type = VideoBuffer::BUFFER_TYPE::DRM_BUFFER_CACHEABLE);

    /**
     * @description: 导出匹配的图像内存。
     * @param {shared_ptr<MediaBuffer>}     match_buffer 需要导出的对象图像内存。
     * @param {shared_ptr<MediaBuffer>}     input_buffer 要和对象内存池交换的图像内存，如果为空，则会自动申请补充图像内存池。
     * @param {int} flag                    操作标志位。0为交换操作，1为拷贝操作。
     * @return {shared_ptr<MediaBuffer>}    成功返回导出后的图像内存；失败返回空指针，match_buffer不在对象的图像内存池里。
     */
    shared_ptr<MediaBuffer> exportUseMediaBuffer(shared_ptr<MediaBuffer> match_buffer, shared_ptr<MediaBuffer> input_buffer, int flag);
    int dstFillColor(int color);

    static void alignStride(uint32_t fmt, uint32_t& wstride, uint32_t& hstride);

public:
    /**
     * @description: 手动调用rga处理图像接口，可将输入图像内存数据处理拷贝到输出图像内存。此调用应在对象停止期间调用。
     * @param {shared_ptr<MediaBuffer>} input_buffer    输入的图像内存。
     * @param {shared_ptr<MediaBuffer>} output_buffer   输出的图像内存。
     * @return {ConsumeResult}                          成功返回CONSUME_SUCCESS。
     */
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
};

#endif  //__MODULE_RGA_HPP__
