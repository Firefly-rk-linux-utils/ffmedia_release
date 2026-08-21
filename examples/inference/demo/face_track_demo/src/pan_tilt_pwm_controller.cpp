#include "pan_tilt_pwm_controller.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>

static constexpr uint32_t PERIOD_NS = 20000000;  // 20ms

static std::string buildPath(unsigned int chip, const std::string& sub)
{
    std::ostringstream oss;
    oss << "/sys/class/pwm/pwmchip" << chip << "/" << sub;
    return oss.str();
}

static void writeFile(const std::string& path, const std::string& value)
{
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open " + path + " for writing");
    }
    ofs << value;
    if (ofs.fail()) {
        throw std::runtime_error("Failed to write to " + path);
    }
}

static bool fileExists(const std::string& path)
{
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

PanTiltPWMController::PanTiltPWMController(unsigned int chip,
                                           unsigned int channel,
                                           double initialAngle,
                                           double minAngle,
                                           double maxAngle,
                                           uint32_t minPulseNs,
                                           uint32_t maxPulseNs)
    : chip_(chip), channel_(channel), enabled_(false),
      minAngle_(minAngle), maxAngle_(maxAngle),
      minPulseNs_(minPulseNs), maxPulseNs_(maxPulseNs)
{

    // 参数合法性检查
    if (minAngle >= maxAngle) {
        throw std::out_of_range("minAngle must be less than maxAngle");
    }
    if (minPulseNs <= 0 || maxPulseNs <= 0 || minPulseNs >= maxPulseNs) {
        throw std::out_of_range("Invalid pulse width range");
    }
    if (initialAngle < minAngle || initialAngle > maxAngle) {
        throw std::out_of_range("Initial angle out of range");
    }

    // 构建 sysfs 路径
    std::string pwmDir = "/sys/class/pwm/pwmchip" + std::to_string(chip_) + "/pwm" + std::to_string(channel_);
    basePath_ = pwmDir;
    polarityPath_ = pwmDir + "/polarity";
    periodPath_ = pwmDir + "/period";
    dutyCyclePath_ = pwmDir + "/duty_cycle";
    enablePath_ = pwmDir + "/enable";

    exportChannel();
    setPeriod(PERIOD_NS);
    setAngle(initialAngle);  // 写入 duty_cycle
    setPolarity("normal");
    enable();
}

PanTiltPWMController::~PanTiltPWMController()
{
}

void PanTiltPWMController::exportChannel()
{
    std::string exportPath = buildPath(chip_, "export");
    // 如果通道已经导出，直接返回
    if (fileExists(basePath_)) {
        return;
    }
    writeFile(exportPath, std::to_string(channel_));
    // 等待 sysfs 创建目录（通常很快，但可以增加短暂延迟）
    usleep(10000);
    if (!fileExists(basePath_)) {
        throw std::runtime_error("Export succeeded but pwm directory not created");
    }
}

void PanTiltPWMController::unexportChannel()
{
    std::string unexportPath = buildPath(chip_, "unexport");
    if (fileExists(basePath_)) {
        writeFile(unexportPath, std::to_string(channel_));
    }
}

void PanTiltPWMController::setPolarity(const std::string& polarity)
{
    writeFile(polarityPath_, polarity);
}

void PanTiltPWMController::setPeriod(uint32_t periodNs)
{
    writeFile(periodPath_, std::to_string(periodNs));
}

void PanTiltPWMController::setDutyCycle(uint32_t dutyNs)
{
    writeFile(dutyCyclePath_, std::to_string(dutyNs));
}

void PanTiltPWMController::setEnable(bool enable)
{
    writeFile(enablePath_, enable ? "1" : "0");
    enabled_ = enable;
}

void PanTiltPWMController::enable()
{
    setEnable(true);
}

void PanTiltPWMController::disable()
{
    setEnable(false);
}

bool PanTiltPWMController::isEnabled() const
{
    return enabled_;
}

uint32_t PanTiltPWMController::angleToDutyNs(double angle) const
{
    // 线性映射：角度从 [minAngle, maxAngle] 映射到 [minPulseNs, maxPulseNs]
    double t = (angle - minAngle_) / (maxAngle_ - minAngle_);
    t = std::clamp(t, 0.0, 1.0);
    double pulseNs = static_cast<double>(minPulseNs_) + t * static_cast<double>(maxPulseNs_ - minPulseNs_);
    return static_cast<uint32_t>(std::round(pulseNs));
}

void PanTiltPWMController::setAngle(double angle)
{
    angle = std::clamp(angle, minAngle_, maxAngle_);
    uint32_t dutyNs = angleToDutyNs(angle);
    setDutyCycle(dutyNs);
    currentAngle_ = angle;
}

double PanTiltPWMController::getAngle() const
{
    return currentAngle_;
}
