// RUN: %target-parse-verify-swift

#if feature(neverType)
  let w = 1
#else
  // This shouldn't emit any diagnostics.
  asdf asdf asdf asdf
#endif

#if feature(Swift.conditionalConformance)
  // This shouldn't emit any diagnostics.
  asdf asdf asdf asdf
#endif

#if feature(Swift.carrierPigeons) // expected-warning {{unknown feature}}
  // This shouldn't emit any diagnostics.
  asdf asdf asdf asdf
#endif
