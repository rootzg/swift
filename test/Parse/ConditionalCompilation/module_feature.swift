// RUN: %target-parse-verify-swift -module-name Rice -enable-feature beans

#if feature(beans)
  let a = 1 // expected-note 3 {{did you mean}}
#endif

#if feature(Rice.beans)
  let b = 1 // expected-note 3 {{did you mean}}
#endif

#if feature(onion) // expected-warning {{unknown feature}}
  let c = 1
#endif

#if feature(Rice.onion) // expected-warning {{unknown feature}}
  let d = 1
#endif

#if feature(Potato.onion) // expected-warning {{unknown module}}
  let e = 1
#endif

let a2 = a;
let b2 = b;
let c2 = c; // expected-error {{use of unresolved identifier}}
let d2 = d; // expected-error {{use of unresolved identifier}}
let e2 = e; // expected-error {{use of unresolved identifier}}

