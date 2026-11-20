// Ghidra headless post-script: decompile every function to <arg0> (one C file).
//   analyzeHeadless <proj dir> <name> -import scscript.dll -postScript DecompAll.java <out.c> -scriptPath <this dir> -deleteProject
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class DecompAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        String out = getScriptArgs()[0];
        if (getScriptArgs().length > 1) {   // functions.txt: "<hex start> <name>" -> make sure each is a function
            try (BufferedReader r = new BufferedReader(new FileReader(getScriptArgs()[1]))) {
                String ln; int made = 0;
                while ((ln = r.readLine()) != null) {
                    String[] p = ln.trim().split("\\s+"); if (p.length < 2) continue;
                    ghidra.program.model.address.Address a = toAddr(Long.parseLong(p[0], 16));
                    if (getFunctionAt(a) == null) {
                        Function prev = currentProgram.getFunctionManager().getFunctionContaining(a);
                        if (prev != null) {
                            prev.setBody(prev.getBody().subtract(new ghidra.program.model.address.AddressSet(a, currentProgram.getMaxAddress())));
                        }
                        if (getInstructionAt(a) == null) { clearListing(a, a.add(16)); }
                        disassemble(a);
                        Function f = createFunction(a, p[1]);
                        if (f == null) {
                            f = currentProgram.getFunctionManager().createFunction(p[1], a, new ghidra.program.model.address.AddressSet(a, a), ghidra.program.model.symbol.SourceType.USER_DEFINED);
                        }
                        if (f != null) made++;
                    } else {
                        getFunctionAt(a).setName(p[1], ghidra.program.model.symbol.SourceType.USER_DEFINED);
                    }
                }
                println("created " + made + " functions");
            }
        }
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(new OutputStreamWriter(new FileOutputStream(out), "UTF-8"))) {
            int n = 0, bad = 0;
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                DecompileResults r = d.decompileFunction(f, 120, monitor);
                w.println("// ==== " + f.getName() + " @ " + f.getEntryPoint());
                if (r != null && r.decompileCompleted()) w.println(r.getDecompiledFunction().getC());
                else { w.println("// decompile failed: " + (r == null ? "null" : r.getErrorMessage())); bad++; }
                n++;
            }
            println("decompiled " + n + " functions, " + bad + " failed");
        }
        d.dispose();
    }
}
