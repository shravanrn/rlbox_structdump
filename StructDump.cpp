#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"

#include <memory>
#include <string>
#include <fstream>
#include <streambuf>

using namespace llvm;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

cl::OptionCategory DumpStructsCategory("Dump Structs.");

cl::opt<std::string> libName("libName", cl::Required,
                                  cl::desc("short library prefix - `jpeglib` for libjpeg"),
                                  cl::cat(DumpStructsCategory));

std::vector<std::string> structsSeen;


std::string readFileContents(std::string fileName) {
  std::ifstream t(fileName.c_str());
  std::string str;

  t.seekg(0, std::ios::end);
  str.reserve(t.tellg());
  t.seekg(0, std::ios::beg);

  str.assign((std::istreambuf_iterator<char>(t)),
              std::istreambuf_iterator<char>());
  return str;
}

void dumpFields(const RecordDecl *D) {
    for (const FieldDecl *F : D->fields()) {
      if (F->isAnonymousStructOrUnion()) {
        if (const CXXRecordDecl *R = F->getType()->getAsCXXRecordDecl()) {
          dumpFields(R);
        }
      } else {
        llvm::outs() << "\tf("
          << F->getTypeSourceInfo()->getType().getAsString() << ", "
          << F->getNameAsString() << ", FIELD_NORMAL, ##__VA_ARGS__) \\\n";
        llvm::outs() << "\tg() \\\n";
      }
    }
}

class DumpCallback : public MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {

    const RecordDecl *D = Result.Nodes.getNodeAs<RecordDecl>("x");

    std::string structName;
    if (const TypedefNameDecl *Typedef = D->getTypedefNameForAnonDecl()) {
      structName = Typedef->getName();
    } else if(const CXXRecordDecl *StructDef = dyn_cast<CXXRecordDecl>(D)) {
      structName = StructDef->getName();
    } else {
      return;
    }

    if(structName == "") {
      return;
    }

    if(std::find(structsSeen.begin(), structsSeen.end(), structName) != structsSeen.end()) {
      return;
    }

    structsSeen.push_back(structName);
    
    llvm::outs() << "#undef sandbox_fields_reflection_" << libName << "_class_" << structName << "\n";
    llvm::outs() << "#define sandbox_fields_reflection_" << libName << "_class_" << structName << "(f, g, ...) \\\n";
    dumpFields(D);
    llvm::outs() << "\n\n";
  }
};

void printStructCollection()
{
  llvm::outs() << "#define sandbox_fields_reflection_" << libName << "_allClasses(f, ...) \\\n";
  for(std::string structSeen : structsSeen){
    llvm::outs() << "\tf("
      << structSeen << ", "
      << libName << ", ##__VA_ARGS__) \\\n";
  }
  llvm::outs() << "\n\n";
}

int main(int argc, const char **argv) {

  CommonOptionsParser OptionsParser(argc, argv, DumpStructsCategory);
  const auto &Files = OptionsParser.getSourcePathList();

  DumpCallback Callback;
  MatchFinder Finder;
  Finder.addMatcher(recordDecl(isDefinition()).bind("x"), &Callback);
  std::unique_ptr<FrontendActionFactory> Factory = newFrontendActionFactory(&Finder);

  for(auto& File : Files) {
    std::string fileContents = readFileContents(File);
    if(!clang::tooling::runToolOnCode(Factory->create(), fileContents)) {
      exit(1);
    }
  }

  printStructCollection();
}
