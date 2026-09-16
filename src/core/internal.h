#ifndef COLONIZE_CORE_INTERNAL_H
#define COLONIZE_CORE_INTERNAL_H

/*
 * COLONIZE_INTERNAL marks a stage function (and its ctx struct / status enum)
 * that a module's <module>_internal.h exposes to unit tests while staying
 * file-local in production builds.
 *
 * colonize_core is built exactly once and shared by every test binary (see
 * CMakeLists.txt), so there is no separate "test build" of the core lib to
 * hang a narrower define off of. Rather than special-case COLONIZE_TESTING
 * per test target (which would require compiling colonize_core twice), the
 * define is applied globally to the one colonize_core target. The only
 * effect is symbol linkage (static -> external): the stage functions keep
 * their exact bodies and call sites, so this changes nothing about program
 * behaviour, only what a test binary linking colonize_core is able to call
 * directly. See docs/conventions.md "Code conventions" for the pattern.
 */
#ifdef COLONIZE_TESTING
#define COLONIZE_INTERNAL /* exported for tests */
#else
#define COLONIZE_INTERNAL static
#endif

#endif /* COLONIZE_CORE_INTERNAL_H */
