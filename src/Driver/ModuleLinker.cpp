//===- ModuleLinker.cpp - Module Linking Implementation --------*- C++ -*-===//
//
// Part of the BlockType Project, under the BSD 3-Clause License.
// See the LICENSE file in the project root for license information.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Driver/ModuleLinker.h"
#include "blocktype/Basic/Diagnostics.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"
#include <cstdlib>

namespace blocktype {

ModuleLinker::ModuleLinker(ModuleManager &MM, DiagnosticsEngine &D)
    : ModMgr(MM), Diags(D) {
  // 检测系统链接器
#ifdef __APPLE__
  // macOS 默认使用 clang 作为链接器驱动
  LinkerPath = "clang";
#elif defined(__linux__)
  // Linux 尝试使用 ld.gold 或 ld
  if (llvm::sys::fs::exists("/usr/bin/ld.gold")) {
    LinkerPath = "/usr/bin/ld.gold";
  } else {
    LinkerPath = "/usr/bin/ld";
  }
#else
  LinkerPath = "ld";
#endif
}

void ModuleLinker::registerObjectFile(llvm::StringRef ModuleName,
                                      llvm::StringRef ObjectPath) {
  ObjectFiles[ModuleName] = ObjectPath.str();
}

std::string ModuleLinker::getObjectFile(llvm::StringRef ModuleName) const {
  auto It = ObjectFiles.find(ModuleName);
  if (It != ObjectFiles.end()) {
    return It->second;
  }
  return "";
}

bool ModuleLinker::hasObjectFile(llvm::StringRef ModuleName) const {
  return ObjectFiles.find(ModuleName) != ObjectFiles.end();
}

void ModuleLinker::setLinkerFlags(llvm::ArrayRef<std::string> Flags) {
  LinkerFlags.clear();
  LinkerFlags.append(Flags.begin(), Flags.end());
}

bool ModuleLinker::linkModules(llvm::ArrayRef<llvm::StringRef> ModuleNames,
                               llvm::StringRef OutputPath) {
  // 1. 收集目标文件
  llvm::SmallVector<std::string, 16> ObjFiles =
      collectObjectFiles(ModuleNames);

  if (ObjFiles.empty()) {
    Diags.report(SourceLocation{}, DiagID::err_not_implemented,
                 "no object files to link");
    return false;
  }

  // 2. 调用链接器
  return invokeLinker(ObjFiles, OutputPath);
}

bool ModuleLinker::linkModuleWithDependencies(llvm::StringRef ModuleName,
                                              llvm::StringRef OutputPath) {
  // 1. 获取模块及其依赖
  llvm::SmallVector<ModuleInfo *, 8> Deps =
      ModMgr.getModuleDependencies(ModuleName);

  // 2. 构建模块名列表
  llvm::SmallVector<llvm::StringRef, 8> ModuleNames;
  ModuleNames.push_back(ModuleName);
  for (ModuleInfo *MI : Deps) {
    ModuleNames.push_back(MI->Name);
  }

  // 3. 链接
  return linkModules(ModuleNames, OutputPath);
}

llvm::SmallVector<std::string, 16>
ModuleLinker::collectObjectFiles(llvm::ArrayRef<llvm::StringRef> ModuleNames) {
  llvm::SmallVector<std::string, 16> Result;

  // 使用依赖图确保正确的链接顺序
  for (llvm::StringRef ModName : ModuleNames) {
    // 检查模块是否已注册目标文件
    std::string ObjFile = getObjectFile(ModName);
    if (ObjFile.empty()) {
      // 尝试推断目标文件路径
      // 假设目标文件在当前目录，名为 <module>.o
      ObjFile = ModName.str() + ".o";
      if (!llvm::sys::fs::exists(ObjFile)) {
        Diags.report(SourceLocation{}, DiagID::err_pp_file_not_found, ObjFile);
        continue;
      }
    }

    // 检查文件是否存在
    if (!llvm::sys::fs::exists(ObjFile)) {
      Diags.report(SourceLocation{}, DiagID::err_pp_file_not_found, ObjFile);
      continue;
    }

    // 避免重复添加
    if (std::find(Result.begin(), Result.end(), ObjFile) == Result.end()) {
      Result.push_back(ObjFile);
    }
  }

  return Result;
}

bool ModuleLinker::invokeLinker(llvm::ArrayRef<std::string> ObjectFiles,
                                llvm::StringRef OutputPath) {
  // 构建命令行
  std::string Command = buildLinkerCommand(ObjectFiles, OutputPath);

  // 执行链接器
  int ExitCode = std::system(Command.c_str());

  if (ExitCode != 0) {
    Diags.report(SourceLocation{}, DiagID::err_not_implemented,
                 "linker failed with exit code " + std::to_string(ExitCode));
    return false;
  }

  return true;
}

std::string
ModuleLinker::buildLinkerCommand(llvm::ArrayRef<std::string> ObjectFiles,
                                 llvm::StringRef OutputPath) {
  std::string Cmd;

  // 链接器路径
  Cmd += LinkerPath;

  // 根据输出类型添加标志
  switch (OutType) {
  case OutputType::Executable:
    // 可执行文件：-o <output>
    Cmd += " -o " + OutputPath.str();
    break;
  case OutputType::SharedLibrary:
    // 共享库：-shared -o <output>
    Cmd += " -shared -o " + OutputPath.str();
    break;
  case OutputType::StaticLibrary:
    // 静态库：使用 ar 命令
    // 注意：这里简化处理，实际应该使用 ar
    Cmd = "ar rcs " + OutputPath.str();
    break;
  }

  // 添加用户指定的链接标志
  for (const std::string &Flag : LinkerFlags) {
    Cmd += " " + Flag;
  }

  // 添加目标文件
  for (const std::string &ObjFile : ObjectFiles) {
    Cmd += " " + ObjFile;
  }

  return Cmd;
}

} // namespace blocktype
