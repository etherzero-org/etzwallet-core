//
//  BREthereumTransfer.c
//  Core
//
//  Created by Ed Gamble on 7/9/18.
//  Copyright (c) 2018 breadwallet LLC
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include <string.h>
#include <assert.h>
#include "BREthereumTransfer.h"
#include "BREthereumPrivate.h"
#include "../blockchain/BREthereumTransaction.h"
#include "../blockchain/BREthereumLog.h"

#define TRANSACTION_NONCE_IS_NOT_ASSIGNED   UINT64_MAX

static void
transferProvideOriginatingTransaction (BREthereumTransfer transfer);

//
// MARK: - Status
//
#define TRANSFER_STATUS_REASON_BYTES   \
(sizeof (BREthereumGas) + sizeof (BREthereumHash) + 2 * sizeof(uint64_t))

typedef struct BREthereumTransferStatusRecord {
    BREthereumTransferStatusType type;
    union {
        struct {
            BREthereumGas gasUsed;      // Internal
            BREthereumHash blockHash;
            uint64_t blockNumber;
            uint64_t transactionIndex;
        } included;
        
        struct {
            char reason[TRANSFER_STATUS_REASON_BYTES + 1];
        } errored;
    } u;
} BREthereumTransferStatus;

//
// MARK: Basis
//
typedef enum  {
    TRANSFER_BASIS_TRANSACTION,
    TRANSFER_BASIS_LOG
} BREthereumTransferBasisType;

typedef struct {
    BREthereumTransferBasisType type;
    union {
        BREthereumTransaction transaction;
        BREthereumLog log;
    } u;
} BREthereumTransferBasis;

//
//
//
struct BREthereumTransferRecord {
    BREthereumAddress sourceAddress;
    
    BREthereumAddress targetAddress;
    
    BREthereumAmount amount;
    
    BREthereumFeeBasis feeBasis;
    
    BREthereumGas gasEstimate;
    
    BREthereumTransaction originatingTransaction;
    
    BREthereumTransferBasis basis;
    
    BREthereumTransferStatus status;
};

static BREthereumTransfer
transferCreateDetailed (BREthereumAddress sourceAddress,
                        BREthereumAddress targetAddress,
                        BREthereumAmount amount,
                        BREthereumFeeBasis feeBasis,
                        BREthereumTransaction originatingTransaction) {
    BREthereumTransfer transfer = calloc (1, sizeof(struct BREthereumTransferRecord));

    transfer->sourceAddress = sourceAddress;
    transfer->targetAddress = targetAddress;
    transfer->amount = amount;
    transfer->feeBasis = feeBasis;
    transfer->gasEstimate = gasCreate(0);
    transfer->originatingTransaction = originatingTransaction;

    return transfer;
}

extern BREthereumTransfer
transferCreate (BREthereumAddress sourceAddress,
                BREthereumAddress targetAddress,
                BREthereumAmount amount,
                BREthereumFeeBasis feeBasis) {
    BREthereumTransfer transfer = transferCreateDetailed(sourceAddress,
                                                         targetAddress,
                                                         amount,
                                                         feeBasis,
                                                         NULL);

    transferProvideOriginatingTransaction(transfer);
    
    return transfer;
}

extern BREthereumTransfer
transferCreateWithTransaction (BREthereumTransaction transaction) {
    BREthereumFeeBasis feeBasis = {
        FEE_BASIS_GAS,
        { .gas = {
            transactionGetGasLimit(transaction),
            transactionGetGasPrice(transaction)
        }}
    };
    
    BREthereumTransfer transfer = transferCreateDetailed (transactionGetSourceAddress(transaction),
                                                          transactionGetTargetAddress(transaction),
                                                          amountCreateEther (transactionGetAmount(transaction)),
                                                          feeBasis,
                                                          NULL);
    // Basis
    transfer->basis.type = TRANSFER_BASIS_TRANSACTION;
    transfer->basis.u.transaction = transaction;

    // Status
    BREthereumTransactionStatus status = transactionGetStatus(transaction);
    switch (status.type) {
        case TRANSACTION_STATUS_UNKNOWN:
        case TRANSACTION_STATUS_QUEUED:
        case TRANSACTION_STATUS_PENDING:
            transfer->status.type = TRANSFER_STATUS_SUBMITTED;
            break;

        case TRANSACTION_STATUS_INCLUDED:
            transfer->status.type = TRANSFER_STATUS_INCLUDED;
            transactionStatusExtractIncluded(&status, NULL, NULL, &transfer->status.u.included.blockNumber, NULL);
            break;

        case TRANSACTION_STATUS_ERRORED:
            transfer->status.type = TRANSFER_STATUS_ERRORED;
            strncpy (transfer->status.u.errored.reason, "Unknown (for now)", TRANSFER_STATUS_REASON_BYTES);
            break;
    }

    return transfer;
}

extern BREthereumTransfer
transferCreateWithLog (BREthereumLog log,
                       BREthereumToken token) {
    BREthereumFeeBasis feeBasis = {
        FEE_BASIS_NONE
    };

    BREthereumAddress sourceAddress = EMPTY_ADDRESS_INIT;
    BREthereumAddress targetAddress = EMPTY_ADDRESS_INIT;

    UInt256 value = UINT256_ZERO;
    BREthereumAmount  amount = amountCreateToken(createTokenQuantity(token, value));

    BREthereumTransfer transfer = transferCreateDetailed (sourceAddress,
                                                          targetAddress,
                                                          amount,
                                                          feeBasis,
                                                          NULL);
    // Basis
    transfer->basis.type = TRANSFER_BASIS_LOG;
    transfer->basis.u.log = log;

    BREthereumTransactionStatus status = logGetStatus(log);
    // ...

    return transfer;
}


extern void
transferRelease (BREthereumTransfer transfer) {
    free (transfer);
}

extern BREthereumAddress
transferGetSourceAddress (BREthereumTransfer transfer) {
    return transfer->sourceAddress;
}

extern BREthereumAddress
transferGetTargetAddress (BREthereumTransfer transfer) {
    return transfer->targetAddress;
}

extern BREthereumAmount
transferGetAmount (BREthereumTransfer transfer) {
    return transfer->amount;
}

extern BREthereumToken
transferGetToken (BREthereumTransfer transfer) {
    return (AMOUNT_TOKEN == amountGetType(transfer->amount)
            ? amountGetToken(transfer->amount)
            : NULL);
}

extern BREthereumFeeBasis
transferGetFeeBasis (BREthereumTransfer transfer) {
    return transfer->feeBasis;
}

extern BREthereumGas
transferGetGasEstimate (BREthereumTransfer transfer) {
    return transfer->gasEstimate;
}

extern void
transferSetGasEstimate (BREthereumTransfer transfer,
                        BREthereumGas gasEstimate) {
    transfer->gasEstimate = gasEstimate;
}

extern BREthereumTransaction
transferGetOriginatingTransaction (BREthereumTransfer transfer) {
    return transfer->originatingTransaction;
}

extern BREthereumTransaction
transferGetBasisTransaction (BREthereumTransfer transfer) {
    return (TRANSFER_BASIS_TRANSACTION == transfer->basis.type
            ? transfer->basis.u.transaction
            : NULL);
}

extern BREthereumLog
transferGetBasisLog (BREthereumTransfer transfer) {
    return (TRANSFER_BASIS_LOG == transfer->basis.type
            ? transfer->basis.u.log
            : NULL);
}

extern void
transferSign (BREthereumTransfer transfer,
              BREthereumNetwork network,
              BREthereumAccount account,
              BREthereumAddress address,
              const char *paperKey) {
    
    if (TRANSACTION_NONCE_IS_NOT_ASSIGNED == transactionGetNonce(transfer->originatingTransaction))
        transactionSetNonce (transfer->originatingTransaction,
                             accountGetThenIncrementAddressNonce(account, address));
    
    // RLP Encode the UNSIGNED transfer
    BRRlpCoder coder = rlpCoderCreate();
    BRRlpItem item = transactionRlpEncode (transfer->originatingTransaction,
                                           network,
                                           RLP_TYPE_TRANSACTION_UNSIGNED,
                                           coder);
    BRRlpData data = rlpGetData(coder, item);
    
    // Sign the RLP Encoded bytes.
    BREthereumSignature signature = accountSignBytes (account,
                                                      address,
                                                      SIGNATURE_TYPE_RECOVERABLE,
                                                      data.bytes,
                                                      data.bytesCount,
                                                      paperKey);
    
    rlpDataRelease(data);
    rlpCoderRelease(coder);
    
    // Attach the signature
    transactionSign (transfer->originatingTransaction, signature);
}

extern void
transferSignWithKey (BREthereumTransfer transfer,
                     BREthereumNetwork network,
                     BREthereumAccount account,
                     BREthereumAddress address,
                     BRKey privateKey) {
    
    if (TRANSACTION_NONCE_IS_NOT_ASSIGNED == transactionGetNonce(transfer->originatingTransaction))
        transactionSetNonce (transfer->originatingTransaction,
                             accountGetThenIncrementAddressNonce(account, address));
    
    // RLP Encode the UNSIGNED transfer
    BRRlpCoder coder = rlpCoderCreate();
    BRRlpItem item = transactionRlpEncode (transfer->originatingTransaction,
                                           network,
                                           RLP_TYPE_TRANSACTION_UNSIGNED,
                                           coder);
    BRRlpData data = rlpGetData(coder, item);
    
    // Sign the RLP Encoded bytes.
    BREthereumSignature signature = accountSignBytesWithPrivateKey (account,
                                                                    address,
                                                                    SIGNATURE_TYPE_RECOVERABLE,
                                                                    data.bytes,
                                                                    data.bytesCount,
                                                                    privateKey);
    
    rlpDataRelease(data);
    rlpCoderRelease(coder);
    
    // Attach the signature
    transactionSign(transfer->originatingTransaction, signature);
}

extern const BREthereumHash
transferGetHash (BREthereumTransfer transfer) {
    switch (transfer->basis.type) {
        case TRANSFER_BASIS_TRANSACTION:
            return transactionGetHash(transfer->basis.u.transaction);
        case TRANSFER_BASIS_LOG:
            return logGetHash(transfer->basis.u.log);
    }
}

extern uint64_t
transferGetNonce (BREthereumTransfer transfer) {
    return (NULL != transfer->originatingTransaction
            ? transactionGetNonce(transfer->originatingTransaction)
            : TRANSACTION_NONCE_IS_NOT_ASSIGNED);
}

extern BREthereumEther
transferGetFee (BREthereumTransfer transfer, int *overflow) {
    if (NULL != overflow) *overflow = 0;
    switch (transfer->basis.type) {
        case TRANSFER_BASIS_LOG:
            return etherCreateZero();
        case TRANSFER_BASIS_TRANSACTION:
            return transactionGetFee(transfer->basis.u.transaction, overflow);
    }
}

///
/// MARK: - Status
///
extern BREthereumBoolean
transferHasStatusType (BREthereumTransfer transfer,
                       BREthereumTransferStatusType type) {
    return AS_ETHEREUM_BOOLEAN(transfer->status.type == type);
}

extern BREthereumBoolean
transferHasStatusTypeOrTwo (BREthereumTransfer transfer,
                            BREthereumTransferStatusType type1,
                            BREthereumTransferStatusType type2) {
    return AS_ETHEREUM_BOOLEAN(transfer->status.type == type1 ||
                               transfer->status.type == type2);
}

extern int
transferExtractStatusIncluded (BREthereumTransfer transfer,
                               uint64_t *blockNumber) {
    if (TRANSFER_STATUS_INCLUDED != transfer->status.type) return 0;
    
    if (NULL != blockNumber) *blockNumber = transfer->status.u.included.blockNumber;
    
    return 1;
}

extern int
transferExtractStatusError (BREthereumTransfer transfer,
                            char **reason) {
    if (TRANSFER_STATUS_ERRORED != transfer->status.type) return 0;
    
    if (NULL != reason) *reason = strdup (transfer->status.u.errored.reason);
    
    return 1;
}

///
/// MARK: - Originating Transaction
///
static char *
transferProvideOriginatingTransactionData (BREthereumTransfer transfer) {
    switch (amountGetType(transfer->amount)) {
        case AMOUNT_ETHER:
            return strdup ("");
        case AMOUNT_TOKEN: {
            UInt256 value = amountGetTokenQuantity(transfer->amount).valueAsInteger;
            
            char address[ADDRESS_ENCODED_CHARS];
            addressFillEncodedString(transfer->targetAddress, 0, address);
            
            // Data is a HEX ENCODED string
            return (char *) contractEncode (contractERC20, functionERC20Transfer,
                                            // Address
                                            (uint8_t *) &address[2], strlen(address) - 2,
                                            // Amount
                                            (uint8_t *) &value, sizeof (UInt256),
                                            NULL);
        }
    }
}

static BREthereumAddress
transferProvideOriginatingTransactionTargetAddress (BREthereumTransfer transfer) {
    switch (amountGetType(transfer->amount)) {
        case AMOUNT_ETHER:
            return transfer->targetAddress;
        case AMOUNT_TOKEN:
            return tokenGetAddressRaw(amountGetToken(transfer->amount));
    }
}

static BREthereumEther
transferProvideOriginatingTransactionAmount (BREthereumTransfer transfer) {
    switch (amountGetType(transfer->amount)) {
        case AMOUNT_ETHER:
            return transfer->amount.u.ether;
        case AMOUNT_TOKEN:
            return etherCreateZero();
    }
}

static void
transferProvideOriginatingTransaction (BREthereumTransfer transfer) {
    if (NULL != transfer->originatingTransaction)
        transactionRelease(transfer->originatingTransaction);
    
    char *data = transferProvideOriginatingTransactionData(transfer);
    
    transfer->originatingTransaction =
    transactionCreate (transfer->sourceAddress,
                       transferProvideOriginatingTransactionTargetAddress (transfer),
                       transferProvideOriginatingTransactionAmount (transfer),
                       feeBasisGetGasPrice(transfer->feeBasis),
                       feeBasisGetGasLimit(transfer->feeBasis),
                       data,
                       TRANSACTION_NONCE_IS_NOT_ASSIGNED);
    free (data);
}


private_extern BREthereumEther
transferGetEffectiveAmountInEther (BREthereumTransfer transfer) {
    switch (transfer->basis.type) {
        case TRANSFER_BASIS_LOG: return etherCreateZero();
        case TRANSFER_BASIS_TRANSACTION: return transactionGetAmount(transfer->basis.u.transaction);
    }
}

extern BREthereumComparison
transferCompare (BREthereumTransfer t1,
                 BREthereumTransfer t2) {
    assert (t1->basis.type == t2->basis.type);
    switch (t1->basis.type) {
        case TRANSFER_BASIS_TRANSACTION:
            return transactionCompare(t1->basis.u.transaction, t2->basis.u.transaction);
        case TRANSFER_BASIS_LOG:
            return logCompare (t1->basis.u.log, t2->basis.u.log);
    }
}



