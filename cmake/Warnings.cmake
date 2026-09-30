# P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#
# /W4 /WX applies to P5X's own sources only (docs/12-conventions.md). JUCE module sources compile
# inside our targets too, so the flags are set per source file, not per target. Headers included
# with angle brackets (JUCE, Catch2, the standard library) are treated as external and don't warn.

add_compile_options(/utf-8 /permissive- /Zc:__cplusplus /external:anglebrackets /external:W0)

set(P5X_STRICT_FLAGS /W4 /WX)

# Marks source files as P5X-owned in the calling directory's scope.
function(p5x_strict_sources)
    set_source_files_properties(${ARGN} PROPERTIES COMPILE_OPTIONS "${P5X_STRICT_FLAGS}")
endfunction()
