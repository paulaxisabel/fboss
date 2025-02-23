/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include "common/stats/MonotonicCounter.h"
#include "fboss/agent/hw/bcm/BcmRouteCounter.h"
#include "fboss/agent/hw/bcm/BcmTableStats.h"
#include "fboss/agent/hw/bcm/types.h"
#include "fboss/agent/hw/common/LanePrbsStatsEntry.h"
#include "fboss/agent/types.h"

#include <boost/container/flat_map.hpp>
#include <folly/Synchronized.h>
#include <queue>

extern "C" {
#include <bcm/port.h>
#include <bcm/types.h>
}

namespace facebook::fboss {

using facebook::stats::MonotonicCounter;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

class BcmSwitch;
class StateDelta;

class BcmStatUpdater {
 public:
  using LanePrbsStatsTable = std::vector<LanePrbsStatsEntry>;

  explicit BcmStatUpdater(BcmSwitch* hw);
  ~BcmStatUpdater() {}

  /* Thread safety:
   *  Accessing Bcm* data structures from stats and update thread is racy:
   *  One thread is iterating through the objects while the other is adding or
   *  removing objects:
   *    - The stats collection thread (read-only) is being called periodically.
   *    - The update thread would only add/remove objects on config change,
   *      which does not happen very often.
   *
   *  The idea here is to accumulate the changes in separate data structures
   *  (toBe* functions) and apply them all at once in the 'refresh()' function.
   *  Note that this class does not store Bcm* objects, but rather cache
   *  the ids needed to collect the stats.
   *
   *  'refresh()' and 'updateStats()' are using folly::Synchronize to ensure
   *  thread safety.
   */

  MonotonicCounter* getAclStatCounterIf(
      BcmAclStatHandle handle,
      cfg::CounterType counterType);
  size_t getAclStatCounterCount() const;

  std::vector<cfg::CounterType> getAclStatCounterType(
      BcmAclStatHandle handle) const;

  /* Functions to be called during state update */
  void toBeAddedAclStat(
      BcmAclStatHandle handle,
      const std::string& aclStatName,
      const std::vector<cfg::CounterType>& counterTypes);
  void toBeRemovedAclStat(BcmAclStatHandle handle);

  void toBeAddedTeFlowStat(
      BcmAclStatHandle handle,
      const std::string& aclStatName,
      const std::vector<cfg::CounterType>& counterTypes) {
    return toBeAddedAclStat(handle, aclStatName, counterTypes);
  }
  void toBeRemovedTeFlowStat(BcmAclStatHandle handle) {
    return toBeRemovedAclStat(handle);
  }

  void refreshPostBcmStateChange(const StateDelta& delta);

  /* Functions to be called during stats collection (UpdateStatsThread) */
  void updateStats();

  void clearPortStats(const std::unique_ptr<std::vector<int32_t>>& ports);

  std::vector<phy::PrbsLaneStats> getPortAsicPrbsStats(int32_t portId);
  void clearPortAsicPrbsStats(int32_t portId);

  void toBeAddedRouteCounter(
      BcmRouteCounterID id,
      const std::string& routeStatName);
  void toBeRemovedRouteCounter(BcmRouteCounterID id);

  HwResourceStats getHwTableStats() {
    return *resourceStats_.rlock();
  }

 private:
  void updateAclStats();
  BcmTrafficCounterStats getAclTrafficStats(
      BcmAclStatHandle handle,
      const std::vector<cfg::CounterType>& counters);

  void updateHwTableStats();
  void updatePrbsStats();
  void refreshHwTableStats(const StateDelta& delta);
  void refreshAclStats();
  void refreshPrbsStats(const StateDelta& delta);

  double calculateLaneRate(std::shared_ptr<Port> swPort);

  void updateRouteCounters();
  void refreshRouteCounters();

  BcmSwitch* hw_;
  std::unique_ptr<BcmHwTableStatManager> bcmTableStatsManager_;

  folly::Synchronized<HwResourceStats> resourceStats_;

  /* ACL stats */
  struct BcmAclStatDescriptor {
    BcmAclStatDescriptor(
        BcmAclStatHandle hdl,
        const std::string& aclStatName,
        bool add)
        : handle(hdl), aclStatName(aclStatName), addStat(add) {}
    bool operator<(const BcmAclStatDescriptor& d) const {
      return std::tie(handle, aclStatName, addStat) <
          std::tie(d.handle, d.aclStatName, d.addStat);
    }
    BcmAclStatHandle handle;
    std::string aclStatName;
    bool addStat;
  };

  std::queue<std::pair<BcmAclStatDescriptor, cfg::CounterType>>
      toBeUpdatedAclStats_;
  // Outer-map key: BcmAclStatHandle
  // Inner-map key: counter type, value: MonotonicCounter
  // Usually one BcmAclStatHandle can get both packets and bytes counters back
  // at one function call.
  folly::Synchronized<std::unordered_map<
      BcmAclStatHandle,
      std::unordered_map<cfg::CounterType, std::unique_ptr<MonotonicCounter>>>>
      aclStats_;

  folly::Synchronized<std::map<int32_t, LanePrbsStatsTable>> portAsicPrbsStats_;

  /* Route stats */
  uint64_t getRouteTrafficStats(BcmRouteCounterID id);
  uint64_t getBcmRouteTrafficStats(BcmRouteCounterID id);
  uint64_t getBcmFlexRouteTrafficStats(BcmRouteCounterID id);
  struct BcmRouteCounterActionDescriptor {
    BcmRouteCounterActionDescriptor(
        BcmRouteCounterID ID,
        const std::string& routeStatName,
        bool addCounterID)
        : id(ID), routeStatName(routeStatName), addCounter(addCounterID) {}
    BcmRouteCounterID id;
    std::string routeStatName;
    bool addCounter;
  };
  std::queue<BcmRouteCounterActionDescriptor> toBeProcessedRouteCounters_;
  folly::Synchronized<
      std::map<BcmRouteCounterID, std::unique_ptr<MonotonicCounter>>>
      routeStats_;
}; // namespace facebook::fboss

} // namespace facebook::fboss
