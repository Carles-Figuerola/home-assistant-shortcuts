var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var messageKeys = require('message_keys');
var customClay = function (minified) {
  var clayInstance = this;
  clayInstance.on(clayInstance.EVENTS.AFTER_BUILD, function () {
    // Show/hide token toggle
    var toggle = clayInstance.getItemById('showToken');
    var tokenItem = clayInstance.getItemById('tokenInput');
    var inputEl = tokenItem.$element[0].querySelector('input');

    toggle.on('change', function () {
      inputEl.type = toggle.get() ? 'text' : 'password';
    });


  });
};
var clay = new Clay(clayConfig, customClay, { autoHandleEvents: false });

var MAX_SHORTCUTS = 8;

// --- Storage helpers ---

function getJsConfig() {
  var raw = localStorage.getItem('haConfig');
  if (raw) {
    try { return JSON.parse(raw); } catch (e) {}
  }
  return { baseUrl: '', token: '', scripts: [] };
}

function saveJsConfig(cfg) {
  localStorage.setItem('haConfig', JSON.stringify(cfg));
}

// Extract a string value from Clay response item (handles both "str" and {value:"str"})
function val(items, key) {
  var item = items[key];
  if (!item) return '';
  if (typeof item === 'string') return item;
  if (typeof item === 'object' && item.value !== undefined) return '' + item.value;
  return '';
}

// --- Clay event handlers ---

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;

  // Let Clay persist settings so they're restored when config page reopens
  clay.getSettings(e.response);

  var items = {};
  try { items = JSON.parse(decodeURIComponent(e.response)); } catch (err) {
    console.log('Failed to parse config response');
    return;
  }

  // Save JS-side config (baseUrl, token, scripts)
  var cfg = { baseUrl: val(items, 'BaseUrl'), token: val(items, 'Token'), scripts: [] };
  for (var i = 0; i < MAX_SHORTCUTS; i++) {
    cfg.scripts.push(val(items, 'Script' + i));
  }
  saveJsConfig(cfg);

  // Send shortcut names, icons, and count to the watch
  var msg = { ShortcutCount: 0 };
  var count = 0;

  for (var j = 0; j < MAX_SHORTCUTS; j++) {
    var name = val(items, 'Name' + j);
    var icon = parseInt(val(items, 'Icon' + j) || '0', 10);

    msg['Name' + j] = name.substring(0, 24);
    msg['Icon' + j] = icon;

    if (name.length > 0) count++;
  }

  msg.ShortcutCount = count;

  Pebble.sendAppMessage(msg);
});

// --- Handle shortcut execution requests from C ---

Pebble.addEventListener('appmessage', function (e) {
  // Try both string name and numeric key (varies between real phone and emulator)
  var index = e.payload.ShortcutIndex;
  if (index === undefined || index === null) {
    index = e.payload['' + messageKeys.ShortcutIndex];
  }
  if (index === undefined || index === null) return;
  var cfg = getJsConfig();

  if (!cfg.baseUrl || !cfg.token) {
    sendResult(0, 'No config');
    return;
  }

  if (index < 0 || index >= cfg.scripts.length || !cfg.scripts[index]) {
    sendResult(400, 'Bad shortcut');
    return;
  }

  var url = cfg.baseUrl + cfg.scripts[index];
  var req = new XMLHttpRequest();

  req.onload = function () {
    if (req.status === 200) {
      sendResult(200, 'OK');
    } else {
      sendResult(req.status, 'HTTP ' + req.status);
    }
  };

  req.onerror = function () {
    sendResult(0, 'Net error');
  };

  req.open('POST', url, true);
  req.setRequestHeader('Authorization', 'Bearer ' + cfg.token);
  req.setRequestHeader('Content-Type', 'application/json');
  req.send('{}');
});

// TODO: remove delay — only for GIF capture
function sendResult(code, text) {
  setTimeout(function () {
    var msg = {};
    msg[messageKeys.ResultCode] = code;
    msg[messageKeys.ResultText] = text;
    Pebble.sendAppMessage(msg);
  }, 1000);
}

Pebble.addEventListener('ready', function () {});
