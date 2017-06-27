//
// Scenario for this test (this actually occurs in the wild):
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//   1. A header for module M, in include-path, that defines @interface Foo
//      and a @protocol Bar that references Foo in one of its methods.
//
//   2. A symlink in include-path that links into M's (modular) header dir,
//      which effectively makes a "second definition" of the same interface
//      and protocol pair, but in a non-modular context.
//
//   3. A bridging header that imports the (non-modular)
//      header-defining-Foo-and-Bar
//
//   4. A swift file that imports M and implements Bar (thus referencing Foo).
//
//   5. Another swift file that does nothing special, but makes for a multi-file
//      compilation (requiring a merge-module step).
//
//
// What was previously going wrong (that we're testing for):
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//   1. Module M gets compiled into a PCM file with a local defn for M.Foo and
//      M.Bar referencing M.Foo.
//
//   2. The bridging header gets precompiled to a .pch with a non-modular
//      definition of Foo and Bar.
//
//   3. Swift compiles the file with the class-implementing-Bar in the context
//      of the PCH and the PCM; having two defns for each decl means that any
//      lookup that finds one defn will cause clang to "complete" the identity
//      of the symbol it found by immediately pulling in the second
//      defn. Normally this wouldn't be a huge problem -- the second defn will
//      deterministically either override or be-overridden-by the first --
//      except for one wrinkle: since clang rev 2ba19793, the rules for
//      interfaces and protocols differ. Interfaces are first-read-wins,
//      protocols are last-read-wins.
//
//   4. This means that when swift looks up (protocol) Bar, the non-modular
//      definition is found in the PCH but clang completes it with a modular
//      definition from the PCM, and since it's a protocol and it's the last
//      defn read, the modular defn sticks. Then while decl-checking the
//      conformance to Bar, swift does a load-all-members on the Bar defn it
//      has, which grabs the directly-referenced modular definition of Foo,
//      which (being the first-read, and being an interface) then sticks.
//
//   5. Back in swift, subsequent lookup for Foo that _would_ otherwise normally
//      have started with the bridging header table and found the non-modular
//      defn thus fails (it's found in the swift bridging header lookup table
//      but filtered-out by comparing against the clang defn's official owning
//      module); so the definition for Foo that actually gets serialized as an
//      xref in the swiftmodule file is M.Foo.
//
//   5. Later, the merge-modules step executes and reloads the .swiftmodule
//      file, reading the XRef to M.Foo. Unfortunately when it's run with a .h
//      bridging header, the defn of Foo interface in that header is the first
//      ones seen (it's force-fed to clang before swift ever gets to doing
//      lookups), so even though it _tries_ to do a modular lookup of "M.Foo",
//      the deserialization logic in clang sees it has an existing defn and
//      ignores the modular defn. This results in a filtered-out result in
//      swift, causing a lookup failure.
//
//
// Why disabling bridging PCH fixes this
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// The first-read defn of Foo is fed into clang early, which sticks (as in
// the merge-modules step). This makes the serialized xref have the form
// __ObjC.Foo, which succeeds when performed the second time in merge-modules.
//
//
// How to fix it:
// ~~~~~~~~~~~~~~
//
// We could theoretically fix this by having the merge-modules step take a .pch
// as well as other steps in the compilation. That unfortuantely only masks the
// problem, which resurfaces in the definition-order of Bar itself: an xref to
// __ObjC.Bar gets serialized as well, and then _it_ can't be found because the
// *last-read* defn of Bar -- the modular one, M.Bar -- wins during lookup.
//
// So while we _do_ propose to have the merge-modules step use the PCH, we also
// put a patch into clang to make load-order determinism stronger: beyond just
// reconciling the first-wins vs. last-wins divergence, we make defn-overriding
// ordered by module ID, such that modular defns win over non-modular, and
// conflicts between two modules are deterministically resolved regardless of
// which order they get referenced during processing (at least insofar as module
// numbers are deterministically assigned).
//

// RUN: rm -rf %t
// RUN: mkdir -p %t/Headers/Simple
// RUN: ln -s %S/Inputs/frameworks/Simple.framework/Headers/Simple.h %t/Headers/Simple/Simple.h
// RUN: %target-build-swift -emit-module -save-temps -module-name test -Xfrontend -disable-deserialization-recovery -v -F %S/Inputs/frameworks -Xcc "-I%t/Headers" -module-cache-path %t/clang-module-cache -import-objc-header %S/Inputs/pch-bridging-header-with-non-modular-import.h %S/Inputs/other.swift %s
// REQUIRES: objc_interop

import Simple
class Foo: SimpleProtocol {
    func foo(_ bar: SimpleInterface) {
    }
}
