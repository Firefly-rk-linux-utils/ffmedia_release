#pragma once

#include <string>
#include <cstdint>

/**
 * @brief Linux PWM 云台控制类，全部以纳秒为单位
 *
 * 周期固定 20ms = 20,000,000 ns
 * 角度与脉宽线性映射，脉宽范围由用户指定（纳秒）
 */
class PanTiltPWMController
{
public:
    /**
     * @brief 构造函数，直接使用纳秒指定脉宽范围
     * @param chip PWM chip 编号，对应 /sys/class/pwm/pwmchipN
     * @param channel PWM 通道号
     * @param initialAngle 初始角度（度），必须在 [minAngle, maxAngle] 内
     * @param minAngle 云台支持的最小角度（物理限位）
     * @param maxAngle 云台支持的最大角度
     * @param minPulseNs 最小角度对应的脉宽（纳秒）
     * @param maxPulseNs 最大角度对应的脉宽（纳秒）
     * @throws std::runtime_error 如果无法导出通道或设置周期失败
     * @throws std::out_of_range 如果参数无效或初始角度超限
     */
    PanTiltPWMController(unsigned int chip,
                         unsigned int channel,
                         double initialAngle,
                         double minAngle = 0.0,
                         double maxAngle = 180.0,
                         uint32_t minPulseNs = 500000,
                         uint32_t maxPulseNs = 1500000);

    ~PanTiltPWMController();

    // 禁止拷贝和赋值
    PanTiltPWMController(const PanTiltPWMController&) = delete;
    PanTiltPWMController& operator=(const PanTiltPWMController&) = delete;

    /**
     * @brief 设置云台角度
     * @param angle 目标角度（度），自动限制在 [minAngle, maxAngle] 内
     * @throws std::runtime_error 如果写入 duty_cycle 失败
     */
    void setAngle(double angle);

    /**
     * @brief 获取当前角度
     */
    double getAngle() const;

    /**
     * @brief 使能 PWM 输出
     */
    void enable();

    /**
     * @brief 禁用 PWM 输出
     */
    void disable();

    /**
     * @brief 检查是否使能
     */
    bool isEnabled() const;

private:
    unsigned int chip_;
    unsigned int channel_;
    double currentAngle_;
    bool enabled_;

    double minAngle_;
    double maxAngle_;
    uint32_t minPulseNs_;
    uint32_t maxPulseNs_;

    std::string basePath_;
    std::string polarityPath_;
    std::string periodPath_;
    std::string dutyCyclePath_;
    std::string enablePath_;

    void exportChannel();
    void unexportChannel();
    void setPolarity(const std::string& polarity);
    void setPeriod(uint32_t periodNs);
    void setDutyCycle(uint32_t dutyNs);
    void setEnable(bool enable);

    // 角度 → 脉宽（纳秒），线性映射
    uint32_t angleToDutyNs(double angle) const;
};
