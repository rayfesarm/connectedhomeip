/**
 *
 *    Copyright (c) 2021-2022 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <memory>

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import "MTRError_Internal.h"
#import "MTRKeypair.h"
#import "MTRP256KeypairBridge.h"
#import "MTRPersistentStorageDelegateBridge.h"

#include <controller/OperationalCredentialsDelegate.h>
#include <crypto/CHIPCryptoPAL.h>
#include <lib/core/CASEAuthTag.h>

NS_ASSUME_NONNULL_BEGIN

class MTROperationalCredentialsDelegate : public chip::Controller::OperationalCredentialsDelegate {
public:
    using ChipP256KeypairPtr = chip::Crypto::P256Keypair *;

    ~MTROperationalCredentialsDelegate() {}

    CHIP_ERROR Init(MTRPersistentStorageDelegateBridge * storage, ChipP256KeypairPtr nocSigner, NSData * ipk, NSData * rootCert,
        NSData * _Nullable icaCert);

    CHIP_ERROR GenerateNOCChain(const chip::ByteSpan & csrElements, const chip::ByteSpan & csrNonce,
        const chip::ByteSpan & attestationSignature, const chip::ByteSpan & attestationChallenge, const chip::ByteSpan & DAC,
        const chip::ByteSpan & PAI, chip::Callback::Callback<chip::Controller::OnNOCChainGeneration> * onCompletion) override;

    void SetNodeIdForNextNOCRequest(chip::NodeId nodeId) override
    {
        mNextRequestedNodeId = nodeId;
        mNodeIdRequested = true;
    }

    void SetFabricIdForNextNOCRequest(chip::FabricId fabricId) override { mNextFabricId = fabricId; }

    void SetDeviceID(chip::NodeId deviceId) { mDeviceBeingPaired = deviceId; }
    void ResetDeviceID() { mDeviceBeingPaired = chip::kUndefinedNodeId; }

    CHIP_ERROR GenerateNOC(chip::NodeId nodeId, chip::FabricId fabricId, const chip::CATValues & cats,
        const chip::Crypto::P256PublicKey & pubkey, chip::MutableByteSpan & noc);

    const chip::Crypto::AesCcm128KeySpan GetIPK() { return mIPK.Span(); }

    // Get the root/intermediate X.509 DER certs as a ByteSpan.
    chip::ByteSpan RootCertSpan() const;
    chip::ByteSpan IntermediateCertSpan() const;

    // Generate a root (self-signed) DER-encoded X.509 certificate for the given
    // MTRKeypair.  If issuerId is provided, it is used; otherwise a random one
    // is generated.  If a fabric id is provided it is added to the subject DN
    // of the certificate.
    //
    // The outparam must not be null and is set to nil on errors.
    static CHIP_ERROR GenerateRootCertificate(id<MTRKeypair> keypair, NSNumber * _Nullable issuerId, NSNumber * _Nullable fabricId,
        NSData * _Nullable __autoreleasing * _Nonnull rootCert);

    // Generate an intermediate DER-encoded X.509 certificate for the given root
    // and intermediate public key.  If issuerId is provided, it is used;
    // otherwise a random one is generated.  If a fabric id is provided it is
    // added to the subject DN of the certificate.
    //
    // The outparam must not be null and is set to nil on errors.
    static CHIP_ERROR GenerateIntermediateCertificate(id<MTRKeypair> rootKeypair, NSData * rootCertificate,
        SecKeyRef intermediatePublicKey, NSNumber * _Nullable issuerId, NSNumber * _Nullable fabricId,
        NSData * _Nullable __autoreleasing * _Nonnull intermediateCert);

    // Generate an operational DER-encoded X.509 certificate for the given
    // signing certificate and operational public key, using the given fabric
    // id, node id, and CATs.
    static CHIP_ERROR GenerateOperationalCertificate(id<MTRKeypair> signingKeypair, NSData * signingCertificate,
        SecKeyRef operationalPublicKey, NSNumber * fabricId, NSNumber * nodeId,
        NSArray<NSNumber *> * _Nullable caseAuthenticatedTags, NSData * _Nullable __autoreleasing * _Nonnull operationalCert);

private:
    static bool ToChipEpochTime(uint32_t offset, uint32_t & epoch);

    static CHIP_ERROR GenerateNOC(chip::Crypto::P256Keypair & signingKeypair, NSData * signingCertificate, chip::NodeId nodeId,
        chip::FabricId fabricId, const chip::CATValues & cats, const chip::Crypto::P256PublicKey & pubkey,
        chip::MutableByteSpan & noc);

    ChipP256KeypairPtr mIssuerKey;

    chip::Crypto::AesCcm128Key mIPK;

    static const uint32_t kCertificateValiditySecs = 365 * 24 * 60 * 60;

    MTRPersistentStorageDelegateBridge * mStorage;

    chip::NodeId mDeviceBeingPaired = chip::kUndefinedNodeId;

    chip::NodeId mNextRequestedNodeId = 1;
    chip::FabricId mNextFabricId = 1;
    bool mNodeIdRequested = false;

    // mRootCert should not really be nullable, but we are constructed before we
    // have a root cert, and at that point it gets initialized to nil.
    NSData * _Nullable mRootCert;
    NSData * _Nullable mIntermediateCert;
};

NS_ASSUME_NONNULL_END
