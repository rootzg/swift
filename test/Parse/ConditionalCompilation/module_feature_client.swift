// RUN: rm -rf %t && mkdir %t
// RUN: %target-build-swift %S/Inputs/module_feature_lib.swift -emit-module -emit-module-path %t -module-name Rice -enable-feature beans
// RUN: %target-parse-verify-swift -I %t

import Rice

#if feature(Rice.beans)
let numLegumes = numBeans
#endif

print(numLegumes)

#if feature(Rice.onions) // expected-warning {{unknown feature}}
let x = 1
#endif

