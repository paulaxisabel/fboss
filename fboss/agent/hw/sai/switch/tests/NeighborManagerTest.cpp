/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/agent/FbossError.h"
#include "fboss/agent/hw/sai/api/SaiApiTable.h"
#include "fboss/agent/hw/sai/fake/FakeSai.h"
#include "fboss/agent/hw/sai/switch/SaiManagerTable.h"
#include "fboss/agent/hw/sai/switch/SaiNeighborManager.h"
#include "fboss/agent/hw/sai/switch/tests/ManagerTestBase.h"
#include "fboss/agent/state/ArpEntry.h"
#include "fboss/agent/state/NdpEntry.h"
#include "fboss/agent/types.h"

using namespace facebook::fboss;
class NeighborManagerTest : public ManagerTestBase {
 public:
  void SetUp() override {
    setupStage = SetupStage::PORT | SetupStage::VLAN | SetupStage::INTERFACE |
        SetupStage::SYSTEM_PORT;
    ManagerTestBase::SetUp();
    intf0 = testInterfaces[0];
    h0 = intf0.remoteHosts[0];
  }

  template <typename NeighborEntryT>
  void checkEntry(
      const NeighborEntryT& neighborEntry,
      const folly::MacAddress& expectedDstMac,
      cfg::InterfaceType expectedRifType = cfg::InterfaceType::VLAN,
      sai_uint32_t expectedMetadata = 0,
      sai_uint32_t expectedEncapIndex = 0,
      bool expectedIsLocal = true) {
    auto saiEntry =
        saiManagerTable->neighborManager().saiEntryFromSwEntry(neighborEntry);
    auto gotMac = saiApiTable->neighborApi().getAttribute(
        saiEntry, SaiNeighborTraits::Attributes::DstMac{});
    EXPECT_EQ(gotMac, expectedDstMac);
    auto gotMetadata = saiApiTable->neighborApi().getAttribute(
        saiEntry, SaiNeighborTraits::Attributes::Metadata{});
    EXPECT_EQ(gotMetadata, expectedMetadata);
    auto gotEncapIndex = saiApiTable->neighborApi().getAttribute(
        saiEntry, SaiNeighborTraits::Attributes::EncapIndex{});
    EXPECT_EQ(gotEncapIndex, expectedEncapIndex);
    auto gotIsLocal = saiApiTable->neighborApi().getAttribute(
        saiEntry, SaiNeighborTraits::Attributes::IsLocal{});
    EXPECT_EQ(gotIsLocal, expectedIsLocal);
    auto saiNeighborHandle =
        saiManagerTable->neighborManager().getNeighborHandle(saiEntry);
    EXPECT_TRUE(saiNeighborHandle);
    EXPECT_TRUE(saiNeighborHandle->neighbor);
    auto rifType =
        saiManagerTable->neighborManager().getNeighborRifType(saiEntry);
    EXPECT_EQ(rifType, expectedRifType);
    switch (rifType) {
      case cfg::InterfaceType::VLAN:
        EXPECT_TRUE(saiNeighborHandle->fdbEntry);
        break;
      case cfg::InterfaceType::SYSTEM_PORT:
        EXPECT_EQ(saiNeighborHandle->fdbEntry, nullptr);
        break;
    }
  }

  template <typename NeighborEntryT>
  void checkMissing(const NeighborEntryT& neighborEntry) {
    auto saiEntry =
        saiManagerTable->neighborManager().saiEntryFromSwEntry(neighborEntry);
    auto saiNeighborHandle =
        saiManagerTable->neighborManager().getNeighborHandle(saiEntry);
    EXPECT_FALSE(saiNeighborHandle);
  }

  template <typename NeighborEntryT>
  void checkUnresolved(const NeighborEntryT& neighborEntry) {
    auto saiEntry =
        saiManagerTable->neighborManager().saiEntryFromSwEntry(neighborEntry);
    auto saiNeighborHandle =
        saiManagerTable->neighborManager().getNeighborHandle(saiEntry);
    EXPECT_TRUE(saiNeighborHandle);
  }

  TestInterface intf0;
  TestRemoteHost h0;
};

TEST_F(NeighborManagerTest, addResolvedNeighbor) {
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
}

TEST_F(NeighborManagerTest, addResolvedPortRifNeighbor) {
  auto arpEntry = resolveArp(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT);
}

TEST_F(NeighborManagerTest, addResolvedNeighborWithMetadata) {
  auto arpEntry = resolveArp(intf0.id, h0, cfg::InterfaceType::VLAN, 42);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::VLAN, 42);
}

TEST_F(NeighborManagerTest, addResolvedPortRifNeighborWithMetadata) {
  auto arpEntry = resolveArp(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT, 42);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT, 42);
}

TEST_F(NeighborManagerTest, addResolvedNeighborWithEncapIndex) {
  EXPECT_THROW(
      resolveArp(intf0.id, h0, cfg::InterfaceType::VLAN, std::nullopt, 42),
      FbossError);
}

TEST_F(NeighborManagerTest, addResolvedPortRifNeighborWithEncapIndex) {
  auto arpEntry = resolveArp(
      intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT, std::nullopt, 42);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT, 0, 42);
}

TEST_F(NeighborManagerTest, addResolvedNeighborWithEncapIndexRemote) {
  EXPECT_THROW(
      resolveArp(
          intf0.id, h0, cfg::InterfaceType::VLAN, std::nullopt, 42, false),
      FbossError);
}

TEST_F(NeighborManagerTest, addResolvedPortRifNeighborWithEncapIndexRemote) {
  auto arpEntry = resolveArp(
      intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT, std::nullopt, 42, false);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT, 0, 42, false);
}

TEST_F(NeighborManagerTest, removeResolvedNeighbor) {
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
  saiManagerTable->neighborManager().removeNeighbor(arpEntry);
  checkMissing(arpEntry);
}

TEST_F(NeighborManagerTest, removeResolvedPortRifNeighbor) {
  auto arpEntry = resolveArp(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT);
  saiManagerTable->neighborManager().removeNeighbor(arpEntry);
  checkMissing(arpEntry);
}

TEST_F(NeighborManagerTest, changeResolvedNeighbor) {
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
  auto arpEntryNew = makeArpEntry(intf0.id, testInterfaces[1].remoteHosts[0]);
  saiManagerTable->neighborManager().changeNeighbor(arpEntry, arpEntryNew);
  checkEntry(arpEntryNew, testInterfaces[1].remoteHosts[0].mac);
}

TEST_F(NeighborManagerTest, changeResolvedPortRifNeighbor) {
  auto arpEntry = resolveArp(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT);
  auto arpEntryNew = makeArpEntry(
      intf0.id,
      testInterfaces[1].remoteHosts[0],
      std::nullopt,
      std::nullopt,
      true,
      cfg::InterfaceType::SYSTEM_PORT);
  saiManagerTable->neighborManager().changeNeighbor(arpEntry, arpEntryNew);
  checkEntry(
      arpEntryNew,
      testInterfaces[1].remoteHosts[0].mac,
      cfg::InterfaceType ::SYSTEM_PORT);
}

TEST_F(NeighborManagerTest, changeResolvedNeighborAddMetadata) {
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
  auto arpEntryNew =
      makeArpEntry(intf0.id, testInterfaces[1].remoteHosts[0], 42);
  saiManagerTable->neighborManager().changeNeighbor(arpEntry, arpEntryNew);
  checkEntry(
      arpEntryNew,
      testInterfaces[1].remoteHosts[0].mac,
      cfg::InterfaceType::VLAN,
      42);
}

TEST_F(NeighborManagerTest, changeResolvedPortRifNeighborAddMetadata) {
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
  auto arpEntryNew = makeArpEntry(
      intf0.id,
      testInterfaces[1].remoteHosts[0],
      42,
      std::nullopt,
      true,
      cfg::InterfaceType::SYSTEM_PORT);
  saiManagerTable->neighborManager().changeNeighbor(arpEntry, arpEntryNew);
  checkEntry(
      arpEntryNew,
      testInterfaces[1].remoteHosts[0].mac,
      cfg::InterfaceType::SYSTEM_PORT,
      42);
}

TEST_F(NeighborManagerTest, changeResolvedNeighborAddEncapIndex) {
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
  auto arpEntryNew = makeArpEntry(
      intf0.id, testInterfaces[1].remoteHosts[0], std::nullopt, 42);
  EXPECT_THROW(
      saiManagerTable->neighborManager().changeNeighbor(arpEntry, arpEntryNew),
      FbossError);
}

TEST_F(NeighborManagerTest, changeResolvedPortRifNeighborAddEncapIndex) {
  auto arpEntry = resolveArp(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT);
  auto arpEntryNew = makeArpEntry(
      intf0.id,
      testInterfaces[1].remoteHosts[0],
      std::nullopt,
      42,
      true,
      cfg::InterfaceType::SYSTEM_PORT);
  saiManagerTable->neighborManager().changeNeighbor(arpEntry, arpEntryNew);
  checkEntry(arpEntryNew, h0.mac, cfg::InterfaceType::SYSTEM_PORT, 0, 42);
}

TEST_F(NeighborManagerTest, changeResolvedNeighborAddEncapIndexRemote) {
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
  auto arpEntryNew = makeArpEntry(
      intf0.id, testInterfaces[1].remoteHosts[0], std::nullopt, 42, false);
  EXPECT_THROW(
      saiManagerTable->neighborManager().changeNeighbor(arpEntry, arpEntryNew),
      FbossError);
}

TEST_F(NeighborManagerTest, changeResolvedPortRifNeighborAddEncapIndexRemote) {
  auto arpEntry = resolveArp(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT);
  auto arpEntryNew = makeArpEntry(
      intf0.id,
      testInterfaces[1].remoteHosts[0],
      std::nullopt,
      42,
      false,
      cfg::InterfaceType::SYSTEM_PORT);
  saiManagerTable->neighborManager().changeNeighbor(arpEntry, arpEntryNew);
  checkEntry(
      arpEntryNew, h0.mac, cfg::InterfaceType::SYSTEM_PORT, 0, 42, false);
}

TEST_F(NeighborManagerTest, changeResolvedNeighborNoFieldChange) {
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
  auto arpEntryNew = makeArpEntry(intf0.id, h0);
  saiManagerTable->neighborManager().changeNeighbor(arpEntry, arpEntryNew);
  checkEntry(arpEntryNew, h0.mac);
}

TEST_F(NeighborManagerTest, changeResolvedPortRifNeighborNoFieldChange) {
  auto arpEntry = resolveArp(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT);
  auto arpEntryNew = makeArpEntry(
      intf0.id,
      h0,
      std::nullopt,
      std::nullopt,
      true,
      cfg::InterfaceType::SYSTEM_PORT);
  saiManagerTable->neighborManager().changeNeighbor(arpEntry, arpEntryNew);
  checkEntry(arpEntryNew, h0.mac, cfg::InterfaceType::SYSTEM_PORT);
}

TEST_F(NeighborManagerTest, addUnresolvedNeighbor) {
  auto pendingEntry = makePendingArpEntry(intf0.id, h0);
  EXPECT_TRUE(pendingEntry->isPending());
  saiManagerTable->neighborManager().addNeighbor(pendingEntry);
  // unresolved entries will not be found by getNeighbor
  checkMissing(pendingEntry);
}

TEST_F(NeighborManagerTest, addUnresolvedPortRifNeighbor) {
  auto pendingEntry =
      makePendingArpEntry(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  EXPECT_TRUE(pendingEntry->isPending());
  saiManagerTable->neighborManager().addNeighbor(pendingEntry);
  // unresolved entries will not be found by getNeighbor
  checkMissing(pendingEntry);
}

TEST_F(NeighborManagerTest, removeUnresolvedNeighbor) {
  auto pendingEntry = makePendingArpEntry(intf0.id, h0);
  EXPECT_TRUE(pendingEntry->isPending());
  saiManagerTable->neighborManager().addNeighbor(pendingEntry);
  checkMissing(pendingEntry);
  saiManagerTable->neighborManager().removeNeighbor(pendingEntry);
  checkMissing(pendingEntry);
}

TEST_F(NeighborManagerTest, removeUnresolvedPortRifNeighbor) {
  auto pendingEntry =
      makePendingArpEntry(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  EXPECT_TRUE(pendingEntry->isPending());
  saiManagerTable->neighborManager().addNeighbor(pendingEntry);
  // unresolved entries will not be found by getNeighbor
  checkMissing(pendingEntry);
  saiManagerTable->neighborManager().removeNeighbor(pendingEntry);
  checkMissing(pendingEntry);
}

TEST_F(NeighborManagerTest, resolveNeighbor) {
  auto pendingEntry = makePendingArpEntry(intf0.id, h0);
  EXPECT_TRUE(pendingEntry->isPending());
  saiManagerTable->neighborManager().addNeighbor(pendingEntry);
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
}

TEST_F(NeighborManagerTest, resolvePortRifNeighbor) {
  auto pendingEntry =
      makePendingArpEntry(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  EXPECT_TRUE(pendingEntry->isPending());
  saiManagerTable->neighborManager().addNeighbor(pendingEntry);
  auto arpEntry = resolveArp(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT);
}

TEST_F(NeighborManagerTest, unresolveNeighbor) {
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
  auto pendingEntry = makePendingArpEntry(intf0.id, h0);
  EXPECT_TRUE(pendingEntry->isPending());
  saiManagerTable->neighborManager().changeNeighbor(arpEntry, pendingEntry);
  checkMissing(pendingEntry);
}

TEST_F(NeighborManagerTest, unresolvePortRifNeighbor) {
  auto arpEntry = resolveArp(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  checkEntry(arpEntry, h0.mac, cfg::InterfaceType::SYSTEM_PORT);
  auto pendingEntry =
      makePendingArpEntry(intf0.id, h0, cfg::InterfaceType::SYSTEM_PORT);
  EXPECT_TRUE(pendingEntry->isPending());
  saiManagerTable->neighborManager().changeNeighbor(arpEntry, pendingEntry);
  checkMissing(pendingEntry);
}

TEST_F(NeighborManagerTest, getNonexistentNeighbor) {
  auto arpEntry = makeArpEntry(intf0.id, h0);
  checkMissing(arpEntry);
}

TEST_F(NeighborManagerTest, getNonexistentPortRifNeighbor) {
  auto arpEntry = makeArpEntry(
      intf0.id,
      h0,
      std::nullopt,
      std::nullopt,
      true,
      cfg::InterfaceType::SYSTEM_PORT);
  checkMissing(arpEntry);
}

TEST_F(NeighborManagerTest, removeNonexistentNeighbor) {
  auto arpEntry = makeArpEntry(intf0.id, h0);
  EXPECT_THROW(
      saiManagerTable->neighborManager().removeNeighbor(arpEntry), FbossError);
}

TEST_F(NeighborManagerTest, removeNonexistentPortRifNeighbor) {
  auto arpEntry = makeArpEntry(
      intf0.id,
      h0,
      std::nullopt,
      std::nullopt,
      true,
      cfg::InterfaceType::SYSTEM_PORT);
  EXPECT_THROW(
      saiManagerTable->neighborManager().removeNeighbor(arpEntry), FbossError);
}

TEST_F(NeighborManagerTest, addDuplicateResolvedNeighbor) {
  auto arpEntry = makeArpEntry(intf0.id, h0);
  saiManagerTable->neighborManager().addNeighbor(arpEntry);
  EXPECT_THROW(
      saiManagerTable->neighborManager().addNeighbor(arpEntry->clone()),
      FbossError);
}

TEST_F(NeighborManagerTest, addDuplicateResolvedPortRifNeighbor) {
  auto arpEntry = makeArpEntry(
      intf0.id,
      h0,
      std::nullopt,
      std::nullopt,
      true,
      cfg::InterfaceType::SYSTEM_PORT);
  saiManagerTable->neighborManager().addNeighbor(arpEntry);
  EXPECT_THROW(
      saiManagerTable->neighborManager().addNeighbor(arpEntry->clone()),
      FbossError);
}

TEST_F(NeighborManagerTest, linkDown) {
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
  saiManagerTable->fdbManager().handleLinkDown(
      SaiPortDescriptor(PortID(h0.port.id)));
  checkUnresolved(arpEntry);
}

TEST_F(NeighborManagerTest, linkDownReResolve) {
  saiManagerTable->bridgeManager().setL2LearningMode(
      cfg::L2LearningMode::SOFTWARE);
  auto arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
  saiManagerTable->fdbManager().handleLinkDown(
      SaiPortDescriptor(PortID(h0.port.id)));
  checkUnresolved(arpEntry);
  saiManagerTable->neighborManager().removeNeighbor(arpEntry);
  // mac table entry for neigbor entry gets kicked out when neighbor is deleted.
  saiManagerTable->fdbManager().removeFdbEntry(
      arpEntry->getIntfID(), arpEntry->getMac());
  arpEntry = resolveArp(intf0.id, h0);
  checkEntry(arpEntry, h0.mac);
}
