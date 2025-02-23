// Copyright 2014-present Facebook. All Rights Reserved.

#include <fboss/lib/CommonFileUtils.h>
#include <fboss/platform/data_corral_service/darwin/DarwinChassisManager.h>
#include <fboss/platform/data_corral_service/darwin/DarwinFruModule.h>
#include <fboss/platform/data_corral_service/darwin/DarwinPlatformConfig.h>
#include <folly/logging/xlog.h>
#include <thrift/lib/cpp2/protocol/Serializer.h>

namespace {
// modules in darwin system
const std::string kDarwinFan = "DarwinFanModule";
const std::string kDarwinPem = "DarwinPemModule";
const std::string kDarwinRackmon = "DarwinRackmonModule";

// leds in darwin chasssis
const std::string kSystemLed = "SystemLed";
const std::string kFanLed = "FanLed";
const std::string kPemLed = "PemLed";
const std::string kRackmonLed = "RackmonLed";

// colors available in leds
const std::string kLedGreen = "Green";
const std::string kLedRed = "Red";
} // namespace

namespace facebook::fboss::platform::data_corral_service {

void DarwinChassisLed::setColorPath(
    DarwinLedColor color,
    const std::string& path) {
  XLOG(DBG2) << "led " << name_ << ", color " << color << ", sysfs path "
             << path;
  paths_[color] = path;
}

void DarwinChassisLed::setColor(DarwinLedColor color) {
  if (color_ != color) {
    if (!facebook::fboss::writeSysfs(paths_[color], "1")) {
      XLOG(ERR) << "failed to set color " << color << " for led " << name_;
      return;
    }
    for (auto const& colorPath : paths_) {
      if (colorPath.first != color) {
        if (!facebook::fboss::writeSysfs(colorPath.second, "0")) {
          XLOG(ERR) << "failed to unset color " << color << " for led "
                    << name_;
          return;
        }
      }
    }
    XLOG(INFO) << "Set led " << name_ << " from color " << color_
               << " to color " << color;
    color_ = color;
  }
}

DarwinLedColor DarwinChassisLed::getColor() {
  for (auto const& colorPath : paths_) {
    std::string brightness = facebook::fboss::readSysfs(colorPath.second);
    try {
      if (std::stoi(brightness) > 0) {
        color_ = colorPath.first;
        return color_;
      }
    } catch (const std::exception& ex) {
      XLOG(ERR) << "failed to parse present state from " << colorPath.second
                << " where the value is " << brightness;
      throw;
    }
  }
  return DarwinLedColor::OFF;
}

void DarwinChassisManager::initModules() {
  XLOG(DBG2) << "instantiate fru modules and chassis leds";
  auto platformConfig = apache::thrift::SimpleJSONSerializer::deserialize<
      DataCorralPlatformConfig>(getDarwinPlatformConfig());
  for (auto fru : *platformConfig.fruModules()) {
    auto name = *fru.name();
    auto fruModule = std::make_unique<DarwinFruModule>(name);
    fruModule->init(*fru.attributes());
    fruModules_.emplace(name, std::move(fruModule));
  }

  sysLed_ = std::make_unique<DarwinChassisLed>(kSystemLed);
  fanLed_ = std::make_unique<DarwinChassisLed>(kFanLed);
  pemLed_ = std::make_unique<DarwinChassisLed>(kPemLed);
  rackmonLed_ = std::make_unique<DarwinChassisLed>(kRackmonLed);
  for (auto attr : *platformConfig.chassisAttributes()) {
    if (*attr.name() == kSystemLed + kLedRed) {
      sysLed_->setColorPath(DarwinLedColor::RED, *attr.path());
    } else if (*attr.name() == kSystemLed + kLedGreen) {
      sysLed_->setColorPath(DarwinLedColor::GREEN, *attr.path());
    } else if (*attr.name() == kFanLed + kLedRed) {
      fanLed_->setColorPath(DarwinLedColor::RED, *attr.path());
    } else if (*attr.name() == kFanLed + kLedGreen) {
      fanLed_->setColorPath(DarwinLedColor::GREEN, *attr.path());
    } else if (*attr.name() == kPemLed + kLedRed) {
      pemLed_->setColorPath(DarwinLedColor::RED, *attr.path());
    } else if (*attr.name() == kPemLed + kLedGreen) {
      pemLed_->setColorPath(DarwinLedColor::GREEN, *attr.path());
    } else if (*attr.name() == kRackmonLed + kLedRed) {
      rackmonLed_->setColorPath(DarwinLedColor::RED, *attr.path());
    } else if (*attr.name() == kRackmonLed + kLedGreen) {
      rackmonLed_->setColorPath(DarwinLedColor::GREEN, *attr.path());
    }
  }
}

void DarwinChassisManager::programChassis() {
  DarwinLedColor sysLedColor = DarwinLedColor::GREEN;
  DarwinLedColor fanLedColor = DarwinLedColor::GREEN;
  DarwinLedColor pemLedColor = DarwinLedColor::GREEN;
  DarwinLedColor rackmonLedColor = DarwinLedColor::GREEN;
  for (auto& fru : fruModules_) {
    if (!fru.second->isPresent()) {
      XLOG(DBG2) << "Fru module " << fru.first << " is absent";
      sysLedColor = DarwinLedColor::RED;
      if (std::strncmp(
              fru.first.c_str(), kDarwinFan.c_str(), kDarwinFan.length()) ==
          0) {
        fanLedColor = DarwinLedColor::RED;
      } else if (
          std::strncmp(
              fru.first.c_str(), kDarwinPem.c_str(), kDarwinPem.length()) ==
          0) {
        pemLedColor = DarwinLedColor::RED;
      } else if (
          std::strncmp(
              fru.first.c_str(),
              kDarwinRackmon.c_str(),
              kDarwinRackmon.length()) == 0) {
        rackmonLedColor = DarwinLedColor::RED;
      }
    }
  }
  if (sysLedColor == DarwinLedColor::GREEN) {
    XLOG(DBG4) << "All fru modules are present";
  }
  sysLed_->setColor(sysLedColor);
  fanLed_->setColor(fanLedColor);
  pemLed_->setColor(pemLedColor);
  rackmonLed_->setColor(rackmonLedColor);
}

} // namespace facebook::fboss::platform::data_corral_service
