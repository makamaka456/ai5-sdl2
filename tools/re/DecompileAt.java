import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;
public class DecompileAt extends GhidraScript {
  public void run() throws Exception {
    String[] args = getScriptArgs();
    PrintWriter w = new PrintWriter(new FileWriter(args[0]));
    DecompInterface d = new DecompInterface();
    d.openProgram(currentProgram);
    for (int i = 1; i < args.length; i++) {
      Address a = toAddr(args[i]);
      Function f = getFunctionAt(a);
      if (f == null) {
        new DisassembleCommand(a, null, true).applyTo(currentProgram, monitor);
        new CreateFunctionCmd(a).applyTo(currentProgram, monitor);
        f = getFunctionAt(a);
      }
      w.println("// ===== " + args[i]);
      if (f == null) { w.println("// no function"); continue; }
      DecompileResults r = d.decompileFunction(f, 60, monitor);
      w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// FAILED");
    }
    w.close();
  }
}
