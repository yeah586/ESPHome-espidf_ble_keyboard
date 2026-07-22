/**
 * BLE Mouse Control Card for Home Assistant
 *
 * A custom Lovelace card that provides a touchpad, 3 mouse buttons,
 * and scroll controls for the ESPHome BLE Keyboard component.
 * Tap a button to click; long-press (400ms) to hold it for dragging
 * (needs the mouse_hold / mouse_release services), tap again to release.
 *
 * Installation:
 *   1. Copy this file to your HA config/www/ folder.
 *   2. Add the resource in HA:
 *        Settings -> Dashboards -> Resources -> Add Resource
 *        URL: /local/mouse-card.js   Type: JavaScript Module
 *   3. Add ESPHome services to your device YAML (see README).
 *   4. Add the card to a dashboard via the UI or YAML.
 *
 * Card YAML:
 *   type: custom:ble-mouse-card
 *   device: bluetooth_keyboard    # your ESPHome device name
 *   # Optional overrides:
 *   # name: Mouse Control          # card title (default "Mouse Control")
 *   # sensitivity: 1.5             # movement multiplier (default 1.5)
 *   # mouse_acceleration: 0.15     # speed-based acceleration factor (default 0.15)
 *   # mouse_max_speed: 4.5         # max sensitivity cap (default 4.5)
 *   # scroll_sensitivity: 2        # scroll multiplier (default 2)
 *   # tap_to_click: true           # tap touchpad = left click (default true)
 *
 * Full example with overrides:
 *   type: custom:ble-mouse-card
 *   device: bluetooth_keyboard
 *   name: Living Room Mouse
 *   sensitivity: 2.0
 *   mouse_acceleration: 0.2
 *   mouse_max_speed: 6.0
 *   scroll_sensitivity: 3
 *   tap_to_click: false
 */

class BleMouseCard extends HTMLElement {
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
      sensitivity: config.sensitivity || 1.5,
      mouse_acceleration: config.mouse_acceleration || 0.15,
      mouse_max_speed: config.mouse_max_speed || 4.5,
      scroll_sensitivity: config.scroll_sensitivity || 2,
      tap_to_click: config.tap_to_click !== false,
    };
  }

  _initialize() {
    if (this._initialized) return;
    this._initialized = true;

    const shadow = this.attachShadow({ mode: 'open' });
    shadow.innerHTML = `
      <style>
        :host {
          display: block;
        }
        .card {
          background: var(--ha-card-background, var(--card-background-color, #fff));
          border-radius: var(--ha-card-border-radius, 12px);
          box-shadow: var(--ha-card-box-shadow, 0 2px 6px rgba(0,0,0,.15));
          padding: 16px;
          color: var(--primary-text-color);
          user-select: none;
          -webkit-user-select: none;
        }
        .header {
          font-size: 16px;
          font-weight: 500;
          margin-bottom: 12px;
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
        .touchpad {
          width: 100%;
          aspect-ratio: 16/9;
          background: var(--secondary-background-color, #f0f0f0);
          border-radius: 12px;
          border: 2px solid var(--divider-color, #e0e0e0);
          cursor: crosshair;
          touch-action: none;
          position: relative;
          overflow: hidden;
          transition: border-color 0.15s;
        }
        .touchpad.active {
          border-color: var(--primary-color, #03a9f4);
        }
        .touchpad-hint {
          position: absolute;
          top: 50%;
          left: 50%;
          transform: translate(-50%, -50%);
          color: var(--secondary-text-color, #999);
          font-size: 13px;
          pointer-events: none;
          opacity: 0.5;
        }
        .touchpad.active .touchpad-hint {
          opacity: 0;
        }
        .buttons-row {
          display: grid;
          grid-template-columns: 1fr 0.7fr 1fr;
          gap: 8px;
          margin-top: 12px;
        }
        .mouse-btn {
          padding: 14px 0;
          border: 2px solid var(--divider-color, #e0e0e0);
          border-radius: 10px;
          background: var(--secondary-background-color, #f0f0f0);
          color: var(--primary-text-color);
          font-size: 13px;
          font-weight: 500;
          cursor: pointer;
          text-align: center;
          touch-action: manipulation;
          transition: background 0.1s, border-color 0.1s;
        }
        .mouse-btn:active, .mouse-btn.pressed {
          background: var(--primary-color, #03a9f4);
          color: #fff;
          border-color: var(--primary-color, #03a9f4);
        }
        .mouse-btn.held {
          background: var(--accent-color, #ff9800);
          color: #fff;
          border-color: var(--accent-color, #ff9800);
        }
        .scroll-row {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 8px;
          margin-top: 8px;
        }
        .scroll-btn {
          padding: 10px 0;
          border: 2px solid var(--divider-color, #e0e0e0);
          border-radius: 10px;
          background: var(--secondary-background-color, #f0f0f0);
          color: var(--primary-text-color);
          font-size: 18px;
          cursor: pointer;
          text-align: center;
          touch-action: manipulation;
          transition: background 0.1s, border-color 0.1s;
        }
        .scroll-btn:active {
          background: var(--primary-color, #03a9f4);
          color: #fff;
          border-color: var(--primary-color, #03a9f4);
        }
      </style>
      <div class="card">
        <div class="header">
          <svg viewBox="0 0 24 24"><path d="M11 1.5v8.5l4 4.5h3.5l-2-2.5L20 8.5 14 5.5V1.5l-3 0zm-1 0L7 1.5v4L3.5 8.5 7 12l-2 2.5H8.5l4-4.5V1.5z" opacity="0"/><path d="M12 2C8.14 2 5 5.14 5 9v6c0 3.86 3.14 7 7 7s7-3.14 7-7V9c0-3.86-3.14-7-7-7zm0 2c2.76 0 5 2.24 5 5v2h-4V5h-2v6H7V9c0-2.76 2.24-5 5-5z"/></svg>
          <span class="header-name">${this._config.name || 'Mouse Control'}</span>
        </div>
        <div class="touchpad" id="touchpad">
          <span class="touchpad-hint">Drag to move cursor</span>
        </div>
        <div class="buttons-row">
          <button class="mouse-btn" id="btn-left">Left</button>
          <button class="mouse-btn" id="btn-middle">Middle</button>
          <button class="mouse-btn" id="btn-right">Right</button>
        </div>
        <div class="scroll-row">
          <button class="scroll-btn" id="scroll-up">&#9650; Scroll Up</button>
          <button class="scroll-btn" id="scroll-down">&#9660; Scroll Down</button>
        </div>
      </div>
    `;

    this._heldBtn = 0;
    this._setupTouchpad();
    this._setupButtons();
    this._setupScroll();

    // Auto-resolve device friendly name from HA if name not set
    if (!this._config.name && this._hass) {
      const nameSpan = shadow.querySelector('.header-name');
      const slug = this._config.device.replace(/-/g, '_');
      this._hass.callWS({ type: 'config/device_registry/list' }).then(devices => {
        const dev = devices.find(d => d.name_by_user
          ? d.name_by_user.replace(/[^a-z0-9]/gi, '_').toLowerCase() === slug
          : (d.name || '').replace(/[^a-z0-9]/gi, '_').toLowerCase() === slug);
        if (dev) nameSpan.textContent = dev.name_by_user || dev.name;
      }).catch(() => { /* keep default */ });
    }
  }

  _callService(service, data) {
    if (!this._hass) return;
    this._hass.callService('esphome', `${this._config.device}_${service}`, data);
  }

  // ── Touchpad: drag-to-move + tap-to-click ────────────────────────

  _setupTouchpad() {
    const pad = this.shadowRoot.getElementById('touchpad');
    let tracking = false;
    let lastX = 0;
    let lastY = 0;
    let lastTime = 0;
    let startTime = 0;
    let moved = false;
    let startSX = 0;
    let startSY = 0;

    // Accumulator for sub-pixel movements
    let accumX = 0;
    let accumY = 0;

    const baseSens = this._config.sensitivity;
    const accelFactor = this._config.mouse_acceleration;
    const maxSens = this._config.mouse_max_speed;
    const tapDeadZone = 5;
    const scrollSensitivity = this._config.scroll_sensitivity;

    const onStart = (x, y) => {
      tracking = true;
      lastX = startSX = x;
      lastY = startSY = y;
      lastTime = startTime = Date.now();
      moved = false;
      accumX = 0;
      accumY = 0;
      pad.classList.add('active');
    };

    const onMove = (x, y) => {
      if (!tracking) return;

      // Ignore tiny jitter so taps still register
      if (!moved) {
        const td = Math.abs(x - startSX) + Math.abs(y - startSY);
        if (td < tapDeadZone) return;
      }

      const now = Date.now();
      const dt = Math.max(now - lastTime, 1);
      const rawDx = x - lastX;
      const rawDy = y - lastY;
      const dist = Math.sqrt(rawDx * rawDx + rawDy * rawDy);
      const speed = dist / dt;
      const sens = Math.min(baseSens + speed * accelFactor, maxSens);

      accumX += rawDx * sens;
      accumY += rawDy * sens;

      const dx = Math.trunc(accumX);
      const dy = Math.trunc(accumY);

      if (dx !== 0 || dy !== 0) {
        const clampedX = Math.max(-127, Math.min(127, dx));
        const clampedY = Math.max(-127, Math.min(127, dy));
        this._callService('mouse_move', { x: clampedX, y: clampedY });
        accumX -= dx;
        accumY -= dy;
        moved = true;
      }

      lastX = x;
      lastY = y;
      lastTime = now;
    };

    const onEnd = () => {
      if (!tracking) return;
      tracking = false;
      pad.classList.remove('active');

      // Tap-to-click: short tap with no drag beyond dead zone. Suppressed
      // while a button is held so a stray tap can't drop the drag.
      if (this._config.tap_to_click && !moved && !this._heldBtn && (Date.now() - startTime) < 300) {
        this._callService('mouse_click', { btn:1 });
      }
    };

    // Pointer events for drag tracking
    pad.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      pad.setPointerCapture(e.pointerId);
      onStart(e.clientX, e.clientY);
    });
    pad.addEventListener('pointermove', (e) => {
      onMove(e.clientX, e.clientY);
    });
    pad.addEventListener('pointerup', (e) => {
      pad.releasePointerCapture(e.pointerId);
      onEnd();
    });
    pad.addEventListener('pointercancel', (e) => {
      pad.releasePointerCapture(e.pointerId);
      onEnd();
    });

    // Mouse wheel / trackpad scroll on desktop
    let wheelAccum = 0;
    pad.addEventListener('wheel', (e) => {
      e.preventDefault();
      wheelAccum += -e.deltaY * scrollSensitivity * 0.02;
      const intScroll = Math.trunc(wheelAccum);
      if (intScroll !== 0) {
        const clamped = Math.max(-127, Math.min(127, intScroll));
        this._callService('mouse_scroll', { amount: clamped });
        wheelAccum -= intScroll;
      }
    }, { passive: false });
  }

  // ── Click buttons: tap = click; long-press (400ms) = hold for dragging;
  // tap the held button = release. Held state is client-side only — a normal
  // click releases everything device-side too, so it also clears the mark.

  _setupButtons() {
    const btnMap = {
      'btn-left': 1,
      'btn-middle': 4,
      'btn-right': 2,
    };
    const setHeld = (b) => {
      this._heldBtn = b;
      for (const [id, button] of Object.entries(btnMap)) {
        this.shadowRoot.getElementById(id).classList.toggle('held', b === button);
      }
    };
    for (const [id, button] of Object.entries(btnMap)) {
      const el = this.shadowRoot.getElementById(id);
      let holdTimer = null;
      let ok = false;
      const clearHold = () => { if (holdTimer) { clearTimeout(holdTimer); holdTimer = null; } };
      el.addEventListener('contextmenu', (e) => e.preventDefault());
      el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        ok = true;
        el.classList.add('pressed');
        clearHold();
        holdTimer = setTimeout(() => {
          holdTimer = null;
          if (!ok) return;
          ok = false;
          el.classList.remove('pressed');
          this._callService('mouse_hold', { btn: button });
          setHeld(button);
        }, 400);
      });
      el.addEventListener('pointerup', () => {
        el.classList.remove('pressed');
        clearHold();
        if (!ok) return;
        ok = false;
        if (this._heldBtn === button) {
          this._callService('mouse_release', {});
          setHeld(0);
        } else {
          this._callService('mouse_click', { btn: button });
          setHeld(0);
        }
      });
      el.addEventListener('pointerleave', () => { ok = false; el.classList.remove('pressed'); clearHold(); });
      el.addEventListener('pointercancel', () => { ok = false; el.classList.remove('pressed'); clearHold(); });
    }
  }

  // ── Scroll buttons ───────────────────────────────────────────────

  _setupScroll() {
    const upBtn = this.shadowRoot.getElementById('scroll-up');
    const downBtn = this.shadowRoot.getElementById('scroll-down');

    let scrollInterval = null;

    const startScroll = (amount) => {
      this._callService('mouse_scroll', { amount });
      scrollInterval = setInterval(() => {
        this._callService('mouse_scroll', { amount });
      }, 150);
    };

    const stopScroll = () => {
      if (scrollInterval) {
        clearInterval(scrollInterval);
        scrollInterval = null;
      }
    };

    for (const [btn, amount] of [[upBtn, 3], [downBtn, -3]]) {
      btn.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        btn.classList.add('pressed');
        startScroll(amount);
      });
      btn.addEventListener('pointerup', () => { btn.classList.remove('pressed'); stopScroll(); });
      btn.addEventListener('pointerleave', () => { btn.classList.remove('pressed'); stopScroll(); });
    }
  }

  getCardSize() {
    return 4;
  }

  // Enables the dashboard's visual editor; YAML editing is unaffected.
  static getConfigElement() {
    return document.createElement('ble-mouse-card-editor');
  }

  static getStubConfig() {
    return { device: 'bluetooth_keyboard' };
  }

  static getStubConfig() {
    return { device: 'bluetooth_keyboard' };
  }
}

customElements.define('ble-mouse-card', BleMouseCard);

/* ── Visual editor ───────────────────────────────────────────────────────
 * Prefers Home Assistant's own <ha-form> so the panel matches built-in cards,
 * falling back to plain inputs if ha-form isn't registered.
 * ---------------------------------------------------------------------- */

const MOUSE_EDITOR_SCHEMA = [
  { name: 'device', required: true, selector: { text: {} } },
  { name: 'name', selector: { text: {} } },
  { name: 'sensitivity', selector: { number: { min: 0.1, max: 10, step: 0.1, mode: 'box' } } },
  { name: 'mouse_acceleration', selector: { number: { min: 0, max: 2, step: 0.05, mode: 'box' } } },
  { name: 'mouse_max_speed', selector: { number: { min: 0.5, max: 20, step: 0.5, mode: 'box' } } },
  { name: 'scroll_sensitivity', selector: { number: { min: 0.1, max: 10, step: 0.1, mode: 'box' } } },
  { name: 'tap_to_click', selector: { boolean: {} } },
];

const MOUSE_EDITOR_LABELS = {
  device: 'ESPHome device name',
  name: 'Card title (optional)',
  sensitivity: 'Pointer sensitivity',
  mouse_acceleration: 'Acceleration',
  mouse_max_speed: 'Max speed multiplier',
  scroll_sensitivity: 'Scroll sensitivity',
  tap_to_click: 'Tap touchpad to click',
};

class BleMouseCardEditor extends HTMLElement {
  setConfig(config) {
    this._config = {
      sensitivity: 1.5,
      mouse_acceleration: 0.15,
      mouse_max_speed: 4.5,
      scroll_sensitivity: 2,
      tap_to_click: true,
      ...config,
    };
    this._render();
  }

  set hass(hass) {
    this._hass = hass;
    if (this._form) this._form.hass = hass;
  }

  _emit(config) {
    this._config = config;
    this.dispatchEvent(new CustomEvent('config-changed', {
      detail: { config },
      bubbles: true,
      composed: true,
    }));
  }

  _render() {
    if (this._rendered) {
      if (this._form) this._form.data = this._config;
      return;
    }
    this._rendered = true;

    if (customElements.get('ha-form')) {
      const form = document.createElement('ha-form');
      form.hass = this._hass;
      form.data = this._config;
      form.schema = MOUSE_EDITOR_SCHEMA;
      form.computeLabel = (s) => MOUSE_EDITOR_LABELS[s.name] || s.name;
      form.addEventListener('value-changed', (e) => this._emit(e.detail.value));
      this.appendChild(form);
      this._form = form;
      return;
    }

    this.appendChild(buildMouseFallbackEditor(
      MOUSE_EDITOR_SCHEMA, MOUSE_EDITOR_LABELS, () => this._config, (cfg) => this._emit(cfg),
    ));
  }
}

// `getConfig` is a function, not an object: _emit() replaces this._config on
// every change, so a captured object would go stale after the first edit.
function buildMouseFallbackEditor(schema, labels, getConfig, onChange) {
  const config = getConfig();
  const wrap = document.createElement('div');
  wrap.style.cssText = 'display:flex;flex-direction:column;gap:10px;padding:8px 0';
  schema.forEach((item) => {
    const row = document.createElement('label');
    row.style.cssText = 'display:flex;align-items:center;gap:8px;font-size:14px';
    const isBool = !!item.selector.boolean;
    const input = document.createElement('input');
    if (isBool) {
      input.type = 'checkbox';
      input.checked = config[item.name] === true;
    } else {
      input.type = item.selector.number ? 'number' : 'text';
      if (item.selector.number && item.selector.number.step) input.step = item.selector.number.step;
      input.value = config[item.name] ?? '';
      input.style.flex = '1';
    }
    const text = document.createElement('span');
    text.textContent = (labels[item.name] || item.name) + (item.required ? ' *' : '');
    text.style.cssText = isBool ? '' : 'min-width:180px';
    if (isBool) { row.appendChild(input); row.appendChild(text); }
    else { row.appendChild(text); row.appendChild(input); }

    input.addEventListener('change', () => {
      const next = { ...getConfig() };
      if (isBool) next[item.name] = input.checked;
      else if (input.value === '') delete next[item.name];
      else next[item.name] = item.selector.number ? Number(input.value) : input.value;
      onChange(next);
    });
    wrap.appendChild(row);
  });
  return wrap;
}

customElements.define('ble-mouse-card-editor', BleMouseCardEditor);

window.customCards = window.customCards || [];
window.customCards.push({
  type: 'ble-mouse-card',
  name: 'BLE Mouse Control',
  description: 'Touchpad, mouse buttons, and scroll for BLE Keyboard component',
  preview: true,
});
