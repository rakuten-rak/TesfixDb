// Runs automatically before `npm pack` / `npm publish` (the "prepack" script)
// and stops you from publishing an incomplete or leaked-source package.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const pkgDir = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const problems = [];

// 1. Verify compiled WASM & JS distribution files exist and are non-empty
for (const name of [
  "tesfix.mjs",
  "tesfix_wasm.mjs",
  "tesfix_wasm.wasm",
  "tesfix.d.ts",
]) {
  const file = join(pkgDir, "dist", name);
  if (!existsSync(file) || statSync(file).size === 0) {
    problems.push(`dist/${name} is missing or empty (run: npm run build)`);
  }
}

// 2. Verify no unfilled placeholders remain
const placeholder =
  /\[YOUR [A-Z ]+\]|YOUR-GITHUB-USERNAME|\[FILL IN|\[Your Name/i;
for (const name of [
  "package.json",
  "README.md",
  "LICENSE",
  "THIRD_PARTY_NOTICES.md",
]) {
  const filePath = join(pkgDir, name);
  if (!existsSync(filePath)) {
    problems.push(`${name} is missing from the npm package root`);
  } else if (placeholder.test(readFileSync(filePath, "utf8"))) {
    problems.push(`${name} still contains a placeholder marker`);
  }
}

// 3. Verify LICENSE is your Proprietary Evaluation & Confidential Source License (not MIT)
const licensePath = join(pkgDir, "LICENSE");
if (existsSync(licensePath)) {
  const licenseText = readFileSync(licensePath, "utf8");
  if (
    !licenseText.includes(
      "PROPRIETARY EVALUATION & CONFIDENTIAL SOURCE LICENSE"
    ) ||
    licenseText.includes("Permission is hereby granted, free of charge")
  ) {
    problems.push(
      "LICENSE is not the Proprietary Evaluation & Confidential Source License"
    );
  }
}

// 4. Safety check: Ensure NO raw C/C++ source files exist anywhere inside npm/
function scanForSourceFiles(dir) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    if (entry.name === "node_modules" || entry.name === ".git") continue;
    const fullPath = join(dir, entry.name);
    if (entry.isDirectory()) {
      scanForSourceFiles(fullPath);
    } else if (/\.(cpp|cc|cxx|c|h|hpp)$/i.test(entry.name)) {
      problems.push(`Source file leak detected in npm folder: ${fullPath}`);
    }
  }
}
scanForSourceFiles(pkgDir);

if (problems.length > 0) {
  console.error("Not ready to pack:\n  - " + problems.join("\n  - "));
  process.exit(1);
}
console.log("Package looks complete and safe to publish.");
