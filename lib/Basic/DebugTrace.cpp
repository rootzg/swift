//===--- DebugTrace.cpp - Utilities for trace-debugging  -----------===//
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

#include "swift/Basic/DebugTrace.h"
#include "swift/AST/Decl.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclObjC.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Debug.h"

namespace swift {

unsigned DebugTracer::indent = 0;

DebugTracer::DebugTracer(DebugTracer &&other)
  : DebugTracer(other.context, other.activeEntity) {
  other.activeEntity = "";
}

DebugTracer::DebugTracer(StringRef context,
                         StringRef entity)
  : context(context),
    activeEntity(entity)
{
  if (isActive()) {
    indented() << "+++ " << context << ' ' << activeEntity << '\n';
    indent += 4;
  }
}

DebugTracer::~DebugTracer() {
  if (isActive()) {
    indent -= 4;
    indented() << "--- " << context << ' ' << activeEntity << '\n';
  }
}

llvm::raw_ostream &DebugTracer::indented() const {
  return llvm::dbgs().indent(indent);
}

void DebugTracer::trap() const {
  __builtin_debugtrap();
}

DebugFilter::DebugFilter(ArrayRef<StringRef> nm) {
  for (auto const n : nm) {
    names.push_back(n);
  }
}

DebugTracer
DebugFilter::check(StringRef context, StringRef entity) {
  for (auto const &c : names) {
    if (entity == c)
      return DebugTracer(context, c);
  }
  return DebugTracer(context);
}

DebugTracer
DebugFilter::check(StringRef context, DeclName entity) {
  for (auto const &c : names) {
    if (entity.isSimpleName(c))
      return DebugTracer(context, c);
  }
  return DebugTracer(context);
}

DebugTracer
DebugFilter::check(StringRef context, const Decl* entity) {
  if (auto const *VD = dyn_cast<const ValueDecl>(entity)) {
    return check(context, VD->getFullName());
  }
  return DebugTracer(context);
}

DebugTracer
DebugFilter::check(StringRef context, const clang::Decl* entity) {
  if (auto const *ND = dyn_cast<const clang::NamedDecl>(entity)) {
    if (ND->getDeclName().isIdentifier()) {
      return check(context, ND->getName());
    }
  }
  return DebugTracer(context);
}


template<> void
DebugDesc<const clang::NamedDecl*>::render(llvm::raw_ostream &out) const {
  out << "[clang::NamedDecl=" << val;
  if (val) {
    out << ' ';
    val->printQualifiedName(out);
    out << " in " << desc(val->getImportedOwningModule());
  }
  out << "]";
}

template<> void
DebugDesc<clang::NamedDecl*>::render(llvm::raw_ostream &out) const {
  out << desc<const clang::NamedDecl*>(val);
}

template<> void
DebugDesc<const clang::Decl*>::render(llvm::raw_ostream &out) const {
  if (auto const *ND = dyn_cast<const clang::NamedDecl>(val)) {
    out << desc(ND);
  } else {
    out << val;
  }
}

template<> void
DebugDesc<clang::Decl*>::render(llvm::raw_ostream &out) const {
  out << desc<const clang::Decl*>(val);
}

template<> void
DebugDesc<const clang::ObjCInterfaceDecl*>::render(llvm::raw_ostream &out) const {
  out << desc<const clang::NamedDecl*>(val);
}

template<> void
DebugDesc<clang::ObjCInterfaceDecl*>::render(llvm::raw_ostream &out) const {
  out << desc<const clang::NamedDecl*>(val);
}

template<> void
DebugDesc<const clang::ObjCProtocolDecl*>::render(llvm::raw_ostream &out) const {
  out << desc<const clang::NamedDecl*>(val);
}

template<> void
DebugDesc<clang::ObjCProtocolDecl*>::render(llvm::raw_ostream &out) const {
  out << desc<const clang::NamedDecl*>(val);
}


template<> void
DebugDesc<const clang::Module*>::render(llvm::raw_ostream &out) const {
  out << "[clang::Module=" << val;
  if (val) {
    out << ' ' << val->Name;
    out << " in " << desc(val->getASTFile());
  }
  out << "]";
}

template<> void
DebugDesc<clang::Module*>::render(llvm::raw_ostream &out) const {
  out << desc<const clang::Module*>(val);
}


template<> void
DebugDesc<const clang::FileEntry*>::render(llvm::raw_ostream &out) const {
  out << "[clang::FileEntry=" << val;
  if (val) {
    out << ' ' << llvm::sys::path::filename(val->getName());
  }
  out << ']';
}

template<> void
DebugDesc<clang::FileEntry*>::render(llvm::raw_ostream &out) const {
  out << desc<const clang::FileEntry*>(val);
}


}
