//===--- DebugTrace.h - Utilities for trace-debugging ---*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_BASIC_DEBUGTRACE_H
#define SWIFT_BASIC_DEBUGTRACE_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Debug.h"
#include <string>
#include <vector>

namespace clang {
  class Decl;
  class FileEntry;
  class Module;
  class NamedDecl;
  class ObjCInterfaceDecl;
  class ObjCProtocolDecl;
}

namespace swift {

class Decl;
class DeclName;

template <class T>
struct DebugDesc {
  const T val;
  explicit DebugDesc(const T& v) : val(v) {}
  template <class U>
  static DebugDesc<U> desc(const U& u) { return DebugDesc<U>(u); }
  // Specialize this method for types you want to render differntly.
  void render(llvm::raw_ostream& out) const {
    out << val;
  }
};

template <class T>
llvm::raw_ostream& operator<<(llvm::raw_ostream& out, const DebugDesc<T>& d) {
  d.render(out);
  return out;
}

// Probably something clever can be done here with enable_if<> but I can't figure
// it out.
template<> void DebugDesc<const clang::Decl*>::render(llvm::raw_ostream &out) const;
template<> void DebugDesc<const clang::NamedDecl*>::render(llvm::raw_ostream &out) const;
template<> void DebugDesc<const clang::ObjCInterfaceDecl*>::render(llvm::raw_ostream &out) const;
template<> void DebugDesc<const clang::ObjCProtocolDecl*>::render(llvm::raw_ostream &out) const;
template<> void DebugDesc<const clang::Module*>::render(llvm::raw_ostream &out) const;
template<> void DebugDesc<const clang::FileEntry*>::render(llvm::raw_ostream &out) const;

template<> void DebugDesc<clang::Decl*>::render(llvm::raw_ostream &out) const;
template<> void DebugDesc<clang::NamedDecl*>::render(llvm::raw_ostream &out) const;
template<> void DebugDesc<clang::ObjCInterfaceDecl*>::render(llvm::raw_ostream &out) const;
template<> void DebugDesc<clang::ObjCProtocolDecl*>::render(llvm::raw_ostream &out) const;
template<> void DebugDesc<clang::Module*>::render(llvm::raw_ostream &out) const;
template<> void DebugDesc<clang::FileEntry*>::render(llvm::raw_ostream &out) const;

// When debugging, use the DebugFilter.check() methods to construct DebugTracer
// objects in various contexts; those that satisfy the filter will be active and
// will trace their creation/destruction, adjust their indentation level; those
// that do not satisfy the filter will be inactive.
class DebugTracer {
  DebugTracer(const DebugTracer&) = delete;
  DebugTracer& operator=(const DebugTracer&) = delete;
  std::string context;
  std::string activeEntity;
  bool isActive() const {
    return !activeEntity.empty();
  }

public:
  static unsigned indent;
  operator bool() const {
    return isActive();
  }
  llvm::raw_ostream& indented() const;
  template<class T> DebugDesc<T> desc(const T& v) { return DebugDesc<T>(v); }
  void trap() const;
  DebugTracer(DebugTracer&&);
  DebugTracer(llvm::StringRef context, llvm::StringRef entity=llvm::StringRef());
  ~DebugTracer();
};

// When debugging, create and configure one of these locally in each file you
// want to add tracing to; then use it as a factory for DebugTracer objects
// based on whether a provided entity matches any of the names of interest.
class DebugFilter {
  DebugFilter(const DebugFilter&) = delete;
  DebugFilter& operator=(const DebugFilter&) = delete;
  DebugFilter() = delete;
  std::vector<std::string> names;
public:
  explicit DebugFilter(llvm::ArrayRef<llvm::StringRef> nm);
  DebugTracer check(llvm::StringRef context, llvm::StringRef entity);
  DebugTracer check(llvm::StringRef context, swift::DeclName entity);
  DebugTracer check(llvm::StringRef context, const swift::Decl* entity);
  DebugTracer check(llvm::StringRef context, const clang::Decl* entity);
};

}

#endif
