//
//  BREthereumPower.h
//  etzwallet
//
//  Created by etz on 2018/7/27.
//  Copyright © 2018年 etzwallet LLC. All rights reserved.
//

#ifndef BR_Ethereum_Power_H
#define BR_Ethereum_Power_H

#include "BRInt.h"
#include "BREthereumEther.h"

#ifdef __cplusplus
extern "C" {
#endif
    
    /**
     * Etherzero Power is a measure of the work associated with a transaction.
     */
    typedef struct BREthereumPowerStruct {
        double amountOfPower;
    } BREthereumPower;
    
    extern BREthereumPower
    PowerCreate(double power);
    
#ifdef __cplusplus
}
#endif 

#endif /* BREthereumPower_h */
