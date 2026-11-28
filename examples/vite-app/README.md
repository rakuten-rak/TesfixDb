# Tesfix + Vite example

A small notes app. Reload the page: the notes are still there, because Tesfix saved them in the
browser's IndexedDB.

From the project root, after building the web-only WebAssembly package (see `npm/README.md`):

```bash
cd npm
npm run build
npm pack                       # creates tesfix-0.1.0-alpha.0.tgz

cd ../examples/vite-app
npm install
npm run dev                    # open the address it prints
npm run build && npm run preview   # also try the production build
```