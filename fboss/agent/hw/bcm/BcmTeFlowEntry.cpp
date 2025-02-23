/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/hw/bcm/BcmTeFlowEntry.h"
#include "fboss/agent/hw/bcm/BcmError.h"
#include "fboss/agent/hw/bcm/BcmExactMatchUtils.h"
#include "fboss/agent/hw/bcm/BcmFieldProcessorUtils.h"
#include "fboss/agent/hw/bcm/BcmMultiPathNextHop.h"
#include "fboss/agent/hw/bcm/BcmPlatform.h"
#include "fboss/agent/hw/bcm/BcmSwitch.h"
#include "fboss/agent/hw/bcm/BcmTeFlowTable.h"
#include "fboss/agent/hw/bcm/BcmWarmBootCache.h"

#include "fboss/agent/AddressUtil.h"
#include "fboss/agent/state/TeFlowEntry.h"

extern "C" {
#include <bcm/field.h>
}

namespace facebook::fboss {

using facebook::network::toIPAddress;
using namespace facebook::fboss::utility;

void BcmTeFlowEntry::setRedirectNexthop(
    const BcmSwitch* hw,
    std::shared_ptr<BcmMultiPathNextHop>& redirectNexthop,
    const std::shared_ptr<TeFlowEntry>& teFlow) {
  auto nhts = teFlow->getResolvedNextHops();
  RouteNextHopSet nexthops = util::toRouteNextHopSet(nhts, true);
  if (nexthops.size() > 0) {
    redirectNexthop =
        hw->writableMultiPathNextHopTable()->referenceOrEmplaceNextHop(
            BcmMultiPathNextHopKey(0, nexthops));
  }
}

bcm_if_t BcmTeFlowEntry::getEgressId(
    const BcmSwitch* hw,
    std::shared_ptr<BcmMultiPathNextHop>& redirectNexthop) {
  bcm_if_t egressId;
  if (redirectNexthop) {
    egressId = redirectNexthop->getEgressId();
  } else {
    egressId = hw->getDropEgressId();
  }

  return egressId;
}

void BcmTeFlowEntry::createTeFlowQualifiers() {
  int rv;
  if (teFlow_->getFlow().srcPort().has_value()) {
    auto srcPort = teFlow_->getFlow().srcPort().value();
    rv = bcm_field_qualify_SrcPort(
        hw_->getUnit(), handle_, 0, 0xff, srcPort, 0xff);
    bcmCheckError(rv, "failed to qualify src port");
  }

  if (teFlow_->getFlow().dstPrefix().has_value()) {
    bcm_ip6_t addr;
    bcm_ip6_t mask{};
    auto prefix = teFlow_->getFlow().dstPrefix().value();
    folly::IPAddress ipaddr = toIPAddress(*prefix.ip());
    // Bcm SDK API for EM takes only full mask.Hence pfxLen is 128.
    uint8_t pfxLen = 128;
    if (!ipaddr.isV6()) {
      throw FbossError("only ipv6 addresses are supported for teflow");
    }
    facebook::fboss::ipToBcmIp6(ipaddr, &addr);
    memcpy(&mask, folly::IPAddressV6::fetchMask(pfxLen).data(), sizeof(mask));
    rv = bcm_field_qualify_DstIp6(hw_->getUnit(), handle_, addr, mask);
    bcmCheckError(rv, "failed to qualify dst Ip6");
  }
}

void BcmTeFlowEntry::createTeFlowStat() {
  auto teFlowTable = hw_->writableTeFlowTable();
  auto counterName = teFlow_->getCounterID().value();
  auto teFlowStat = teFlowTable->incRefOrCreateBcmTeFlowStat(counterName, gid_);
  teFlowStat->attach(handle_);
}

void BcmTeFlowEntry::deleteTeFlowStat() {
  if (teFlow_->getCounterID().has_value() && teFlow_->getEnabled()) {
    auto counterName = teFlow_->getCounterID().value();
    auto teFlowTable = hw_->writableTeFlowTable();
    auto teFlowStat = teFlowTable->getTeFlowStat(counterName);
    teFlowStat->detach(handle_);
    teFlowTable->derefBcmTeFlowStat(counterName);
  }
}

void BcmTeFlowEntry::createTeFlowActions() {
  setRedirectNexthop(hw_, redirectNexthop_, teFlow_);
  auto egressId = getEgressId(hw_, redirectNexthop_);
  XLOG(DBG3) << "Setting egress Id:" << egressId << " for handle :" << handle_;
  auto rv = bcm_field_action_add(
      hw_->getUnit(), handle_, bcmFieldActionL3Switch, egressId, 0);
  bcmCheckError(rv, "failed to action add");
}

void BcmTeFlowEntry::removeTeFlowActions() {
  auto rv =
      bcm_field_action_remove(hw_->getUnit(), handle_, bcmFieldActionL3Switch);
  bcmCheckError(rv, "failed to action remove ");
}

void BcmTeFlowEntry::installTeFlowEntry() {
  auto rv = bcm_field_entry_install(hw_->getUnit(), handle_);
  bcmCheckError(rv, "failed to install field group");
}

void BcmTeFlowEntry::createTeFlowEntry() {
  /* EM Entry Configuration and Creation */
  auto rv =
      bcm_field_entry_create(hw_->getUnit(), getEMGroupID(gid_), &handle_);
  bcmCheckError(rv, "failed to create field entry");

  createTeFlowQualifiers();
  int enabled = teFlow_->getEnabled() ? 1 : 0;
  // enable/disable per entry is not supported in EM. Hence we are relying on
  // actions.
  if (enabled) {
    createTeFlowActions();
    if (teFlow_->getCounterID().has_value()) {
      createTeFlowStat();
    }
  }
  installTeFlowEntry();
}

void BcmTeFlowEntry::updateTeFlowEntry(
    const std::shared_ptr<TeFlowEntry>& newTeFlow) {
  // Entry disabled
  if (!teFlow_->getEnabled() && !newTeFlow->getEnabled()) {
    return;
  }

  // disabled to enabled
  if (!teFlow_->getEnabled() && newTeFlow->getEnabled()) {
    teFlow_ = newTeFlow;
    createTeFlowActions();
    if (teFlow_->getCounterID().has_value()) {
      createTeFlowStat();
    }
    installTeFlowEntry();
    return;
  }

  // enabled to disabled
  if (teFlow_->getEnabled() && !newTeFlow->getEnabled()) {
    removeTeFlowActions();
    redirectNexthop_ = nullptr;
    if (teFlow_->getCounterID().has_value()) {
      deleteTeFlowStat();
    }
    teFlow_ = newTeFlow;
    installTeFlowEntry();
    return;
  }

  // enabled to enabled
  if (teFlow_->getCounterID().has_value()) {
    deleteTeFlowStat();
  }
  removeTeFlowActions();
  teFlow_ = newTeFlow;
  createTeFlowActions();
  if (teFlow_->getCounterID().has_value()) {
    createTeFlowStat();
  }
  installTeFlowEntry();
}

BcmTeFlowEntry::BcmTeFlowEntry(
    BcmSwitch* hw,
    int gid,
    const std::shared_ptr<TeFlowEntry>& teFlow)
    : hw_(hw), gid_(gid), teFlow_(teFlow) {
  createTeFlowEntry();
}

BcmTeFlowEntry::~BcmTeFlowEntry() {
  // Delete the TeFlow stat
  deleteTeFlowStat();

  // Destroy the TeFlow entry
  auto rv = bcm_field_entry_destroy(hw_->getUnit(), handle_);
  bcmLogFatal(rv, hw_, "failed to destroy the TeFlow entry");
}

TeFlow BcmTeFlowEntry::getID() const {
  return teFlow_->getID();
}

bool BcmTeFlowEntry::isStateSame(
    const BcmSwitch* hw,
    int gid,
    BcmTeFlowEntryHandle handle,
    const std::shared_ptr<TeFlowEntry>& teFlow) {
  auto teFlowMsg = folly::to<std::string>(
      "Group=", gid, ", teflow=", teFlow->str(), ", handle=", handle);
  bool isSame = true;

  XLOG(DBG3) << "Verify the state of Teflow entry: " << teFlowMsg;
  // check src port
  if (teFlow->getFlow().srcPort().has_value()) {
    auto srcPort = teFlow->getFlow().srcPort().value();
    isSame &= isBcmPortQualFieldStateSame(
        hw->getUnit(),
        handle,
        teFlowMsg,
        "SrcPort",
        bcm_field_qualify_SrcPort_get,
        srcPort,
        false);
  }

  // check dst Ip
  if (teFlow->getFlow().dstPrefix().has_value()) {
    bcm_ip6_t addr;
    bcm_ip6_t mask{};
    auto prefix = teFlow->getFlow().dstPrefix().value();
    folly::IPAddress ipaddr = toIPAddress(*prefix.ip());
    if (!ipaddr.isV6()) {
      throw FbossError("only ipv6 addresses are supported for teflow");
    }
    facebook::fboss::ipToBcmIp6(ipaddr, &addr);
    isSame &= isBcmQualFieldStateSame(
        bcm_field_qualify_DstIp6_get,
        hw->getUnit(),
        handle,
        true,
        addr,
        mask,
        teFlowMsg,
        "Dst IP",
        false);
  }

  int enabled = teFlow->getEnabled() ? 1 : 0;
  // check egressId
  uint32_t egressId = 0, param1 = 0;
  auto rv = bcm_field_action_get(
      hw->getUnit(), handle, bcmFieldActionL3Switch, &egressId, &param1);
  if (rv == BCM_E_NOT_FOUND) {
    if (!enabled) {
      return isSame;
    }
    bcmCheckError(
        rv, teFlowMsg, " failed to get action=", bcmFieldActionL3Switch);
    return false;
  }

  std::shared_ptr<BcmMultiPathNextHop> redirectNexthop;
  setRedirectNexthop(hw, redirectNexthop, teFlow);
  auto expectedEgressId = getEgressId(hw, redirectNexthop);
  if (egressId != static_cast<uint32_t>(expectedEgressId)) {
    XLOG(ERR) << teFlowMsg
              << " redirect nexthop egressId mismatched. SW expected="
              << expectedEgressId << " HW value=" << egressId;
    return false;
  }

  // check flow stat
  auto teFlowStatHandle = BcmTeFlowStat::getAclStatHandleFromAttachedAcl(
      hw, getEMGroupID(gid), handle);
  if (teFlowStatHandle && teFlow->getCounterID().has_value()) {
    cfg::TrafficCounter counter;
    counter.name() = teFlow->getCounterID().value();
    counter.types() = {cfg::CounterType::BYTES};
    isSame &=
        ((BcmTeFlowStat::isStateSame(hw, *teFlowStatHandle, counter)) ? 1 : 0);
  } else {
    isSame &= ((!teFlowStatHandle.has_value()) ? 1 : 0);
  }

  return isSame;
}

} // namespace facebook::fboss
