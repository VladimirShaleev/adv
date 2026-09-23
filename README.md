# Adv (Adventure Language)

**Adv** is a lightweight, embeddable scripting language, compiler,
and bytecode virtual machine designed for game engines, interactive
narratives, and application scripting.

## Project Structure

The project is built as a monorepo using CMake and consists of
three main components:

* **`libvm`** — Embeddable runtime & bytecode virtual machine.
  Executes compiled stack/register bytecode with zero dependencies
  on the compiler.
* **`libadv`** — Embeddable compiler library. Parses `.adv` source
  code, performs semantic analysis and generates bytecode.
* **`advc`** — Command-line interface tool that integrates `libadv`
  and `libvm` for standalone source code compilation and script
  execution.
