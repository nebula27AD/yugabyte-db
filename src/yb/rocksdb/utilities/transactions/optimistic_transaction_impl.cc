//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// The following only applies to changes made to this file as part of YugaByte development.
//
// Portions Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//

#ifndef ROCKSDB_LITE

#include "yb/rocksdb/utilities/transactions/optimistic_transaction_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "yb/rocksdb/comparator.h"
#include "yb/rocksdb/db.h"
#include "yb/rocksdb/db/db_impl.h"
#include "yb/rocksdb/status.h"
#include "yb/rocksdb/utilities/optimistic_transaction_db.h"
#include "yb/rocksdb/utilities/transactions/transaction_util.h"

namespace rocksdb {

struct WriteOptions;

OptimisticTransactionImpl::OptimisticTransactionImpl(
    OptimisticTransactionDB* txn_db, const WriteOptions& write_options,
    const OptimisticTransactionOptions& txn_options)
    : TransactionBaseImpl(txn_db->GetBaseDB(), write_options), txn_db_(txn_db) {
  Initialize(txn_options);
}

void OptimisticTransactionImpl::Initialize(
    const OptimisticTransactionOptions& txn_options) {
  if (txn_options.set_snapshot) {
    SetSnapshot();
  }
}

void OptimisticTransactionImpl::Reinitialize(
    OptimisticTransactionDB* txn_db, const WriteOptions& write_options,
    const OptimisticTransactionOptions& txn_options) {
  TransactionBaseImpl::Reinitialize(txn_db->GetBaseDB(), write_options);
  Initialize(txn_options);
}

OptimisticTransactionImpl::~OptimisticTransactionImpl() {
}

void OptimisticTransactionImpl::Clear() {
  TransactionBaseImpl::Clear();
}

Status OptimisticTransactionImpl::Commit() {
  // Set up callback which will call CheckTransactionForConflicts() to
  // check whether this transaction is safe to be committed.
  OptimisticTransactionCallback callback(this);

  DBImpl* db_impl = dynamic_cast<DBImpl*>(db_->GetRootDB());
  if (db_impl == nullptr) {
    // This should only happen if we support creating transactions from
    // a StackableDB and someone overrides GetRootDB().
    return STATUS(InvalidArgument,
        "DB::GetRootDB() returned an unexpected DB class");
  }

  Status s = db_impl->WriteWithCallback(
      write_options_, GetWriteBatch()->GetWriteBatch(), &callback);

  if (s.ok()) {
    Clear();
  }

  return s;
}

void OptimisticTransactionImpl::Rollback() { Clear(); }

// Record this key so that we can check it for conflicts at commit time.
Status OptimisticTransactionImpl::TryLock(ColumnFamilyHandle* column_family,
                                          const Slice& key, bool read_only,
                                          bool untracked) {
  if (untracked) {
    return Status::OK();
  }
  uint32_t cfh_id = GetColumnFamilyID(column_family);

  SetSnapshotIfNeeded();

  SequenceNumber seq;
  if (snapshot_) {
    seq = snapshot_->GetSequenceNumber();
  } else {
    seq = db_->GetLatestSequenceNumber();
  }

  std::string key_str = key.ToString();

  TrackKey(cfh_id, key_str, seq, read_only);

  // Always return OK. Confilct checking will happen at commit time.
  return Status::OK();
}

// Returns OK if it is safe to commit this transaction.  Returns Status::Busy
// if there are read or write conflicts that would prevent us from committing OR
// if we can not determine whether there would be any such conflicts.
//
// Should only be called on writer thread in order to avoid any race conditions
// in detecting write conflicts.
Status OptimisticTransactionImpl::CheckTransactionForConflicts(DB* db) {
  Status result;

  assert(dynamic_cast<DBImpl*>(db) != nullptr);
  auto db_impl = reinterpret_cast<DBImpl*>(db);

  // Since we are on the write thread and do not want to block other writers,
  // we will do a cache-only conflict check.  This can result in TryAgain
  // getting returned if there is not sufficient memtable history to check
  // for conflicts.
  return TransactionUtil::CheckKeysForConflicts(db_impl, GetTrackedKeys(),
                                                true /* cache_only */);
}

}  // namespace rocksdb

#endif  // ROCKSDB_LITE
