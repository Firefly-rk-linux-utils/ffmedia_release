#include "module_infer_rknn2.hpp"
#include <cstring>
#include <fstream>

static void dump_tensor_attr(rknn_tensor_attr* attr)
{
    ff_info("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
            "zp=%d, scale=%f\n",
            attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
            attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
            get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}


namespace FFMedia
{

ModuleInferRKNN2::ModuleInferRKNN2(const std::string& module_name,
                                   const std::string& model_path,
                                   const std::string& label_path)
    : ModuleInfer(module_name),
      _model_path(model_path),
      _label_path(label_path),
      _output_data_type(FFTensorDataType::UNKNOWN),
      _ctx(0),
      _input_mem(nullptr)
{
    _converter = std::make_shared<ModuleRga>();
    _converter->setBufferCount(0);

    if (_label_path != "")
        loadLabel();
}

ModuleInferRKNN2::~ModuleInferRKNN2()
{
    removeModel();
}

void ModuleInferRKNN2::loadLabel()
{
    try {
        std::ifstream ifs(_label_path);
        if (!ifs.is_open()) {
            ff_warn_m("Failed to open label file: %s\n", _label_path.c_str());
            return;
        }
        for (std::string line; std::getline(ifs, line);) {
            if (!line.empty() && line[line.length() - 1] == '\r') {
                line.erase(line.length() - 1);
            }
            _labels.push_back(line);
        }
    } catch (const std::exception& e) {
    }
}

int ModuleInferRKNN2::preprocess(const std::shared_ptr<MediaBuffer>& input_buffer, std::shared_ptr<MediaBuffer>& output_buffer)
{
    auto input_image_param = input_buffer->getImagePara();
    auto last_input_image_param = _converter->getInputImagePara();
    if (last_input_image_param.width != input_image_param.width || last_input_image_param.height != input_image_param.height) {
        auto output_image_param = output_buffer->getImagePara();
        auto scale_w = (float)output_image_param.width / input_image_param.width;
        auto scale_h = (float)output_image_param.height / input_image_param.height;
        this->_scale = std::min(scale_w, scale_h);

        ImageCrop letterbox_crop;
        letterbox_crop.x = 0;
        letterbox_crop.y = 0;
        letterbox_crop.w = static_cast<uint32_t>(input_image_param.width * this->_scale);
        letterbox_crop.h = static_cast<uint32_t>(input_image_param.height * this->_scale);
        _converter->setOutputImageCrop(letterbox_crop);
    }

    auto ret = _converter->doConsume(input_buffer, output_buffer);
    return ret != ModuleMedia::CONSUME_SUCCESS ? -1 : 0;
}

int ModuleInferRKNN2::infer(const std::shared_ptr<MediaBuffer>& input_buffer, std::vector<FFTensor>& output_tensors)
{
    int ret;
    ret = rknn_run(_ctx, nullptr);
    if (ret < 0) {
        ff_error_m("inference fail! ret =%d\n", ret);
        return ret;
    }
    output_tensors = _output_tensors;
    return 0;
}

int ModuleInferRKNN2::inferCombinations(const std::shared_ptr<MediaBuffer>& input_buffer)
{
    int ret;
    std::vector<FFTensor> tensors;
    auto start_time = std::chrono::high_resolution_clock::now();
    ret = preprocess(input_buffer, _model_input_buffer);
    if (ret < 0) {
        ff_error_m("Failed to preprocess input buffer");
        return ret;
    }
    auto preprocess_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time);

    start_time = std::chrono::high_resolution_clock::now();
    ret = infer(_model_input_buffer, tensors);
    if (ret < 0) {
        ff_error_m("Failed to infer");
        return ret;
    }
    auto infer_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time);

    start_time = std::chrono::high_resolution_clock::now();
    postprocess(tensors, input_buffer);
    auto postprocess_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time);
    ff_debug_m("preprocess time: %lld us, infer time: %lld us, postprocess time: %lld us\n",
               preprocess_time.count(), infer_time.count(), postprocess_time.count());
    return 0;
}

int ModuleInferRKNN2::initConverter(const ImagePara& input_image_param,
                                    const ImagePara& output_image_param)
{
    _converter->setSrcPara(input_image_param.v4l2Fmt, 0, 0, input_image_param.width,
                           input_image_param.height, input_image_param.hstride,
                           input_image_param.vstride);
    _converter->setDstPara(output_image_param.v4l2Fmt, 0, 0, output_image_param.width,
                           output_image_param.height, output_image_param.hstride,
                           output_image_param.vstride);
    return 0;
}

std::vector<FFTensor> ModuleInferRKNN2::rknnTensorsToFFTensors()
{
    std::vector<FFTensor> tensors;
    for (uint32_t i = 0; i < _output_attrs.size(); i++) {
        FFTensor tensor = {};
        if (_output_mems[i] == nullptr)
            return {};
        tensor.data = _output_mems[i]->virt_addr;
        tensor.size = _output_mems[i]->size;
        tensor.type = rknnToFFTensorsDataType(_output_attrs[i].type);

        for (uint32_t j = 0; j < _output_attrs[i].n_dims; j++)
            tensor.shape.push_back(_output_attrs[i].dims[j]);

        tensor.qnt_info.scale = _output_attrs[i].scale;
        tensor.qnt_info.zero_point = _output_attrs[i].zp;

        tensors.push_back(tensor);
    }
    return tensors;
}

int ModuleInferRKNN2::setCoremask(int core)
{
    if (!_ctx) {
        ff_error_m("model has not been initialized\n");
        return -1;
    }
    if (core < 0)
        return -1;

    // set core mask
    rknn_core_mask core_mask = RKNN_NPU_CORE_AUTO;
    switch (core % 3) {
        case 0:
            core_mask = RKNN_NPU_CORE_0;
            break;
        case 1:
            core_mask = RKNN_NPU_CORE_1;
            break;
        case 2:
            core_mask = RKNN_NPU_CORE_2;
            break;
    }

    int ret = rknn_set_core_mask(_ctx, core_mask);
    if (ret < 0) {
        ff_error_m("set core mask fail! ret =%d\n", ret);
    }

    return ret;
}

void ModuleInferRKNN2::removeModel()
{
    if (_ctx) {
        if (_input_mem) {
            rknn_destroy_mem(_ctx, _input_mem);
            _input_mem = nullptr;
        }
        _model_input_buffer.reset();

        for (auto it : _output_mems) {
            if (it != nullptr)
                rknn_destroy_mem(_ctx, it);
        }
        _output_mems.clear();
        _output_attrs.clear();
        _output_tensors.clear();

        rknn_destroy(_ctx);
        _ctx = 0;
    }
}

int ModuleInferRKNN2::initModel()
{
    int ret;

    if (_ctx) {
        ff_warn_m("model has been initialized\n");
        return 0;
    }

    do {
        ret = setModel((void*)_model_path.c_str(), 0);
        if (ret < 0) {
            ff_error_m("load model failed! ret =%d\n", ret);
            break;
        }

        ret = initModelBuffer();
        if (ret < 0) {
            ff_error_m("init buffer failed! ret =%d\n", ret);
            break;
        }

        _output_tensors = rknnTensorsToFFTensors();
        auto param = _model_input_buffer->getImagePara();
        ret = initConverter(param, param);
        if (ret < 0) {
            ff_error_m("initConverter fail! ret =%d\n", ret);
            break;
        }
    } while (0);

    if (ret < 0) {
        removeModel();
        return ret;
    }
    return 0;
}

int ModuleInferRKNN2::setModel(void* model, size_t model_size)
{
    int ret;
    ret = rknn_init(&_ctx, model, model_size, 0, NULL);
    if (ret < 0) {
        ff_error_m("rknn_init fail! ret=%d\n", ret);
        return ret;
    }

    rknn_sdk_version sdk_ver;
    ret = rknn_query(_ctx, RKNN_QUERY_SDK_VERSION, &sdk_ver, sizeof(sdk_ver));
    if (ret != RKNN_SUCC) {
        ff_error_m("rknn_query fail! ret=%d\n", ret);
        return -1;
    }
    ff_info_m("rknnrt version: %s, driver version: %s\n", sdk_ver.api_version, sdk_ver.drv_version);

    rknn_input_output_num io_num;
    ret = rknn_query(_ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC) {
        ff_error_m("rknn_query fail! ret=%d\n", ret);
        return -1;
    }

    ff_debug_m("model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);
    memset(&_input_attr, 0, sizeof(rknn_tensor_attr));
    _input_attr.index = 0;
    ret = rknn_query(_ctx, RKNN_QUERY_INPUT_ATTR, &_input_attr, sizeof(rknn_tensor_attr));
    if (ret < 0) {
        ff_error_m("rknn_init error! ret=%d\n", ret);
        return -1;
    }
    dump_tensor_attr(&_input_attr);

    _output_mems.resize(io_num.n_output, nullptr);
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        rknn_tensor_attr output_attr = {};
        output_attr.index = i;
        // query info
        ret = rknn_query(_ctx, RKNN_QUERY_OUTPUT_ATTR, &output_attr, sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            ff_error_m("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        _output_attrs.push_back(output_attr);
        dump_tensor_attr(&output_attr);
    }

    return 0;
}

int ModuleInferRKNN2::initModelBuffer()
{
    int ret;
    if (_input_attr.fmt == RKNN_TENSOR_NCHW) {
        ff_debug_m("model input format is NCHW\n");
        _channel = _input_attr.dims[1];
        _height = _input_attr.dims[2];
        _width = _input_attr.dims[3];
    } else if (_input_attr.fmt == RKNN_TENSOR_NHWC) {
        ff_debug_m("model input format is NHWC\n");
        _height = _input_attr.dims[1];
        _width = _input_attr.dims[2];
        _channel = _input_attr.dims[3];
    } else {
        ff_error_m("model input format is not supported\n");
        return -1;
    }

    if (_channel != 3) {
        ff_error_m("Unsupported channels %d\n", _channel);
        return -1;
    }

    ff_info_m("model input height=%d, width=%d, channel=%d\n", _height, _width, _channel);

    _input_attr.type = RKNN_TENSOR_UINT8;
    _input_attr.fmt = RKNN_TENSOR_NHWC;

    auto output_image_param = ImagePara(_width, _height, _input_attr.w_stride, _height, V4L2_PIX_FMT_RGB24);

#ifdef FF_USE_RKNN_INTERNAL_MEM
    ret = setModelIntInputMem(_input_attr.size_with_stride);
    if (ret < 0) {
        ff_error_m("setInputMem fail! ret =%d\n", ret);
        return -1;
    }
    std::shared_ptr<VideoBuffer> img_buf = std::make_shared<VideoBuffer>(VideoBuffer::EXTERNAL_BUFFER);
    img_buf->setImagePara(output_image_param);
    img_buf->initWithExternalBuffer(_input_mem->virt_addr, _input_mem->size, _input_mem->fd);
#else
    std::shared_ptr<VideoBuffer> img_buf = std::make_shared<VideoBuffer>(VideoBuffer::DRM_BUFFER_NONCACHEABLE);
    img_buf->allocBuffer(output_image_param);
    if (img_buf->getBufFd() < 0) {
        ff_error_m("alloc buffer fail!\n");
        return -1;
    }

    ret = setModelExtInputMem(img_buf->getBufFd(), img_buf->getData(), img_buf->getSize());
    if (ret < 0) {
        ff_error_m("setInputMem fail! ret =%d\n", ret);
        return ret;
    }
#endif
    // memset(img_buf->getData(), 0, img_buf->getSize());
    _model_input_buffer = img_buf;

    for (uint32_t i = 0; i < _output_attrs.size(); i++) {
        size_t size = _output_attrs[i].size_with_stride;
        if (_output_data_type != FFTensorDataType::UNKNOWN) {
            _output_attrs[i].type = FFTensorsDataTypeToRknn(_output_data_type);
            _output_attrs[i].fmt = RKNN_TENSOR_NCHW;
            size = _output_attrs[i].n_elems * sizeof(float);
        }

        ret = setModelIntOutputMem(i, size);
        if (ret < 0) {
            ff_error_m("setOutputMem fail! ret =%d\n", ret);
            return ret;
        }
    }
    return ret;
}

void ModuleInferRKNN2::setOutputTensorDataType(FFTensorDataType data_type)
{
    _output_data_type = data_type;
}

int ModuleInferRKNN2::setModelExtInputMem(int fd, void* addr, size_t size)
{
    _input_mem = rknn_create_mem_from_fd(_ctx, fd, addr, size, 0);
    if (_input_mem == nullptr)
        return -1;
    int ret = rknn_set_io_mem(_ctx, _input_mem, &_input_attr);
    if (ret < 0) {
        rknn_destroy_mem(_ctx, _input_mem);
        _input_mem = nullptr;
    }

    return ret;
}

int ModuleInferRKNN2::setModelIntInputMem(size_t size)
{
    _input_mem = rknn_create_mem2(_ctx, size, RKNN_FLAG_MEMORY_NON_CACHEABLE);
    if (_input_mem == nullptr)
        return -1;
    int ret = rknn_set_io_mem(_ctx, _input_mem, &_input_attr);
    if (ret < 0) {
        rknn_destroy_mem(_ctx, _input_mem);
        _input_mem = nullptr;
    }

    return ret;
}

int ModuleInferRKNN2::setModelIntOutputMem(int index, size_t size)
{
    rknn_tensor_mem* output_mem = rknn_create_mem2(_ctx, size, RKNN_FLAG_MEMORY_NON_CACHEABLE);
    if (output_mem == nullptr)
        return -1;
    int ret = rknn_set_io_mem(_ctx, output_mem, &_output_attrs[index]);
    if (ret < 0) {
        rknn_destroy_mem(_ctx, output_mem);
        return ret;
    }
    _output_mems[index] = output_mem;
    return ret;
}

int ModuleInferRKNN2::setModelExtOutputMem(int index, int fd, void* addr, size_t size)
{
    rknn_tensor_mem* output_mem = rknn_create_mem_from_fd(_ctx, fd, addr, size, 0);
    if (output_mem == nullptr)
        return -1;
    int ret = rknn_set_io_mem(_ctx, output_mem, &_output_attrs[index]);
    if (ret < 0) {
        rknn_destroy_mem(_ctx, output_mem);
        return ret;
    }
    _output_mems[index] = output_mem;
    return ret;
}

FFTensorDataType ModuleInferRKNN2::rknnToFFTensorsDataType(rknn_tensor_type type)
{
    switch (type) {
        case RKNN_TENSOR_FLOAT32:
            return FFTensorDataType::FLOAT32;
        case RKNN_TENSOR_FLOAT16:
            return FFTensorDataType::FLOAT16;
        case RKNN_TENSOR_INT8:
            return FFTensorDataType::INT8;
        case RKNN_TENSOR_UINT8:
            return FFTensorDataType::UINT8;
        case RKNN_TENSOR_INT16:
            return FFTensorDataType::INT16;
        case RKNN_TENSOR_UINT16:
            return FFTensorDataType::UINT16;
        case RKNN_TENSOR_INT32:
            return FFTensorDataType::INT32;
        case RKNN_TENSOR_UINT32:
            return FFTensorDataType::UINT32;
        case RKNN_TENSOR_INT64:
            return FFTensorDataType::INT64;
        case RKNN_TENSOR_BOOL:
            return FFTensorDataType::BOOL;
        default:
            return FFTensorDataType::UNKNOWN;
    }
    return FFTensorDataType::UNKNOWN;
}

rknn_tensor_type ModuleInferRKNN2::FFTensorsDataTypeToRknn(FFTensorDataType type)
{
    switch (type) {
        case FFTensorDataType::FLOAT32:
            return RKNN_TENSOR_FLOAT32;
        case FFTensorDataType::FLOAT16:
            return RKNN_TENSOR_FLOAT16;
        case FFTensorDataType::INT8:
            return RKNN_TENSOR_INT8;
        case FFTensorDataType::UINT8:
            return RKNN_TENSOR_UINT8;
        case FFTensorDataType::INT16:
            return RKNN_TENSOR_INT16;
        case FFTensorDataType::UINT16:
            return RKNN_TENSOR_UINT16;
        case FFTensorDataType::INT32:
            return RKNN_TENSOR_INT32;
        case FFTensorDataType::UINT32:
            return RKNN_TENSOR_UINT32;
        case FFTensorDataType::INT64:
            return RKNN_TENSOR_INT64;
        case FFTensorDataType::BOOL:
            return RKNN_TENSOR_BOOL;
        default:
            return RKNN_TENSOR_INT8;
    }
    return RKNN_TENSOR_INT8;
}

}  // namespace FFMedia
