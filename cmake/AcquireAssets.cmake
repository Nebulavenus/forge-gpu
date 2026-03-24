# AcquireAssets.cmake — Three-tier asset acquisition
#
# Called as a CMake script (-P) by the forge-assets custom command.
# Acquires processed assets using a fallback chain:
#
#   1. Local assets/processed/ already exists → use it
#   2. Download pre-built tarball from the assets-latest GitHub release
#   3. Run the pipeline locally via uv
#
# Expected variables (passed via -D):
#   ASSETS_DIR   — absolute path to assets/processed/
#   RELEASE_TAG  — GitHub release tag for pre-built assets
#   SOURCE_DIR   — repository root (for running the pipeline)

# ── Tier 1: Local directory exists and is non-empty ──────────────────────────
if(EXISTS "${ASSETS_DIR}")
    file(GLOB _contents "${ASSETS_DIR}/*")
    list(FILTER _contents EXCLUDE REGEX "/\\.stamp$")
    list(LENGTH _contents _count)
    if(_count GREATER 0)
        message(STATUS "Processed assets found locally (${_count} entries)")
        # Touch the stamp file so CMake knows we're up to date
        file(WRITE "${ASSETS_DIR}/.stamp" "local")
        return()
    endif()
endif()

# ── Tier 2: Download from GitHub release ─────────────────────────────────────
find_program(GH_EXECUTABLE gh)
if(GH_EXECUTABLE)
    message(STATUS "Downloading processed assets from release '${RELEASE_TAG}'...")
    execute_process(
        COMMAND ${GH_EXECUTABLE} release download ${RELEASE_TAG}
            --pattern "processed-assets.tar.gz"
            --dir "${SOURCE_DIR}"
            --clobber
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE _dl_result
        OUTPUT_VARIABLE _dl_output
        ERROR_VARIABLE  _dl_error
    )
    if(_dl_result EQUAL 0 AND EXISTS "${SOURCE_DIR}/processed-assets.tar.gz")
        # Extract into assets/processed/.  The tarball MUST contain flat
        # contents (no root directory) — a rooted tarball would create a
        # nested processed/processed/ path.  Phase 6 CI creates the tarball
        # with: tar czf processed-assets.tar.gz -C assets/processed .
        message(STATUS "Extracting processed assets...")
        file(MAKE_DIRECTORY "${ASSETS_DIR}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf
                "${SOURCE_DIR}/processed-assets.tar.gz"
            WORKING_DIRECTORY "${ASSETS_DIR}"
            RESULT_VARIABLE _tar_result
        )
        file(REMOVE "${SOURCE_DIR}/processed-assets.tar.gz")
        if(_tar_result EQUAL 0)
            message(STATUS "Processed assets extracted successfully")
            file(WRITE "${ASSETS_DIR}/.stamp" "downloaded")
            return()
        else()
            # Clean up partial extraction so Tier 1 does not treat a
            # broken directory as valid on the next run.
            file(REMOVE_RECURSE "${ASSETS_DIR}")
            message(WARNING "Failed to extract processed-assets.tar.gz (exit ${_tar_result}) — removed ${ASSETS_DIR}")
        endif()
    else()
        message(STATUS "No release artifact available (${_dl_error})")
    endif()
else()
    message(STATUS "gh CLI not found — skipping release download")
endif()

# ── Tier 3: Run the pipeline locally ─────────────────────────────────────────
find_program(UV_EXECUTABLE uv)
if(UV_EXECUTABLE)
    message(STATUS "Running asset pipeline locally...")
    execute_process(
        COMMAND ${UV_EXECUTABLE} run python -m pipeline --verbose
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE _uv_result
    )
    if(_uv_result EQUAL 0)
        message(STATUS "Pipeline completed successfully")
        file(WRITE "${ASSETS_DIR}/.stamp" "pipeline")
        return()
    else()
        message(WARNING "Pipeline failed with exit code ${_uv_result}")
    endif()
else()
    message(STATUS "uv not found — cannot run pipeline locally")
endif()

message(FATAL_ERROR
    "Could not acquire processed assets.\n"
    "Options:\n"
    "  1. Run 'uv sync && uv run python -m pipeline' manually\n"
    "  2. Download assets from a GitHub release\n"
    "  3. Install uv (https://docs.astral.sh/uv/) and re-run CMake"
)
