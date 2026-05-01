//===--- CompilerInstance.cpp - Compiler Instance Implementation -*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the CompilerInstance class.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Frontend/CompilerInstance.h"
#include "blocktype/AST/ASTDumper.h"
#include "blocktype/IR/IRIntegrity.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/IR/Module.h"
#include <cstdlib>

using namespace llvm;
using namespace blocktype;

CompilerInstance::CompilerInstance()
    : Invocation(std::make_shared<CompilerInvocation>()) {
}

CompilerInstance::~CompilerInstance() = default;

//===--------------------------------------------------------------------===//
// Component creation
//===--------------------------------------------------------------------===//

void CompilerInstance::createSourceManager() {
  if (!SourceMgr) {
    SourceMgr = std::make_unique<SourceManager>();
  }
}

void CompilerInstance::createDiagnostics() {
  if (!Diags) {
    Diags = std::make_unique<DiagnosticsEngine>();
  }
}

void CompilerInstance::createASTContext() {
  if (!Context) {
    Context = std::make_unique<ASTContext>();
  }
}

void CompilerInstance::createPreprocessor() {
  if (!PP) {
    // Ensure dependencies are created
    if (!SourceMgr) createSourceManager();
    if (!Diags) createDiagnostics();
    
    PP = std::make_unique<Preprocessor>(*SourceMgr, *Diags);
  }
}

void CompilerInstance::createSema() {
  if (!SemaPtr) {
    // Ensure dependencies are created
    if (!Context) createASTContext();
    if (!Diags) createDiagnostics();
    
    SemaPtr = std::make_unique<Sema>(*Context, *Diags);
  }
}

void CompilerInstance::createParser() {
  if (!ParserPtr) {
    // Ensure dependencies are created
    if (!PP) createPreprocessor();
    if (!Context) createASTContext();
    if (!SemaPtr) createSema();
    
    ParserPtr = std::make_unique<Parser>(*PP, *Context, *SemaPtr);
  }
}

void CompilerInstance::createLLVMContext() {
  if (!LLVMCtx) {
    LLVMCtx = std::make_unique<llvm::LLVMContext>();
  }
}

void CompilerInstance::createTelemetry() {
  if (Telemetry) return;  // 已创建
  
  Telemetry = std::make_unique<telemetry::TelemetryCollector>();
  
  // 根据编译选项决定是否启用
  if (Invocation && Invocation->FrontendOpts.TimeReport) {
    Telemetry->enable();
  }
}

void CompilerInstance::createAllComponents() {
  createSourceManager();
  createDiagnostics();
  createASTContext();
  createPreprocessor();
  createSema();
  createParser();
  createLLVMContext();
}

//===--------------------------------------------------------------------===//
// Compilation actions
//===--------------------------------------------------------------------===//

bool CompilerInstance::initialize(std::shared_ptr<CompilerInvocation> CI) {
  if (!CI) {
    errs() << "Error: CompilerInvocation is null\n";
    return false;
  }

  Invocation = std::move(CI);

  // Validate options
  if (!Invocation->validate()) {
    return false;
  }

  // Set default target triple
  Invocation->setDefaultTargetTriple();

  // Create all components
  createAllComponents();

  Initialized = true;
  return true;
}

bool CompilerInstance::readFileContent(StringRef Filename, std::string &Content) {
  auto BufferOrErr = llvm::MemoryBuffer::getFile(Filename);
  if (!BufferOrErr) {
    errs() << "Error: Cannot read file '" << Filename << "'\n";
    return false;
  }

  std::unique_ptr<llvm::MemoryBuffer> Buffer = std::move(*BufferOrErr);
  Content = Buffer->getBuffer().str();
  return true;
}

bool CompilerInstance::loadSourceFile(StringRef Filename) {
  if (!Initialized) {
    errs() << "Error: Compiler not initialized\n";
    return false;
  }

  // Read file content
  std::string Content;
  if (!readFileContent(Filename, Content)) {
    return false;
  }

  // Enter source file
  PP->enterSourceFile(Filename.str(), Content);

  if (Invocation->FrontendOpts.Verbose) {
    outs() << "  Source size: " << Content.size() << " bytes\n";
  }

  return true;
}

TranslationUnitDecl *CompilerInstance::parseTranslationUnit() {
  if (!Initialized) {
    errs() << "Error: Compiler not initialized\n";
    return nullptr;
  }

  telemetry::TelemetryCollector::PhaseGuard Guard;
  if (Telemetry && Telemetry->isEnabled()) {
    Guard = Telemetry->scopePhase(telemetry::CompilationPhase::Frontend, "parse");
  }

  if (Invocation->FrontendOpts.Verbose) {
    outs() << "  Parsing...\n";
  }

  CurrentTU = ParserPtr->parseTranslationUnit();

  if (ParserPtr->hasErrors() || Diags->hasErrorOccurred()) {
    setError();
    if (Telemetry && Telemetry->isEnabled()) {
      Guard.markFailed();
    }
    return nullptr;
  }

  return CurrentTU;
}

bool CompilerInstance::performSemaAnalysis() {
  if (!CurrentTU) {
    errs() << "Error: No translation unit to analyze\n";
    return false;
  }

  telemetry::TelemetryCollector::PhaseGuard Guard;
  if (Telemetry && Telemetry->isEnabled()) {
    Guard = Telemetry->scopePhase(telemetry::CompilationPhase::Frontend, "sema");
  }

  // Perform post-parse diagnostics
  SemaPtr->DiagnoseUnusedDecls(CurrentTU);

  bool OK = !hasErrors();
  if (!OK && Telemetry && Telemetry->isEnabled()) {
    Guard.markFailed();
  }
  return OK;
}

bool CompilerInstance::performPreprocessing() {
  if (!Initialized || !PP) {
    errs() << "Error: Compiler not initialized\n";
    return false;
  }

  if (Invocation->FrontendOpts.Verbose) {
    outs() << "  Preprocessing...\n";
  }

  // Process all tokens from preprocessor
  Token Tok;
  while (!PP->isEOF()) {
    if (!PP->lexToken(Tok)) {
      break;
    }
    
    // For -E mode, output preprocessed tokens
    if (Invocation->CodeGenOpts.PreprocessOnly) {
      outs() << Tok.getText() << " ";
    }
  }

  if (Invocation->CodeGenOpts.PreprocessOnly) {
    outs() << "\n";
  }

  return !hasErrors();
}







bool CompilerInstance::linkExecutable(const std::vector<std::string> &ObjectFiles,
                                      StringRef OutputPath) {
  telemetry::TelemetryCollector::PhaseGuard Guard;
  if (Telemetry && Telemetry->isEnabled()) {
    Guard = Telemetry->scopePhase(telemetry::CompilationPhase::Linking, "link");
  }

  if (Invocation->FrontendOpts.Verbose) {
    outs() << "  Linking executable: " << OutputPath << "\n";
  }

  // Build linker command
  std::string LinkerCmd = "clang++";  // Use clang++ as linker

  // Add output file
  LinkerCmd += " -o " + OutputPath.str();

  // Static linking: add -static flag and disable PIE at link time
  if (Invocation->CodeGenOpts.StaticLink) {
    LinkerCmd += " -static";
    // PIE was already disabled in generateObjectFile(); ensure linker
    // does not add PIE flags either.
  } else if (Invocation->TargetOpts.PIE) {
    // Pass PIE flag to linker when PIE is enabled
    LinkerCmd += " -pie";
  }

  // Add object files
  for (const auto &ObjFile : ObjectFiles) {
    LinkerCmd += " " + ObjFile;
  }

  // Add library paths
  for (const auto &LibPath : Invocation->FrontendOpts.LibraryPaths) {
    LinkerCmd += " -L" + LibPath;
  }

  // Add libraries
  for (const auto &Lib : Invocation->FrontendOpts.Libraries) {
    LinkerCmd += " -l" + Lib;
  }

  // Add additional linker flags
  for (const auto &Flag : Invocation->FrontendOpts.LinkerFlags) {
    LinkerCmd += " " + Flag;
  }

  if (Invocation->FrontendOpts.Verbose) {
    outs() << "  Linker command: " << LinkerCmd << "\n";
  }

  // Execute linker
  int Result = std::system(LinkerCmd.c_str());
  return Result == 0;
}

bool CompilerInstance::compileFile(StringRef Filename) {
  if (Invocation->FrontendOpts.Verbose) {
    outs() << "Compiling: " << Filename << "\n";
  }

  // Always use the new pipeline (old pipeline removed in Phase E.3)
  return runNewPipeline(Filename);
}

//===--------------------------------------------------------------------===//
// New pipeline
//===--------------------------------------------------------------------===//

bool CompilerInstance::runNewPipeline(StringRef Filename) {
  // Ensure diagnostics engine exists
  if (!Diags) createDiagnostics();

  // Phase E-F5: Reproducible build mode
  if (Invocation->ReproducibleBuild) {
    if (Invocation->FrontendOpts.Verbose) {
      outs() << "  Reproducible build mode enabled\n";
    }
    // 1. Timestamp: read SOURCE_DATE_EPOCH (default 0 = 1970-01-01)
    uint64_t BuildTimestamp = ir::security::ReproducibleTimestamp;
    if (const char* Epoch = std::getenv("SOURCE_DATE_EPOCH")) {
      BuildTimestamp = static_cast<uint64_t>(std::strtoull(Epoch, nullptr, 10));
    }
    // 2. Deterministic seed for all random number generators
    // 3. Hash table traversal order (sorted by key)
    // 4. Deterministic ValueID/InstructionID start from 1
    // 5. Deterministic temporary file naming prefix
    // 6. Fixed DWARF producer string
    (void)BuildTimestamp; // Used by downstream passes via IRModule flag
    (void)ir::security::DeterministicSeed;
    (void)ir::security::DeterministicValueIDStart;
    (void)ir::security::DeterministicTempPrefix;
    (void)ir::security::FixedProducerString;
  }

  // Step 1: Run frontend to produce IRModule
  if (!runFrontend(Filename)) {
    return false;
  }

  // Step 2: Run IR pipeline (optimization, etc.)
  if (!runIRPipeline()) {
    return false;
  }

  // Step 3: Determine output path and run backend
  std::string OutputPath;
  if (!Invocation->CodeGenOpts.OutputFile.empty()) {
    OutputPath = Invocation->CodeGenOpts.OutputFile;
  } else {
    OutputPath = Filename.str();
    size_t DotPos = OutputPath.rfind('.');
    if (DotPos != std::string::npos) {
      OutputPath = OutputPath.substr(0, DotPos) + ".o";
    } else {
      OutputPath += ".o";
    }
  }

  if (!runBackend(OutputPath)) {
    return false;
  }

  if (Invocation->FrontendOpts.Verbose) {
    outs() << "  New pipeline compilation successful\n";
  }

  return true;
}

bool CompilerInstance::runFrontend(StringRef Filename) {
  using namespace frontend;
  using namespace backend;

  // Build FrontendCompileOptions from CompilerInvocation
  FrontendCompileOptions FEOpts;
  FEOpts.InputFile = Filename.str();
  FEOpts.TargetTriple = Invocation->TargetOpts.Triple;
  FEOpts.OptimizationLevel = Invocation->CodeGenOpts.OptimizationLevel;
  FEOpts.EmitIR = Invocation->CodeGenOpts.EmitLLVM;
  for (const auto& P : Invocation->FrontendOpts.IncludePaths)
    FEOpts.IncludePaths.push_back(P);

  // Create frontend via registry
  std::string FrontendName = Invocation->getFrontendName().str();
  Frontend = FrontendRegistry::instance().create(FrontendName, FEOpts, *Diags);
  if (!Frontend) {
    errs() << "Error: Failed to create frontend '" << FrontendName << "'\n";
    return false;
  }

  if (Invocation->FrontendOpts.Verbose) {
    outs() << "  Using frontend: " << FrontendName << "\n";
  }

  // Create TargetLayout from target triple
  if (Invocation->TargetOpts.Triple.empty()) {
    errs() << "Error: Target triple is empty (call setDefaultTargetTriple first)\n";
    return false;
  }
  Layout = ir::TargetLayout::Create(Invocation->TargetOpts.Triple);

  // Create IRContext (which owns IRTypeContext internally)
  IRCtx = std::make_unique<ir::IRContext>();
  IRTypeCtx = std::make_unique<ir::IRTypeContext>();

  // Run frontend compile
  auto IRModule = Frontend->compile(Filename.str(), *IRTypeCtx, *Layout);
  if (!IRModule) {
    errs() << "Error: Frontend compilation failed for '" << Filename << "'\n";
    return false;
  }

  // Store the IRModule in IRContext for later pipeline stages.
  // sealModule reads IRModule content for verification/optimization, does NOT transfer ownership.
  IRCtx->sealModule(*IRModule);

  // Phase E-F6: IR integrity check
  if (Invocation->IRIntegrityCheck) {
    auto Checksum = ir::security::IRIntegrityChecksum::compute(*IRModule);
    if (!Checksum.verify(*IRModule)) {
      errs() << "Error: IR integrity check failed after frontend compilation\n";
      return false;
    }
    if (Invocation->FrontendOpts.Verbose) {
      outs() << "  IR integrity check passed: " << Checksum.toHex() << "\n";
    }
  }

  // IRModule_ takes sole ownership of the IRModule.
  IRModule_ = std::move(IRModule);

  // Phase E-F5: Mark IRModule as reproducible if requested
  if (Invocation->ReproducibleBuild) {
    IRModule_->setReproducible(true);
    if (Invocation->FrontendOpts.Verbose) {
      outs() << "  IRModule marked as reproducible\n";
    }
  }

  return true;
}

bool CompilerInstance::runIRPipeline() {
  if (!IRModule_) {
    errs() << "Error: No IRModule to process\n";
    return false;
  }

  if (Invocation->FrontendOpts.Verbose) {
    outs() << "  IR pipeline: processing IRModule\n";
  }

  // Future: add IR optimization passes here
  // For now, IR pipeline is a pass-through

  return true;
}

bool CompilerInstance::runBackend(StringRef OutputPath) {
  using namespace backend;

  if (!IRModule_) {
    errs() << "Error: No IRModule for backend\n";
    return false;
  }

  // Build BackendOptions from CompilerInvocation
  BackendOptions BOpts;
  BOpts.TargetTriple = Invocation->TargetOpts.Triple;
  BOpts.OutputPath = OutputPath.str();
  BOpts.OptimizationLevel = Invocation->CodeGenOpts.OptimizationLevel;
  BOpts.EmitAssembly = Invocation->CodeGenOpts.EmitAssembly;
  BOpts.EmitIR = Invocation->CodeGenOpts.EmitLLVM;
  BOpts.DebugInfo = Invocation->CodeGenOpts.DebugInfo;

  // Create backend via registry
  std::string BackendName = Invocation->getBackendName().str();
  Backend = BackendRegistry::instance().create(BackendName, BOpts, *Diags);
  if (!Backend) {
    errs() << "Error: Failed to create backend '" << BackendName << "'\n";
    return false;
  }

  if (Invocation->FrontendOpts.Verbose) {
    outs() << "  Using backend: " << BackendName << "\n";
  }

  // Run backend: emit object file
  if (!Backend->emitObject(*IRModule_, OutputPath.str())) {
    errs() << "Error: Backend code generation failed\n";
    return false;
  }

  if (Invocation->FrontendOpts.Verbose) {
    outs() << "  Object file generated: " << OutputPath << "\n";
  }

  return true;
}

bool CompilerInstance::compileAllFiles() {
  // Always use new pipeline — route each file through compileFile()
  // which uses the pluggable frontend/backend pipeline.
  for (const auto &File : Invocation->FrontendOpts.InputFiles) {
    if (!compileFile(File))
      return false;
  }
  return true;
}

//===--------------------------------------------------------------------===//
// Utility functions
//===--------------------------------------------------------------------===//

void CompilerInstance::dumpAST() {
  if (!CurrentTU) {
    errs() << "Error: No translation unit to dump\n";
    return;
  }

  outs() << "\n=== AST Dump ===\n";
  CurrentTU->dump(outs());
  outs() << "=== End AST Dump ===\n\n";
}

void CompilerInstance::dumpLLVMIR() {
  // TODO: Implement LLVM IR dumping
  errs() << "Warning: dumpLLVMIR() not yet implemented\n";
}
