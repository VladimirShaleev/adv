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

## WebAssembly VM

The Emscripten build packages `libvm` as both a CommonJS-compatible factory
(`adv-vm.js`) and an ES module factory (`adv-vm.esm.mjs`). It also emits
TypeScript declarations and the matching `.wasm` files into
`build/wasm-release/dist`.

With Emscripten and vcpkg available through the `EMSDK` and `VCPKG_ROOT`
environment variables, build the npm package with:

```sh
cmake --preset wasm-release
cmake --build --preset wasm-release
```

To create an npm tarball after the build, run `npm pack` from
`build/wasm-release/dist`.

The same presets are available with the `wasm-debug` name. From JavaScript,
both module formats initialize asynchronously and provide the same API:

```js
// CommonJS
const createAdvVM = require("adv-vm");
createAdvVM().then(({ Vm }) => {
  const vm = new Vm();
  // Use vm.loadScript(), vm.runThread(), and vm.tick() here.
  vm.delete();
});
```

```js
// ES module
import createAdvVM from "adv-vm";
const { Vm } = await createAdvVM();
const vm = new Vm();
const script = vm.loadScript(new Uint8Array(compiledBytecode));
const thread = vm.runThread(script);
vm.tick(16);
vm.shutdownThread(thread);
vm.delete();
```

`loadScript` accepts a `Uint8Array`, `ArrayBuffer`, or byte array.
`registerNative(name, callback)` invokes the callback with the script's
arguments and passes supported return values (boolean, number, string, or
`undefined` for void native functions) back to the VM. `setLogger` receives
diagnostic objects with `thread`, `instruction`, and `message` fields.
`setImporter` may synchronously resolve an imported module name to its
compiled bytecode buffer.
