import { Tesfix } from "tesfix";
import wasmUrl from "tesfix/tesfix.wasm?url"; // Vite turns this into the URL of the .wasm asset

const status = document.getElementById("status");
const list = document.getElementById("list");
const form = document.getElementById("form");
const input = document.getElementById("text");

async function main() {
  const db = await Tesfix.open("vite-example", { wasmUrl }); // saved in IndexedDB
  db.createIndex("notes", "createdAt");

  function render() {
    const notes = db.find("notes", { order_by: [["createdAt", "desc"]] });
    status.textContent = `${db.count("notes")} note(s), saved in this browser`;

    list.replaceChildren(
      ...notes.map((note) => {
        const item = document.createElement("li");
        item.textContent = note.text;

        const remove = document.createElement("button");
        remove.textContent = "delete";
        remove.addEventListener("click", () => {
          db.delete("notes", note.id);
          render();
        });

        item.append(remove);
        return item;
      })
    );
  }

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const id = crypto.randomUUID();
    db.insert("notes", id, { id, text: input.value, createdAt: Date.now() });
    input.value = "";
    render();
    await db.flush(); // make sure it is saved before the user can close the tab
  });

  render();
}

main().catch((e) => {
  status.textContent = `Error: ${e.message}`;
});
