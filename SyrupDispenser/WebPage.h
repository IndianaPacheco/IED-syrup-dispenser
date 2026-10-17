/*
  WebPage.h
  ---------
  The barista web page WebRemote serves at "/". Plain HTML, CSS, and
  JavaScript in one string, so nothing else needs to be on the ESP32.

  Two tabs: Order (tap a drink to send it to the machine) and Drinks
  (add, edit, and delete drinks).

  The page talks to the dispenser with:
    GET  /drinks   drink names as a JSON array (position = index)
    GET  /status   {"state": "...", "drink": "..."}, polled every second
    GET  /recipe   index -> {"name": "...", "amounts": [8 numbers]}
    POST /order    index, name, pin
    POST /stop     pin
    POST /save     index (-1 = new), old, name, a0 .. a7, pin
    POST /delete   index, old, pin
  ("old" is the name the page showed, so a stale list is caught.)
*/

#pragma once

#include <Arduino.h>

static const char WEB_PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Syrup Dispenser</title>
<style>
  body { font-family: system-ui, sans-serif; margin: 0; background: #f4f1ec; color: #222; }
  header { background: #4a2c1a; color: #fff; padding: 12px 16px; position: sticky; top: 0; z-index: 1; }
  h1 { font-size: 20px; margin: 0 0 8px; }
  h2 { font-size: 18px; margin: 0 0 12px; }
  #status { font-size: 16px; padding: 8px 10px; border-radius: 6px; background: #6b4a35; }
  #status.waiting { background: #b8860b; }
  #status.pouring { background: #2e7d32; }
  #status.error, #status.offline, #status.cancelled { background: #b23b3b; }
  nav { display: flex; gap: 6px; margin-top: 10px; }
  nav button { flex: 1; padding: 10px; background: #6b4a35; color: #fff; }
  nav button.active { background: #f4f1ec; color: #4a2c1a; font-weight: 700; }
  main { padding: 12px 16px; }
  .row { display: flex; gap: 8px; margin-bottom: 10px; }
  input { font-size: 16px; padding: 10px; border: 1px solid #bbb; border-radius: 6px; min-width: 0; }
  #search { flex: 1; }
  #pin { width: 90px; }
  #message { min-height: 22px; margin: 4px 0 10px; font-weight: 600; }
  #drinks { display: grid; grid-template-columns: repeat(auto-fill, minmax(150px, 1fr)); gap: 8px; }
  button { font-size: 17px; padding: 16px 10px; border: none; border-radius: 8px; cursor: pointer; }
  .drink { background: #fff; border: 1px solid #d8cfc4; text-align: left; }
  .drink:active { background: #efe6da; }
  .drink.add { background: #e7f0e4; border-color: #9bbf91; font-weight: 600; }
  #stop { width: 100%; background: #c62828; color: #fff; font-weight: 700; margin-top: 16px; }
  #editor { background: #fff; border: 1px solid #d8cfc4; border-radius: 8px; padding: 14px; }
  label { display: block; font-size: 14px; color: #555; margin-bottom: 4px; }
  #name { width: 100%; box-sizing: border-box; margin-bottom: 12px; }
  #pumps { display: grid; grid-template-columns: repeat(auto-fill, minmax(110px, 1fr)); gap: 10px; margin-bottom: 14px; }
  #pumps input { width: 100%; box-sizing: border-box; }
  .actions { display: flex; gap: 8px; flex-wrap: wrap; }
  .actions button { flex: 1; min-width: 90px; }
  #save { background: #2e7d32; color: #fff; font-weight: 700; }
  #delete { background: #fff; color: #c62828; border: 1px solid #c62828; }
  #cancel { background: #ddd; }
  [hidden] { display: none !important; }
</style>
</head>
<body>
<header>
  <h1>Syrup Dispenser</h1>
  <div id="status">Connecting...</div>
  <nav>
    <button id="tabOrder" class="active">Order</button>
    <button id="tabDrinks">Drinks</button>
  </nav>
</header>
<main>
  <div class="row">
    <input id="search" placeholder="Search drinks" autocomplete="off">
    <input id="pin" placeholder="PIN" inputmode="numeric" type="password">
  </div>
  <div id="message"></div>

  <div id="listView">
    <div id="drinks"></div>
    <button id="stop">STOP</button>
  </div>

  <!-- The limits here are only hints; the ESP32 checks the real ones
       (MAX_NAME_LENGTH and MAX_ML_PER_PUMP in Config.h). -->
  <div id="editor" hidden>
    <h2 id="editorTitle">New drink</h2>
    <label for="name">Name</label>
    <input id="name" maxlength="39" autocomplete="off">
    <div id="pumps"></div>
    <div class="actions">
      <button id="save">Save</button>
      <button id="delete">Delete</button>
      <button id="cancel">Cancel</button>
    </div>
  </div>
</main>
<script>
const PUMP_COUNT = 8;
const LABELS = {
  idle: "Ready",
  busy: "In use at the machine",
  waiting: "Waiting for knob press",
  pouring: "Pouring",
  done: "Done",
  cancelled: "Stopped",
  error: "Error - check the machine",
  offline: "Machine needs a reset"
};

const $ = id => document.getElementById(id);
let drinks = [];
let tab = "order";
let editing = null;  // {index, old} while the editor is open; index -1 = new

// Remember the PIN on this device (if the browser allows it).
try { $("pin").value = localStorage.getItem("pin") || ""; } catch (e) {}
$("pin").addEventListener("change", () => {
  try { localStorage.setItem("pin", $("pin").value); } catch (e) {}
});

// One mL box per pump.
for (let i = 0; i < PUMP_COUNT; i++) {
  const box = document.createElement("div");
  box.innerHTML = '<label for="a' + i + '">Pump ' + (i + 1) + ' (mL)</label>' +
    '<input id="a' + i + '" type="number" min="0" max="100" step="0.5" inputmode="decimal">';
  $("pumps").appendChild(box);
}

async function loadDrinks() {
  try {
    drinks = await (await fetch("/drinks")).json();
    render();
  } catch (e) {
    $("message").textContent = "Couldn't load drinks.";
  }
}

function showTab(name) {
  tab = name;
  $("tabOrder").classList.toggle("active", tab === "order");
  $("tabDrinks").classList.toggle("active", tab === "drinks");
  $("stop").hidden = tab !== "order";
  closeEditor();
  $("message").textContent = "";
}

// The drink grid. On the Order tab a tap orders; on the Drinks tab
// it opens the editor, and there is a "+ New drink" button first.
function render() {
  const query = $("search").value.trim().toLowerCase();
  const list = $("drinks");
  list.replaceChildren();

  if (tab === "drinks") {
    const add = document.createElement("button");
    add.className = "drink add";
    add.textContent = "+ New drink";
    add.onclick = () => openEditor(-1);
    list.appendChild(add);
  }

  let shown = 0;
  drinks.forEach((name, index) => {
    if (query && !name.toLowerCase().includes(query)) return;
    const button = document.createElement("button");
    button.className = "drink";
    button.textContent = (tab === "drinks" ? "Edit: " : "") + name;
    button.onclick = () => tab === "order" ? order(index) : openEditor(index);
    list.appendChild(button);
    shown++;
  });

  if (!shown && tab === "order") {
    list.textContent = drinks.length ? "No matches." : "No drinks saved yet.";
  }
}

// POST to the dispenser and show its answer. Returns true on success.
async function send(path, fields) {
  fields.pin = $("pin").value;
  try {
    const response = await fetch(path, { method: "POST", body: new URLSearchParams(fields) });
    const text = await response.text();
    $("message").textContent = text;
    if (text.startsWith("Drink list changed")) {
      closeEditor();
      loadDrinks();
    }
    pollStatus();
    return response.ok;
  } catch (e) {
    $("message").textContent = "Couldn't reach the machine.";
    return false;
  }
}

function order(index) {
  send("/order", { index: index, name: drinks[index] });
}

async function openEditor(index) {
  $("message").textContent = "";
  let name = "";
  let amounts = new Array(PUMP_COUNT).fill(0);

  if (index >= 0) {
    try {
      const response = await fetch("/recipe?index=" + index);
      if (!response.ok) throw new Error(await response.text());
      const recipe = await response.json();
      name = recipe.name;
      amounts = recipe.amounts;
    } catch (e) {
      $("message").textContent = e.message || "Couldn't load that drink.";
      loadDrinks();
      return;
    }
  }

  editing = { index: index, old: index >= 0 ? drinks[index] : "" };
  $("editorTitle").textContent = index >= 0 ? "Edit drink" : "New drink";
  $("name").value = name;
  for (let i = 0; i < PUMP_COUNT; i++) {
    $("a" + i).value = amounts[i] ? amounts[i] : "";
  }
  $("delete").hidden = index < 0;
  $("listView").hidden = true;
  $("editor").hidden = false;
  $("name").focus();
}

function closeEditor() {
  editing = null;
  $("editor").hidden = true;
  $("listView").hidden = false;
  render();
}

async function save() {
  const fields = { index: editing.index, old: editing.old, name: $("name").value };
  for (let i = 0; i < PUMP_COUNT; i++) {
    fields["a" + i] = $("a" + i).value;
  }
  if (await send("/save", fields)) {
    closeEditor();
    loadDrinks();
  }
}

async function remove() {
  if (!confirm('Delete "' + editing.old + '"? This can\'t be undone.')) return;
  if (await send("/delete", { index: editing.index, old: editing.old })) {
    closeEditor();
    loadDrinks();
  }
}

async function pollStatus() {
  const box = $("status");
  try {
    const s = await (await fetch("/status")).json();
    box.textContent = (LABELS[s.state] || s.state) + (s.drink ? ": " + s.drink : "");
    box.className = s.state;
  } catch (e) {
    box.textContent = "Can't reach the machine";
    box.className = "offline";
  }
}

$("tabOrder").onclick = () => showTab("order");
$("tabDrinks").onclick = () => showTab("drinks");
$("search").addEventListener("input", render);
$("stop").onclick = () => send("/stop", {});
$("save").onclick = save;
$("delete").onclick = remove;
$("cancel").onclick = closeEditor;
loadDrinks();
pollStatus();
setInterval(pollStatus, 1000);
</script>
</body>
</html>
)rawliteral";
