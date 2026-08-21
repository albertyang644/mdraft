Tree Structure

├── MAP.md  # This is the file that will map out where the important things are
├── GOALS.md  # These are the ultimate goals of the project
├── PLANS.md  # Canonical approved plan artifact
├── TASKS.md  # Bite-sized implementation checklist
├── CMakeLists.txt  # Future C++/Qt build configuration
├── src/  # Future C++/Qt application source
├── assets/  # usable assets for reference
├── archives/   # This is where old test results and stuff that no longer apply lives
│   └── deprecated/  # These are mistakes that got deprecated
├── contracts/  # This is where the hard contracts with AI lives. The Non-negotiables
│   └── HARD_CONTRACT.md  # No WebView on initial load and other hard performance rules
├── docs/  # AI generated Human Readable docs
├── logs/  # This is where the test logs lives
├── tests/  # This is where the tests for correctness lives
├── state/  # this is the directory of ephemeral memory
└── success/  # These are the criterias for success


Generate a plan and ask for human approval. Upon Human approval:
Skip if a PLANS.md already exists.

1) Write the plan out to PLANS.md
2) Generate TASKS.md and break down the plan into bite sized chunks, with a checkmark in front; and every task that is completed; check it off.
3) Generate logs when there are errors and how you overcame them in logs/
4) if there were tests and test results, after the tests have passed, move them to archives/ if there are deprecations put them in archives/deprecations/
5) Look in the contracts/ folders to see if there are hard invariables to the project
6) look in the success folder to see what "success" looks like and when the iteration will stop.
7) when the project has fulfilled all success criteria, go ahead and generate relevant user documents in docs/
8) confirm one final time that everything in TASKS.md have been met.


- If it's a program; then src/ should be created to keep source code there.

- This project is now planned as a compiled C++/Qt desktop application, not a Python application.

- If it's got a web interface, www/ should be generated and all interfaces should be there

- If you have art and icons and jpgs etc.. then assets/ should be created.

- If there are .css files; then assets/css should be the location for it.
