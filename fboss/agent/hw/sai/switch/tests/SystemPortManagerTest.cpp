/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/agent/hw/sai/switch/SaiSystemPortManager.h"
#include "fboss/agent/hw/sai/switch/tests/ManagerTestBase.h"
#include "fboss/agent/state/SwitchState.h"
#include "fboss/agent/state/SystemPort.h"
#include "fboss/agent/types.h"

#include <string>

using namespace facebook::fboss;

class SystemPortManagerTest : public ManagerTestBase {
 public:
  void SetUp() override {
    setupStage = SetupStage::PORT;
    ManagerTestBase::SetUp();
  }
};

TEST_F(SystemPortManagerTest, addSystemPort) {
  std::shared_ptr<SystemPort> swSystemPort = makeSystemPort(std::nullopt);
  SystemPortSaiId saiId =
      saiManagerTable->systemPortManager().addSystemPort(swSystemPort);
  auto configInfo = saiApiTable->systemPortApi().getAttribute(
      saiId, SaiSystemPortTraits::Attributes::ConfigInfo());
  EXPECT_EQ(configInfo.port_id, 1);
}

TEST_F(SystemPortManagerTest, addTwoSystemPort) {
  SystemPortSaiId saiId = saiManagerTable->systemPortManager().addSystemPort(
      makeSystemPort(std::nullopt));
  auto configInfo = saiApiTable->systemPortApi().getAttribute(
      saiId, SaiSystemPortTraits::Attributes::ConfigInfo());
  EXPECT_EQ(configInfo.port_id, 1);
  SystemPortSaiId saiId2 = saiManagerTable->systemPortManager().addSystemPort(
      makeSystemPort(std::nullopt, 2));
  auto configInfo2 = saiApiTable->systemPortApi().getAttribute(
      saiId2, SaiSystemPortTraits::Attributes::ConfigInfo());
  EXPECT_EQ(configInfo2.port_id, 2);
}

TEST_F(SystemPortManagerTest, addDupSystemPort) {
  saiManagerTable->systemPortManager().addSystemPort(
      makeSystemPort(std::nullopt));
  EXPECT_THROW(
      saiManagerTable->systemPortManager().addSystemPort(
          makeSystemPort(std::nullopt)),
      FbossError);
}

TEST_F(SystemPortManagerTest, addSystemPortViaSwitchState) {
  std::shared_ptr<SystemPort> swSystemPort = makeSystemPort(std::nullopt);
  auto state = programmedState->clone();
  state->addSystemPort(swSystemPort);
  applyNewState(state);
  auto handle =
      saiManagerTable->systemPortManager().getSystemPortHandle(SystemPortID(1));
  EXPECT_NE(handle, nullptr);
  auto configInfo =
      GET_ATTR(SystemPort, ConfigInfo, handle->systemPort->attributes());
  EXPECT_EQ(configInfo.port_id, 1);
}

TEST_F(SystemPortManagerTest, changeSystemPort) {
  std::shared_ptr<SystemPort> swSystemPort = makeSystemPort(std::nullopt);
  saiManagerTable->systemPortManager().addSystemPort(swSystemPort);
  auto swSystemPort2 = makeSystemPort(std::nullopt, 1, 0);
  saiManagerTable->systemPortManager().changeSystemPort(
      swSystemPort, swSystemPort2);
  auto handle =
      saiManagerTable->systemPortManager().getSystemPortHandle(SystemPortID(1));
  EXPECT_NE(handle, nullptr);
  auto configInfo =
      GET_ATTR(SystemPort, ConfigInfo, handle->systemPort->attributes());
  EXPECT_EQ(configInfo.port_id, 1);
  EXPECT_EQ(configInfo.attached_switch_id, 0);
}

TEST_F(SystemPortManagerTest, changeSystemPortViaSwitchState) {
  std::shared_ptr<SystemPort> swSystemPort = makeSystemPort(std::nullopt);
  auto state = programmedState->clone();
  state->addSystemPort(swSystemPort);
  applyNewState(state);
  auto newState = state->clone();
  auto sysPorts = newState->getSystemPorts()->modify(&newState);
  auto newSysPort = sysPorts->getNodeIf(SystemPortID(1))->clone();
  newSysPort->setSwitchId(SwitchID(0));
  sysPorts->updateNode(newSysPort);
  applyNewState(newState);
  auto handle =
      saiManagerTable->systemPortManager().getSystemPortHandle(SystemPortID(1));
  EXPECT_NE(handle, nullptr);
  auto configInfo =
      GET_ATTR(SystemPort, ConfigInfo, handle->systemPort->attributes());
  EXPECT_EQ(configInfo.port_id, 1);
  EXPECT_EQ(configInfo.attached_switch_id, 0);
}

TEST_F(SystemPortManagerTest, removeSystemPort) {
  std::shared_ptr<SystemPort> swSystemPort = makeSystemPort(std::nullopt);
  SystemPortSaiId saiId =
      saiManagerTable->systemPortManager().addSystemPort(swSystemPort);

  saiManagerTable->systemPortManager().removeSystemPort(swSystemPort);
  EXPECT_THROW(
      saiApiTable->systemPortApi().getAttribute(
          saiId, SaiSystemPortTraits::Attributes::ConfigInfo()),
      std::exception);
}

TEST_F(SystemPortManagerTest, removeSystemPortViaSwitchState) {
  std::shared_ptr<SystemPort> swSystemPort = makeSystemPort(std::nullopt);
  auto state = programmedState->clone();
  state->addSystemPort(swSystemPort);
  applyNewState(state);
  auto newState = state->clone();
  auto sysPorts = newState->getSystemPorts()->modify(&newState);
  sysPorts->removeNode(swSystemPort->getID());
  applyNewState(newState);
  auto handle =
      saiManagerTable->systemPortManager().getSystemPortHandle(SystemPortID(1));
  EXPECT_EQ(handle, nullptr);
}
