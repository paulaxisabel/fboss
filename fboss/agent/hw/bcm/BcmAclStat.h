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

#include "fboss/agent/FbossError.h"
#include "fboss/agent/hw/bcm/types.h"

namespace facebook::fboss {

class BcmSwitch;
class BcmSwitchIf;
class BcmIngressFieldProcessorFlexCounter;

/* Identifies whether a stat object is associated with tcam or
 * exact match entry
 */
enum class BcmAclStatType { IFP, EM };

/**
 *  BcmAclStat is the class to abstract a stat's resource and functions
 */
class BcmAclStat {
 public:
  BcmAclStat(
      BcmSwitch* hw,
      int gid,
      const std::vector<cfg::CounterType>& counters,
      BcmAclStatType type = BcmAclStatType::IFP);
  BcmAclStat(
      BcmSwitch* hw,
      BcmAclStatHandle statHandle,
      BcmAclStatType type = BcmAclStatType::IFP);
  ~BcmAclStat();

  BcmAclStatHandle getHandle() const {
    return handle_;
  }

  void attach(BcmAclEntryHandle acl);

  void detach(BcmAclEntryHandle acl);

  static void detach(
      const BcmSwitchIf* hw,
      BcmAclEntryHandle acl,
      BcmAclStatHandle aclStatHandle);

  static void destroy(const BcmSwitchIf* hw, BcmAclStatHandle aclStatHandle);

  static int getNumAclStatsInFpGroup(const BcmSwitch* hw, int gid);

  static std::optional<BcmAclStatHandle> getAclStatHandleFromAttachedAcl(
      const BcmSwitchIf* hw,
      int groupID,
      BcmAclEntryHandle acl);

  /**
   * Check whether the acl details of handle in h/w matches the s/w acl and
   * ranges
   */
  static bool isStateSame(
      const BcmSwitch* hw,
      BcmAclStatHandle statHandle,
      cfg::TrafficCounter& counter);

 private:
  BcmSwitch* hw_;
  BcmAclStatHandle handle_;
  std::unique_ptr<BcmIngressFieldProcessorFlexCounter> flexCounter_;
  BcmAclStatType statType_;
};

} // namespace facebook::fboss
