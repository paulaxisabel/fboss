#
# Copyright 2004-present Facebook. All Rights Reserved.
#
namespace cpp2 facebook.fboss.cfg
namespace go neteng.fboss.qsfp_service_config
namespace py neteng.fboss.qsfp_service_config
namespace py3 neteng.fboss
namespace py.asyncio neteng.fboss.asyncio.qsfp_service_config

include "fboss/qsfp_service/if/transceiver.thrift"

struct QsfpSdkVersion {
  // A vendor for the particular version requested
  1: string vendor;
  // The version associated with the desired Sdk
  2: string version;
}

enum TransceiverPartNumber {
  UNKNOWN = 1,
}

// The matching logic will treat the absent of an optional field as match all.
struct TransceiverConfigOverrideFactor {
  1: optional TransceiverPartNumber transceiverPartNumber;
  2: optional transceiver.SMFMediaInterfaceCode applicationCode;
}

struct Sff8636Overrides {
  1: optional i16 rxPreemphasis;
  2: optional i16 rxAmplitude;
  3: optional i16 txEqualization;
}

struct CmisOverrides {
  1: optional transceiver.RxEqualizerSettings rxEqualizerSettings;
}

union TransceiverOverrides {
  1: Sff8636Overrides sff;
  2: CmisOverrides cmis;
}

struct TransceiverConfigOverride {
  1: TransceiverConfigOverrideFactor factor;
  2: TransceiverOverrides config;
}

struct QsfpServiceConfig {
  // This is used to override the default command line arguments we
  // pass to qsfp service.
  1: map<string, string> defaultCommandLineArgs = {};

  // This has a list of overrides of settings on the optic together with the
  // factor that specify the condition to apply.
  2: list<TransceiverConfigOverride> transceiverConfigOverrides = [];

  // Sdk versions for QSFP service
  3: optional QsfpSdkVersion sdk_version;
}
