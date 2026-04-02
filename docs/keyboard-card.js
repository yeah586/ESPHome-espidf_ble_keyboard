/**
 * BLE Keyboard Control Card for Home Assistant
 *
 * A custom Lovelace card that provides a full on-screen QWERTY keyboard
 * for the ESPHome BLE Keyboard component.
 *
 * Installation:
 *   1. Copy this file to your HA config/www/ folder.
 *   2. Add the resource in HA:
 *        Settings -> Dashboards -> Resources -> Add Resource
 *        URL: /local/keyboard-card.js   Type: JavaScript Module
 *   3. Add ESPHome services to your device YAML (see README).
 *   4. Add the card to a dashboard via the UI or YAML.
 *
 * Card YAML:
 *   type: custom:ble-keyboard-card
 *   device: bluetooth_keyboard    # your ESPHome device name
 *   # Optional overrides:
 *   # name: My Keyboard            # card title (default "BLE Keyboard")
 *   # show_fkeys: true             # show F1-F12 row (default true)
 *   # host_slots: 4                # show host switcher bar (default 0 = hidden)
 *   # host_names:                   # custom names for each host slot (optional)
 *   #   - TV
 *   #   - Phone
 *
 * Full example with overrides:
 *   type: custom:ble-keyboard-card
 *   device: bluetooth_keyboard
 *   name: Living Room Keyboard
 *   show_fkeys: false
 *   host_slots: 4
 *   host_names:
 *     - TV
 *     - Phone
 *     - Laptop
 *     - Tablet
 */

// HID keycodes for printable characters (used when Ctrl/Alt/Win modifiers are active)
const CHAR_TO_KEYCODE = {
  'a':0x04,'b':0x05,'c':0x06,'d':0x07,'e':0x08,'f':0x09,'g':0x0A,
  'h':0x0B,'i':0x0C,'j':0x0D,'k':0x0E,'l':0x0F,'m':0x10,'n':0x11,
  'o':0x12,'p':0x13,'q':0x14,'r':0x15,'s':0x16,'t':0x17,'u':0x18,
  'v':0x19,'w':0x1A,'x':0x1B,'y':0x1C,'z':0x1D,
  '1':0x1E,'2':0x1F,'3':0x20,'4':0x21,'5':0x22,'6':0x23,'7':0x24,
  '8':0x25,'9':0x26,'0':0x27,
  '`':0x35,'-':0x2D,'=':0x2E,'[':0x2F,']':0x30,'\\':0x31,
  ';':0x33,"'":0x34,',':0x36,'.':0x37,'/':0x38,' ':0x2C,
};

// Keyboard layout rows — each key: { label, shiftLabel?, type, char?, shiftChar?, keycode?, mod?, flex? }
const ROWS = [
  // F-key row
  [
    { label: 'Esc', type: 'special', keycode: 0x29, flex: 1.2 },
    { label: 'F1', type: 'special', keycode: 0x3A },
    { label: 'F2', type: 'special', keycode: 0x3B },
    { label: 'F3', type: 'special', keycode: 0x3C },
    { label: 'F4', type: 'special', keycode: 0x3D },
    { label: 'F5', type: 'special', keycode: 0x3E },
    { label: 'F6', type: 'special', keycode: 0x3F },
    { label: 'F7', type: 'special', keycode: 0x40 },
    { label: 'F8', type: 'special', keycode: 0x41 },
    { label: 'F9', type: 'special', keycode: 0x42 },
    { label: 'F10', type: 'special', keycode: 0x43 },
    { label: 'F11', type: 'special', keycode: 0x44 },
    { label: 'F12', type: 'special', keycode: 0x45 },
  ],
  // Number row
  [
    { label: '`', shiftLabel: '~', type: 'char', char: '`', shiftChar: '~' },
    { label: '1', shiftLabel: '!', type: 'char', char: '1', shiftChar: '!' },
    { label: '2', shiftLabel: '@', type: 'char', char: '2', shiftChar: '@' },
    { label: '3', shiftLabel: '#', type: 'char', char: '3', shiftChar: '#' },
    { label: '4', shiftLabel: '$', type: 'char', char: '4', shiftChar: '$' },
    { label: '5', shiftLabel: '%', type: 'char', char: '5', shiftChar: '%' },
    { label: '6', shiftLabel: '^', type: 'char', char: '6', shiftChar: '^' },
    { label: '7', shiftLabel: '&', type: 'char', char: '7', shiftChar: '&' },
    { label: '8', shiftLabel: '*', type: 'char', char: '8', shiftChar: '*' },
    { label: '9', shiftLabel: '(', type: 'char', char: '9', shiftChar: '(' },
    { label: '0', shiftLabel: ')', type: 'char', char: '0', shiftChar: ')' },
    { label: '-', shiftLabel: '_', type: 'char', char: '-', shiftChar: '_' },
    { label: '=', shiftLabel: '+', type: 'char', char: '=', shiftChar: '+' },
    { label: 'Bksp', type: 'special', keycode: 0x2A, flex: 1.5 },
  ],
  // QWERTY row
  [
    { label: 'Tab', type: 'special', keycode: 0x2B, flex: 1.3 },
    { label: 'q', shiftLabel: 'Q', type: 'char', char: 'q', shiftChar: 'Q' },
    { label: 'w', shiftLabel: 'W', type: 'char', char: 'w', shiftChar: 'W' },
    { label: 'e', shiftLabel: 'E', type: 'char', char: 'e', shiftChar: 'E' },
    { label: 'r', shiftLabel: 'R', type: 'char', char: 'r', shiftChar: 'R' },
    { label: 't', shiftLabel: 'T', type: 'char', char: 't', shiftChar: 'T' },
    { label: 'y', shiftLabel: 'Y', type: 'char', char: 'y', shiftChar: 'Y' },
    { label: 'u', shiftLabel: 'U', type: 'char', char: 'u', shiftChar: 'U' },
    { label: 'i', shiftLabel: 'I', type: 'char', char: 'i', shiftChar: 'I' },
    { label: 'o', shiftLabel: 'O', type: 'char', char: 'o', shiftChar: 'O' },
    { label: 'p', shiftLabel: 'P', type: 'char', char: 'p', shiftChar: 'P' },
    { label: '[', shiftLabel: '{', type: 'char', char: '[', shiftChar: '{' },
    { label: ']', shiftLabel: '}', type: 'char', char: ']', shiftChar: '}' },
    { label: '\\', shiftLabel: '|', type: 'char', char: '\\', shiftChar: '|' },
  ],
  // Home row
  [
    { label: 'Caps', type: 'caps', keycode: 0x39, flex: 1.5 },
    { label: 'a', shiftLabel: 'A', type: 'char', char: 'a', shiftChar: 'A' },
    { label: 's', shiftLabel: 'S', type: 'char', char: 's', shiftChar: 'S' },
    { label: 'd', shiftLabel: 'D', type: 'char', char: 'd', shiftChar: 'D' },
    { label: 'f', shiftLabel: 'F', type: 'char', char: 'f', shiftChar: 'F' },
    { label: 'g', shiftLabel: 'G', type: 'char', char: 'g', shiftChar: 'G' },
    { label: 'h', shiftLabel: 'H', type: 'char', char: 'h', shiftChar: 'H' },
    { label: 'j', shiftLabel: 'J', type: 'char', char: 'j', shiftChar: 'J' },
    { label: 'k', shiftLabel: 'K', type: 'char', char: 'k', shiftChar: 'K' },
    { label: 'l', shiftLabel: 'L', type: 'char', char: 'l', shiftChar: 'L' },
    { label: ';', shiftLabel: ':', type: 'char', char: ';', shiftChar: ':' },
    { label: "'", shiftLabel: '"', type: 'char', char: "'", shiftChar: '"' },
    { label: 'Enter', type: 'special', keycode: 0x28, flex: 1.8 },
  ],
  // Shift row
  [
    { label: 'Shift', type: 'modifier', mod: 'shift', bit: 0x02, flex: 2 },
    { label: 'z', shiftLabel: 'Z', type: 'char', char: 'z', shiftChar: 'Z' },
    { label: 'x', shiftLabel: 'X', type: 'char', char: 'x', shiftChar: 'X' },
    { label: 'c', shiftLabel: 'C', type: 'char', char: 'c', shiftChar: 'C' },
    { label: 'v', shiftLabel: 'V', type: 'char', char: 'v', shiftChar: 'V' },
    { label: 'b', shiftLabel: 'B', type: 'char', char: 'b', shiftChar: 'B' },
    { label: 'n', shiftLabel: 'N', type: 'char', char: 'n', shiftChar: 'N' },
    { label: 'm', shiftLabel: 'M', type: 'char', char: 'm', shiftChar: 'M' },
    { label: ',', shiftLabel: '<', type: 'char', char: ',', shiftChar: '<' },
    { label: '.', shiftLabel: '>', type: 'char', char: '.', shiftChar: '>' },
    { label: '/', shiftLabel: '?', type: 'char', char: '/', shiftChar: '?' },
    { label: 'Shift', type: 'modifier', mod: 'shift', bit: 0x02, flex: 2 },
  ],
  // Bottom row
  [
    { label: 'Ctrl', type: 'modifier', mod: 'ctrl', bit: 0x01, flex: 1.2 },
    { label: 'Win', type: 'modifier', mod: 'win', bit: 0x08, flex: 1.2 },
    { label: 'Alt', type: 'modifier', mod: 'alt', bit: 0x04, flex: 1.2 },
    { label: '', type: 'char', char: ' ', shiftChar: ' ', flex: 6 },
      { label: 'Alt R', type: 'modifier', mod: 'altgr', bit: 0x40, flex: 1.2 },
    { label: 'Del', type: 'special', keycode: 0x4C, flex: 1.2 },
    { label: '\u2190', type: 'special', keycode: 0x50 },
    { label: '\u2191', type: 'special', keycode: 0x52 },
    { label: '\u2193', type: 'special', keycode: 0x51 },
    { label: '\u2192', type: 'special', keycode: 0x4F },
  ],
];

class BleKeyboardCard extends HTMLElement {
  set hass(hass) {
    this._hass = hass;
    if (!this._initialized) {
      this._initialize();
    }
  }

  setConfig(config) {
    if (!config.device) {
      throw new Error('Please define a "device" (your ESPHome device name)');
    }
    this._config = {
      device: config.device,
      name: config.name || null,
      show_fkeys: config.show_fkeys !== false,
      host_slots: config.host_slots || 0,
      host_names: config.host_names || [],
    };
  }

  _initialize() {
    if (this._initialized) return;
    this._initialized = true;

    this._shift = false;
    this._capsLock = false;
    this._ctrl = false;
    this._alt = false;
    this._win = false;

    const shadow = this.attachShadow({ mode: 'open' });

    const style = document.createElement('style');
    style.textContent = `
      :host {
        display: block;
      }
      .card {
        background: var(--ha-card-background, var(--card-background-color, #fff));
        border-radius: var(--ha-card-border-radius, 12px);
        box-shadow: var(--ha-card-box-shadow, 0 2px 6px rgba(0,0,0,.15));
        padding: 12px;
        color: var(--primary-text-color);
        user-select: none;
        -webkit-user-select: none;
      }
      .header {
        font-size: 16px;
        font-weight: 500;
        margin-bottom: 8px;
        display: flex;
        align-items: center;
        gap: 8px;
      }
      .header svg {
        width: 20px;
        height: 20px;
        fill: var(--primary-text-color);
        opacity: 0.7;
      }
      .kb-row {
        display: flex;
        gap: 3px;
        margin-bottom: 3px;
      }
      .kb-row:last-child {
        margin-bottom: 0;
      }
      .key {
        flex: 1;
        min-width: 0;
        padding: 10px 2px;
        border: 1px solid var(--divider-color, #e0e0e0);
        border-radius: 6px;
        background: var(--secondary-background-color, #f0f0f0);
        color: var(--primary-text-color);
        font-size: 13px;
        font-weight: 500;
        cursor: pointer;
        text-align: center;
        touch-action: manipulation;
        transition: background 0.08s, border-color 0.08s;
        line-height: 1.2;
        overflow: hidden;
        white-space: nowrap;
      }
      .key:active, .key.pressed {
        background: var(--primary-color, #03a9f4);
        color: #fff;
        border-color: var(--primary-color, #03a9f4);
      }
      .key.active {
        background: var(--primary-color, #03a9f4);
        color: #fff;
        border-color: var(--primary-color, #03a9f4);
      }
      .key.caps-active {
        background: var(--warning-color, #ff9800);
        color: #fff;
        border-color: var(--warning-color, #ff9800);
      }
      .key-fkey {
        font-size: 11px;
        padding: 6px 2px;
      }
      .key-space {
        font-size: 11px;
        letter-spacing: 2px;
      }
      .fkey-row {
        display: ${this._config.show_fkeys ? 'flex' : 'none'};
      }
      .header-right {
        margin-left: auto;
        display: flex;
        align-items: center;
        gap: 6px;
      }
      .host-btn {
        width: 24px;
        height: 24px;
        border: 1px solid var(--divider-color, #e0e0e0);
        border-radius: 4px;
        background: var(--secondary-background-color, #f0f0f0);
        color: var(--primary-text-color);
        font-size: 12px;
        font-weight: 700;
        cursor: pointer;
        display: flex;
        align-items: center;
        justify-content: center;
        touch-action: manipulation;
        padding: 0;
      }
      .host-btn:active {
        background: var(--primary-color, #03a9f4);
        color: #fff;
      }
      .host-info {
        text-align: center;
        min-width: 0;
      }
      .host-name {
        font-size: 12px;
        font-weight: 600;
        color: var(--primary-text-color);
        line-height: 1.2;
      }
      .host-addr {
        font-size: 10px;
        color: var(--secondary-text-color, #888);
        font-family: monospace;
        line-height: 1.2;
      }
    `;

    const card = document.createElement('div');
    card.className = 'card';

    // Header
    const header = document.createElement('div');
    header.className = 'header';
    const defaultName = 'BLE Keyboard';
    header.innerHTML = `
      <svg viewBox="0 0 24 24"><path d="M19 10h-2V8h2v2zm0 4h-2v-2h2v2zm-4-4h-2V8h2v2zm0 4h-2v-2h2v2zm0 4H9v-2h6v2zm-8-8H5V8h2v2zm0 4H5v-2h2v2zM20 5H4c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V7c0-1.1-.9-2-2-2z"/></svg>
      <span class="header-name">${this._config.name || defaultName}</span>
    `;
    // Host switcher in header
    if (this._config.host_slots > 1) {
      this._activeSlot = 0;
      this._hostSlots = [];

      const hostRight = document.createElement('div');
      hostRight.className = 'header-right';

      const prevBtn = document.createElement('button');
      prevBtn.className = 'host-btn';
      prevBtn.textContent = '\u25C0';
      prevBtn.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        this._switchHost((this._activeSlot - 1 + this._config.host_slots) % this._config.host_slots);
      });

      const nextBtn = document.createElement('button');
      nextBtn.className = 'host-btn';
      nextBtn.textContent = '\u25B6';
      nextBtn.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        this._switchHost((this._activeSlot + 1) % this._config.host_slots);
      });

      const hostInfo = document.createElement('div');
      hostInfo.className = 'host-info';
      this._hostNameEl = document.createElement('div');
      this._hostNameEl.className = 'host-name';
      this._hostAddrEl = document.createElement('div');
      this._hostAddrEl.className = 'host-addr';
      hostInfo.appendChild(this._hostNameEl);
      hostInfo.appendChild(this._hostAddrEl);

      hostRight.appendChild(prevBtn);
      hostRight.appendChild(hostInfo);
      hostRight.appendChild(nextBtn);
      header.appendChild(hostRight);

      this._pollHosts();
      this._hostPollInterval = setInterval(() => this._pollHosts(), 5000);
    }

    card.appendChild(header);
    // Auto-resolve device friendly name from HA if name not set
    if (!this._config.name && this._hass) {
      const nameSpan = header.querySelector('.header-name');
      const slug = this._config.device.replace(/-/g, '_');
      this._hass.callWS({ type: 'config/device_registry/list' }).then(devices => {
        const dev = devices.find(d => d.name_by_user
          ? d.name_by_user.replace(/[^a-z0-9]/gi, '_').toLowerCase() === slug
          : (d.name || '').replace(/[^a-z0-9]/gi, '_').toLowerCase() === slug);
        if (dev) nameSpan.textContent = dev.name_by_user || dev.name;
      }).catch(() => { /* keep default */ });
    }

    // Store key elements for label updates
    this._charKeys = [];
    this._modifierBtns = { shift: [], ctrl: [], alt: [], win: [] };

    // Build keyboard rows
    ROWS.forEach((row, rowIdx) => {
      const rowDiv = document.createElement('div');
      rowDiv.className = rowIdx === 0 ? 'kb-row fkey-row' : 'kb-row';

      row.forEach((keyDef, keyIdx) => {
        const btn = document.createElement('button');
        btn.className = 'key';
        if (rowIdx === 0) btn.classList.add('key-fkey');
        if (keyDef.char === ' ') btn.classList.add('key-space');
        if (keyDef.flex) btn.style.flex = keyDef.flex;

        btn.textContent = keyDef.char === ' ' ? 'Space' : keyDef.label;
        btn.dataset.row = rowIdx;
        btn.dataset.key = keyIdx;

        // Track char keys for label updates
        if (keyDef.type === 'char' && keyDef.shiftLabel) {
          this._charKeys.push({ btn, keyDef });
        }

        // Track modifier buttons
        if (keyDef.type === 'modifier' && this._modifierBtns[keyDef.mod]) {
          this._modifierBtns[keyDef.mod].push(btn);
        }

        // Track caps button
        if (keyDef.type === 'caps') {
          this._capsBtn = btn;
        }

        rowDiv.appendChild(btn);
      });

      card.appendChild(rowDiv);
    });

    shadow.appendChild(style);
    shadow.appendChild(card);

    // Delegated event handler
    card.addEventListener('pointerdown', (e) => {
      const btn = e.target.closest('.key');
      if (!btn) return;
      e.preventDefault();

      const rowIdx = parseInt(btn.dataset.row);
      const keyIdx = parseInt(btn.dataset.key);
      const keyDef = ROWS[rowIdx][keyIdx];

      // Visual press feedback
      btn.classList.add('pressed');
      setTimeout(() => btn.classList.remove('pressed'), 120);

      this._onKeyPress(keyDef);
    });
  }

  _sendString(text) {
    if (!this._hass) return;
    this._hass.callService('esphome', `${this._config.device}_send_string`, { keys: text });
  }

  _sendKey(modifier, keycode) {
    if (!this._hass) return;
    this._hass.callService('esphome', `${this._config.device}_send_key`, { modifier, keycode });
  }

  _onKeyPress(keyDef) {
    if (keyDef.type === 'modifier') {
      this._toggleModifier(keyDef.mod);
      return;
    }

    if (keyDef.type === 'caps') {
      this._capsLock = !this._capsLock;
      if (this._capsBtn) {
        this._capsBtn.classList.toggle('caps-active', this._capsLock);
      }
      this._sendKey(0x00, 0x39);
      this._updateKeyLabels();
      return;
    }

    // Build modifier bitmask
    let modBits = 0;
    if (this._ctrl) modBits |= 0x01;
    if (this._alt) modBits |= 0x04;
    if (this._win) modBits |= 0x08;
      if (this._altgr) modBits |= 0x40;
      if (this._rshift) modBits |= 0x20;
        const code = CHAR_TO_KEYCODE[keyDef.char];
        if (code !== undefined) {
          this._sendKey(modBits, code);
        }
      } else {
        // Pure typing — use send_string
        const isLetter = keyDef.char >= 'a' && keyDef.char <= 'z';
        let shifted;
        if (isLetter) {
          shifted = this._shift !== this._capsLock; // XOR
        } else {
          shifted = this._shift;
        }
        const ch = shifted ? keyDef.shiftChar : keyDef.char;
        this._sendString(ch);
      }
    } else if (keyDef.type === 'special') {
      if (this._shift) modBits |= 0x02;
      this._sendKey(modBits, keyDef.keycode);
    }

    // Auto-release one-shot modifiers (not caps lock)
    if (this._shift) this._toggleModifier('shift');
      if (this._rshift) this._toggleModifier('rshift');
      if (this._ctrl) this._toggleModifier('ctrl');
      if (this._alt) this._toggleModifier('alt');
      if (this._altgr) this._toggleModifier('altgr');

  _toggleModifier(mod) {
    this[`_${mod}`] = !this[`_${mod}`];
    const active = this[`_${mod}`];

    if (this._modifierBtns[mod]) {
      this._modifierBtns[mod].forEach(btn => btn.classList.toggle('active', active));
    }

    if (mod === 'shift') {
      this._updateKeyLabels();
    }
  }

  _updateKeyLabels() {
    const showShifted = this._shift !== this._capsLock;
    this._charKeys.forEach(({ btn, keyDef }) => {
      const isLetter = keyDef.char >= 'a' && keyDef.char <= 'z';
      if (isLetter) {
        btn.textContent = showShifted ? keyDef.shiftLabel : keyDef.label;
      } else {
        btn.textContent = this._shift ? keyDef.shiftLabel : keyDef.label;
      }
    });
  }

  _switchHost(slot) {
    if (!this._hass) return;
    this._activeSlot = slot;
    this._hass.callService('esphome', `${this._config.device}_switch_host`, { slot });
    this._updateHostDisplay();
  }

  _pollHosts() {
    if (!this._hass || !this._config.host_slots) return;
    const slug = this._config.device;
    const ipEntity = Object.keys(this._hass.states).find(eid =>
      eid.startsWith('text_sensor.') && eid.includes(slug) && this._hass.states[eid].state.startsWith('http')
    );
    let baseUrl = '';
    if (ipEntity) {
      baseUrl = this._hass.states[ipEntity].state.replace(/\/ble_keyboard$/, '');
    }
    if (!baseUrl) {
      this._updateHostDisplay();
      return;
    }
    // Use no-cors mode won't give us data, so try with cors and handle failure
    fetch(baseUrl + '/api/ble_keyboard/hosts', { signal: AbortSignal.timeout(3000) })
      .then(r => { if (!r.ok) throw new Error(); return r.json(); })
      .then(data => {
        this._activeSlot = data.active;
        this._hostSlots = data.slots || [];
        this._hostDataAvailable = true;
        this._updateHostDisplay();
      })
      .catch(() => this._updateHostDisplay());
  }

  _updateHostDisplay() {
    if (!this._hostNameEl) return;
    const names = this._config.host_names;
    const apiSlot = this._hostSlots.find(s => s.slot === this._activeSlot);
    this._hostNameEl.textContent = (names && names[this._activeSlot])
      ? names[this._activeSlot]
      : (apiSlot && apiSlot.name) ? apiSlot.name
      : 'Host ' + (this._activeSlot + 1);
    if (this._hostDataAvailable) {
      const slot = this._hostSlots.find(s => s.slot === this._activeSlot);
      this._hostAddrEl.textContent = (slot && slot.occupied && slot.addr) ? slot.addr : 'Empty';
    } else {
      this._hostAddrEl.textContent = '';
    }
  }

  getCardSize() {
    return this._config && this._config.show_fkeys !== false ? 6 : 5;
  }

  static getStubConfig() {
    return { device: 'bluetooth_keyboard' };
  }
}

customElements.define('ble-keyboard-card', BleKeyboardCard);

window.customCards = window.customCards || [];
window.customCards.push({
  type: 'ble-keyboard-card',
  name: 'BLE Keyboard Control',
  description: 'Full on-screen QWERTY keyboard for BLE Keyboard component',
  preview: true,
});
