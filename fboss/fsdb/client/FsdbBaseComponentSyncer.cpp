// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include "fboss/fsdb/client/FsdbBaseComponentSyncer.h"
#include "fboss/fsdb/client/FsdbSyncManager.h"

namespace facebook::fboss::fsdb {

void FsdbBaseDeltaComponentSyncer::publishDelta(OperDelta&& data) {
  readyForPublishing_.withRLock([&](bool ready) {
    if (!ready) {
      return;
    }
    if (isStats()) {
      pubSubManager()->publishStat(std::move(data));
    } else {
      pubSubManager()->publishState(std::move(data));
    }
  });
}

void FsdbBasePathComponentSyncer::publishPath(OperState&& data) {
  readyForPublishing_.withRLock([&](bool ready) {
    if (!ready) {
      return;
    }
    if (isStats()) {
      pubSubManager()->publishStat(std::move(data));
    } else {
      pubSubManager()->publishState(std::move(data));
    }
  });
}

void FsdbStatsComponentSyncer::publisherStateChanged(
    FsdbStreamClient::State oldState,
    FsdbStreamClient::State newState) {
  CHECK(oldState != newState);
  if (newState == FsdbStreamClient::State::CONNECTED) {
    start();
  } else if (newState != FsdbStreamClient::State::CONNECTED) {
    stop();
  }
}
} // namespace facebook::fboss::fsdb
