/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include <lib/support/CodeUtils.h>
#include <lib/support/DefaultStorageKeyAllocator.h>
#include <lib/support/SafeInt.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/KeyValueStoreManager.h>
#include <platform/NetworkCommissioning.h>
#include <platform/OpenThread/GenericNetworkCommissioningThreadDriver.h>
#include <platform/OpenThread/GenericThreadStackManagerImpl_OpenThread.h>
#include <platform/ThreadStackManager.h>

#include <limits>

using namespace chip;
using namespace chip::Thread;
using namespace chip::DeviceLayer::PersistedStorage;

namespace chip {
namespace DeviceLayer {
namespace NetworkCommissioning {

// NOTE: For GenericThreadDriver, we assume that the network configuration is persisted by
// the OpenThread stack after ConnectNetwork command is called, and before that, all the changes
// are made to a local copy of the dataset stored in mStagingNetwork.
// Also, in order to support the fail-safe mechanism, the configuration is backed up in a temporary
// KVS slot upon any changes and restored when the fail-safe timeout is triggered or the device
// reboots without completing all the changes.

// Not all KVS implementations support zero-length values, so use this special value, that is not a valid
// dataset, to represent an empty dataset. We need that to be able to revert the network configuration
// in the case of an unsuccessful commissioning.
constexpr uint8_t kEmptyDataset[1] = {};

CHIP_ERROR GenericThreadDriver::Init(Internal::BaseDriver::NetworkStatusChangeCallback * statusChangeCallback)
{
    ThreadStackMgrImpl().SetNetworkStatusChangeCallback(statusChangeCallback);

    VerifyOrReturnError(ThreadStackMgrImpl().IsThreadAttached(), CHIP_NO_ERROR);
    VerifyOrReturnError(ThreadStackMgrImpl().GetThreadProvision(mStagingNetwork) == CHIP_NO_ERROR, CHIP_NO_ERROR);

    return CHIP_NO_ERROR;
}

void GenericThreadDriver::Shutdown()
{
    ThreadStackMgrImpl().SetNetworkStatusChangeCallback(nullptr);
}

CHIP_ERROR GenericThreadDriver::CommitConfiguration()
{
    // OpenThread persists the network configuration on AttachToThreadNetwork, so simply remove
    // the backup, so that it cannot be restored. If no backup could be found, it means that the
    // configuration has not been modified since the fail-safe was armed, so return with no error.

    DefaultStorageKeyAllocator key;
    CHIP_ERROR error = KeyValueStoreMgr().Delete(key.FailSafeNetworkConfig());

    return error == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND ? CHIP_NO_ERROR : error;
}

CHIP_ERROR GenericThreadDriver::RevertConfiguration()
{
    DefaultStorageKeyAllocator key;
    uint8_t datasetBytes[Thread::kSizeOperationalDataset];
    size_t datasetLength;

    CHIP_ERROR error = KeyValueStoreMgr().Get(key.FailSafeNetworkConfig(), datasetBytes, sizeof(datasetBytes), &datasetLength);

    // If no backup could be found, it means that the network configuration has not been modified
    // since the fail-safe was armed, so return with no error.
    ReturnErrorCodeIf(error == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND, CHIP_NO_ERROR);
    ReturnErrorOnFailure(error);

    ChipLogError(NetworkProvisioning, "Found Thread configuration backup: reverting configuration");

    // Not all KVS implementations support zero-length values, so handle a special value representing an empty dataset.
    ByteSpan dataset(datasetBytes, datasetLength);

    if (dataset.data_equal(ByteSpan(kEmptyDataset)))
    {
        dataset = {};
    }

    ReturnErrorOnFailure(mStagingNetwork.Init(dataset));
    ReturnErrorOnFailure(DeviceLayer::ThreadStackMgrImpl().AttachToThreadNetwork(mStagingNetwork, /* callback */ nullptr));

    // TODO: What happens on errors above? Why do we not remove the failsafe?
    return KeyValueStoreMgr().Delete(key.FailSafeNetworkConfig());
}

Status GenericThreadDriver::AddOrUpdateNetwork(ByteSpan operationalDataset, MutableCharSpan & outDebugText,
                                               uint8_t & outNetworkIndex)
{
    ByteSpan newExtpanid;
    Thread::OperationalDataset newDataset;

    outDebugText.reduce_size(0);
    outNetworkIndex = 0;

    VerifyOrReturnError(newDataset.Init(operationalDataset) == CHIP_NO_ERROR && newDataset.IsCommissioned(), Status::kOutOfRange);
    newDataset.GetExtendedPanIdAsByteSpan(newExtpanid);

    // We only support one active operational dataset. Add/Update based on either:
    // Staging network not commissioned yet (active) or we are updating the dataset with same Extended Pan ID.
    VerifyOrReturnError(!mStagingNetwork.IsCommissioned() || MatchesNetworkId(mStagingNetwork, newExtpanid) == Status::kSuccess,
                        Status::kBoundsExceeded);
    VerifyOrReturnError(BackupConfiguration() == CHIP_NO_ERROR, Status::kUnknownError);

    mStagingNetwork = newDataset;
    return Status::kSuccess;
}

Status GenericThreadDriver::RemoveNetwork(ByteSpan networkId, MutableCharSpan & outDebugText, uint8_t & outNetworkIndex)
{
    outDebugText.reduce_size(0);
    outNetworkIndex = 0;

    NetworkCommissioning::Status status = MatchesNetworkId(mStagingNetwork, networkId);

    VerifyOrReturnError(status == Status::kSuccess, status);
    VerifyOrReturnError(BackupConfiguration() == CHIP_NO_ERROR, Status::kUnknownError);

    mStagingNetwork.Clear();

    return Status::kSuccess;
}

Status GenericThreadDriver::ReorderNetwork(ByteSpan networkId, uint8_t index, MutableCharSpan & outDebugText)
{
    outDebugText.reduce_size(0);

    NetworkCommissioning::Status status = MatchesNetworkId(mStagingNetwork, networkId);

    VerifyOrReturnError(status == Status::kSuccess, status);
    VerifyOrReturnError(index == 0, Status::kOutOfRange);

    return Status::kSuccess;
}

void GenericThreadDriver::ConnectNetwork(ByteSpan networkId, ConnectCallback * callback)
{
    NetworkCommissioning::Status status = MatchesNetworkId(mStagingNetwork, networkId);

    if (status == Status::kSuccess && BackupConfiguration() != CHIP_NO_ERROR)
    {
        status = Status::kUnknownError;
    }

    if (status == Status::kSuccess &&
        DeviceLayer::ThreadStackMgrImpl().AttachToThreadNetwork(mStagingNetwork, callback) != CHIP_NO_ERROR)
    {
        status = Status::kUnknownError;
    }

    if (status != Status::kSuccess)
    {
        callback->OnResult(status, CharSpan(), 0);
    }
}

void GenericThreadDriver::ScanNetworks(ThreadDriver::ScanCallback * callback)
{
    CHIP_ERROR err = DeviceLayer::ThreadStackMgrImpl().StartThreadScan(callback);
    if (err != CHIP_NO_ERROR)
    {
        mScanStatus.SetValue(Status::kUnknownError);
        callback->OnFinished(Status::kUnknownError, CharSpan(), nullptr);
    }
    else
    {
        // OpenThread's "scan" will always success once started, so we can set the value of scan result here.
        mScanStatus.SetValue(Status::kSuccess);
    }
}

Status GenericThreadDriver::MatchesNetworkId(const Thread::OperationalDataset & dataset, const ByteSpan & networkId) const
{
    ByteSpan extpanid;

    if (!dataset.IsCommissioned())
    {
        return Status::kNetworkIDNotFound;
    }

    if (dataset.GetExtendedPanIdAsByteSpan(extpanid) != CHIP_NO_ERROR)
    {
        return Status::kUnknownError;
    }

    return networkId.data_equal(extpanid) ? Status::kSuccess : Status::kNetworkIDNotFound;
}

CHIP_ERROR GenericThreadDriver::BackupConfiguration()
{
    DefaultStorageKeyAllocator key;
    uint8_t dummy;

    // If configuration is already backed up, return with no error
    if (KeyValueStoreMgr().Get(key.FailSafeNetworkConfig(), &dummy, 0) == CHIP_ERROR_BUFFER_TOO_SMALL)
    {
        return CHIP_NO_ERROR;
    }

    // Not all KVS implementations support zero-length values, so use a special value in such a case.
    ByteSpan dataset = mStagingNetwork.IsEmpty() ? ByteSpan(kEmptyDataset) : mStagingNetwork.AsByteSpan();

    return KeyValueStoreMgr().Put(key.FailSafeNetworkConfig(), dataset.data(), dataset.size());
}

size_t GenericThreadDriver::ThreadNetworkIterator::Count()
{
    return driver->mStagingNetwork.IsCommissioned() ? 1 : 0;
}

bool GenericThreadDriver::ThreadNetworkIterator::Next(Network & item)
{
    if (exhausted || !driver->mStagingNetwork.IsCommissioned())
    {
        return false;
    }
    uint8_t extpanid[kSizeExtendedPanId];
    VerifyOrReturnError(driver->mStagingNetwork.GetExtendedPanId(extpanid) == CHIP_NO_ERROR, false);
    memcpy(item.networkID, extpanid, kSizeExtendedPanId);
    item.networkIDLen = kSizeExtendedPanId;
    item.connected    = false;
    exhausted         = true;

    Thread::OperationalDataset currentDataset;
    uint8_t enabledExtPanId[Thread::kSizeExtendedPanId];

    // The Thread network is not actually enabled.
    VerifyOrReturnError(ConnectivityMgrImpl().IsThreadAttached(), true);
    VerifyOrReturnError(ThreadStackMgrImpl().GetThreadProvision(currentDataset) == CHIP_NO_ERROR, true);
    // The Thread network is not enabled, but has a different extended pan id.
    VerifyOrReturnError(currentDataset.GetExtendedPanId(enabledExtPanId) == CHIP_NO_ERROR, true);
    VerifyOrReturnError(memcmp(extpanid, enabledExtPanId, kSizeExtendedPanId) == 0, true);
    // The Thread network is enabled and has the same extended pan id as the one in our record.
    item.connected = true;

    return true;
}

} // namespace NetworkCommissioning
} // namespace DeviceLayer
} // namespace chip
