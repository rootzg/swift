// RUN: %target-swift-frontend -typecheck -verify %s %S/Inputs/mixed-external-accessibility.swift -primary-file %s -module-name foo
struct Struct {}
let x = Struct.Inner().hashValue

