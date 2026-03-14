# Basis Universal transcoder — UASTC/ETC1S -> BC7/BC5/etc at runtime
# Apache 2.0 license (see LICENSE)
add_library(basisu_transcoder STATIC
    transcoder/basisu_transcoder.cpp
    zstd/zstddeclib.c
    basisu_c_wrapper.cpp
)
target_include_directories(basisu_transcoder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# The transcoder uses if constexpr (C++17) in recent versions
set_target_properties(basisu_transcoder PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
# Suppress compiler warnings in third-party code
if(MSVC)
    target_compile_options(basisu_transcoder PRIVATE /W0)
else()
    target_compile_options(basisu_transcoder PRIVATE -w)
endif()
# Avoid MSVC iterator debug level issues in Release builds.
# Enable Zstd supercompression support using the bundled zstddeclib
# (decode-only, single-file).  Our pipeline produces non-Zstd KTX2, but
# enabling it allows loading third-party KTX2 files with Zstd compression.
target_compile_definitions(basisu_transcoder PRIVATE
    BASISU_NO_ITERATOR_DEBUG_LEVEL=1
    BASISD_SUPPORT_KTX2_ZSTD=1
)
