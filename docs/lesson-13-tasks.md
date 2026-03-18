# Engine Lesson 13 — Stretchy Containers: Task Checklist

## Phase 1: Library Core (Arrays)

- [x] Write `common/containers/forge_containers.h` chunk A (header, config, platform, array helpers + macros)
- [x] Write array tests in `tests/containers/test_containers.c`
- [x] Write `tests/containers/CMakeLists.txt`
- [x] Register test in root `CMakeLists.txt`
- [x] Build and verify array tests pass

## Phase 2: Hash Maps

- [x] Write `forge_containers.h` chunk B (hash functions + bucket/index structs)
- [x] Write `forge_containers.h` chunk C (hash map put/get/remove + `forge_hm_*` macros)
- [x] Add hash map tests
- [x] Build and verify hash map tests pass

## Phase 3: String Maps

- [x] Write `forge_containers.h` chunk D (string arena + `forge_shm_*` macros)
- [x] Add string map tests
- [x] Build and verify all tests pass

## Phase 4: Lesson

- [x] Write `lessons/engine/13-stretchy-containers/CMakeLists.txt`
- [x] Write `lessons/engine/13-stretchy-containers/main.c` (6 demos, console-only)
- [x] Register lesson in root `CMakeLists.txt`
- [x] Build and run lesson

## Phase 5: Diagrams

- [x] Write `scripts/forge_diagrams/engine/lesson_13.py` (5 diagrams)
- [x] Register in `__init__.py` and `__main__.py`
- [x] Generate diagrams

## Phase 6: README + Integration

- [x] Write `lessons/engine/13-stretchy-containers/README.md`
- [x] Update `lessons/engine/README.md` (add row)
- [x] Markdown lint check

## Phase 7: Quality

- [x] Full build, all tests pass
- [x] `/dev-local-review`
