/*
 * greeting.h — A tiny helper module for Engine Lesson 02.
 *
 * This file exists so the lesson can demonstrate:
 *   - add_executable with multiple source files
 *   - How the compiler resolves #include paths
 *   - What happens when you forget to list a .c file in CMake
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef GREETING_H
#define GREETING_H

/* Return a greeting string that includes the SDL version.
 * This function lives in greeting.c — a separate translation unit from main.c.
 * The linker combines both object files into the final executable. */
const char *get_greeting(void);

/* Return a short description of what this lesson demonstrates. */
const char *get_lesson_topic(void);

#endif /* GREETING_H */
