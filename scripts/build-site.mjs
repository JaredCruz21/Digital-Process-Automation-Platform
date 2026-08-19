import { mkdir, readFile, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const dist = path.join(root, "dist");

const files = {
  "/": {
    source: "portfolio/index.html",
    type: "text/html; charset=utf-8"
  },
  "/index.html": {
    source: "portfolio/index.html",
    type: "text/html; charset=utf-8"
  },
  "/styles.css": {
    source: "portfolio/styles.css",
    type: "text/css; charset=utf-8"
  },
  "/app.js": {
    source: "portfolio/app.js",
    type: "application/javascript; charset=utf-8"
  }
};

await rm(dist, { recursive: true, force: true });
await mkdir(path.join(dist, "server"), { recursive: true });
await mkdir(path.join(dist, ".openai"), { recursive: true });

const assets = {};
for (const [route, file] of Object.entries(files)) {
  assets[route] = {
    body: await readFile(path.join(root, file.source), "utf8"),
    type: file.type
  };
}

const serverSource = `const assets = ${JSON.stringify(assets)};

function resolveAsset(pathname) {
  if (assets[pathname]) return assets[pathname];
  if (!pathname.includes(".") && assets["/index.html"]) return assets["/index.html"];
  return null;
}

async function fetch(request) {
  const url = new URL(request.url);
  const asset = resolveAsset(url.pathname);

  if (!asset) {
    return new Response("Not found", {
      status: 404,
      headers: { "content-type": "text/plain; charset=utf-8" }
    });
  }

  return new Response(asset.body, {
    headers: {
      "content-type": asset.type,
      "cache-control": asset.type.startsWith("text/html")
        ? "no-store"
        : "public, max-age=3600"
    }
  });
}

export { fetch };
export default { fetch };
`;

await writeFile(path.join(dist, "server", "index.js"), serverSource, "utf8");
await writeFile(
  path.join(dist, ".openai", "hosting.json"),
  await readFile(path.join(root, ".openai", "hosting.json"), "utf8"),
  "utf8"
);

console.log("Built digital process site to dist/server/index.js");
