// RUN: %empty-directory(%t)
// RUN: %target-swiftc_driver -o %t/main -module-name main -stats-output-dir %t %s -trace-stats-events
// RUN: %FileCheck -input-file %t/*.csv %s

// CHECK: {{[0-9]+,[0-9]+,"exit","typecheck-expr","Sema.NumTypesDeserialized",[0-9]+,[0-9]+,"Call","\[.*stats_dir_tracer.swift:9:5 - line:9:18\]"}}
// CHECK: {{[0-9]+,[0-9]+,"exit","typecheck-expr","Sema.NumConstraintScopes",[0-9]+,[0-9]+,"IntegerLiteral","\[.*stats_dir_tracer.swift:22:13 - line:22:13\]"}}

public func foo() {
    print("hello")
}

protocol Proto {
  func bar() -> Int;
}

class Bar {
  typealias T = Int
}

extension Bar : Proto {
  func bar() -> T {
    let x = 1
    let y = Int(1.0)
    return x + y
  }
}
