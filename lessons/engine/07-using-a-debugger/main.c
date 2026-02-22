/*
 * Engine Lesson 07 — Using a Debugger
 *
 * Demonstrates: How to use a debugger (GDB, LLDB, or Visual Studio) to find
 * and fix two categories of bugs that error messages alone cannot solve:
 *
 *   1. Crashes      — The program stops with a segfault or access violation,
 *                     and the error message does not tell you which line.
 *   2. Logic errors — The program runs to completion, but produces wrong
 *                     results.  No error message at all.
 *
 * This program contains several small functions that illustrate the concepts
 * a debugger provides: breakpoints, stepping, variable inspection, and
 * call stack navigation.
 *
 * Why this lesson exists:
 *   Engine Lesson 06 taught you to read error messages from the compiler,
 *   linker, and runtime.  But two kinds of bugs produce messages that do not
 *   point to a source line:  crashes say "Segmentation fault" with no file
 *   or line number, and logic errors produce no message at all — the output
 *   is simply wrong.  A debugger fills this gap by letting you pause the
 *   program, inspect its state, and step through it line by line.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>

/* ── Forward declarations ──────────────────────────────────────────────────── */

static void demo_why_debugger(void);
static void demo_breakpoints(void);
static void demo_stepping(void);
static void demo_inspecting_variables(void);
static void demo_finding_a_crash(void);
static void demo_finding_a_logic_error(void);
static void demo_call_stack(void);
static void demo_conditional_breakpoints(void);
static void demo_watchpoints(void);

/* ── Helpers ───────────────────────────────────────────────────────────────── */

/* Print a horizontal divider to separate sections in the output. */
static void print_divider(const char *title)
{
    SDL_Log(" ");
    SDL_Log("============================================================");
    SDL_Log("  %s", title);
    SDL_Log("============================================================");
}

/* ── Main ──────────────────────────────────────────────────────────────────── */

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

    SDL_Log("=== Engine Lesson 07: Using a Debugger ===");

    demo_why_debugger();
    demo_breakpoints();
    demo_stepping();
    demo_inspecting_variables();
    demo_finding_a_crash();
    demo_finding_a_logic_error();
    demo_call_stack();
    demo_conditional_breakpoints();
    demo_watchpoints();

    SDL_Log(" ");
    SDL_Log("=== All sections complete ===");

    SDL_Quit();
    return 0;
}

/* ── Section 1: Why you need a debugger ────────────────────────────────────── */
/*
 * Engine Lesson 06 covered three categories of errors:
 *   - Compiler errors   (caught at build time — have file:line:col)
 *   - Linker errors     (caught at link time — mention symbol names)
 *   - Runtime errors    (happen during execution — SDL_GetError helps)
 *
 * But two kinds of runtime problems do NOT give you a source location:
 *
 *   1. Crashes (segfault / access violation)
 *      The OS kills the program.  You get "Segmentation fault" but no line.
 *
 *   2. Logic errors
 *      The program runs fine but produces wrong output.  No error at all.
 *
 * A debugger solves both by letting you:
 *   - PAUSE the program at any line  (breakpoints)
 *   - EXECUTE one line at a time     (stepping)
 *   - READ variable values at pause  (inspecting)
 *   - SEE the function call chain    (call stack)
 */
static void demo_why_debugger(void)
{
    print_divider("1. Why You Need a Debugger");

    SDL_Log("Engine Lesson 06 taught you to read error messages.");
    SDL_Log("But two kinds of bugs do not give you a source line:");
    SDL_Log(" ");
    SDL_Log("  1. Crashes (segfault / access violation)");
    SDL_Log("     You see: 'Segmentation fault (core dumped)'");
    SDL_Log("     Missing: which file, which line, which variable");
    SDL_Log(" ");
    SDL_Log("  2. Logic errors");
    SDL_Log("     You see: wrong output (e.g. total = 0 instead of 150)");
    SDL_Log("     Missing: any error message at all");
    SDL_Log(" ");
    SDL_Log("A debugger fills this gap with four core capabilities:");
    SDL_Log(" ");
    SDL_Log("  Breakpoints  -> Pause the program at a specific line");
    SDL_Log("  Stepping     -> Execute one line at a time");
    SDL_Log("  Inspecting   -> Read the value of any variable while paused");
    SDL_Log("  Call stack   -> See which function called which");
    SDL_Log(" ");
    SDL_Log("The rest of this lesson demonstrates each one.");
}

/* ── Section 2: Breakpoints ────────────────────────────────────────────────── */
/*
 * A breakpoint tells the debugger: "Pause the program when execution
 * reaches this line."  The program runs at full speed until it hits the
 * breakpoint, then stops and waits for your commands.
 *
 * Setting a breakpoint does NOT change the program.  It is purely an
 * instruction to the debugger.  You can add and remove breakpoints at
 * any time, even while the program is paused.
 */
static void demo_breakpoints(void)
{
    print_divider("2. Breakpoints");

    SDL_Log("A breakpoint pauses the program at a specific line.");
    SDL_Log("The program runs at full speed until it hits that line.");
    SDL_Log(" ");
    SDL_Log("Setting a breakpoint (by line number):");
    SDL_Log(" ");
    SDL_Log("  GDB:    break main.c:42");
    SDL_Log("  LLDB:   breakpoint set --file main.c --line 42");
    SDL_Log("  VS:     Click the left margin on line 42 (red dot)");
    SDL_Log(" ");
    SDL_Log("Setting a breakpoint (by function name):");
    SDL_Log(" ");
    SDL_Log("  GDB:    break calculate_total");
    SDL_Log("  LLDB:   breakpoint set --name calculate_total");
    SDL_Log("  VS:     Right-click function name -> 'Run to Cursor'");
    SDL_Log(" ");

    /* Walk through a concrete example. */
    int prices[] = {25, 50, 75};
    int count = 3;
    int total = 0;

    SDL_Log("Example: stepping through a loop to compute a total.");
    SDL_Log(" ");
    SDL_Log("  int prices[] = {25, 50, 75};");
    SDL_Log("  int total = 0;");
    SDL_Log("  for (int i = 0; i < count; i++)");
    SDL_Log("      total += prices[i];");
    SDL_Log(" ");
    SDL_Log("Set a breakpoint on the 'total += prices[i]' line.");
    SDL_Log("Each time the debugger pauses there, inspect 'i' and 'total':");
    SDL_Log(" ");

    for (int i = 0; i < count; i++) {
        total += prices[i];
        /* If you set a breakpoint on the line above, the debugger pauses
         * here on each iteration.  You can inspect 'i', 'total', and
         * 'prices[i]' to watch the accumulation happen step by step. */
        SDL_Log("  Hit breakpoint: i=%d, prices[i]=%d, total=%d", i, prices[i], total);
    }

    SDL_Log(" ");
    SDL_Log("Final total: %d (expected 150)", total);
    SDL_Log(" ");
    SDL_Log("Managing breakpoints:");
    SDL_Log(" ");
    SDL_Log("  GDB:    info breakpoints     (list all)");
    SDL_Log("          delete 1             (remove breakpoint #1)");
    SDL_Log("          disable 2            (keep but skip breakpoint #2)");
    SDL_Log("  LLDB:   breakpoint list");
    SDL_Log("          breakpoint delete 1");
    SDL_Log("          breakpoint disable 2");
    SDL_Log("  VS:     Debug -> Windows -> Breakpoints (list all)");
}

/* ── Section 3: Stepping ──────────────────────────────────────────────────── */
/*
 * Once the program is paused at a breakpoint, you control execution one
 * line at a time.  The three stepping commands are:
 *
 *   Step Over  — Execute the current line, including any function calls on
 *                it, then pause on the NEXT line.  You stay at the same
 *                level of the call stack.
 *
 *   Step Into  — If the current line calls a function, enter that function
 *                and pause at its first line.  You go deeper into the call
 *                stack.
 *
 *   Step Out   — Run until the current function returns, then pause at the
 *                line that called it.  You go back up the call stack.
 */

/* Helper: compute the dot product of two 3-element float arrays.
 * Used to demonstrate stepping INTO a function. */
static float dot_product(const float *a, const float *b, int n)
{
    float result = 0.0f;
    for (int i = 0; i < n; i++) {
        result += a[i] * b[i];
    }
    return result;
}

/* Helper: normalize a 3-element vector in-place.
 * Used to demonstrate stepping OVER a function. */
static void normalize(float *v, int n)
{
    float len_sq = dot_product(v, v, n);
    if (len_sq > 0.0f) {
        float inv_len = 1.0f / SDL_sqrtf(len_sq);
        for (int i = 0; i < n; i++) {
            v[i] *= inv_len;
        }
    }
}

static void demo_stepping(void)
{
    print_divider("3. Stepping");

    SDL_Log("Once paused at a breakpoint, you control execution line by line.");
    SDL_Log(" ");
    SDL_Log("Three stepping commands:");
    SDL_Log(" ");
    SDL_Log("                  GDB       LLDB      VS Shortcut");
    SDL_Log("  Step Over:      next      next      F10");
    SDL_Log("  Step Into:      step      step      F11");
    SDL_Log("  Step Out:       finish    finish    Shift+F11");
    SDL_Log("  Continue:       continue  continue  F5");
    SDL_Log(" ");
    SDL_Log("What each command does:");
    SDL_Log(" ");
    SDL_Log("  Step Over  -> Execute the line (including function calls),");
    SDL_Log("                pause on the NEXT line. Stay at the same level.");
    SDL_Log("  Step Into  -> Enter the function call on the current line.");
    SDL_Log("                Pause at the FIRST line of that function.");
    SDL_Log("  Step Out   -> Run until the current function returns.");
    SDL_Log("                Pause at the line that called it.");
    SDL_Log("  Continue   -> Resume full-speed execution until the next");
    SDL_Log("                breakpoint (or program exit).");
    SDL_Log(" ");

    /* Demonstrate with real code. */
    float light_dir[] = {1.0f, 2.0f, 3.0f};
    float normal[]    = {0.0f, 1.0f, 0.0f};

    SDL_Log("Example: computing a lighting dot product.");
    SDL_Log(" ");
    SDL_Log("  float light_dir[] = {1.0, 2.0, 3.0};");
    SDL_Log("  float normal[]    = {0.0, 1.0, 0.0};");
    SDL_Log("  normalize(light_dir, 3);");
    SDL_Log("  float intensity = dot_product(light_dir, normal, 3);");
    SDL_Log(" ");
    SDL_Log("Set a breakpoint on the normalize() call:");
    SDL_Log(" ");
    SDL_Log("  Step Over  -> Runs normalize() to completion,");
    SDL_Log("                pauses on the dot_product() line.");
    SDL_Log("                light_dir is now normalized. You see the result.");
    SDL_Log(" ");
    SDL_Log("  Step Into  -> Enters normalize(), pauses at its first line:");
    SDL_Log("                'float len_sq = dot_product(v, v, n);'");
    SDL_Log("                You can watch it compute the length step by step.");

    normalize(light_dir, 3);
    float intensity = dot_product(light_dir, normal, 3);

    SDL_Log(" ");
    SDL_Log("After stepping through:");
    SDL_Log("  light_dir (normalized) = {%.3f, %.3f, %.3f}",
            light_dir[0], light_dir[1], light_dir[2]);
    SDL_Log("  intensity = dot(light_dir, normal) = %.3f", intensity);
    SDL_Log(" ");
    SDL_Log("When to use each:");
    SDL_Log("  Step Over  -> You trust the function works; skip the details");
    SDL_Log("  Step Into  -> You suspect the bug is INSIDE this function");
    SDL_Log("  Step Out   -> You stepped into a function by mistake; get out");
}

/* ── Section 4: Inspecting variables ───────────────────────────────────────── */
/*
 * While the program is paused, you can read (and sometimes modify) any
 * variable that is in scope at the current line.  This is the most
 * frequently used debugger feature: seeing what the program's state
 * actually is, versus what you expected it to be.
 */

/* A simple struct representing a 2D sprite, used to show struct inspection. */
typedef struct Sprite {
    float x, y;          /* position */
    float width, height; /* size */
    int   frame;         /* current animation frame */
    bool  visible;       /* whether to draw */
} Sprite;

static void demo_inspecting_variables(void)
{
    print_divider("4. Inspecting Variables");

    SDL_Log("While paused, you can read any variable in the current scope.");
    SDL_Log(" ");
    SDL_Log("Printing a variable:");
    SDL_Log(" ");
    SDL_Log("  GDB:    print total        (print one variable)");
    SDL_Log("          print prices[2]    (print an array element)");
    SDL_Log("          print *ptr         (dereference a pointer)");
    SDL_Log("  LLDB:   frame variable total");
    SDL_Log("          p total            (shorthand for 'expression total')");
    SDL_Log("  VS:     Hover over any variable in the source editor");
    SDL_Log("          (or use the Locals/Watch window)");
    SDL_Log(" ");

    /* Demonstrate with a struct. */
    Sprite player;
    player.x       = 100.0f;
    player.y       = 200.0f;
    player.width   = 32.0f;
    player.height  = 48.0f;
    player.frame   = 3;
    player.visible = true;

    SDL_Log("Example: inspecting a Sprite struct.");
    SDL_Log(" ");
    SDL_Log("  Sprite player = {100, 200, 32, 48, 3, true};");
    SDL_Log(" ");
    SDL_Log("In GDB, you can print the whole struct at once:");
    SDL_Log(" ");
    SDL_Log("  (gdb) print player");
    SDL_Log("  $1 = {x = 100, y = 200, width = 32, height = 48,");
    SDL_Log("        frame = 3, visible = true}");
    SDL_Log(" ");
    SDL_Log("Or individual members:");
    SDL_Log(" ");
    SDL_Log("  (gdb) print player.x");
    SDL_Log("  $2 = 100");
    SDL_Log(" ");

    /* Arrays */
    float vertices[] = {0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f};
    int vertex_count = 3;
    int floats_per_vertex = 2;

    SDL_Log("Inspecting arrays:");
    SDL_Log(" ");
    SDL_Log("  (gdb) print vertices");
    SDL_Log("  $3 = {0, 0.5, -0.5, -0.5, 0.5, -0.5}");
    SDL_Log(" ");
    SDL_Log("  (gdb) print vertices[0]");
    SDL_Log("  $4 = 0");
    SDL_Log(" ");
    SDL_Log("For heap-allocated arrays (pointers), GDB needs the size:");
    SDL_Log(" ");
    SDL_Log("  (gdb) print *heap_array@10    (print 10 elements)");
    SDL_Log("  (lldb) memory read -t float -c 10 heap_array");
    SDL_Log(" ");

    /* Watch expressions */
    SDL_Log("Watch expressions (auto-print when value changes):");
    SDL_Log(" ");
    SDL_Log("  GDB:    watch total");
    SDL_Log("  LLDB:   watchpoint set variable total");
    SDL_Log("  VS:     Right-click variable -> 'Add Watch'");
    SDL_Log(" ");
    SDL_Log("The debugger pauses whenever 'total' changes value.");
    SDL_Log("This is how you find 'who is modifying my variable?'");
    SDL_Log(" ");

    /* Modifying variables */
    SDL_Log("You can also MODIFY variables while paused:");
    SDL_Log(" ");
    SDL_Log("  GDB:    set variable player.x = 50");
    SDL_Log("  LLDB:   expression player.x = 50");
    SDL_Log("  VS:     Double-click the value in the Watch window and type");
    SDL_Log(" ");
    SDL_Log("This lets you test fixes without recompiling.");

    /* Suppress unused warnings */
    (void)player;
    (void)vertices;
    (void)vertex_count;
    (void)floats_per_vertex;
}

/* ── Section 5: Finding a crash ────────────────────────────────────────────── */
/*
 * Crashes (segmentation faults, access violations) are the most common
 * reason beginners reach for a debugger.  The crash message tells you
 * nothing about WHERE it happened.  The debugger tells you exactly:
 *   - Which line caused the crash
 *   - Which pointer was NULL or invalid
 *   - The entire call stack leading to the crash
 */

/* A particle with a pointer to its parent emitter.
 * This setup creates a scenario where a NULL pointer causes a crash. */
typedef struct Emitter {
    float spawn_rate;
} Emitter;

typedef struct Particle {
    float    x, y;
    float    velocity_x, velocity_y;
    Emitter *emitter;  /* NULL if the particle is orphaned */
} Particle;

/* Updates a particle's position.  Contains a crash bug: it reads from
 * the emitter pointer without checking for NULL. */
static void update_particle(Particle *p, float dt)
{
    p->x += p->velocity_x * dt;
    p->y += p->velocity_y * dt;

    /* BUG: p->emitter might be NULL.  In a debugger, when this crashes:
     *
     *   (gdb) run
     *   Program received signal SIGSEGV, Segmentation fault.
     *   0x... in update_particle (p=0x..., dt=0.016) at main.c:THIS_LINE
     *
     *   (gdb) print p->emitter
     *   $1 = (Emitter *) 0x0          <-- NULL!
     *
     * The debugger shows you exactly which pointer was NULL and on which
     * line the crash happened. */
    float rate = p->emitter->spawn_rate;
    (void)rate;
}

static void demo_finding_a_crash(void)
{
    print_divider("5. Finding a Crash");

    SDL_Log("When a program crashes, the OS says 'Segmentation fault'");
    SDL_Log("but does NOT tell you which line or variable caused it.");
    SDL_Log(" ");
    SDL_Log("A debugger catches the crash and shows you:");
    SDL_Log("  - The exact line where the crash happened");
    SDL_Log("  - The value of every variable at that point");
    SDL_Log("  - The call stack (which function called which)");
    SDL_Log(" ");

    /* Set up particles — the third one has a NULL emitter. */
    Emitter em = {10.0f};

    Particle particles[3];
    particles[0] = (Particle){0.0f,  0.0f,  1.0f, 0.5f, &em};
    particles[1] = (Particle){5.0f,  3.0f, -1.0f, 0.0f, &em};
    particles[2] = (Particle){10.0f, 7.0f,  0.0f, 1.0f, NULL};  /* orphaned */

    float dt = 0.016f;

    SDL_Log("Example: updating 3 particles (one has emitter = NULL).");
    SDL_Log(" ");
    SDL_Log("  Particle particles[3];");
    SDL_Log("  particles[2].emitter = NULL;  // orphaned particle");
    SDL_Log("  for (int i = 0; i < 3; i++)");
    SDL_Log("      update_particle(&particles[i], dt);  // crashes on i=2");
    SDL_Log(" ");
    SDL_Log("Without a debugger, you see:");
    SDL_Log("  Segmentation fault (core dumped)");
    SDL_Log("  -- That is all.  No file, no line, no variable name.");
    SDL_Log(" ");
    SDL_Log("With a debugger:");
    SDL_Log(" ");
    SDL_Log("  (gdb) run");
    SDL_Log("  Program received signal SIGSEGV, Segmentation fault.");
    SDL_Log("  update_particle (p=0x7fff..., dt=0.016) at main.c:NNN");
    SDL_Log("  NNN    float rate = p->emitter->spawn_rate;");
    SDL_Log(" ");
    SDL_Log("  (gdb) print p->emitter");
    SDL_Log("  $1 = (Emitter *) 0x0");
    SDL_Log(" ");
    SDL_Log("  (gdb) backtrace");
    SDL_Log("  #0  update_particle (p=0x7fff...) at main.c:NNN");
    SDL_Log("  #1  demo_finding_a_crash () at main.c:NNN");
    SDL_Log("  #2  main (argc=1, argv=0x7fff...) at main.c:NNN");
    SDL_Log(" ");
    SDL_Log("Now you know: p->emitter is NULL on the third particle.");
    SDL_Log("The fix: check for NULL before dereferencing.");
    SDL_Log(" ");
    SDL_Log("  if (p->emitter) {");
    SDL_Log("      float rate = p->emitter->spawn_rate;");
    SDL_Log("  }");

    /* Run the safe version so the program does not actually crash. */
    for (int i = 0; i < 3; i++) {
        if (particles[i].emitter) {
            update_particle(&particles[i], dt);
            SDL_Log(" ");
            SDL_Log("  Particle %d updated: (%.2f, %.2f)",
                    i, particles[i].x, particles[i].y);
        } else {
            SDL_Log("  Particle %d skipped: emitter is NULL", i);
        }
    }
}

/* ── Section 6: Finding a logic error ──────────────────────────────────────── */
/*
 * Logic errors are harder than crashes because the program does not fail —
 * it produces wrong results silently.  The debugger helps by letting you
 * step through the code and compare actual values against expected values
 * at each step.
 *
 * Common logic errors in graphics code:
 *   - Off-by-one errors in loops (processing N-1 or N+1 elements)
 *   - Wrong operator (e.g., = instead of +=)
 *   - Integer division truncation (5 / 2 = 2, not 2.5)
 *   - Incorrect order of operations in math expressions
 */

/* Calculates the average brightness of an array of pixel values (0-255).
 * Deliberately uses integer division to demonstrate truncation as a
 * teaching point — the debugger lets you see where precision is lost. */
static int calculate_average_brightness(const int *pixels, int count)
{
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += pixels[i];
    }
    /* Integer division truncates: with pixels = {201, 150, 180, 220, 170},
     * sum = 921, count = 5, so 921 / 5 = 184 — the true average 184.2 is
     * silently truncated to 184.  A debugger lets you print 'sum' and
     * 'count' at this line to verify the calculation yourself.
     *
     * A more serious bug: if you accidentally wrote 'sum / (count + 1)' —
     * an off-by-one in the divisor — the result would be 153 instead of
     * 184.  The debugger lets you watch 'sum' accumulate and inspect the
     * divisor to catch exactly this kind of mistake. */
    return sum / count;
}

/* Applies a 1D box blur to an array of values.
 * Correct implementation — use the debugger to verify each step. */
static void box_blur(const float *input, float *output, int count, int radius)
{
    for (int i = 0; i < count; i++) {
        float sum = 0.0f;
        int samples = 0;

        /* Sum the neighborhood: [i - radius, i + radius] */
        for (int j = i - radius; j <= i + radius; j++) {
            /* Clamp to array bounds */
            if (j >= 0 && j < count) {
                sum += input[j];
                samples++;
            }
        }

        output[i] = (samples > 0) ? sum / (float)samples : 0.0f;
    }
}

static void demo_finding_a_logic_error(void)
{
    print_divider("6. Finding a Logic Error");

    SDL_Log("Logic errors are silent — the program runs but gives wrong results.");
    SDL_Log("There is no error message.  The debugger lets you step through the");
    SDL_Log("code and compare actual values against what you expected.");
    SDL_Log(" ");
    SDL_Log("Technique: set a breakpoint inside the loop, then step through");
    SDL_Log("each iteration, checking the running total at each step.");
    SDL_Log(" ");

    /* Example 1: average brightness — pixel values deliberately sum to 921,
     * which is NOT a multiple of 5, so integer division truncates. */
    int pixels[] = {201, 150, 180, 220, 170};
    int pixel_count = 5;
    int average = calculate_average_brightness(pixels, pixel_count);

    SDL_Log("Example 1: average brightness of {201, 150, 180, 220, 170}");
    SDL_Log(" ");
    SDL_Log("  True average: (201 + 150 + 180 + 220 + 170) / 5 = 184.2");
    SDL_Log("  Got (int):    %d  (truncated from 184.2)", average);
    SDL_Log(" ");

    SDL_Log("Debugging approach:");
    SDL_Log("  1. Set a breakpoint on 'sum += pixels[i]'");
    SDL_Log("  2. Run the program — it pauses on the first iteration");
    SDL_Log("  3. Print 'i', 'pixels[i]', and 'sum' at each pause:");
    SDL_Log(" ");
    SDL_Log("  (gdb) print i");
    SDL_Log("  (gdb) print pixels[i]");
    SDL_Log("  (gdb) print sum");
    SDL_Log("  (gdb) continue      (go to next iteration)");
    SDL_Log(" ");
    SDL_Log("  Iteration | i | pixels[i] | sum after add");
    SDL_Log("  ----------+---+-----------+--------------");

    /* Replay the calculation with logging. */
    int sum = 0;
    for (int i = 0; i < pixel_count; i++) {
        sum += pixels[i];
        SDL_Log("  %9d | %d | %9d | %d", i + 1, i, pixels[i], sum);
    }

    SDL_Log(" ");
    SDL_Log("  Final: sum=%d / count=%d = %d (true: %.1f — truncated)",
            sum, pixel_count, sum / pixel_count, (float)sum / pixel_count);
    SDL_Log(" ");

    /* Example 2: box blur */
    SDL_Log("Example 2: 1D box blur (radius=1) on {10, 20, 30, 40, 50}");
    SDL_Log(" ");

    float blur_in[]  = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    float blur_out[5] = {0};
    int blur_count  = 5;
    int blur_radius = 1;

    box_blur(blur_in, blur_out, blur_count, blur_radius);

    SDL_Log("  Input:    ");
    for (int i = 0; i < blur_count; i++) {
        SDL_Log("    [%d] = %.1f", i, blur_in[i]);
    }
    SDL_Log("  Output (radius=%d):", blur_radius);
    for (int i = 0; i < blur_count; i++) {
        SDL_Log("    [%d] = %.1f", i, blur_out[i]);
    }
    SDL_Log(" ");
    SDL_Log("  Verify by hand: output[2] = (20+30+40)/3 = 30.0  [OK]");
    SDL_Log("  If the output were wrong, set a breakpoint in box_blur()");
    SDL_Log("  and watch 'sum' and 'samples' accumulate for one element.");
}

/* ── Section 7: Call stack ─────────────────────────────────────────────────── */
/*
 * The call stack (also called backtrace or stack trace) shows you the chain
 * of function calls that led to the current line.  This is essential for
 * understanding HOW the program reached a particular point.
 *
 * Reading a call stack:
 *   - Frame #0 is WHERE you are now (the current function)
 *   - Frame #1 is WHO called the current function
 *   - Frame #2 is WHO called that caller, and so on up to main()
 */

static float apply_damage(float health, float damage, float armor)
{
    float effective_damage = damage * (1.0f - armor);
    float new_health = health - effective_damage;
    /* A breakpoint here lets you inspect the calculation. */
    return (new_health < 0.0f) ? 0.0f : new_health;
}

static float process_hit(float health, float base_damage, float armor)
{
    /* Doubles the damage for a critical hit */
    float crit_damage = base_damage * 2.0f;
    return apply_damage(health, crit_damage, armor);
}

static void demo_call_stack(void)
{
    print_divider("7. The Call Stack");

    SDL_Log("The call stack shows the chain of function calls leading");
    SDL_Log("to the current line.  It answers: 'How did I get here?'");
    SDL_Log(" ");
    SDL_Log("Commands:");
    SDL_Log(" ");
    SDL_Log("  GDB:    backtrace          (or 'bt' for short)");
    SDL_Log("  LLDB:   thread backtrace   (or 'bt')");
    SDL_Log("  VS:     Debug -> Windows -> Call Stack");
    SDL_Log(" ");

    float health = 100.0f;
    float damage = 30.0f;
    float armor  = 0.25f;

    float new_health = process_hit(health, damage, armor);

    SDL_Log("Example: breakpoint inside apply_damage().");
    SDL_Log(" ");
    SDL_Log("  float new_health = process_hit(100.0, 30.0, 0.25);");
    SDL_Log(" ");
    SDL_Log("  (gdb) backtrace");
    SDL_Log("  #0  apply_damage (health=100, damage=60, armor=0.25) at main.c:NNN");
    SDL_Log("  #1  process_hit (health=100, base_damage=30, armor=0.25) at main.c:NNN");
    SDL_Log("  #2  demo_call_stack () at main.c:NNN");
    SDL_Log("  #3  main (argc=1, argv=0x7fff...) at main.c:NNN");
    SDL_Log(" ");
    SDL_Log("Reading the call stack from top to bottom:");
    SDL_Log("  #0  You are HERE: inside apply_damage()");
    SDL_Log("  #1  apply_damage was called by process_hit()");
    SDL_Log("  #2  process_hit was called by demo_call_stack()");
    SDL_Log("  #3  demo_call_stack was called by main()");
    SDL_Log(" ");
    SDL_Log("Notice: damage=60 in frame #0 but base_damage=30 in frame #1.");
    SDL_Log("process_hit doubled the damage (crit hit). The call stack");
    SDL_Log("shows you where and how the value was transformed.");
    SDL_Log(" ");
    SDL_Log("Switching between frames:");
    SDL_Log(" ");
    SDL_Log("  GDB:    frame 1           (switch to process_hit's frame)");
    SDL_Log("          print crit_damage  (30 * 2 = 60)");
    SDL_Log("  LLDB:   frame select 1");
    SDL_Log("          frame variable     (list all locals in that frame)");
    SDL_Log(" ");
    SDL_Log("  Result: health=%.1f -> new_health=%.1f (damage=%.1f, armor=%.0f%%)",
            health, new_health, damage * 2.0f, armor * 100.0f);
}

/* ── Section 8: Conditional breakpoints ────────────────────────────────────── */
/*
 * When a loop runs thousands of times, you do not want to stop on every
 * iteration.  A conditional breakpoint only pauses when a condition is true.
 *
 * This is especially useful in graphics code where loops process thousands
 * of vertices, pixels, or particles per frame.
 */
static void demo_conditional_breakpoints(void)
{
    print_divider("8. Conditional Breakpoints");

    SDL_Log("A conditional breakpoint only pauses when a condition is true.");
    SDL_Log("Essential when a loop runs thousands of times but the bug only");
    SDL_Log("appears on one specific iteration.");
    SDL_Log(" ");
    SDL_Log("Setting conditional breakpoints:");
    SDL_Log(" ");
    SDL_Log("  GDB:    break main.c:42 if i == 999");
    SDL_Log("  LLDB:   breakpoint set -f main.c -l 42 -c 'i == 999'");
    SDL_Log("  VS:     Right-click breakpoint -> Conditions -> i == 999");
    SDL_Log(" ");

    /* Demonstrate: find the first negative value in a data set. */
    #define DATA_COUNT 10
    float data[DATA_COUNT] = {
        1.2f, 3.4f, 0.5f, 2.1f, -0.3f,
        4.5f, 1.1f, -1.7f, 0.8f, 3.3f
    };

    SDL_Log("Example: find the first negative value in a dataset.");
    SDL_Log(" ");
    SDL_Log("  break main.c:NNN if data[i] < 0.0f");
    SDL_Log(" ");
    SDL_Log("The debugger skips all positive values and pauses only when");
    SDL_Log("it finds a negative one:");
    SDL_Log(" ");

    for (int i = 0; i < DATA_COUNT; i++) {
        if (data[i] < 0.0f) {
            SDL_Log("  Conditional breakpoint hit: i=%d, data[i]=%.1f", i, data[i]);
        }
    }

    SDL_Log(" ");
    SDL_Log("In graphics code, this is common:");
    SDL_Log("  break vertex_shader if vertex_id == 1024");
    SDL_Log("  break render_mesh if mesh->material == NULL");
    SDL_Log("  break update_particle if particle->lifetime < 0");
}

/* ── Section 9: Watchpoints ────────────────────────────────────────────────── */
/*
 * A watchpoint (also called a data breakpoint) pauses the program whenever
 * a specific variable changes value.  Unlike a breakpoint that fires at a
 * specific line, a watchpoint fires wherever the variable is modified —
 * even if you do not know which function modifies it.
 *
 * This is invaluable when a variable has the wrong value and you do not
 * know where it was last written.
 */
static void demo_watchpoints(void)
{
    print_divider("9. Watchpoints (Data Breakpoints)");

    SDL_Log("A watchpoint pauses the program whenever a variable's value");
    SDL_Log("changes.  Unlike breakpoints, you do not need to know WHICH");
    SDL_Log("line modifies the variable — the debugger finds it for you.");
    SDL_Log(" ");
    SDL_Log("Setting watchpoints:");
    SDL_Log(" ");
    SDL_Log("  GDB:    watch score");
    SDL_Log("  LLDB:   watchpoint set variable score");
    SDL_Log("  VS:     Debug -> New Breakpoint -> Data Breakpoint");
    SDL_Log("          Address: &score, Byte Count: 4");
    SDL_Log(" ");

    int score = 0;

    SDL_Log("Example: tracking changes to 'score'.");
    SDL_Log(" ");
    SDL_Log("  (gdb) watch score");
    SDL_Log("  Hardware watchpoint 1: score");
    SDL_Log(" ");

    score += 10;   /* Kill bonus */
    SDL_Log("  score changed: 0 -> %d  (kill bonus)", score);

    score += 50;   /* Quest complete */
    SDL_Log("  score changed: 10 -> %d  (quest complete)", score);

    score -= 5;    /* Penalty */
    SDL_Log("  score changed: 60 -> %d  (penalty)", score);

    SDL_Log(" ");
    SDL_Log("Each time score changes, the debugger pauses and shows:");
    SDL_Log(" ");
    SDL_Log("  Hardware watchpoint 1: score");
    SDL_Log("  Old value = 10");
    SDL_Log("  New value = 60");
    SDL_Log("  process_quest_reward (quest=0x...) at game.c:42");
    SDL_Log(" ");
    SDL_Log("This tells you exactly which function and line modified the");
    SDL_Log("variable, even if you did not expect that code to touch it.");
    SDL_Log(" ");
    SDL_Log("Watchpoints use hardware debug registers, so most CPUs support");
    SDL_Log("only 4 watchpoints at a time.  Use them for the variable you");
    SDL_Log("are investigating, not for general monitoring.");
}
