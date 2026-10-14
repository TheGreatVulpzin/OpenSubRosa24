// Put the structs and the global arrays of src/include/sr_types.h into the Ghidra program.
//
// Input: build/ghidra/types.json (python -I tools/ghidra/types_json.py). The script:
// 1. Creates each struct in the category /SubVulpes, field by field at its offset, with the
//    field names of sr_types.h. A struct that exists is replaced.
// 2. Creates the global arrays (Human[512] at human, Object[1024] at object...) and the
//    Game struct, and gives every global of the JSON its name.
// 3. Types the fread and fwrite pointers of getpaths as pointers to the import signature.
//
// The decompiler then prints "human[i].field" instead of "(&DAT_019cc048)[i * 0xad9]", and
// an address in an array index is a field access, not a number.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript ApplyTypes.java <path of types.json>
//@category SubVulpes

import java.io.FileReader;
import java.util.HashMap;
import java.util.Map;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.ByteDataType;
import ghidra.program.model.data.CategoryPath;
import ghidra.program.model.data.CharDataType;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeConflictHandler;
import ghidra.program.model.data.DataTypeManager;
import ghidra.program.model.data.FloatDataType;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.data.IntegerDataType;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.data.ShortDataType;
import ghidra.program.model.data.StructureDataType;
import ghidra.program.model.data.UnsignedIntegerDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

public class ApplyTypes extends GhidraScript {

    private final Map<String, DataType> types = new HashMap<>();
    private DataTypeManager dtm;

    private DataType base(String name) {
        switch (name) {
            case "int": return IntegerDataType.dataType;
            case "uint32_t": return UnsignedIntegerDataType.dataType;
            case "float": return FloatDataType.dataType;
            case "char": return CharDataType.dataType;
            case "uint8_t": return ByteDataType.dataType;
            case "int16_t": return ShortDataType.dataType;
            default:
                DataType t = types.get(name);
                if (t == null) throw new IllegalArgumentException("unknown type " + name);
                return t;
        }
    }

    @Override
    protected void run() throws Exception {
        String path = getScriptArgs().length > 0 ? getScriptArgs()[0] : "build/ghidra/types.json";
        JsonObject root;
        try (FileReader r = new FileReader(path)) {
            root = JsonParser.parseReader(r).getAsJsonObject();
        }
        dtm = currentProgram.getDataTypeManager();
        CategoryPath cat = new CategoryPath("/SubVulpes");

        // 1. Structs, in file order (a struct only uses the ones before it).
        for (JsonElement e : root.getAsJsonArray("structs")) {
            JsonObject s = e.getAsJsonObject();
            String name = s.get("name").getAsString();
            StructureDataType st = new StructureDataType(cat, name, s.get("size").getAsInt(), dtm);
            for (JsonElement fe : s.getAsJsonArray("fields")) {
                JsonObject f = fe.getAsJsonObject();
                DataType ft = base(f.get("type").getAsString());
                int count = f.get("count").getAsInt();
                if (count > 0)
                    ft = new ArrayDataType(ft, count, ft.getLength(), dtm);
                st.replaceAtOffset(f.get("offset").getAsInt(), ft, ft.getLength(), f.get("name").getAsString(), null);
            }
            DataType added = dtm.addDataType(st, DataTypeConflictHandler.REPLACE_HANDLER);
            types.put(name, added);
            println(String.format("struct %s: 0x%x bytes", name, added.getLength()));
        }

        // 2. Globals.
        long imageBase = currentProgram.getImageBase().getOffset();
        Listing listing = currentProgram.getListing();
        int typed = 0, named = 0;
        for (JsonElement e : root.getAsJsonArray("globals")) {
            JsonObject g = e.getAsJsonObject();
            String name = g.get("name").getAsString();
            Address at = toAddr(imageBase + g.get("rva").getAsLong());
            DataType dt = null;
            if (!g.get("type").isJsonNull()) {
                String t = g.get("type").getAsString();
                if (t.startsWith("*")) {
                    dt = importPointer(t.substring(1));
                } else {
                    dt = base(t);
                    int count = g.get("count").getAsInt();
                    if (count > 0)
                        dt = new ArrayDataType(dt, count, dt.getLength(), dtm);
                }
            }
            if (dt != null) {
                Address end = at.add(dt.getLength() - 1);
                listing.clearCodeUnits(at, end, false);
                listing.createData(at, dt);
                typed++;
            }
            Symbol old = getSymbolAt(at);
            if (old == null || !old.getName().equals(name)) {
                createLabel(at, name, true, SourceType.USER_DEFINED);
                named++;
            }
        }
        println(String.format("globals: %d typed, %d named", typed, named));
    }

    /* A pointer to the signature of an import (fread, fwrite). */
    private DataType importPointer(String name) throws Exception {
        for (Function f : currentProgram.getFunctionManager().getExternalFunctions()) {
            if (f.getName().equals(name)) {
                FunctionDefinitionDataType fd = new FunctionDefinitionDataType(f, false);
                fd.setCategoryPath(new CategoryPath("/SubVulpes"));
                fd.setName("fn_" + name);
                DataType added = dtm.addDataType(fd, DataTypeConflictHandler.REPLACE_HANDLER);
                return new PointerDataType(added, dtm);
            }
        }
        throw new IllegalArgumentException("import not found: " + name);
    }
}
