#include "compiler.h"
#include "bitcode-emitter.h"
#include "checker.h"
#include "parser.h"
#include "ast.h"
#include "types.h"
#include "scopes.h"
#include "text-input-stream.h"
#include "text-output-stream.h"
#include "simple-file-system.h"
#include <unordered_set>

namespace mio {

ParsingError::ParsingError()
    : line(0)
    , column(0)
    , position(0)
    , message("ok") {
}

/*static*/ ParsingError ParsingError::NoError() {
    return ParsingError();
}

std::string ParsingError::ToString() const {
    std::unique_ptr<char[]> buf(new char[1024]);

    if (!file_name.empty()) {
        snprintf(buf.get(), 1024, "%s[%d:%d] %s",
                 file_name.c_str(),
                 line, column,
                 message.c_str());
    } else {
        snprintf(buf.get(), 1024, "%s", message.c_str());
    }
    
    return std::string(buf.get());
}

// Source File Structure:
//
// project_dir/
//             src/
//                 main/
//                      1.mio
//                      2.mio
//                 foo/
//                      1.mio
//                      2.mio
//                 bar/
//

/*static*/ ParsedUnitMap *
Compiler::ParseProject(const char *project_dir,
                       SimpleFileSystem *sfs,
                       TypeFactory *types,
                       Scope *global,
                       Zone *zone,
                       ParsingError *error) {

    std::string base_path(DCHECK_NOTNULL(project_dir));
    std::string src_path(base_path);
    src_path.append("/src");

    if (!sfs->IsDir(src_path.c_str())) {
        error->message = TextOutputStream::sprintf("project dir: %s is not a dir.", project_dir);
        return nullptr;
    }

    std::vector<std::string> module_dirs;
    if (sfs->GetNames(src_path.c_str(), nullptr, &module_dirs) < 0) {
        error->message = "file system error";
        return nullptr;
    }

    auto text_streams = CreateFileStreamFactory();
    Parser parser(types, text_streams, global, zone);

    auto all_units = new (zone) ParsedUnitMap(zone);
    std::vector<std::string> names;
    for (const auto &dir_name : module_dirs) {
        std::string module_path(src_path);
        module_path.append("/");
        module_path.append(dir_name);


        names.clear();
        if (sfs->GetNames(module_path.c_str(), ".mio", &names) < 0) {
            error->message = "file system error";
            return nullptr;
        }

        for (const auto unit_name : names) {
            std::string unit_path(module_path);
            unit_path.append("/");
            unit_path.append(unit_name);

            parser.SwitchInputStream(unit_path);

            bool ok = true;
            auto pkg = parser.ParsePackageImporter(&ok);
            if (!ok) {
                *error = parser.last_error();
                return nullptr;
            }
            auto stmts = new (zone) ZoneVector<Statement *>(zone);
            stmts->Add(pkg);

            auto module = global->FindInnerScopeOrNull(pkg->package_name());
            if (!module) {
                module = new (zone) Scope(global, MODULE_SCOPE, zone);
                module->set_name(pkg->package_name());
            }
            parser.EnterScope(module);
            parser.EnterScope(unit_path, UNIT_SCOPE);

            for (;;) {
                ok = true;
                auto stmt = parser.ParseStatement(&ok);
                if (!ok) {
                    *error = parser.last_error();
                    return nullptr;
                }
                if (!stmt) {
                    break;
                }
                stmts->Add(stmt);
            }

            parser.LeaveScope();
            parser.LeaveScope();

            all_units->Put(RawString::Create(unit_path, zone), stmts);
        }
    }
    return all_units;
}

int RecursiveParseModule(const std::string &module_name,
                         const std::vector<std::string> &builtin_modules,
                         const std::vector<std::string> &search_paths,
                         std::unordered_set<std::string> *unique_parsed,
                         SimpleFileSystem *sfs,
                         TypeFactory *types,
                         ParsedUnitMap *all_unit,
                         Scope *global,
                         Zone *zone,
                         ParsingError *error) {
    std::string module_path = sfs->Search(module_name.c_str(), search_paths);
    if (module_path.empty()) {
        error->message = TextOutputStream::sprintf("module: %s not found!", module_name.c_str());
        return -1;
    }

    std::vector<std::string> names;
    if (sfs->GetNames(module_path.c_str(), ".mio", &names) < 0) {
        error->message = "file system error";
        return -1;
    }

    std::unique_ptr<TextStreamFactory> text_streams(CreateFileStreamFactory());
    Parser parser(types, text_streams.get(), global, zone);

    for (const auto unit_name : names) {
        std::string unit_path(module_path);
        unit_path.append("/");
        unit_path.append(unit_name);

        parser.SwitchInputStream(unit_path);

        bool ok = true;
        auto pkg = parser.ParsePackageImporter(&ok);
        if (!ok) {
            *error = parser.last_error();
            return -1;
        }
        if (module_name.compare(pkg->package_name()->ToString()) != 0) {
            error->message = TextOutputStream::sprintf("path: %s has other module name %s, should be %s",
                                                       module_path.c_str(),
                                                       pkg->package_name()->c_str(),
                                                       module_name.c_str());
            return -1;
        }
        for (const auto &builtin : builtin_modules) {
            if (builtin.compare(module_name) == 0) {
                continue;
            }
            pkg->mutable_import_list()->Put(RawString::Create(builtin, zone),
                                            RawString::kEmpty);
        }
        PackageImporter::ImportList::Iterator iter(pkg->mutable_import_list());
        for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
            if (unique_parsed->find(iter->key()->ToString()) != unique_parsed->end()) {
                continue;
            }
            auto rv = RecursiveParseModule(iter->key()->ToString(),
                    builtin_modules, search_paths, unique_parsed, sfs, types,
                    all_unit, global, zone, error);
            if (rv < 0) {
                return -1;
            }
        }

        auto stmts = new (zone) ZoneVector<Statement *>(zone);
        stmts->Add(pkg);

        auto module = global->FindInnerScopeOrNull(pkg->package_name());
        if (!module) {
            module = new (zone) Scope(global, MODULE_SCOPE, zone);
            module->set_name(pkg->package_name());
        }
        parser.EnterScope(module);
        parser.EnterScope(unit_path, UNIT_SCOPE);

        for (;;) {
            ok = true;
            auto stmt = parser.ParseStatement(&ok);
            if (!ok) {
                *error = parser.last_error();
                return -1;
            }
            if (!stmt) {
                break;
            }
            stmts->Add(stmt);
        }

        parser.LeaveScope();
        parser.LeaveScope();

        all_unit->Put(RawString::Create(unit_path, zone), stmts);
    }
    unique_parsed->insert(module_name);
    return 0;
}

    /*static*/
ParsedUnitMap *Compiler::ParseProject(const char *project_dir,
                                      const char *entry_module,
                                      const std::vector<std::string> &builtin_modules,
                                      const std::vector<std::string> &search_path,
                                      SimpleFileSystem *sfs,
                                      TypeFactory *types,
                                      Scope *global,
                                      Zone *zone,
                                      ParsingError *error) {
    std::string base_path(DCHECK_NOTNULL(project_dir));
    std::string src_path(base_path);
    src_path.append("/src");

    if (!sfs->IsDir(src_path.c_str())) {
        error->message = TextOutputStream::sprintf("project dir: %s is not a dir.", project_dir);
        return nullptr;
    }

    std::unordered_set<std::string> unique_parsed;

    auto all_units = new (zone) ParsedUnitMap(zone);
    std::vector<std::string> paths(search_path);
    paths.push_back(src_path);
    auto rv = RecursiveParseModule(entry_module, builtin_modules, paths,
                                   &unique_parsed, sfs, types, all_units,
                                   global, zone, error);
    return rv < 0 ? nullptr : all_units;
}

/*static*/ ParsedModuleMap *Compiler::Check(ParsedUnitMap *all_units,
                                              TypeFactory *types,
                                              Scope *global,
                                              Zone *zone,
                                              ParsingError *error) {
    Checker checker(types, all_units, global, zone);
    if (!checker.Run()) {
        *error = checker.last_error();
        return nullptr;
    }

    return checker.all_modules();
}

/*static*/ void Compiler::AstEmitToBitCode(ParsedModuleMap *all_modules,
                                           MemorySegment *p_global,
                                           MemorySegment *o_global,
                                           TypeFactory *types,
                                           ObjectFactory *object_factory,
                                           ObjectExtraFactory *extra_factory,
                                           FunctionRegister *function_register,
                                           MIOHashMapStub<Handle<MIOString>, mio_i32_t> *all_var,
                                           MIOArrayStub<Handle<MIOReflectionType>> *all_type,
                                           std::unordered_map<int64_t, int> *type_id2index,
                                           CompiledInfo *info,
                                           int next_function_id) {
    BitCodeEmitter emitter(p_global,
                           o_global,
                           types,
                           object_factory,
                           extra_factory,
                           function_register,
                           all_var,
                           all_type,
                           type_id2index,
                           next_function_id);
    emitter.Init();
    emitter.Run(all_modules, info);
}

} // namespace mio
