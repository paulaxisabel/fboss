// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "fboss/fsdb/client/FsdbPubSubManager.h"
#include "fboss/fsdb/common/Flags.h"
#include "fboss/lib/CommonUtils.h"

#include <folly/experimental/coro/AsyncGenerator.h>
#include <folly/experimental/coro/AsyncPipe.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/logging/xlog.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>

namespace facebook::fboss::fsdb::test {
void stateChangeCb(
    FsdbStreamClient::State /*old*/,
    FsdbStreamClient::State /*new*/) {}
void operDeltaCb(OperDelta /*delta*/) {}
void operStateCb(OperState /*state*/) {}

class PubSubManagerTest : public ::testing::Test {
 protected:
  void addStateDeltaSubscription(
      const std::vector<std::string>& path,
      const std::string& host = "::1") {
    pubSubManager_.addStateDeltaSubscription(
        path, stateChangeCb, operDeltaCb, host);
  }
  void addStatDeltaSubscription(
      const std::vector<std::string>& path,
      const std::string& host = "::1") {
    pubSubManager_.addStatDeltaSubscription(
        path, stateChangeCb, operDeltaCb, host);
  }
  void addStatePathSubscription(
      const std::vector<std::string>& path,
      const std::string& host = "::1") {
    pubSubManager_.addStatePathSubscription(
        path, stateChangeCb, operStateCb, host);
  }
  void addStatPathSubscription(
      const std::vector<std::string>& path,
      const std::string& host = "::1") {
    pubSubManager_.addStatPathSubscription(
        path, stateChangeCb, operStateCb, host);
  }
  FsdbPubSubManager pubSubManager_{"testMgr"};
};

TEST_F(PubSubManagerTest, createStateAndStatDeltaPublisher) {
  pubSubManager_.createStateDeltaPublisher({}, stateChangeCb);
  EXPECT_THROW(
      pubSubManager_.createStateDeltaPublisher({}, stateChangeCb),
      std::runtime_error);
  EXPECT_THROW(
      pubSubManager_.createStatePathPublisher({}, stateChangeCb),
      std::runtime_error);
  pubSubManager_.createStatDeltaPublisher({}, stateChangeCb);
  EXPECT_THROW(
      pubSubManager_.createStatDeltaPublisher({}, stateChangeCb),
      std::runtime_error);
  EXPECT_THROW(
      pubSubManager_.createStatPathPublisher({}, stateChangeCb),
      std::runtime_error);
}

TEST_F(PubSubManagerTest, createStateAndStatPathPublisher) {
  pubSubManager_.createStatePathPublisher({}, stateChangeCb);
  EXPECT_THROW(
      pubSubManager_.createStatePathPublisher({}, stateChangeCb),
      std::runtime_error);
  EXPECT_THROW(
      pubSubManager_.createStateDeltaPublisher({}, stateChangeCb),
      std::runtime_error);
  pubSubManager_.createStatPathPublisher({}, stateChangeCb);
  EXPECT_THROW(
      pubSubManager_.createStatPathPublisher({}, stateChangeCb),
      std::runtime_error);
  EXPECT_THROW(
      pubSubManager_.createStatDeltaPublisher({}, stateChangeCb),
      std::runtime_error);
}

TEST_F(PubSubManagerTest, publishStateAndStatDelta) {
  pubSubManager_.createStateDeltaPublisher({}, stateChangeCb);
  pubSubManager_.publishState(OperDelta{});
  EXPECT_THROW(pubSubManager_.publishState(OperState{}), std::runtime_error);
  pubSubManager_.createStatDeltaPublisher({}, stateChangeCb);
  pubSubManager_.publishStat(OperDelta{});
  EXPECT_THROW(pubSubManager_.publishStat(OperState{}), std::runtime_error);
}

TEST_F(PubSubManagerTest, publishPathStatState) {
  pubSubManager_.createStatePathPublisher({}, stateChangeCb);
  pubSubManager_.publishState(OperState{});
  EXPECT_THROW(pubSubManager_.publishState(OperDelta{}), std::runtime_error);
  pubSubManager_.createStatPathPublisher({}, stateChangeCb);
  pubSubManager_.publishStat(OperState{});
  EXPECT_THROW(pubSubManager_.publishStat(OperDelta{}), std::runtime_error);
}

TEST_F(PubSubManagerTest, addRemoveSubscriptions) {
  addStateDeltaSubscription({});
  addStateDeltaSubscription({"foo"});
  // multiple delta subscriptions to same host, path
  EXPECT_THROW(addStateDeltaSubscription({}), std::runtime_error);
  // Stat delta subscriptions to same path as state delta ok
  addStatDeltaSubscription({"foo"});
  // Dup stat delta subscription should throw
  EXPECT_THROW(addStatDeltaSubscription({"foo"}), std::runtime_error);
  EXPECT_EQ(pubSubManager_.numSubscriptions(), 3);
  pubSubManager_.removeStateDeltaSubscription({});
  EXPECT_EQ(pubSubManager_.numSubscriptions(), 2);
  addStateDeltaSubscription({});
  // Same subscripton path, different host
  addStateDeltaSubscription({"foo"}, "::2");
  EXPECT_EQ(pubSubManager_.numSubscriptions(), 4);
  // remove non existent state subscription. No effect
  pubSubManager_.removeStatePathSubscription({});
  EXPECT_EQ(pubSubManager_.numSubscriptions(), 4);

  // Add state, stat subscription for same path, host as delta
  addStatePathSubscription({});
  addStatPathSubscription({});
  EXPECT_EQ(pubSubManager_.numSubscriptions(), 6);
  // multiple state, stat subscriptions to same host, path
  EXPECT_THROW(addStatePathSubscription({}), std::runtime_error);
  EXPECT_THROW(addStatPathSubscription({}), std::runtime_error);
  // Same subscription path, different host
  addStatePathSubscription({}, "::2");
  addStatPathSubscription({}, "::2");
  EXPECT_EQ(pubSubManager_.numSubscriptions(), 8);
}

} // namespace facebook::fboss::fsdb::test
