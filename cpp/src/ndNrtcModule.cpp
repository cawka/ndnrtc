//
//  ndNrtcModule.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 7/29/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "ndNrtc.h"

// For details, check http://mxr.mozilla.org/mozilla-central/source/xpcom/sample/nsSampleModule.cpp

NS_GENERIC_FACTORY_CONSTRUCTOR(ndNrtc)

// The following line defines a kNS_SAMPLE_CID CID variable.
NS_DEFINE_NAMED_CID(NRTC_CID);

// Build a table of ClassIDs (CIDs) which are implemented by this module. CIDs
// should be completely unique UUIDs.
// each entry has the form { CID, service, factoryproc, constructorproc }
// where factoryproc is usually NULL.
static const mozilla::Module::CIDEntry kNrtcCIDs[] = {
    { &kNRTC_CID, false, NULL, ndNrtcConstructor},
    { NULL }
};

// Build a table which maps contract IDs to CIDs.
// A contract is a string which identifies a particular set of functionality. In some
// cases an extension component may override the contract ID of a builtin gecko component
// to modify or extend functionality.
static const mozilla::Module::ContractIDEntry kNrtcContracts[] = {
    { NRTC_CONTRACTID, &kNRTC_CID },
    { NULL }
};

// Category entries are category/key/value triples which can be used
// to register contract ID as content handlers or to observe certain
// notifications. Most modules do not need to register any category
// entries: this is just a sample of how you'd do it.
// @see nsICategoryManager for information on retrieving category data.
static const mozilla::Module::CategoryEntry kNrtcCategories[] = {
    { NULL }
};

static const mozilla::Module kNrtcModule = {
    mozilla::Module::kVersion,
    kNrtcCIDs,
    kNrtcContracts,
    kNrtcCategories
};

// The following line implements the one-and-only "NSModule" symbol exported from this
// shared library.
NSMODULE_DEFN(ndNrtcModule) = &kNrtcModule;

// The following line implements the one-and-only "NSGetModule" symbol
// for compatibility with mozilla 1.9.2. You should only use this
// if you need a binary which is backwards-compatible and if you use
// interfaces carefully across multiple versions.
NS_IMPL_MOZILLA192_NSGETMODULE(&kNrtcModule)
