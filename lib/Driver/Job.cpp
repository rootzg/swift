//===--- Job.cpp - Command to Execute -------------------------------------===//
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

#include "swift/Basic/STLExtras.h"
#include "swift/Driver/Job.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Option/Arg.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "swift-driver-job"

using namespace swift;
using namespace swift::driver;

StringRef
CommandOutput::getOutputForInputAndType(StringRef Input, types::ID Type) const {
  auto const *M = DerivedOutputMap.getOutputMapForInput(Input);
  if (!M)
    return StringRef();
  auto const Out = M->find(Type);
  if (Out == M->end())
    return StringRef();
  return StringRef(Out->second);
}


void
CommandOutput::checkConflictAndAdd(StringRef OutputFile,
                                   StringRef InputFile,
                                   types::ID type) {
  auto &M = DerivedOutputMap.getOrCreateOutputMapForInput(InputFile);
  auto const Out = M.find(type);
  if (Out == M.end()) {
    M.insert(std::make_pair(type, OutputFile));
  } else {
    DEBUG({
        if (Out->second != OutputFile) {
          llvm::errs() << "Error: Adding OFM"
                       << "['" << InputFile << "']"
                       << "[" << types::getTypeName(type) << "] = "
                       << "'" << OutputFile << "' "
                       << "(existing entry: '" << Out->second << "')\n";
          dump();
        }
      });
    assert(Out->second == OutputFile);
  }
}

void CommandOutput::setAdditionalOutputForType(types::ID type,
                                               StringRef OutputFilename) {
  assert(InputFiles.size() == 1);
  checkConflictAndAdd(OutputFilename, InputFiles[0], type);
}

StringRef
CommandOutput::getAdditionalOutputForType(types::ID type) const {
  assert(InputFiles.size() == 1);
  return getOutputForInputAndType(InputFiles[0], type);
}

static void escapeAndPrintString(llvm::raw_ostream &os, StringRef Str) {
  if (Str.empty()) {
    // Special-case the empty string.
    os << "\"\"";
    return;
  }

  bool NeedsEscape = Str.find_first_of(" \"\\$") != StringRef::npos;

  if (!NeedsEscape) {
    // This string doesn't have anything we need to escape, so print it directly
    os << Str;
    return;
  }

  // Quote and escape. This isn't really complete, but is good enough, and
  // matches how Clang's Command handles escaping arguments.
  os << '"';
  for (const char c : Str) {
    switch (c) {
    case '"':
    case '\\':
    case '$':
      // These characters need to be escaped.
      os << '\\';
      // Fall-through to the default case, since we still need to print the
      // character.
      LLVM_FALLTHROUGH;
    default:
      os << c;
    }
  }
  os << '"';
}

void
CommandOutput::dump() const {
  llvm::errs()
    << "CommandOutput {\n"
    << "    PrimaryOutputType = "
    << types::getTypeName(PrimaryOutputType)
    << ";\n"
    << "    InputFiles = [";
  interleave(InputFiles,
             [&](StringRef S) { escapeAndPrintString(llvm::errs(), S); },
             [&] { llvm::errs() << ' '; });
  llvm::errs()
    << "];\n"
    << "    DerivedOutputFileMap = {\n";
  DerivedOutputMap.dump(llvm::errs(), true);
  llvm::errs()
    << "\n    };\n}\n";
}

void Job::printArguments(raw_ostream &os,
                         const llvm::opt::ArgStringList &Args) {
  interleave(Args,
             [&](const char *Arg) { escapeAndPrintString(os, Arg); },
             [&] { os << ' '; });
}

void Job::dump() const {
  printCommandLineAndEnvironment(llvm::errs());
}

void Job::printCommandLineAndEnvironment(raw_ostream &Stream,
                                         StringRef Terminator) const {
  printCommandLine(Stream, /*Terminator=*/"");
  if (!ExtraEnvironment.empty()) {
    Stream << "  #";
    for (auto &pair : ExtraEnvironment) {
      Stream << " " << pair.first << "=" << pair.second;
    }
  }
  Stream << "\n";
}

void Job::printCommandLine(raw_ostream &os, StringRef Terminator) const {
  escapeAndPrintString(os, Executable);
  os << ' ';
  printArguments(os, Arguments);
  os << Terminator;
}

void Job::printSummary(raw_ostream &os) const {
  // Deciding how to describe our inputs is a bit subtle; if we are a Job built
  // from a JobAction that itself has InputActions sources, then we collect
  // those up. Otherwise it's more correct to talk about our inputs as the
  // outputs of our input-jobs.
  SmallVector<std::string, 4> Inputs;

  for (const Action *A : getSource().getInputs())
    if (const auto *IA = dyn_cast<InputAction>(A))
      Inputs.push_back(IA->getInputArg().getValue());

  for (const Job *J : getInputs())
    for (const std::string &f : J->getOutput().getPrimaryOutputFilenames())
      Inputs.push_back(f);

  size_t limit = 3;
  size_t actual = Inputs.size();
  if (actual > limit) {
    Inputs.erase(Inputs.begin() + limit, Inputs.end());
  }

  os << "{" << getSource().getClassName() << ": ";
  interleave(getOutput().getPrimaryOutputFilenames(),
             [&](const std::string &Arg) {
               os << llvm::sys::path::filename(Arg);
             },
             [&] { os << ' '; });
  os << " <= ";
  interleave(Inputs,
             [&](const std::string &Arg) {
               os << llvm::sys::path::filename(Arg);
             },
             [&] { os << ' '; });
  if (actual > limit) {
    os << " ... " << (actual-limit) << " more";
  }
  os << "}";
}

BatchJob::BatchJob(const JobAction &Source,
                   SmallVectorImpl<const Job *> &&Inputs,
                   std::unique_ptr<CommandOutput> Output,
                   const char *Executable, llvm::opt::ArgStringList Arguments,
                   EnvironmentVector ExtraEnvironment,
                   std::vector<FilelistInfo> Infos,
                   ArrayRef<const Job *> Combined)
    : Job(Source, std::move(Inputs), std::move(Output), Executable, Arguments,
          ExtraEnvironment, Infos),
      CombinedJobs(Combined.begin(), Combined.end()) {}
