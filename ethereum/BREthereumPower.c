//
//  BREthereumPower.c
//  etzwallet
//
//  Created by etz on 2018/7/27.
//  Copyright © 2018年 etzwallet LLC. All rights reserved.
//

#include "BREthereumPower.h"
#include <assert.h>

//
// Power
//
extern BREthereumPower
PowerCreate(double amountOfPower) {
    BREthereumPower power;
    power.amountOfPower = amountOfPower;
    return power;
}
