# Claude Code Skills

Skills are Claude Code commands that automate common workflows in forge-gpu.
Users invoke them with `/skill-name` in chat; Claude can also invoke them
automatically when they match the current task.

Each skill directory contains a `SKILL.md` file that defines the skill's
purpose, inputs, and step-by-step instructions.

## Development skills (`dev-*`)

| Skill                    | Purpose                                        |
|--------------------------|-------------------------------------------------|
| `dev-gpu-lesson`         | GPU lesson scaffolding with forge_scene.h      |
| `dev-math-lesson`        | Create a math lesson + update math library      |
| `dev-engine-lesson`      | Scaffold an engine lesson                      |
| `dev-ui-lesson`          | Scaffold a UI lesson                           |
| `dev-physics-lesson`     | Scaffold a physics lesson                      |
| `dev-asset-lesson`       | Scaffold an asset pipeline lesson              |
| `dev-create-pr`          | Create a pull request                          |
| `dev-add-screenshot`     | Capture and embed screenshots/GIFs             |
| `dev-create-diagram`     | Generate matplotlib diagrams for lessons       |
| `dev-review-diagrams`    | Review diagram quality and accuracy            |
| `dev-docs-review`        | Review documentation for completeness          |
| `dev-review-pr`          | Review a pull request                          |
| `dev-final-pass`         | Final quality pass before merging              |
| `dev-markdown-lint`      | Run markdownlint checks                        |
| `dev-ui-review`          | Review UI library changes                      |
| `dev-reset-workspace`    | Reset workspace to a clean state               |

## Topic skills (`forge-*`)

Topic skills teach Claude about specific lesson implementations so it can
answer questions and help debug code related to each lesson.
