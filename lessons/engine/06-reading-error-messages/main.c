/*
 * Engine Lesson 06 — Reading Error Messages
 *
 * Demonstrates: How to read and interpret the three types of errors you
 * encounter when building and running C programs:
 *
 *   1. Compiler errors  — Syntax and type mistakes caught during compilation
 *   2. Linker errors    — Missing symbols discovered when combining object files
 *   3. Runtime errors   — Crashes and failures that happen while the program runs
 *
 * This program cannot trigger real compiler or linker errors (those prevent
 * the program from being built at all).  Instead, it walks through annotated
 * examples of each error type and demonstrates runtime error handling using
 * SDL's error reporting.
 *
 * Why this lesson exists:
 *   Error messages are how the toolchain communicates with you.  Learning to
 *   read them turns a frustrating wall of text into a precise diagnosis.
 *   Every forge-gpu lesson produces errors when something is misconfigured —
 *   this lesson teaches you to fix them quickly.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>

/* ── Forward declarations ────────────────────────────────────────────────── */

static void demo_build_pipeline(void);
static void demo_compiler_errors(void);
static void demo_linker_errors(void);
static void demo_runtime_errors(void);
static void demo_warnings(void);
static void demo_fixing_strategy(void);

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Print a horizontal divider to separate sections in the output. */
static void print_divider(const char *title)
{
    SDL_Log(" ");
    SDL_Log("============================================================");
    SDL_Log("  %s", title);
    SDL_Log("============================================================");
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* SDL_Init(0) initializes core SDL state without enabling any subsystem
     * (video, audio, etc.).  We get SDL_Log and SDL_GetError — everything
     * this console program needs. */
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== Engine Lesson 06: Reading Error Messages ===");

    demo_build_pipeline();
    demo_compiler_errors();
    demo_linker_errors();
    demo_runtime_errors();
    demo_warnings();
    demo_fixing_strategy();

    SDL_Log(" ");
    SDL_Log("=== All sections complete ===");

    SDL_Quit();
    return 0;
}

/* ── Section 1: The build pipeline ───────────────────────────────────────── */
/*
 * Before you can read an error message, you need to know WHEN it happens.
 * Building a C program has distinct phases, and each phase produces
 * different kinds of errors:
 *
 *   source.c  -->  [preprocess]  -->  [compile]  -->  source.o
 *                                                        |
 *   other.c   -->  [preprocess]  -->  [compile]  -->  other.o
 *                                                        |
 *                                                   [link] --> program
 *                                                               |
 *                                                            [run]
 *
 * Phase 1: Preprocess  — Expands #include, #define, #ifdef
 * Phase 2: Compile     — Checks syntax and types, produces object files (.o)
 * Phase 3: Link        — Combines object files, resolves function references
 * Phase 4: Run         — Executes the program (crashes happen here)
 */
static void demo_build_pipeline(void)
{
    print_divider("1. The Build Pipeline");

    SDL_Log("Building a C program happens in phases:");
    SDL_Log(" ");
    SDL_Log("  source.c -> [preprocess] -> [compile] -> source.o -+");
    SDL_Log("                                                     |");
    SDL_Log("  other.c  -> [preprocess] -> [compile] -> other.o  -+-> [link] -> program");
    SDL_Log("                                                     |");
    SDL_Log("  libSDL3  -----------------------------------------+");
    SDL_Log(" ");
    SDL_Log("Each phase catches different kinds of mistakes:");
    SDL_Log(" ");
    SDL_Log("  Phase        | What it checks        | Error type");
    SDL_Log("  -------------+-----------------------+-----------------------");
    SDL_Log("  Preprocess   | #include, #define     | 'file not found'");
    SDL_Log("  Compile      | Syntax, types         | 'expected', 'undeclared'");
    SDL_Log("  Link         | Symbol references     | 'undefined reference'");
    SDL_Log("  Run          | Logic, resources       | Crash, wrong output");
    SDL_Log(" ");
    SDL_Log("The error message tells you WHICH phase failed.");
    SDL_Log("That immediately narrows down where to look.");
}

/* ── Section 2: Compiler errors ──────────────────────────────────────────── */
/*
 * Compiler errors happen during compilation (phase 2).  The compiler reads
 * your source code, checks that it follows C syntax rules and type rules,
 * and reports any violations.
 *
 * A compiler error has a consistent structure across GCC, Clang, and MSVC:
 *
 *   file:line:column: error: description
 *
 * Learning to parse this structure is the most important skill in this lesson.
 */
static void demo_compiler_errors(void)
{
    print_divider("2. Compiler Errors");

    SDL_Log("Compiler errors are caught during compilation (phase 2).");
    SDL_Log("They follow a consistent format:");
    SDL_Log(" ");
    SDL_Log("  file:line:col: error: description");
    SDL_Log(" ");
    SDL_Log("Let's break down a real example. This code has a typo:");
    SDL_Log(" ");
    SDL_Log("  int count = 10;");
    SDL_Log("  SDL_Log(\"count = %%d\", coutn);  // <-- misspelled 'count'");
    SDL_Log(" ");
    SDL_Log("GCC/Clang would report:");
    SDL_Log(" ");
    SDL_Log("  main.c:42:30: error: use of undeclared identifier 'coutn'");
    SDL_Log("      SDL_Log(\"count = %%d\", coutn);");
    SDL_Log("                               ^~~~~");
    SDL_Log(" ");
    SDL_Log("Reading this message piece by piece:");
    SDL_Log("  main.c     -> the file containing the error");
    SDL_Log("  42         -> line number (go to line 42)");
    SDL_Log("  30         -> column number (character 30 on that line)");
    SDL_Log("  error      -> severity (not a warning -- build WILL fail)");
    SDL_Log("  undeclared identifier 'coutn' -> what went wrong");
    SDL_Log("  ^~~~~      -> caret points to the exact location");
    SDL_Log(" ");
    SDL_Log("MSVC formats it slightly differently:");
    SDL_Log(" ");
    SDL_Log("  main.c(42): error C2065: 'coutn': undeclared identifier");
    SDL_Log(" ");
    SDL_Log("Same information, different format:");
    SDL_Log("  main.c(42) -> file and line (parentheses instead of colons)");
    SDL_Log("  C2065      -> MSVC error code (searchable in documentation)");

    /* Show more common compiler error types. */
    SDL_Log(" ");
    SDL_Log("------------------------------------------------------------");
    SDL_Log("Common compiler errors you will encounter:");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");

    /* Missing semicolon */
    SDL_Log("[A] Missing semicolon");
    SDL_Log(" ");
    SDL_Log("  Code:    int x = 5");
    SDL_Log("           int y = 10;");
    SDL_Log(" ");
    SDL_Log("  GCC:     main.c:5:15: error: expected ';' before 'int'");
    SDL_Log("  Clang:   main.c:4:16: error: expected ';' after expression");
    SDL_Log("  MSVC:    main.c(5): error C2146: syntax error: missing ';'");
    SDL_Log(" ");
    SDL_Log("  Notice: the error points to line 5, but the missing ';'");
    SDL_Log("  is on line 4.  The compiler does not realize the statement");
    SDL_Log("  is incomplete until it sees the NEXT token.  When an error");
    SDL_Log("  message does not make sense on the reported line, check");
    SDL_Log("  the line ABOVE.");

    /* Type mismatch */
    SDL_Log(" ");
    SDL_Log("[B] Type mismatch");
    SDL_Log(" ");
    SDL_Log("  Code:    float *ptr = 42;");
    SDL_Log(" ");
    SDL_Log("  GCC:     main.c:8:18: warning: initialization of 'float *'");
    SDL_Log("           from 'int' makes pointer from integer without a cast");
    SDL_Log("  Clang:   main.c:8:18: warning: incompatible integer to");
    SDL_Log("           pointer conversion");
    SDL_Log("  MSVC:    main.c(8): warning C4047: 'initializing': 'float *'");
    SDL_Log("           differs in levels of indirection from 'int'");
    SDL_Log(" ");
    SDL_Log("  A pointer should hold an address, not a plain integer.");
    SDL_Log("  The compiler warns that this is almost certainly a mistake.");

    /* Missing #include */
    SDL_Log(" ");
    SDL_Log("[C] Missing header / #include");
    SDL_Log(" ");
    SDL_Log("  Code:    #include \"nonexistent.h\"");
    SDL_Log(" ");
    SDL_Log("  GCC:     main.c:1:10: fatal error: nonexistent.h:");
    SDL_Log("           No such file or directory");
    SDL_Log("  Clang:   main.c:1:10: fatal error: 'nonexistent.h'");
    SDL_Log("           file not found");
    SDL_Log("  MSVC:    main.c(1): fatal error C1083: Cannot open include");
    SDL_Log("           file: 'nonexistent.h': No such file or directory");
    SDL_Log(" ");
    SDL_Log("  'fatal error' means the compiler stops immediately.");
    SDL_Log("  Check spelling, include paths (target_include_directories),");
    SDL_Log("  and whether the file actually exists.");

    /* Implicit function declaration */
    SDL_Log(" ");
    SDL_Log("[D] Calling a function without declaring it");
    SDL_Log(" ");
    SDL_Log("  Code:    int main(void) { my_function(); }");
    SDL_Log("           // my_function is not declared anywhere");
    SDL_Log(" ");
    SDL_Log("  GCC:     main.c:2:5: error: implicit declaration of function");
    SDL_Log("           'my_function'");
    SDL_Log("  Clang:   main.c:2:5: error: call to undeclared function");
    SDL_Log("           'my_function'");
    SDL_Log("  MSVC:    main.c(2): error C3861: 'my_function': identifier");
    SDL_Log("           not found");
    SDL_Log(" ");
    SDL_Log("  Either #include the header that declares it, add a forward");
    SDL_Log("  declaration, or check the spelling.");
}

/* ── Section 3: Linker errors ────────────────────────────────────────────── */
/*
 * Linker errors happen during linking (phase 3).  By this point, every
 * individual .c file compiled successfully into a .o file.  The linker
 * now tries to combine them into one executable and must resolve every
 * function call and variable reference to an actual definition.
 *
 * If the linker cannot find a definition, it reports an "undefined
 * reference" (GCC/Clang) or "unresolved external symbol" (MSVC).
 *
 * Key insight: the code COMPILED fine.  The compiler trusted that the
 * function exists somewhere — but the linker could not find it.
 */
static void demo_linker_errors(void)
{
    print_divider("3. Linker Errors");

    SDL_Log("Linker errors happen AFTER compilation succeeds.");
    SDL_Log("The compiler trusts your declarations.  The linker verifies them.");
    SDL_Log(" ");
    SDL_Log("A linker error means: 'I found a call to function X, but");
    SDL_Log("no object file or library provides a definition for X.'");
    SDL_Log(" ");

    /* Undefined reference — the most common linker error */
    SDL_Log("------------------------------------------------------------");
    SDL_Log("[A] Undefined reference (missing function definition)");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");
    SDL_Log("  Code:    void render(void);    // declaration only");
    SDL_Log("           int main(void) { render(); }");
    SDL_Log("           // render() is declared but never defined");
    SDL_Log(" ");
    SDL_Log("  GCC:     /usr/bin/ld: main.o: in function 'main':");
    SDL_Log("           main.c:(.text+0x9): undefined reference to 'render'");
    SDL_Log("           collect2: error: ld returned 1 exit status");
    SDL_Log(" ");
    SDL_Log("  Clang:   ld: error: undefined symbol: render");
    SDL_Log("           >>> referenced by main.c");
    SDL_Log("           >>>               main.o:(main)");
    SDL_Log(" ");
    SDL_Log("  MSVC:    main.obj : error LNK2019: unresolved external symbol");
    SDL_Log("           render referenced in function main");
    SDL_Log("           main.exe : fatal error LNK1120: 1 unresolved externals");
    SDL_Log(" ");
    SDL_Log("  Reading GCC's message:");
    SDL_Log("    /usr/bin/ld   -> the linker program (not the compiler)");
    SDL_Log("    main.o        -> which object file contains the reference");
    SDL_Log("    'main'        -> which function makes the call");
    SDL_Log("    'render'      -> the missing symbol");
    SDL_Log("    collect2      -> GCC's linker driver reporting the failure");
    SDL_Log(" ");
    SDL_Log("  Common causes:");
    SDL_Log("    1. Forgot to add the .c file to add_executable() in CMake");
    SDL_Log("    2. Forgot target_link_libraries() for an external library");
    SDL_Log("    3. Misspelled the function name (declaration != definition)");
    SDL_Log("    4. The function is defined but marked 'static' (file-private)");

    /* Missing library */
    SDL_Log(" ");
    SDL_Log("------------------------------------------------------------");
    SDL_Log("[B] Missing library (forgot target_link_libraries)");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");
    SDL_Log("  Code:    #include <SDL3/SDL.h>");
    SDL_Log("           int main(void) { SDL_Init(0); }");
    SDL_Log("           // Compiled with: gcc main.c  (no -lSDL3)");
    SDL_Log(" ");
    SDL_Log("  GCC:     undefined reference to 'SDL_Init'");
    SDL_Log("           undefined reference to 'SDL_Log'");
    SDL_Log("           undefined reference to 'SDL_Quit'");
    SDL_Log(" ");
    SDL_Log("  When you see MANY undefined references to functions from the");
    SDL_Log("  same library, the library is not linked.  In CMake, add:");
    SDL_Log("    target_link_libraries(my_program PRIVATE SDL3::SDL3)");

    /* Duplicate symbol */
    SDL_Log(" ");
    SDL_Log("------------------------------------------------------------");
    SDL_Log("[C] Duplicate symbol (multiple definitions)");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");
    SDL_Log("  Situation: Two .c files both define a function with the");
    SDL_Log("  same name (and neither marks it 'static').");
    SDL_Log(" ");
    SDL_Log("  GCC:     /usr/bin/ld: b.o: in function 'helper':");
    SDL_Log("           b.c:(.text+0x0): multiple definition of 'helper';");
    SDL_Log("           a.o:a.c:(.text+0x0): first defined here");
    SDL_Log(" ");
    SDL_Log("  MSVC:    b.obj : error LNK2005: helper already defined in a.obj");
    SDL_Log(" ");
    SDL_Log("  Fix: mark the function 'static' if it is file-private, or");
    SDL_Log("  keep only one definition and declare it in a shared header.");
    SDL_Log("  (See Engine Lesson 05 — Header-Only Libraries)");

    /* How to tell compiler errors from linker errors */
    SDL_Log(" ");
    SDL_Log("------------------------------------------------------------");
    SDL_Log("How to tell compiler errors from linker errors:");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");
    SDL_Log("  Compiler error        | Linker error");
    SDL_Log("  ----------------------+----------------------------");
    SDL_Log("  Has file:line:col     | Has .o or .obj file name");
    SDL_Log("  Says 'error:' or      | Says 'undefined reference'");
    SDL_Log("    'fatal error:'      |   or 'unresolved external'");
    SDL_Log("  Points to source code | Points to a symbol name");
    SDL_Log("  From gcc/clang/cl     | From ld/link.exe/collect2");
    SDL_Log(" ");
    SDL_Log("  If the message mentions a source file with a line number,");
    SDL_Log("  it is a compiler error.  If it mentions .o/.obj files and");
    SDL_Log("  symbol names, it is a linker error.");
}

/* ── Section 4: Runtime errors ───────────────────────────────────────────── */
/*
 * Runtime errors happen after the program is built and starts executing.
 * The compiler and linker cannot catch these — the code is syntactically
 * correct and all symbols resolve, but the program does something wrong
 * at execution time.
 *
 * Common runtime errors in graphics programming:
 *   - Segmentation fault (accessing invalid memory)
 *   - SDL function failures (GPU not available, file not found)
 *   - Logic errors (wrong output, visual artifacts)
 */
static void demo_runtime_errors(void)
{
    print_divider("4. Runtime Errors");

    SDL_Log("Runtime errors happen when the program is RUNNING.");
    SDL_Log("The build succeeded, but something goes wrong at execution time.");

    /* Segmentation fault */
    SDL_Log(" ");
    SDL_Log("------------------------------------------------------------");
    SDL_Log("[A] Segmentation fault (SIGSEGV)");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");
    SDL_Log("  Code:    int *ptr = NULL;");
    SDL_Log("           *ptr = 42;  // writing to address 0");
    SDL_Log(" ");
    SDL_Log("  Linux:   Segmentation fault (core dumped)");
    SDL_Log("  macOS:   Segmentation fault: 11");
    SDL_Log("  Windows: Exception: Access violation writing 0x00000000");
    SDL_Log(" ");
    SDL_Log("  A 'segfault' means the program tried to read or write memory");
    SDL_Log("  it does not own.  Common causes:");
    SDL_Log("    - Dereferencing a NULL pointer");
    SDL_Log("    - Using a pointer after the memory was freed (use-after-free)");
    SDL_Log("    - Array index out of bounds");
    SDL_Log("    - Stack overflow from infinite recursion");
    SDL_Log(" ");
    SDL_Log("  The operating system kills the program immediately.");
    SDL_Log("  There is no error message from the compiler — the code");
    SDL_Log("  compiled and linked fine.  Use a debugger to find the");
    SDL_Log("  exact line (see Engine Lesson 07).");

    /* SDL function failures */
    SDL_Log(" ");
    SDL_Log("------------------------------------------------------------");
    SDL_Log("[B] SDL function failures");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");
    SDL_Log("  Many SDL functions return false or NULL on failure.");
    SDL_Log("  When that happens, SDL_GetError() returns a human-readable");
    SDL_Log("  description of what went wrong.");
    SDL_Log(" ");

    /* Demonstrate SDL error checking with a deliberate failure. */
    SDL_Log("  Live demonstration — calling SDL_LoadFile on a missing file:");
    SDL_Log(" ");

    size_t file_size = 0;
    void *data = SDL_LoadFile("this_file_does_not_exist.txt", &file_size);
    if (!data) {
        SDL_Log("    SDL_LoadFile returned NULL");
        SDL_Log("    SDL_GetError(): %s", SDL_GetError());
        SDL_Log(" ");
        SDL_Log("  The pattern for every SDL call that can fail:");
        SDL_Log(" ");
        SDL_Log("    if (!SDL_SomeFunction(...)) {");
        SDL_Log("        SDL_Log(\"SomeFunction failed: %%s\", SDL_GetError());");
        SDL_Log("        // clean up and return");
        SDL_Log("    }");
    } else {
        /* This should not happen, but handle it correctly. */
        SDL_free(data);
    }

    /* GPU-specific runtime errors */
    SDL_Log(" ");
    SDL_Log("------------------------------------------------------------");
    SDL_Log("[C] GPU-specific runtime errors");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");
    SDL_Log("  GPU programs have additional failure modes:");
    SDL_Log(" ");
    SDL_Log("  1. Shader compilation failure:");
    SDL_Log("     SDL_CreateGPUShader failed: compilation error at line 12");
    SDL_Log("     -> Check the HLSL source in shaders/*.hlsl");
    SDL_Log("     -> Recompile shaders: python scripts/compile_shaders.py");
    SDL_Log(" ");
    SDL_Log("  2. Pipeline creation failure:");
    SDL_Log("     SDL_CreateGPUGraphicsPipeline failed: ...");
    SDL_Log("     -> Vertex layout does not match shader inputs");
    SDL_Log("     -> Check attribute formats and offsets");
    SDL_Log(" ");
    SDL_Log("  3. Missing or corrupt assets:");
    SDL_Log("     SDL_LoadFile failed: file not found");
    SDL_Log("     -> Check that assets/ is next to the executable");
    SDL_Log("     -> Check the file path and spelling");
    SDL_Log(" ");
    SDL_Log("  4. Black screen (no visible error):");
    SDL_Log("     -> Clear color is the same as the geometry color");
    SDL_Log("     -> Vertices are outside the clip volume (-1 to +1)");
    SDL_Log("     -> Back-face culling is discarding front-facing triangles");
    SDL_Log("     -> Depth test is failing (depth buffer not configured)");

    /* Assertion failures */
    SDL_Log(" ");
    SDL_Log("------------------------------------------------------------");
    SDL_Log("[D] Assertion failures");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");
    SDL_Log("  Code:    SDL_assert(texture != NULL);");
    SDL_Log(" ");
    SDL_Log("  Output:  Assertion failure at main.c:85 'texture != NULL'");
    SDL_Log(" ");
    SDL_Log("  An assertion is a check that a condition MUST be true.");
    SDL_Log("  When it fails, the program prints the file, line, and the");
    SDL_Log("  exact condition that was violated.  Assertions are one of");
    SDL_Log("  the most helpful debugging tools because they tell you");
    SDL_Log("  exactly what assumption was wrong.");
}

/* ── Section 5: Warnings ─────────────────────────────────────────────────── */
/*
 * Warnings are not errors — the program will still compile.  But warnings
 * frequently indicate real bugs.  Treating warnings as errors catches
 * problems early.
 */
static void demo_warnings(void)
{
    print_divider("5. Warnings");

    SDL_Log("Warnings are the compiler saying: 'This is technically legal,");
    SDL_Log("but it looks like a mistake.'");
    SDL_Log(" ");
    SDL_Log("The program WILL compile despite warnings.  But warnings");
    SDL_Log("frequently point to real bugs.");
    SDL_Log(" ");

    SDL_Log("------------------------------------------------------------");
    SDL_Log("Common warnings:");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");

    /* Unused variable */
    SDL_Log("[A] Unused variable");
    SDL_Log("  Code:    int result = compute();  // result is never read");
    SDL_Log("  GCC:     main.c:10:9: warning: unused variable 'result'");
    SDL_Log("  Fix:     Remove the variable, or use (void)result;");
    SDL_Log(" ");

    /* Implicit conversion */
    SDL_Log("[B] Implicit conversion losing precision");
    SDL_Log("  Code:    int x = 3.14f;  // float -> int truncates");
    SDL_Log("  GCC:     main.c:5:13: warning: conversion from 'float' to");
    SDL_Log("           'int' may change value [-Wfloat-conversion]");
    SDL_Log("  Fix:     Use an explicit cast: int x = (int)3.14f;");
    SDL_Log(" ");

    /* Format specifier mismatch */
    SDL_Log("[C] Format specifier mismatch");
    SDL_Log("  Code:    float f = 3.14f;");
    SDL_Log("           SDL_Log(\"%%d\", f);  // %%d expects int, got float");
    SDL_Log("  GCC:     main.c:7:20: warning: format '%%d' expects argument");
    SDL_Log("           of type 'int', but argument 2 has type 'double'");
    SDL_Log("  Fix:     Use %%f for float/double: SDL_Log(\"%%f\", f);");
    SDL_Log(" ");

    /* Uninitialized variable */
    SDL_Log("[D] Uninitialized variable");
    SDL_Log("  Code:    int x;");
    SDL_Log("           SDL_Log(\"%%d\", x);  // x has no value assigned");
    SDL_Log("  GCC:     main.c:8:5: warning: 'x' is used uninitialized");
    SDL_Log("  Fix:     Always initialize variables: int x = 0;");
    SDL_Log(" ");

    /* Recommended warning flags */
    SDL_Log("------------------------------------------------------------");
    SDL_Log("Recommended compiler flags:");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");
    SDL_Log("  GCC/Clang:  -Wall -Wextra -Wpedantic");
    SDL_Log("    -Wall    -> Enable most common warnings");
    SDL_Log("    -Wextra  -> Enable additional useful warnings");
    SDL_Log("    -Wpedantic -> Enforce strict C standard conformance");
    SDL_Log(" ");
    SDL_Log("  MSVC:       /W4");
    SDL_Log("    /W4      -> Warning level 4 (most warnings enabled)");
    SDL_Log(" ");
    SDL_Log("  To treat warnings as errors (recommended for new projects):");
    SDL_Log("    GCC/Clang: -Werror");
    SDL_Log("    MSVC:      /WX");
    SDL_Log(" ");
    SDL_Log("  In CMake:");
    SDL_Log("    target_compile_options(my_target PRIVATE");
    SDL_Log("        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>)");
}

/* ── Section 6: Strategy for fixing errors ───────────────────────────────── */
/*
 * When you see a wall of error messages, the most important skill is
 * knowing WHERE to start.  This section teaches a systematic approach.
 */
static void demo_fixing_strategy(void)
{
    print_divider("6. Strategy for Fixing Errors");

    SDL_Log("When you see many errors, follow this strategy:");
    SDL_Log(" ");
    SDL_Log("  RULE 1: Fix the FIRST error first.");
    SDL_Log(" ");
    SDL_Log("    One mistake can cause a cascade of follow-on errors.");
    SDL_Log("    A missing semicolon on line 10 might produce 20 errors");
    SDL_Log("    on lines 11-50.  Fix line 10, recompile, and most of");
    SDL_Log("    those 20 errors will disappear.");
    SDL_Log(" ");
    SDL_Log("  RULE 2: Read the FULL error message.");
    SDL_Log(" ");
    SDL_Log("    Do not just read the word 'error'.  Read:");
    SDL_Log("      - The file name and line number");
    SDL_Log("      - The error description");
    SDL_Log("      - The source line and caret (^) position");
    SDL_Log("      - Any 'note:' messages that follow (they add context)");
    SDL_Log(" ");
    SDL_Log("  RULE 3: Check one line above the reported line.");
    SDL_Log(" ");
    SDL_Log("    Many errors (missing semicolons, unclosed braces) are");
    SDL_Log("    reported on the line AFTER the actual mistake.  The");
    SDL_Log("    compiler does not know the statement is incomplete until");
    SDL_Log("    it reaches the next token.");
    SDL_Log(" ");
    SDL_Log("  RULE 4: Identify the error PHASE.");
    SDL_Log(" ");
    SDL_Log("    Compiler error?  -> Look at the source code on that line.");
    SDL_Log("    Linker error?    -> Check CMakeLists.txt (missing library");
    SDL_Log("                       or source file).");
    SDL_Log("    Runtime crash?   -> Use a debugger or add SDL_Log() calls");
    SDL_Log("                       to narrow down the location.");
    SDL_Log(" ");
    SDL_Log("  RULE 5: Search the error message.");
    SDL_Log(" ");
    SDL_Log("    Copy the key phrase (e.g., 'undefined reference to') into");
    SDL_Log("    a search engine.  Thousands of developers have seen the");
    SDL_Log("    same error.  The top results usually explain the cause");
    SDL_Log("    and the fix.");
    SDL_Log(" ");

    SDL_Log("------------------------------------------------------------");
    SDL_Log("Quick reference: Error -> likely cause -> fix");
    SDL_Log("------------------------------------------------------------");
    SDL_Log(" ");
    SDL_Log("  'expected ;'               -> Missing semicolon above");
    SDL_Log("  'undeclared identifier'     -> Typo or missing #include");
    SDL_Log("  'implicit declaration'      -> Missing #include or forward decl");
    SDL_Log("  'file not found'            -> Wrong #include path or");
    SDL_Log("                                missing target_include_directories");
    SDL_Log("  'undefined reference'       -> Missing source file in CMake or");
    SDL_Log("                                missing target_link_libraries");
    SDL_Log("  'multiple definition'       -> Function defined in multiple .c");
    SDL_Log("                                files (use 'static' or fix ODR)");
    SDL_Log("  'unresolved external'       -> MSVC linker, same as above");
    SDL_Log("  'segmentation fault'        -> NULL deref, use-after-free,");
    SDL_Log("                                or out-of-bounds access");
    SDL_Log("  'access violation'          -> Windows equivalent of segfault");
    SDL_Log("  'SDL_GetError: ...'         -> SDL function failed, read the");
    SDL_Log("                                error string for details");
}
