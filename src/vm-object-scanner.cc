#include "vm-object-scanner.h"
#include "vm-objects.h"

namespace mio {

void ObjectScanner::Scan(HeapObject *ob, Callback callback) {
    if (!ob) {
        return;
    }
    callback(ob);
    if (traced_objects_.find(ob) != traced_objects_.end()) {
        return;
    }
    traced_objects_.insert(ob);

    switch (ob->GetKind()) {
        case HeapObject::kString:
            break;

        case HeapObject::kError: {
            auto err = ob->AsError();
            Scan(err->GetLinkedError(), callback);
            Scan(err->GetMessage(), callback);
        } break;

        case HeapObject::kUnion: {
            auto uni = ob->AsUnion();
            Scan(uni->GetTypeInfo(), callback);
            if (uni->GetTypeInfo()->IsObject()) {
                Scan(uni->GetObject(), callback);
            }
        } break;

        case HeapObject::kUpValue: {
            auto upval = ob->AsUpValue();
            if (upval->IsObjectValue()) {
                Scan(upval->GetObject(), callback);
            }
        } break;

        case HeapObject::kClosure: {
            auto fn = ob->AsClosure();
            Scan(fn->GetName(), callback);
            if (fn->IsOpen()) {
                return;
            }
            Scan(fn->GetFunction(), callback);
            auto buf = fn->GetUpValuesBuf();
            for (int i = 0; i < buf.n; ++i) {
                auto val = buf.z[i].val;
                Scan(val, callback);
                if (val->IsObjectValue()) {
                    Scan(val->GetObject(), callback);
                }
            }
        } break;

        case HeapObject::kNormalFunction: {
            auto fn = ob->AsNormalFunction();
            Scan(fn->GetName(), callback);
            auto buf = fn->GetConstantObjectBuf();
            for (int i = 0; i < buf.n; ++i) {
                Scan(buf.z[i], callback);
            }
        } break;

        case HeapObject::kNativeFunction: {
            auto fn = ob->AsNativeFunction();
            Scan(fn->GetName(), callback);
            Scan(fn->GetSignature(), callback);
        } break;

        case HeapObject::kHashMap: {
            auto map = ob->AsHashMap();
            Scan(map->GetKey(), callback);
            Scan(map->GetValue(), callback);
            if (map->GetKey()->IsPrimitive() && map->GetValue()->IsPrimitive()) {
                break;
            }

            for (int i = 0; i < map->GetSlotSize(); ++i) {
                auto node = map->GetSlot(i)->head;
                while (node) {
                    if (map->GetKey()->IsObject()) {
                        Scan(*static_cast<HeapObject **>(node->GetKey()), callback);
                    }
                    if (map->GetValue()->IsObject()) {
                        Scan(*static_cast<HeapObject **>(node->GetValue()), callback);
                    }
                    node = node->GetNext();
                }
            }
        } break;

        case HeapObject::kReflectionVoid:
        case HeapObject::kReflectionString:
        case HeapObject::kReflectionError:
        case HeapObject::kReflectionFloating:
        case HeapObject::kReflectionIntegral:
        case HeapObject::kReflectionUnion:
            break;

        case HeapObject::kReflectionMap: {
            auto type = ob->AsReflectionMap();
            Scan(type->GetKey(), callback);
            Scan(type->GetValue(), callback);
        } break;

        case HeapObject::kReflectionFunction: {
            auto type = ob->AsReflectionFunction();
            Scan(type->GetReturn(), callback);
            for (int i = 0; i < type->GetNumberOfParameters(); ++i) {
                Scan(type->GetParamter(i), callback);
            }
        } break;

        default:
            DLOG(FATAL) << "kind not be supported. " << ob->GetKind();
            break;
    }
}

} // namespace mio
