// SPDX-License-Identifier: GPL-3.0-or-later
// Uses the actual compiled REA service from the user-supplied REA source tree.
// It does not replace REA with a direct objdump subprocess.
import { mkdir, writeFile } from "node:fs/promises";
import { resolve, join } from "node:path";
import { pathToFileURL } from "node:url";

const [reaRoot, binary, outputDirectory, ...extra] = process.argv.slice(2);
if (!reaRoot || !binary || !outputDirectory || extra.length) {
  throw new Error("Usage: node analyze-with-rea.mjs REA_SOURCE_ROOT BINARY OUTPUT_DIRECTORY");
}
const { createReverseEngineeringService } = await import(
  pathToFileURL(join(resolve(reaRoot), "dist/composition/reverseEngineering.js")).href
);
const service = createReverseEngineeringService();
await mkdir(outputDirectory, { recursive: true });
try {
  for (const operation of ["file_headers", "section_headers", "symbols"]) {
    const result = await service.inspectWithObjdump({ path: resolve(binary), operation });
    const destination = join(outputDirectory, `${operation}.json`);
    await writeFile(destination, JSON.stringify(result, null, 2) + "\n");
    if (!result.ok) {
      throw new Error(`REA ${operation} failed; retained result: ${destination}`);
    }
    const raw = result.value.raw_result;
    if (raw.exit_code !== 0 || raw.completion_status !== "complete" || raw.output_truncated) {
      throw new Error(`Provider ${operation} incomplete; retained result: ${destination}`);
    }
    console.log(`${operation}: ${result.value.evidence_id}`);
  }
} finally {
  await service.close();
}
