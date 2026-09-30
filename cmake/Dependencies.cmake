# P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#
# Third-party code fetched at configure time. Versions are pinned (see docs/DECISIONS.md);
# change them only with the user's approval. Every entry is listed in THIRD_PARTY.md.

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# JUCE 8 (AGPLv3 while P5X is open source; a paid release needs a commercial JUCE license)
FetchContent_Declare(juce
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.zip
    URL_HASH SHA256=72838128c730eba22c63dfdc8bf155304887a9f4e9d9f61ec569c7f8e4c3fab1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(juce)

if(P5X_BUILD_TESTS)
    # Catch2 v3 (BSL-1.0)
    FetchContent_Declare(Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.zip
        URL_HASH SHA256=1e96cca4ce3bfbf1f20efff50c3a16fcc818d4bbd65c2c51155f976ee2bd0b8a
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()

if(P5X_ENABLE_ASIO)
    # Steinberg ASIO SDK 2.3.4, used under the GPLv3 option of its dual license.
    # Fetched at build time and never committed (docs/12-conventions.md).
    FetchContent_Declare(asiosdk
        URL https://download.steinberg.net/sdk_downloads/ASIO-SDK_2.3.4_2025-10-15.zip
        URL_HASH SHA256=d5ebf0c20dd2c5f43771fd0c1418f4b361bf52434ee670097cfa6b3a335e2eca
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(asiosdk)
endif()
