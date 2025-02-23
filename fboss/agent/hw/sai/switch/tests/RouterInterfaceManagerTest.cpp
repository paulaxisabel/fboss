/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/agent/hw/sai/store/SaiStore.h"
#include "fboss/agent/hw/sai/switch/SaiManagerTable.h"
#include "fboss/agent/hw/sai/switch/SaiRouterInterfaceManager.h"
#include "fboss/agent/hw/sai/switch/SaiSystemPortManager.h"
#include "fboss/agent/hw/sai/switch/tests/ManagerTestBase.h"
#include "fboss/agent/state/Interface.h"
#include "fboss/agent/types.h"

#include <string>

using namespace facebook::fboss;

/*
 * In these tests, we will assume 4 ports with one lane each, with IDs
 */

class RouterInterfaceManagerTest : public ManagerTestBase {
 public:
  void SetUp() override {
    setupStage = SetupStage::PORT | SetupStage::VLAN;
    ManagerTestBase::SetUp();
    intf0 = testInterfaces[0];
    intf1 = testInterfaces[1];
    sysPort1 =
        saiManagerTable->systemPortManager().addSystemPort(makeSystemPort());
    sysPort2 = saiManagerTable->systemPortManager().addSystemPort(
        makeSystemPort(std::nullopt, 2));
  }

  void checkRouterInterface(
      RouterInterfaceSaiId saiRouterInterfaceId,
      VlanSaiId expectedSaiVlanId,
      const folly::MacAddress& expectedSrcMac,
      int expectedMtu = 1500) const {
    auto saiVlanIdGot = saiApiTable->routerInterfaceApi().getAttribute(
        saiRouterInterfaceId,
        SaiVlanRouterInterfaceTraits::Attributes::VlanId{});
    auto srcMacGot = saiApiTable->routerInterfaceApi().getAttribute(
        saiRouterInterfaceId,
        SaiVlanRouterInterfaceTraits::Attributes::SrcMac{});
    auto vlanIdGot = saiApiTable->vlanApi().getAttribute(
        VlanSaiId{saiVlanIdGot}, SaiVlanTraits::Attributes::VlanId{});
    auto mtuGot = saiApiTable->routerInterfaceApi().getAttribute(
        saiRouterInterfaceId, SaiVlanRouterInterfaceTraits::Attributes::Mtu{});
    EXPECT_EQ(VlanSaiId{vlanIdGot}, expectedSaiVlanId);
    EXPECT_EQ(srcMacGot, expectedSrcMac);
    EXPECT_EQ(mtuGot, expectedMtu);
  }
  void checkPortRouterInterface(
      RouterInterfaceSaiId saiRouterInterfaceId,
      SystemPortSaiId expectedSaiPortId,
      const folly::MacAddress& expectedSrcMac,
      int expectedMtu = 1500) const {
    SystemPortSaiId saiPortIdGot{saiApiTable->routerInterfaceApi().getAttribute(
        saiRouterInterfaceId,
        SaiPortRouterInterfaceTraits::Attributes::PortId{})};
    auto srcMacGot = saiApiTable->routerInterfaceApi().getAttribute(
        saiRouterInterfaceId,
        SaiPortRouterInterfaceTraits::Attributes::SrcMac{});
    auto mtuGot = saiApiTable->routerInterfaceApi().getAttribute(
        saiRouterInterfaceId, SaiPortRouterInterfaceTraits::Attributes::Mtu{});
    EXPECT_EQ(saiPortIdGot, expectedSaiPortId);
    EXPECT_EQ(srcMacGot, expectedSrcMac);
    EXPECT_EQ(mtuGot, expectedMtu);
  }

  void checkAdapterKey(
      RouterInterfaceSaiId saiRouterInterfaceId,
      InterfaceID intfId) const {
    EXPECT_EQ(
        saiManagerTable->routerInterfaceManager()
            .getRouterInterfaceHandle(intfId)
            ->adapterKey(),
        saiRouterInterfaceId);
  }
  void checkType(
      RouterInterfaceSaiId saiRouterInterfaceId,
      InterfaceID intfId,
      cfg::InterfaceType expectedType) const {
    EXPECT_EQ(
        saiManagerTable->routerInterfaceManager()
            .getRouterInterfaceHandle(intfId)
            ->type(),
        expectedType);
  }

  void checkSubnets(
      const std::vector<SaiRouteTraits::RouteEntry>& subnetRoutes,
      bool shouldExist) const {
    for (const auto& route : subnetRoutes) {
      auto& store = saiStore->get<SaiRouteTraits>();
      auto routeObj = store.get(route);
      if (shouldExist) {
        EXPECT_NE(routeObj, nullptr);
      } else {
        EXPECT_EQ(routeObj, nullptr);
      }
    }
  }

  std::vector<SaiRouteTraits::RouteEntry> getSubnetKeys(InterfaceID id) const {
    std::vector<SaiRouteTraits::RouteEntry> keys;
    auto toMeRoutes = saiManagerTable->routerInterfaceManager()
                          .getRouterInterfaceHandle(id)
                          ->toMeRoutes;
    for (const auto& toMeRoute : toMeRoutes) {
      keys.emplace_back(toMeRoute->adapterHostKey());
    }
    return keys;
  }

  TestInterface intf0;
  TestInterface intf1;
  SystemPortSaiId sysPort1, sysPort2;
};

TEST_F(RouterInterfaceManagerTest, addRouterInterface) {
  auto swInterface = makeInterface(intf0);
  auto saiId =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  checkRouterInterface(
      saiId, static_cast<VlanSaiId>(intf0.id), intf0.routerMac);
}

TEST_F(RouterInterfaceManagerTest, addPortRouterInterface) {
  auto swSysPort = makeSystemPort();
  auto swInterface = makeInterface(*swSysPort, {intf0.subnet});
  auto saiId =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  checkPortRouterInterface(saiId, sysPort1, swInterface->getMac());
}

TEST_F(RouterInterfaceManagerTest, addTwoRouterInterfaces) {
  auto swInterface0 = makeInterface(intf0);
  auto saiId0 = saiManagerTable->routerInterfaceManager().addRouterInterface(
      swInterface0);
  auto swInterface1 = makeInterface(intf1);
  auto saiId1 = saiManagerTable->routerInterfaceManager().addRouterInterface(
      swInterface1);
  checkRouterInterface(
      saiId0, static_cast<VlanSaiId>(intf0.id), intf0.routerMac);
  checkRouterInterface(
      saiId1, static_cast<VlanSaiId>(intf1.id), intf1.routerMac);
}

TEST_F(RouterInterfaceManagerTest, addTwoPortRouterInterfaces) {
  auto swSysPort0 = makeSystemPort();
  auto swInterface = makeInterface(*swSysPort0, {intf0.subnet});
  auto saiId =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  checkPortRouterInterface(saiId, sysPort1, swInterface->getMac());
  auto swSysPort1 = makeSystemPort(std::nullopt, 2);
  auto swInterface1 = makeInterface(*swSysPort1, {intf0.subnet});
  auto saiId1 = saiManagerTable->routerInterfaceManager().addRouterInterface(
      swInterface1);
  checkPortRouterInterface(saiId1, sysPort2, swInterface1->getMac());
}

TEST_F(RouterInterfaceManagerTest, addDupRouterInterface) {
  auto swInterface = makeInterface(intf0);
  saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  EXPECT_THROW(
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface),
      FbossError);
}

TEST_F(RouterInterfaceManagerTest, addDupPortRouterInterface) {
  auto swSysPort = makeSystemPort();
  auto swInterface = makeInterface(*swSysPort, {intf0.subnet});
  std::ignore =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  EXPECT_THROW(
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface),
      FbossError);
}

TEST_F(RouterInterfaceManagerTest, changeRouterInterfaceMac) {
  auto swInterface = makeInterface(intf0);
  auto saiId =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  checkRouterInterface(
      saiId, static_cast<VlanSaiId>(intf0.id), intf0.routerMac);
  auto newMac = intf1.routerMac;
  CHECK_NE(swInterface->getMac(), newMac);
  auto newInterface = swInterface->clone();
  newInterface->setMac(newMac);
  saiManagerTable->routerInterfaceManager().changeRouterInterface(
      swInterface, newInterface);
  checkRouterInterface(saiId, static_cast<VlanSaiId>(intf0.id), newMac);
}

TEST_F(RouterInterfaceManagerTest, changePortRouterInterfaceMac) {
  auto swSysPort = makeSystemPort();
  auto swInterface = makeInterface(*swSysPort, {intf0.subnet});
  auto saiId =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  checkPortRouterInterface(saiId, sysPort1, swInterface->getMac());
  auto newMac = intf0.routerMac;
  CHECK_NE(swInterface->getMac(), newMac);
  auto newInterface = swInterface->clone();
  newInterface->setMac(newMac);
  saiManagerTable->routerInterfaceManager().changeRouterInterface(
      swInterface, newInterface);
  checkPortRouterInterface(saiId, sysPort1, newMac);
}

TEST_F(RouterInterfaceManagerTest, changeRouterInterfaceMtu) {
  auto swInterface = makeInterface(intf0);
  auto saiId =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  checkRouterInterface(
      saiId, static_cast<VlanSaiId>(intf0.id), intf0.routerMac);
  auto newMtu = intf0.mtu + 1000;
  auto newInterface = swInterface->clone();
  newInterface->setMtu(newMtu);
  saiManagerTable->routerInterfaceManager().changeRouterInterface(
      swInterface, newInterface);
  checkRouterInterface(
      saiId, static_cast<VlanSaiId>(intf0.id), intf0.routerMac, newMtu);
}

TEST_F(RouterInterfaceManagerTest, changePortRouterInterfaceMtu) {
  auto swSysPort = makeSystemPort();
  auto swInterface = makeInterface(*swSysPort, {intf0.subnet});
  auto saiId =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  checkPortRouterInterface(saiId, sysPort1, swInterface->getMac());
  auto newMtu = swInterface->getMtu() + 1000;
  auto newInterface = swInterface->clone();
  newInterface->setMtu(newMtu);
  saiManagerTable->routerInterfaceManager().changeRouterInterface(
      swInterface, newInterface);
  checkPortRouterInterface(saiId, sysPort1, swInterface->getMac(), newMtu);
}

TEST_F(RouterInterfaceManagerTest, removeRouterInterfaceSubnets) {
  auto oldInterface = makeInterface(intf0);
  auto saiId = saiManagerTable->routerInterfaceManager().addRouterInterface(
      oldInterface);
  checkRouterInterface(
      saiId, static_cast<VlanSaiId>(intf0.id), intf0.routerMac);
  auto newInterface = oldInterface->clone();
  newInterface->setAddresses({});

  auto oldToMeRoutes = getSubnetKeys(oldInterface->getID());
  saiManagerTable->routerInterfaceManager().changeRouterInterface(
      oldInterface, newInterface);
  checkSubnets(oldToMeRoutes, false /* should no longer exist*/);
}

TEST_F(RouterInterfaceManagerTest, removePortRouterInterfaceSubnets) {
  auto swSysPort = makeSystemPort();
  auto oldInterface = makeInterface(*swSysPort, {intf0.subnet});
  auto saiId = saiManagerTable->routerInterfaceManager().addRouterInterface(
      oldInterface);
  checkPortRouterInterface(saiId, sysPort1, oldInterface->getMac());
  auto newInterface = oldInterface->clone();
  newInterface->setAddresses({});

  auto oldToMeRoutes = getSubnetKeys(oldInterface->getID());
  saiManagerTable->routerInterfaceManager().changeRouterInterface(
      oldInterface, newInterface);
  checkSubnets(oldToMeRoutes, false /* should no longer exist*/);
}

TEST_F(RouterInterfaceManagerTest, changeRouterInterfaceSubnets) {
  auto oldInterface = makeInterface(intf0);
  auto saiId = saiManagerTable->routerInterfaceManager().addRouterInterface(
      oldInterface);
  checkRouterInterface(
      saiId, static_cast<VlanSaiId>(intf0.id), intf0.routerMac);
  auto newInterface = oldInterface->clone();
  newInterface->setAddresses({{folly::IPAddress{"100.100.100.1"}, 24}});

  auto oldToMeRoutes = getSubnetKeys(oldInterface->getID());
  saiManagerTable->routerInterfaceManager().changeRouterInterface(
      oldInterface, newInterface);
  auto newToMeRoutes = getSubnetKeys(oldInterface->getID());
  checkSubnets(oldToMeRoutes, false /* should no longer exist*/);
  checkSubnets(newToMeRoutes, true /* should  exist*/);
}

TEST_F(RouterInterfaceManagerTest, changePortRouterInterfaceSubnets) {
  auto swSysPort = makeSystemPort();
  auto oldInterface = makeInterface(*swSysPort, {intf0.subnet});
  auto saiId = saiManagerTable->routerInterfaceManager().addRouterInterface(
      oldInterface);
  checkPortRouterInterface(saiId, sysPort1, oldInterface->getMac());
  auto newInterface = oldInterface->clone();
  newInterface->setAddresses({{folly::IPAddress{"100.100.100.1"}, 24}});

  auto oldToMeRoutes = getSubnetKeys(oldInterface->getID());
  saiManagerTable->routerInterfaceManager().changeRouterInterface(
      oldInterface, newInterface);
  auto newToMeRoutes = getSubnetKeys(oldInterface->getID());
  checkSubnets(oldToMeRoutes, false /* should no longer exist*/);
  checkSubnets(newToMeRoutes, true /* should  exist*/);
}

TEST_F(RouterInterfaceManagerTest, addRouterInterfaceSubnets) {
  auto oldInterface = makeInterface(intf0);
  auto saiId = saiManagerTable->routerInterfaceManager().addRouterInterface(
      oldInterface);
  checkRouterInterface(
      saiId, static_cast<VlanSaiId>(intf0.id), intf0.routerMac);
  auto newInterface = oldInterface->clone();

  auto addresses = newInterface->getAddresses();
  addresses.emplace(folly::IPAddress{"100.100.100.1"}, 24);
  EXPECT_EQ(oldInterface->getAddresses().size() + 1, addresses.size());
  newInterface->setAddresses(addresses);

  auto oldToMeRoutes = getSubnetKeys(oldInterface->getID());
  saiManagerTable->routerInterfaceManager().changeRouterInterface(
      oldInterface, newInterface);
  auto newToMeRoutes = getSubnetKeys(oldInterface->getID());
  checkSubnets(oldToMeRoutes, true /* should exist*/);
  checkSubnets(newToMeRoutes, true /* should  exist*/);
}

TEST_F(RouterInterfaceManagerTest, addPortRouterInterfaceSubnets) {
  auto swSysPort = makeSystemPort();
  auto oldInterface = makeInterface(*swSysPort, {intf0.subnet});
  auto saiId = saiManagerTable->routerInterfaceManager().addRouterInterface(
      oldInterface);
  checkPortRouterInterface(saiId, sysPort1, oldInterface->getMac());
  auto newInterface = oldInterface->clone();
  auto addresses = newInterface->getAddresses();
  addresses.emplace(folly::IPAddress{"100.100.100.1"}, 24);
  EXPECT_EQ(oldInterface->getAddresses().size() + 1, addresses.size());
  newInterface->setAddresses(addresses);

  auto oldToMeRoutes = getSubnetKeys(oldInterface->getID());
  saiManagerTable->routerInterfaceManager().changeRouterInterface(
      oldInterface, newInterface);
  auto newToMeRoutes = getSubnetKeys(oldInterface->getID());
  checkSubnets(oldToMeRoutes, true /* should exist*/);
  checkSubnets(newToMeRoutes, true /* should  exist*/);
}

TEST_F(RouterInterfaceManagerTest, getRouterInterface) {
  auto oldInterface = makeInterface(intf0);
  auto saiId = saiManagerTable->routerInterfaceManager().addRouterInterface(
      oldInterface);
  auto routerInterfaceHandle =
      saiManagerTable->routerInterfaceManager().getRouterInterfaceHandle(
          InterfaceID(0));
  EXPECT_TRUE(routerInterfaceHandle);
  EXPECT_EQ(routerInterfaceHandle->adapterKey(), saiId);
}

TEST_F(RouterInterfaceManagerTest, getPortRouterInterface) {
  auto swSysPort = makeSystemPort();
  auto oldInterface = makeInterface(*swSysPort, {intf0.subnet});
  auto saiId = saiManagerTable->routerInterfaceManager().addRouterInterface(
      oldInterface);
  checkPortRouterInterface(saiId, sysPort1, oldInterface->getMac());
  auto routerInterfaceHandle =
      saiManagerTable->routerInterfaceManager().getRouterInterfaceHandle(
          InterfaceID(swSysPort->getID()));
  EXPECT_TRUE(routerInterfaceHandle);
  EXPECT_EQ(routerInterfaceHandle->adapterKey(), saiId);
}

TEST_F(RouterInterfaceManagerTest, getNonexistentRouterInterface) {
  auto swInterface = makeInterface(intf0);
  saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  auto routerInterfaceHandle =
      saiManagerTable->routerInterfaceManager().getRouterInterfaceHandle(
          InterfaceID(2));
  EXPECT_FALSE(routerInterfaceHandle);
}

TEST_F(RouterInterfaceManagerTest, removeRouterInterface) {
  auto swInterface = makeInterface(intf1);
  saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  auto& routerInterfaceManager = saiManagerTable->routerInterfaceManager();
  InterfaceID swId(1);
  routerInterfaceManager.removeRouterInterface(swInterface);
  auto routerInterfaceHandle =
      saiManagerTable->routerInterfaceManager().getRouterInterfaceHandle(swId);
  EXPECT_FALSE(routerInterfaceHandle);
}

TEST_F(RouterInterfaceManagerTest, removePortRouterInterface) {
  auto swSysPort = makeSystemPort();
  auto swInterface = makeInterface(*swSysPort, {intf0.subnet});
  std::ignore =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  auto& routerInterfaceManager = saiManagerTable->routerInterfaceManager();
  routerInterfaceManager.removeRouterInterface(swInterface);
  auto routerInterfaceHandle =
      saiManagerTable->routerInterfaceManager().getRouterInterfaceHandle(
          InterfaceID(swSysPort->getID()));
  EXPECT_FALSE(routerInterfaceHandle);
}

TEST_F(RouterInterfaceManagerTest, removeNonexistentRouterInterface) {
  auto swInterface = makeInterface(intf1);
  saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  auto& routerInterfaceManager = saiManagerTable->routerInterfaceManager();
  auto swInterface2 = makeInterface(testInterfaces[2]);
  EXPECT_THROW(
      routerInterfaceManager.removeRouterInterface(swInterface2), FbossError);
}

TEST_F(RouterInterfaceManagerTest, adapterKeyAndTypeRouterInterface) {
  auto swInterface = makeInterface(intf1);
  auto saiId =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  checkAdapterKey(saiId, swInterface->getID());
  checkType(saiId, swInterface->getID(), cfg::InterfaceType::VLAN);
}

TEST_F(RouterInterfaceManagerTest, adapterKeyAndTypePortRouterInterface) {
  auto swSysPort = makeSystemPort();
  auto swInterface = makeInterface(*swSysPort, {intf0.subnet});
  auto saiId =
      saiManagerTable->routerInterfaceManager().addRouterInterface(swInterface);
  checkAdapterKey(saiId, swInterface->getID());
  checkType(saiId, swInterface->getID(), cfg::InterfaceType::SYSTEM_PORT);
}
