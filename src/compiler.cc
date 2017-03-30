#include "compiler.h"
#include "parser.h"
#include "ast.h"
#include "types.h"
#include "scopes.h"
#include "text-input-stream.h"
#include "simple-file-system.h"

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

/*static*/ CompiledUnitMap *
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
        error->message = src_path + " is not a dir.";
        return nullptr;
    }

    std::vector<std::string> module_dirs;
    if (sfs->GetNames(src_path.c_str(), nullptr, &module_dirs) < 0) {
        error->message = "file system error";
        return nullptr;
    }

    auto text_streams = CreateFileStreamFactory();
    Parser parser(types, text_streams, global, zone);

    auto all_units = new (zone) CompiledUnitMap(zone);
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

} // namespace mio
