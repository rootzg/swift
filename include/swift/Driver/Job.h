//===--- Job.h - Commands to Execute ----------------------------*- C++ -*-===//
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

#ifndef SWIFT_DRIVER_JOB_H
#define SWIFT_DRIVER_JOB_H

#include "swift/Basic/LLVM.h"
#include "swift/Driver/Action.h"
#include "swift/Driver/OutputFileMap.h"
#include "swift/Driver/Types.h"
#include "swift/Driver/Util.h"
#include "llvm/Option/Option.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Chrono.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>

namespace swift {
namespace driver {

class Job;
class JobAction;

class CommandOutput {

  /// A CommandOutput designates one type of output as primary, though there
  /// may be multiple outputs of that type.
  types::ID PrimaryOutputType;

  /// The set of input filenames for this \c CommandOutput; combined with \c
  /// DerivedOutputMap, specifies a set of output filenames (of which one -- the
  /// one of type \c PrimaryOutputType) is the primary output filename.
  SmallVector<StringRef, 1> InputFiles;

  /// All CommandOutputs in a Compilation share the same \c
  /// DerivedOutputMap. This is computed both from any user-provided input file
  /// map, and any inference steps.
  OutputFileMap &DerivedOutputMap;

  // If there is an entry in the DerivedOutputMap for a given (Input, Type)
  // pair, return a nonempty StringRef, otherwise return an empty StringRef.
  StringRef
  getOutputForInputAndType(StringRef Input, types::ID Type) const;

  /// Add an entry to the \c DerivedOutputMap if it doesn't exist. If an entry
  /// already exists, assert that it has the same value as the call was
  /// attempting to add.
  void checkConflictAndAdd(StringRef OutputFile,
                           StringRef InputFile,
                           types::ID type);

public:
  CommandOutput(types::ID PrimaryOutputType, OutputFileMap &Derived)
    : PrimaryOutputType(PrimaryOutputType),
      DerivedOutputMap(Derived) { }

  /// Return the primary output type for this CommandOutput.
  types::ID getPrimaryOutputType() const { return PrimaryOutputType; }

  /// Add a primary input file and associate a given primary output
  /// file with it (of type \c getPrimaryOutputType())
  void addPrimaryOutput(StringRef OutputFile, StringRef InputFile) {
    InputFiles.push_back(InputFile);
    checkConflictAndAdd(OutputFile, InputFile, PrimaryOutputType);
  }

  /// Assuming (and asserting) that there is only one primary input file,
  /// return the primary output file associated with it. Note that the
  /// returned StringRef may be invalidated by subsequent mutations to
  /// the \c CommandOutput.
  StringRef getPrimaryOutputFilename() const {
    assert(InputFiles.size() == 1);
    return getOutputForInputAndType(InputFiles[0], PrimaryOutputType);
  }

  /// Return a all of the outputs of type \c getPrimaryOutputType() associated
  /// with a primary input. Note that the returned \c StringRef vector may be
  /// invalidated by subsequent mutations to the \c CommandOutput.
  SmallVector<StringRef, 16> getPrimaryOutputFilenames() const {
    SmallVector<StringRef, 16> V;
    for (auto I : InputFiles) {
      auto Out = getOutputForInputAndType(I, PrimaryOutputType);
      if (!Out.empty())
        V.push_back(Out);
    }
    return V;
  }

  /// Assuming (and asserting) that there is only one primary input, associate
  /// an additional output named \p OutputFilename of type \p type with that
  /// primary input.
  void setAdditionalOutputForType(types::ID type, StringRef OutputFilename);

  /// Assuming (and asserting) that there is only one primary input, return the
  /// additional output of type \p type associated with that primary input.
  StringRef getAdditionalOutputForType(types::ID type) const;

  /// Return the primary input numbered by \p Index.
  StringRef getBaseInput(int Index) const { return InputFiles[Index]; }

  void dump() const LLVM_ATTRIBUTE_USED;
};

class Job {
public:
  enum class Condition {
    Always,
    RunWithoutCascading,
    CheckDependencies,
    NewlyAdded
  };

  using EnvironmentVector = std::vector<std::pair<const char *, const char *>>;

private:
  /// The action which caused the creation of this Job, and the conditions
  /// under which it must be run.
  llvm::PointerIntPair<const JobAction *, 2, Condition> SourceAndCondition;

  /// The list of other Jobs which are inputs to this Job.
  SmallVector<const Job *, 4> Inputs;

  /// The output of this command.
  std::unique_ptr<CommandOutput> Output;

  /// The executable to run.
  const char *Executable;

  /// The list of program arguments (not including the implicit first argument,
  /// which will be the Executable).
  ///
  /// These argument strings must be kept alive as long as the Job is alive.
  llvm::opt::ArgStringList Arguments;

  /// Additional variables to set in the process environment when running.
  ///
  /// These strings must be kept alive as long as the Job is alive.
  EnvironmentVector ExtraEnvironment;

  /// Whether the job wants a list of input or output files created.
  std::vector<FilelistInfo> FilelistFileInfos;

  /// The modification time of the main input file, if any.
  llvm::sys::TimePoint<> InputModTime = llvm::sys::TimePoint<>::max();

public:
  Job(const JobAction &Source,
      SmallVectorImpl<const Job *> &&Inputs,
      std::unique_ptr<CommandOutput> Output,
      const char *Executable,
      llvm::opt::ArgStringList Arguments,
      EnvironmentVector ExtraEnvironment = {},
      std::vector<FilelistInfo> Infos = {})
      : SourceAndCondition(&Source, Condition::Always),
        Inputs(std::move(Inputs)), Output(std::move(Output)),
        Executable(Executable), Arguments(std::move(Arguments)),
        ExtraEnvironment(std::move(ExtraEnvironment)),
        FilelistFileInfos(std::move(Infos)) {}

  const JobAction &getSource() const {
    return *SourceAndCondition.getPointer();
  }

  const char *getExecutable() const { return Executable; }
  const llvm::opt::ArgStringList &getArguments() const { return Arguments; }
  ArrayRef<FilelistInfo> getFilelistInfos() const { return FilelistFileInfos; }

  ArrayRef<const Job *> getInputs() const { return Inputs; }
  const CommandOutput &getOutput() const { return *Output; }

  Condition getCondition() const {
    return SourceAndCondition.getInt();
  }
  void setCondition(Condition Cond) {
    SourceAndCondition.setInt(Cond);
  }

  void setInputModTime(llvm::sys::TimePoint<> time) {
    InputModTime = time;
  }

  llvm::sys::TimePoint<> getInputModTime() const {
    return InputModTime;
  }

  ArrayRef<std::pair<const char *, const char *>> getExtraEnvironment() const {
    return ExtraEnvironment;
  }

  /// Print the command line for this Job to the given \p stream,
  /// terminating output with the given \p terminator.
  void printCommandLine(raw_ostream &Stream, StringRef Terminator = "\n") const;

  /// Print a short summary of this Job to the given \p Stream.
  void printSummary(raw_ostream &Stream) const;

  /// Print the command line for this Job to the given \p stream,
  /// and include any extra environment variables that will be set.
  ///
  /// \sa printCommandLine
  void printCommandLineAndEnvironment(raw_ostream &Stream,
                                      StringRef Terminator = "\n") const;

  void dump() const LLVM_ATTRIBUTE_USED;

  static void printArguments(raw_ostream &Stream,
                             const llvm::opt::ArgStringList &Args);
};

/// A BatchJob comprises a _set_ of jobs, each of which is sufficiently similar
/// to the others that the whole set can be combined into a single subprocess
/// (and thus run potentially more-efficiently than running each Job in the set
/// individually).
///
/// Not all Jobs can be combined into a BatchJob: at present, only those Jobs
/// that come from CompileJobActions, and which otherwise have the exact same
/// input file list and arguments as one another, aside from their primary-file.
/// See ToolChain::jobsAreBatchCombinable for details.

class BatchJob : public Job {
  SmallVector<const Job *, 4> CombinedJobs;
public:
  BatchJob(const JobAction &Source, SmallVectorImpl<const Job *> &&Inputs,
           std::unique_ptr<CommandOutput> Output, const char *Executable,
           llvm::opt::ArgStringList Arguments,
           EnvironmentVector ExtraEnvironment,
           std::vector<FilelistInfo> Infos,
           ArrayRef<const Job *> Combined);

  ArrayRef<const Job*> getCombinedJobs() const {
    return CombinedJobs;
  }
};

} // end namespace driver
} // end namespace swift

#endif
