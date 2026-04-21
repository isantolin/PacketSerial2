# Contributing

## Code of Conduct

First, please see the [Code of Conduct](CODE_OF_CONDUCT.md).

## How to Help

Check out the [Todo](TODO.md) list to see if there are any planned items.

Check out the [Help Wanted](../../../issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22) tag in the issues section for specific ideas or propose your own new ideas.

## Making Pull Requests

Pull Requests are always welcome, so if you make any improvements please feel free to float them back upstream :)

### ⚖️ Design Principles (v2.2)

All contributions to the core of PacketSerial MUST strictly follow these rules:

1.  **Zero-Heap**: No `malloc`, `free`, `new`, or `delete`.
2.  **Zero-STL**: Do not include C++ standard headers like `<algorithm>` or `<vector>`. Use C standard headers (`<stdint.h>`) or ETL equivalents.
3.  **Pure ETL (Functional)**: Avoid manual `for` and `while` loops in core logic. Use `etl::algorithm` or `etl::for_each`.
4.  **Deterministic RAM**: Buffers must be provided by the user via `etl::span` or `etl::array`.
5.  **SIL-2 Determinism**: Use explicit lambda captures and avoid post-increments inside complex expressions.
6.  **C++17 Freestanding**: Compatible with any toolchain without a full OS runtime.

New features MUST include unit tests in the `tests/` directory to confirm functionality.

1. Fork this repository.
2. Create your feature branch (`git checkout -b my-new-feature`).
3. Commit your changes (`git commit -am 'Add some feature'`).
4. Push to the branch (`git push origin my-new-feature`).
5. Create new Pull Request.

New features should include tests (if possible) to confirm functionality.

All code should be documented inline using [Doxygen](http://www.doxygen.nl/manual/docblocks.html) documentation. See the project's source for examples of the preferred style.
