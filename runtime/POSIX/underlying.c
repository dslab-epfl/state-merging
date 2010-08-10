/*
 * underlying.c
 *
 *  Created on: Aug 10, 2010
 *      Author: stefan
 */

#define __FORCE_USE_UNDERLYING
#include "underlying.h"

__attribute__((used)) void __underlying_linkage(void) {
  // Just do nothing
}
