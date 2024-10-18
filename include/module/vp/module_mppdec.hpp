/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-14 13:38:06
 * @Description: 视频解码组件。支持H264、H265及MJPEG解码。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE__MPPDEC_HPP__
#define __MODULE__MPPDEC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

class MppDecoder;

class ModuleMppDec : public ModuleMedia
{
private:
    shared_ptr<MppDecoder> dec;
    DecodeType decode_type;
    uint32_t need_split;

protected:
    void setAlign(DecodeType decode_type);
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> buffer) override;
    virtual int initBuffer() override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer) override;
    void reset() override;

public:
    ModuleMppDec();
    ModuleMppDec(const ImagePara& input_para);
    /**
     * @description: ModuleMppDec 的构造函数。
     * @param {ImagePara&} input_para   输入图像参数信息。
     * @param {DecodeType} type         解码类型。
     * @return {*}
     */
    ModuleMppDec(const ImagePara& input_para, DecodeType type);
    ~ModuleMppDec();

    /**
     * @description: 设置分帧模式，默认不分帧。
     * @param {uint32_t} split  分帧参数，0关闭分帧模式，1开启分帧模式。
     * @return {*}
     */
    void setNeedSplit(uint32_t split);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;
};

#endif
