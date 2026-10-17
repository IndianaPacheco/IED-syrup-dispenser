/*
  WebPage.h
  ---------
  The barista web page WebRemote serves at "/". Plain HTML, CSS, and
  JavaScript in one string, so nothing else needs to be on the ESP32.

  The page talks to the dispenser with:
    GET  /drinks   drink names as a JSON array (position = index)
    GET  /status   {"state": "...", "drink": "..."}, polled every second
    POST /order    index, name, pin
    POST /stop     pin
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
  header { background: #4a2c1a; color: #fff; padding: 12px 16px; position: sticky; top: 0; }
  h1 { font-size: 20px; margin: 0 0 8px; }
  #status { font-size: 16px; padding: 8px 10px; border-radius: 6px; background: #6b4a35; }
  #status.waiting { background: #b8860b; }
  #status.pouring { background: #2e7d32; }
  #status.error, #status.offline, #status.cancelled { background: #b23b3b; }
  main { padding: 12px 16px; }
  .row { display: flex; gap: 8px; margin-bottom: 10px; }
  input { font-size: 16px; padding: 10px; border: 1px solid #bbb; border-radius: 6px; }
  #search { flex: 1; min-width: 0; }
  #pin { width: 90px; }
  #message { min-height: 22px; margin: 4px 0 10px; font-weight: 600; }
  #drinks { display: grid; grid-template-columns: repeat(auto-fill, minmax(150px, 1fr)); gap: 8px; }
  button { font-size: 17px; padding: 16px 10px; border: none; border-radius: 8px; cursor: pointer; }
  .drink { background: #fff; border: 1px solid #d8cfc4; text-align: left; }
  .drink:active { background: #efe6da; }
  #stop { width: 100%; background: #c62828; color: #fff; font-weight: 700; margin-top: 16px; }
</style>
</head>
<body>
<header>
  <h1>Syrup Dispenser</h1>
  <div id="status">Connecting...</div>
</header>
<main>
  <div class="row">
    <input id="search" placeholder="Search drinks" autocomplete="off">
    <input id="pin" placeholder="PIN" inputmode="numeric" type="password">
  </div>
  <div id="message"></div>
  <div id="drinks"></div>
  <button id="stop">STOP</button>
</main>
<script>
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

// Remember the PIN on this device (if the browser allows it).
try { $("pin").value = localStorage.getItem("pin") || ""; } catch (e) {}
$("pin").addEventListener("change", () => {
  try { localStorage.setItem("pin", $("pin").value); } catch (e) {}
});

async function loadDrinks() {
  try {
    drinks = await (await fetch("/drinks")).json();
    render();
  } catch (e) {
    $("message").textContent = "Couldn't load drinks.";
  }
}

function render() {
  const query = $("search").value.trim().toLowerCase();
  const list = $("drinks");
  list.replaceChildren();
  drinks.forEach((name, index) => {
    if (query && !name.toLowerCase().includes(query)) return;
    const button = document.createElement("button");
    button.className = "drink";
    button.textContent = name;
    button.onclick = () => order(index);
    list.appendChild(button);
  });
  if (!list.children.length) {
    list.textContent = drinks.length ? "No matches." : "No drinks saved yet.";
  }
}

async function send(path, fields) {
  fields.pin = $("pin").value;
  try {
    const response = await fetch(path, { method: "POST", body: new URLSearchParams(fields) });
    const text = await response.text();
    $("message").textContent = text;
    if (text.startsWith("Drink list changed")) loadDrinks();
  } catch (e) {
    $("message").textContent = "Couldn't reach the machine.";
  }
  pollStatus();
}

function order(index) {
  send("/order", { index: index, name: drinks[index] });
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

$("search").addEventListener("input", render);
$("stop").onclick = () => send("/stop", {});
loadDrinks();
pollStatus();
setInterval(pollStatus, 1000);
</script>
</body>
</html>
)rawliteral";
