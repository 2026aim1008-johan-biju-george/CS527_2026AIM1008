This is a simple **computer system simulator** with three stages that mirror real hardware: a **compiler** (translates human-readable assembly into machine code), a **memory system** (stores instructions and data), and a **processor** (fetches, decodes, and executes instructions). Let's go through it piece by piece.

## 1. The Big Picture (main.c)

```c
compile(argv[1]);   // Stage 1: assembly text -> bytecode file
initialize();        // Stage 2: load bytecode into memory
reset();              // Stage 3: prepare CPU state

while(!end_of_simulation){
    fetch();
    decode();
    execute();
}

finalize();           // write data memory back to disk
```

This is the classic **fetch–decode–execute cycle** that every real CPU runs. The program takes one input file (an assembly-like program), compiles it, loads it, then simulates running it instruction by instruction until a `HALT` is hit.

## 2. The Compiler (compiler.c / compiler.h)

The compiler turns lines like `x1 = x2 + 3` into 4-byte machine instructions (`opcode, dest, src1, src2`), written as hex bytes into `program.byte`.

**Two-pass design:**

- **Pass 1 — find labels.** It scans the file, and whenever a line starts with `.` (e.g. `.loop`), it records that label's name and the current byte address in the `labels[]` array. Every non-label instruction advances `address` by 4 (since every instruction is 4 bytes).
- **Pass 2 — generate bytecode.** It scans the file again, this time matching each line against a series of `sscanf` patterns (in order of specificity) to figure out what instruction it is:

| Pattern | Meaning |
|---|---|
| `x%d = [x%d]` | load from address held in a register |
| `x%d = [%d]` | load from constant address |
| `[x%d] = x%d` | store to address held in a register |
| `[%d] = x%d` | store to constant address |
| `x%d = x%d + x%d` / `+ %d` | add register or constant |
| similarly for `-`, `*`, `/` |
| `x%d = x%d` | register copy (MOV) |
| `x%d = %d` | load constant into register (MOV_CONST) |
| `BEQ/BNE/.../BAL label` | branch instructions |

Each opcode has two variants — one for a register operand (e.g. `ADD = 0x01`) and one for a constant operand (e.g. `ADD_CONST = 0x09`) — that's why you see the `_CONST` suffix opcodes.

**Branch encoding (`generate_branch`)**: it looks up the label's address, computes `offset = target - current`, truncates it to a single signed byte (`unsigned char`), and stores that offset as the 4th byte. This means branches are **PC-relative** with an 8-bit range — important limitation to know about.

At the end, it appends a `HALT` instruction so the processor knows when to stop.

## 3. Memory (memory.c / memory.h)

Two flat byte arrays simulate RAM:
```c
char Instruction[256];  // 256 bytes = 64 instructions max
char Data[4096];        // 4096 bytes = 1024 words max
```

- **`initialize()`** reads `program.byte` (hex pairs like `05 01 00 00`) into `Instruction[]`, and reads `data.byte` similarly into `Data[]` for initial data values.
- **`finalize()`** writes `Data[]` back to `data.byte` after the simulation finishes, so you can inspect the results.
- **`read_word` / `write_word`** handle **big-endian** 32-bit word access: 4 consecutive bytes are combined/split via bit-shifting (`<< 24`, `<< 16`, etc.), with bounds checking (address must leave room for 4 bytes: `i <= 4092`).

## 4. Processor (processor.c / processor.h)

This is the CPU core — global state includes 256 registers, `PC` (program counter), and 4 flags: `N` (negative), `Z` (zero), `C` (carry), `V` (overflow).

**`reset()`** zeroes everything and sets `PC = 0`.

**`fetch()`** reads 4 bytes starting at `PC` (`opcode, dest, src1, src2`) and advances `PC += 4`.

**`decode()`** currently just prints — there's no real decode logic split out, since `fetch` already extracted the fields. (Worth noting if you're asked to "properly" separate fetch/decode.)

**`execute()`** is a big `switch` on `opcode`:
- **Arithmetic** (`ADD`, `SUB`, `MUL`, `DIV` + `_CONST` versions): computes the result, stores it in `Register[dest]`, and for add/sub updates the flags via `update_add_flags` / `update_sub_flags` (multiply/divide don't update flags here).
- **Memory ops** (`LOAD`, `STORE` + `_CONST`): use `read_word`/`write_word` from memory.c.
- **Data movement** (`MOV`, `MOV_CONST`): simple register copy or constant load.
- **Branches**: `branch_taken(opcode)` checks the flag condition (e.g. `BEQ` → `Z==1`, `BGE` → `N==V`, standard ARM-style condition codes). If taken, `PC = instruction_PC + src2` — jumping relative to the branch instruction's own address using the signed offset byte computed at compile time.
- **`HALT`**: sets `end_of_simulation = 1`, ending the main loop.
- **`DIV`** by zero halts the simulation with an error message; interestingly, **`DIV_CONST` has no zero-check**, which is an inconsistency worth flagging if you're reviewing/fixing this code.

## Flag logic worth understanding

- `update_add_flags`: `C` is set if the unsigned result is *smaller* than either unsigned operand (indicates unsigned overflow/carry-out). `V` is set if both operands have the same sign but the result's sign differs (signed overflow).
- `update_sub_flags`: `C` set if `operand1 > operand2` (unsigned) — i.e., no borrow needed. `V` set if operands have different signs and the result's sign matches `operand2`'s original sign.

These match standard ARM condition-flag semantics, which is why `branch_taken` conditions (`BGE: N==V`, `BLT: N!=V`, etc.) look the way they do.

---

Example:
```text
x1 = 0          % index
x2 = 0          % sum
x3 = 0          % array address
x4 = [x3]       % read size of array
.loopback
x15 = x1 - x4   % compare index with size
BEQ .exit
x3 = x3 + 4     % increase the address (move to next element)
x6 = [x3]       % load array element
x2 = x2 + x6    % update sum
x1 = x1 + 1     % increment index
BAL .loopback
.exit
x3 = x3 + 4     % move to result address
[x3] = x2       % store sum
```

Let's trace this exact program through **both stages** — compilation and execution — since that's the best way to see how all three files (compiler, memory, processor) work together.

## What the program does

In plain English: `x3` walks through an array in memory. Address `0` holds the array's **size**, and the elements start right after it at address `4, 8, 12, ...`. The loop sums all elements into `x2`, then stores the result one word past the last element.

## Step 1 — Pass 1 of the compiler: label addresses

Every non-label line advances the address by 4 bytes. This gives us:

| Address | Line |
|---|---|
| 0 | `x1 = 0` |
| 4 | `x2 = 0` |
| 8 | `x3 = 0` |
| 12 | `x4 = [x3]` |
| **16** | `.loopback` ← label recorded here |
| 16 | `x15 = x1 - x4` |
| 20 | `BEQ .exit` |
| 24 | `x3 = x3 + 4` |
| 28 | `x6 = [x3]` |
| 32 | `x2 = x2 + x6` |
| 36 | `x1 = x1 + 1` |
| 40 | `BAL .loopback` |
| **44** | `.exit` ← label recorded here |
| 44 | `x3 = x3 + 4` |
| 48 | `[x3] = x2` |
| 52 | *(HALT appended automatically)* |

So `labels[] = { ".loopback"→16, ".exit"→44 }`.

## Step 2 — Pass 2: generated bytecode

Each `sscanf` pattern in `compile()` picks the matching instruction form. Here's the full byte-by-byte breakdown (opcode values from `compiler.h`):

| Addr | Source | Match | Opcode | Bytes (`op dest src1 src2`) |
|---|---|---|---|---|
| 0 | `x1 = 0` | `x%d = %d` | MOV_CONST (0x0F) | `0F 01 00 00` |
| 4 | `x2 = 0` | `x%d = %d` | MOV_CONST | `0F 02 00 00` |
| 8 | `x3 = 0` | `x%d = %d` | MOV_CONST | `0F 03 00 00` |
| 12 | `x4 = [x3]` | `x%d = [x%d]` | LOAD (0x05) | `05 04 03 00` |
| 16 | `x15 = x1 - x4` | `x%d = x%d - x%d` | SUB (0x02) | `02 0F 01 04` |
| 20 | `BEQ .exit` | branch | BEQ (0x10) | `10 00 00 18` |
| 24 | `x3 = x3 + 4` | `x%d = x%d + %d` | ADD_CONST (0x09) | `09 03 03 04` |
| 28 | `x6 = [x3]` | `x%d = [x%d]` | LOAD | `05 06 03 00` |
| 32 | `x2 = x2 + x6` | `x%d = x%d + x%d` | ADD (0x01) | `01 02 02 06` |
| 36 | `x1 = x1 + 1` | `x%d = x%d + %d` | ADD_CONST | `09 01 01 01` |
| 40 | `BAL .loopback` | branch | BAL (0x1E) | `1E 00 00 E8` |
| 44 | `x3 = x3 + 4` | `x%d = x%d + %d` | ADD_CONST | `09 03 03 04` |
| 48 | `[x3] = x2` | `[x%d] = x%d` | STORE (0x06) | `06 03 02 00` |
| 52 | *(halt)* | — | HALT (0x00) | `00 00 00 00` |

**The two branch offsets are worth walking through carefully** — this is the trickiest part of the whole system:

- **`BEQ .exit`** is at address 20. Target `.exit` = 44. `offset = 44 - 20 = 24` → stored as byte `0x18`.
- **`BAL .loopback`** is at address 40. Target `.loopback` = 16. `offset = 16 - 40 = -24`. Since `generate_branch` casts this to `unsigned char`, `-24` becomes `232` (`0xE8`) in the file.

At runtime, `Instruction[]` is declared as `char` (signed on most systems), so when `0xE8` is read back in, it's reinterpreted as **signed** `-24` again. That's why `PC = instruction_PC + src2` correctly jumps *backward*: `40 + (-24) = 16`. This round-trip (signed → unsigned-for-storage → signed-again) is intentional but subtle — it only works because `char` is signed on the target platform. If this ran on a platform with unsigned `char` by default, backward branches would break.

## Step 3 — Execution trace

Say `data.byte` encodes an array of size 3 with elements `1, 2, 3`, so `Data[0]=3, Data[4]=1, Data[8]=2, Data[12]=3` (all as 32-bit words). After `reset()`, all registers are 0.

| Iter | x1 | x4 | x15 = x1−x4 | Z flag | BEQ taken? | x3 (after +4) | x6 = Data[x3] | x2 (sum so far) | x1 (after +1) |
|---|---|---|---|---|---|---|---|---|---|
| — | 0 | 3 *(loaded)* | — | — | — | 0 | — | 0 | — |
| 1 | 0 | 3 | −3 | 0 | no | 4 | 1 | 1 | 1 |
| 2 | 1 | 3 | −2 | 0 | no | 8 | 2 | 3 | 2 |
| 3 | 2 | 3 | −1 | 0 | no | 12 | 3 | 6 | 3 |
| 4 | 3 | 3 | 0 | **1** | **yes → jump to `.exit`** | 12 | — | 6 | — |

At `.exit`: `x3 = 12 + 4 = 16`, then `[x3] = x2` → `Data[16] = 6`.

**Result:** `Data[16]` holds `6`, the sum of `1+2+3` — correctly placed one word past the last array element, exactly as the algorithm intended.

## A couple of things worth flagging while revising

1. **`x15 = x1 - x4`** uses `SUB`, which calls `update_sub_flags`. The `Z` flag from that subtraction is what `BEQ` checks — this is the standard "compare via subtraction" trick, and it's implemented correctly here.
2. **The branch range is only ±127/−128** (signed 8-bit offset), since `generate_branch` truncates to one byte. For this program the offsets (24, −24) are tiny, so it's fine — but a much longer program with a `BAL` back to the very top could silently produce a wrong offset if it exceeds that range, since there's no overflow check in `generate_branch`.
3. **`x15` as a register name** works fine here since the register file has 256 entries, but it's easy to typo a register number when hand-writing assembly like this — nothing in the compiler validates that `dest`/`src` stay in a sane range.

## Possible extensions of the assignment

### Function Calls

Adding function calls means giving the simulator two things it currently lacks: a **call stack** (to remember where to return to) and two new instructions, **CALL** and **RET**, that use it. Here's how to build that on top of your existing design.

## 1. Design decisions to make first

- **Where does the stack live?** Reuse `Data[]` — it's already 4096 bytes and unused space at the top works well as a stack growing *downward*.
- **What tracks the stack top?** Add a new global `int SP` (stack pointer), separate from the general-purpose `Register[]` array, initialized in `reset()`.
- **Calling convention** (a rule your compiled programs must follow, same as any real ISA): e.g. arguments passed in `x1`–`x4`, return value in `x1`. This isn't enforced by the CPU — it's just a convention your assembly source follows.

## 2. New opcodes

You have unused opcode slots after `BAL (0x1E)`. Add to both `compiler.h` and `processor.h`:

```c
#define CALL 0x1F
#define RET  0x20
```

## 3. Compiler changes (`compiler.c`)

`CALL` can reuse your existing `generate_branch()` machinery exactly like `BEQ`/`BAL` — it's just a jump with a saved return address, so it still encodes as a PC-relative offset in the 4th byte:

```c
else if(sscanf(line, "CALL %s", label) == 1){
    generate_branch(optr, label, CALL, curr_addr);
}
else if(strncmp(line, "RET", 3) == 0){
    fprintf(optr, "%02X %02X %02X %02X\n", RET, 0, 0, 0);
}
```

Add these as new `else if` branches before your final `else { printf("Invalid instruction..."); }` catch-all.

## 4. Processor changes (`processor.c` / `processor.h`)

**Add the stack pointer and initialize it in `reset()`:**

```c
int SP; // stack pointer, points to next free word (grows downward)
...
void reset(){
   ...
   SP = 4092; // top of Data memory, last valid word address
}
```

**Add execution logic in `execute()`'s switch:**

```c
case CALL:{
   int return_addr = instruction_PC + 4; // address of the instruction after CALL
   SP -= 4;
   write_word(SP, return_addr);          // push return address
   PC = instruction_PC + src2;           // jump into the function (same offset trick as branches)
   printf("Call: pushed return address %d, PC <- %d\n", return_addr, PC);
   break;
}

case RET:{
   PC = read_word(SP);   // pop return address
   SP += 4;
   printf("Return: PC <- %d\n", PC);
   break;
}
```

That's the entire mechanism — `CALL` behaves like `BAL` except it remembers where to come back to.

## The universal recipe — 4 touch points for *any* new instruction

No matter what they ask you to add, it follows this same shape (exactly like `CALL`/`RET`):

1. **Define the opcode** in both `compiler.h` and `processor.h` (they must match — an easy point to lose if you edit one and forget the other). Pick an unused hex value — you have `0x08`, and everything from `0x1F` up, free.
2. **Add a parsing rule in `compile()`** — an `sscanf`/`strncmp` pattern matching the new syntax, placed *before* the final `else { printf("Invalid instruction..."); }`. Order matters: more specific patterns must come before more general ones that could also match.
3. **Add an `execute()` case** in `processor.c` doing the actual operation, reading `dest`/`src1`/`src2` as needed.
4. **Hand-trace a tiny test program** before trusting it — compute expected register/memory values yourself, then verify.

If you only remember one thing walking into the test: **grep both header files for the opcode range you're about to use**, so you don't accidentally collide with an existing one.

## Likely extensions, grouped by category

**A. Bitwise/logical operations** (AND, OR, XOR, NOT, shifts) — very common addition since the ISA only has arithmetic right now.
```c
// processor.h / compiler.h
#define AND 0x20
#define OR  0x21
#define XOR 0x22
#define NOT 0x23
#define SHL 0x24
#define SHR 0x25
```
```c
// compiler.c — same pattern as ADD
else if(sscanf(line, "x%d = x%d & x%d", &dest, &src1, &src2) == 3)
    fprintf(optr, "%02X %02X %02X %02X\n", AND, dest, src1, src2);
```
```c
// processor.c
case AND: Register[dest] = Register[src1] & Register[src2]; break;
case NOT: Register[dest] = ~Register[src1]; break; // only 1 source operand
```
Watch for: `NOT` only needs `src1`, no `src2` — mirror the compiler pattern accordingly (`sscanf(line, "x%d = ~x%d", ...)`, 2 fields not 3).

**B. Stack instructions** — `PUSH`/`POP` as standalone ops (separate from `CALL`), used to save/restore registers:
```c
case PUSH: SP -= 4; write_word(SP, Register[dest]); break;
case POP:  Register[dest] = read_word(SP); SP += 4; break;
```
This is almost certainly testable if your course already covered `CALL`/`RET` with you — it's the natural next lab step, and lets you build **caller-saved register conventions** around function calls.

**C. New addressing modes** — e.g. indexed load/store: `x%d = [x%d + %d]` (base register + constant offset):
```c
else if(sscanf(line, "x%d = [x%d + %d]", &dest, &addr_reg, &val) == 3)
    fprintf(optr, "%02X %02X %02X %02X\n", LOAD_INDEXED, dest, addr_reg, val);
```
```c
case LOAD_INDEXED: Register[dest] = read_word(Register[src1] + src2); break;
```
Careful: `src2` here is a small int stored in one byte — same range limit as branch offsets (max 255, or -128..127 if treated signed).

**D. A NOP or new branch condition** — trivial but sometimes explicitly tested to check you understand the opcode table:
```c
case NOP: break; // does nothing, just consumes a cycle
```

**E. Simple I/O instructions** — `PRINT x%d` or `READ x%d` (reads from stdin) — tests whether you can add an instruction that *doesn't* fit the load/store/arithmetic mold:
```c
case PRINT: printf("Output: %d\n", Register[dest]); break;
case READ: scanf("%d", &Register[dest]); break;
```

**F. Decode() implementation** - `decode()` currently exists only as a placeholder — it prints a line but does no actual decoding work; `fetch()` already extracted the raw bytes, so **all** the interpretation currently happens implicitly inside `execute()`'s switch statement. A proper `decode()` step should do two things a real CPU's decode stage does: **translate the raw opcode into a meaningful instruction name**, and **catch illegal opcodes before execution is attempted** (rather than only after, in `execute()`'s `default` case).

Here's the implementation, continuing in the same lab copy:

```c
/* Decodes fetched instruction: translates the raw opcode into a
   human-readable mnemonic, and flags illegal opcodes before execute() runs. */
void decode(){
   printf("Decoding instruction bytes %d-%d:\n", instruction_PC, instruction_PC + 3);

   char *mnemonic;
   switch(opcode){
      case HALT: mnemonic = "HALT"; break;
      case ADD: mnemonic = "ADD"; break;
      case SUB: mnemonic = "SUB"; break;
      case MUL: mnemonic = "MUL"; break;
      case DIV: mnemonic = "DIV"; break;
      case LOAD: mnemonic = "LOAD"; break;
      case STORE: mnemonic = "STORE"; break;
      case MOV: mnemonic = "MOV"; break;
      case ADD_CONST: mnemonic = "ADD_CONST"; break;
      case SUB_CONST: mnemonic = "SUB_CONST"; break;
      case MUL_CONST: mnemonic = "MUL_CONST"; break;
      case DIV_CONST: mnemonic = "DIV_CONST"; break;
      case LOAD_CONST: mnemonic = "LOAD_CONST"; break;
      case STORE_CONST: mnemonic = "STORE_CONST"; break;
      case MOV_CONST: mnemonic = "MOV_CONST"; break;
      case BEQ: mnemonic = "BEQ"; break;
      case BNE: mnemonic = "BNE"; break;
      case BCS: mnemonic = "BCS"; break;
      case BCC: mnemonic = "BCC"; break;
      case BMI: mnemonic = "BMI"; break;
      case BPL: mnemonic = "BPL"; break;
      case BVS: mnemonic = "BVS"; break;
      case BVC: mnemonic = "BVC"; break;
      case BHI: mnemonic = "BHI"; break;
      case BLS: mnemonic = "BLS"; break;
      case BGE: mnemonic = "BGE"; break;
      case BLT: mnemonic = "BLT"; break;
      case BGT: mnemonic = "BGT"; break;
      case BLE: mnemonic = "BLE"; break;
      case BAL: mnemonic = "BAL"; break;
      case AND: mnemonic = "AND"; break;
      case PUSH: mnemonic = "PUSH"; break;
      case POP: mnemonic = "POP"; break;
      default:
         mnemonic = "INVALID";
         printf("Decode error: unrecognized opcode %d at PC=%d\n", opcode, instruction_PC);
         end_of_simulation = 1;
         break;
   }

   printf("-> %s  (dest=%d, src1=%d, src2=%d)\n", mnemonic, dest, src1, src2);
}
```
Now let's verify both behaviors — normal decoding, and catching an illegal opcode. For the normal case, reuse the `AND`/`PUSH`/`POP` test program:To demonstrate the illegal-opcode catch, I'll bypass the compiler and hand-write a `program.byte` containing an opcode that doesn't exist (`0xFF`), then run the fetch–decode–execute loop directly:Both cases confirmed:

**Valid program** — `decode()` now prints the actual instruction name (`MOV_CONST`, `AND`, `HALT`) instead of a blank line, making trace output far easier to read and debug.

**Illegal opcode** — the fetch stage read the invalid byte, and **`decode()` caught it and set `end_of_simulation = 1` before `execute()` even ran the switch statement**. (Notice `execute()`'s existing `default` case still also fires afterward and prints its own message — that's harmless redundancy since `main.c`'s loop only rechecks `end_of_simulation` *between* iterations, not mid-iteration. If you wanted decode's catch to fully preempt execute in the same cycle, you'd add `if(end_of_simulation) return;` as the first line of `execute()` — worth mentioning to a grader as a design choice even if you don't implement it.)

## A bonus bug this test surfaced

Look closely at the output: `Opcode: -1`, not `255`. That's because `Instruction[]` is declared `char Instruction[256]` — a **signed** type on most platforms — so the byte `0xFF` gets sign-extended to `-1` when assigned into the `int opcode` variable. It happened not to matter here (an invalid opcode is still correctly rejected either way), but it's a real correctness issue worth knowing: **any legitimate opcode value ≥ 0x80** would sign-extend into a large negative number and never match any `case` in the switch, silently falling into `default`. Your actual opcode table tops out at `0x22` (well under `0x80`), so this isn't currently biting you — but if a test asks you to extend the opcode range, or asks "what's a latent bug in this code," this is a strong answer: **`Instruction[]` should be `unsigned char[]`**, not `char[]`.

## Recap of `decode()`'s job in this architecture

- `fetch()` — pulls raw bytes out of memory
- `decode()` (now) — interprets what those bytes *mean*: names the instruction, and rejects it early if it's not a legal opcode
- `execute()` — performs the actual computation

This is the correct separation of concerns for a lab answer: if asked "why have a separate decode stage at all," the answer is exactly this — **validation and interpretation belong before execution, not tangled inside it**, and real pipelined CPUs stall or fault at decode for precisely this reason.
