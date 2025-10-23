#pragma once

#include "threadPool.hpp"
#include <vector>
#include <iostream>
#include <mutex>
#include <queue>
#include <memory>

// Inference模型类, inputType模型输入类型, outputType模型输出类型
template <typename InferenceModel, typename inputType, typename outputType>
class InferencePool
{
private:
    int threadNum;
    std::string modelPath;
    std::string modelLabelPath;

    long long id;
    std::mutex idMtx, queueMtx;
    std::unique_ptr<ThreadPool> pool;
    std::queue<std::future<outputType>> futs;
    std::vector<std::shared_ptr<InferenceModel>> models;

protected:
    int getModelId();

public:
    InferencePool(const std::string& modelPath, int threadNum, const std::string& modelLabelPath);
    int init();
    // 模型推理/Model Inference
    int put(inputType inputData);
    // 获取推理结果/Get the results of your Inference
    int get(outputType& outputData);
    ~InferencePool();
};

template <typename InferenceModel, typename inputType, typename outputType>
InferencePool<InferenceModel, inputType, outputType>::InferencePool(const std::string& modelPath,
                                                                    int threadNum, const std::string& modelLabelPath)
    : threadNum(threadNum), modelPath(modelPath), modelLabelPath(modelLabelPath), id(0)
{
}

template <typename InferenceModel, typename inputType, typename outputType>
int InferencePool<InferenceModel, inputType, outputType>::init()
{
    try {
        this->pool = std::make_unique<ThreadPool>(this->threadNum);
        for (int i = models.size(); i < this->threadNum; i++)
            models.push_back(std::make_shared<InferenceModel>());
    } catch (const std::bad_alloc& e) {
        std::cout << "Out of memory: " << e.what() << std::endl;
        return -1;
    }
    // 初始化模型/Initialize the model
    for (int i = 0, ret = 0; i < threadNum; i++) {
        ret = models[i]->init(modelPath, i, modelLabelPath);
        if (ret != 0)
            return ret;
    }

    return 0;
}

template <typename InferenceModel, typename inputType, typename outputType>
int InferencePool<InferenceModel, inputType, outputType>::getModelId()
{
    std::lock_guard<std::mutex> lock(idMtx);
    int modelId = id % threadNum;
    id++;
    return modelId;
}

template <typename InferenceModel, typename inputType, typename outputType>
int InferencePool<InferenceModel, inputType, outputType>::put(inputType inputData)
{
    std::lock_guard<std::mutex> lock(queueMtx);
    futs.push(pool->submit(&InferenceModel::infer, models[this->getModelId()], inputData));
    return 0;
}

template <typename InferenceModel, typename inputType, typename outputType>
int InferencePool<InferenceModel, inputType, outputType>::get(outputType& outputData)
{
    std::lock_guard<std::mutex> lock(queueMtx);
    if (futs.empty() == true)
        return 1;
    outputData = futs.front().get();
    futs.pop();
    return 0;
}

template <typename InferenceModel, typename inputType, typename outputType>
InferencePool<InferenceModel, inputType, outputType>::~InferencePool()
{
    while (!futs.empty()) {
        outputType temp = futs.front().get();
        futs.pop();
    }
}
