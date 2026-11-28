// Collects the files that make up the package into npm/dist/.

import { copyFileSync, existsSync, mkdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const pkgDir = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const buildDir = resolve(
  process.argv[2] ?? join(pkgDir, "..", "build-wasm-web")
);
const distDir = join(pkgDir, "dist");

const files = [
  [join(buildDir, "tesfix_wasm.mjs"), "tesfix_wasm.mjs"],
  [join(buildDir, "tesfix_wasm.wasm"), "tesfix_wasm.wasm"],
  [join(pkgDir, "..", "web", "tesfix.mjs"), "tesfix.mjs"],
  [join(pkgDir, "tesfix.d.ts"), "tesfix.d.ts"],
];

const missing = files
  .filter(([from]) => !existsSync(from))
  .map(([from]) => from);
if (missing.length > 0) {
  console.error("Missing files:\n  " + missing.join("\n  "));
  console.error("\nBuild the web-only WebAssembly package first:");
  console.error(
    "  emcmake cmake -B build-wasm-web -DCMAKE_BUILD_TYPE=Release -DTESFIX_WASM_ENV=web,worker"
  );
  console.error("  cmake --build build-wasm-web");
  process.exit(1);
}

mkdirSync(distDir, { recursive: true });
for (const [from, name] of files) {
  copyFileSync(from, join(distDir, name));
  console.log(`copied ${name}`);
}

// A build that still supports Node asks for Node-only modules, which bundlers then try to resolve.
if (
  readFileSync(join(distDir, "tesfix_wasm.mjs"), "utf8").includes(
    "createRequire"
  )
) {
  console.warn(
    "\nWARNING: tesfix_wasm.mjs contains Node.js support (createRequire).\n" +
      "Rebuild with -DTESFIX_WASM_ENV=web,worker so bundlers do not trip over Node-only imports."
  );
}
