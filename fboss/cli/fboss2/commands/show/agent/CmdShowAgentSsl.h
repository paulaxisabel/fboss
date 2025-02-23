// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include <fboss/cli/fboss2/utils/CmdUtils.h>
#include "fboss/cli/fboss2/CmdHandler.h"

#include <re2/re2.h>
#include "fboss/cli/fboss2/commands/show/agent/gen-cpp2/model_types.h"
#include "fboss/cli/fboss2/commands/show/agent/gen-cpp2/model_visitation.h"

namespace facebook::fboss {

struct CmdShowAgentSslTraits : public BaseCommandTraits {
  using ObjectArgType = utils::NoneArgType;
  using RetType = cli::ShowAgentSslModel;
};

class CmdShowAgentSsl
    : public CmdHandler<CmdShowAgentSsl, CmdShowAgentSslTraits> {
 public:
  RetType queryClient(const HostInfo& hostInfo) {
    auto client = utils::createAgentClient(hostInfo);
    SSLType sslType = client->sync_getSSLPolicy();

    RetType model{};
    model.AgentSslStatus() = sslTypeToString(sslType);
    return model;
  }

  void printOutput(const RetType& model, std::ostream& out = std::cout) {
    out << "Secure Thrift server SSL config: " << model.get_AgentSslStatus()
        << std::endl;
  }

 private:
  static std::string sslTypeToString(const facebook::fboss::SSLType& sslType) {
    switch (sslType) {
      case SSLType::DISABLED:
        return "DISABLED (allow plaintext clients only)";
      case SSLType::PERMITTED:
        return "PERMITTED (allow plaintext & encrypted clients)";
      case SSLType::REQUIRED:
        return "REQUIRED (allow encrypted clients only)";
      default:
        return "UNKNOWN - unexpected value of SSLType";
    }
  }
};

} // namespace facebook::fboss
