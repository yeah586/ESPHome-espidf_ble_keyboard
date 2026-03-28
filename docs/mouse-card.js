/**
 * BLE Mouse Control Card for Home Assistant
 *
 * A custom Lovelace card that provides a touchpad, 3 mouse buttons,
 * and scroll controls for the ESPHome BLE Keyboard component.
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
 *   # scroll_sensitivity: 2        # scroll multiplier (default 2)
 *   # tap_to_click: true           # tap touchpad = left click (default true)
 *   # two_finger_scroll: true      # two-finger drag = scroll (default true)
 *
 * Full example with overrides:
 *   type: custom:ble-mouse-card
 *   device: bluetooth_keyboard
 *   name: Living Room Mouse
 *   sensitivity: 2.0
 *   scroll_sensitivity: 3
 *   tap_to_click: false
 *   two_finger_scroll: false
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
      name: config.name || 'Mouse Control',
      sensitivity: config.sensitivity || 1.5,
      scroll_sensitivity: config.scroll_sensitivity || 2,
      tap_to_click: config.tap_to_click !== false,
      two_finger_scroll: config.two_finger_scroll !== false,
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
          height: 200px;
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
          ${this._config.name}
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

    this._setupTouchpad();
    this._setupButtons();
    this._setupScroll();
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
    let startTime = 0;
    let moved = false;
    let fingers = 0;
    let lastScrollY = 0;

    // Accumulator for sub-pixel movements
    let accumX = 0;
    let accumY = 0;

    const sensitivity = this._config.sensitivity;
    const scrollSensitivity = this._config.scroll_sensitivity;

    const onStart = (x, y, touchCount) => {
      tracking = true;
      lastX = x;
      lastY = y;
      startTime = Date.now();
      moved = false;
      fingers = touchCount || 1;
      lastScrollY = y;
      accumX = 0;
      accumY = 0;
      pad.classList.add('active');
    };

    const onMove = (x, y) => {
      if (!tracking) return;

      if (this._config.two_finger_scroll && fingers >= 2) {
        // Two-finger scroll
        const scrollDelta = (lastScrollY - y) * scrollSensitivity;
        accumY += scrollDelta;
        const intScroll = Math.trunc(accumY);
        if (intScroll !== 0) {
          const clamped = Math.max(-127, Math.min(127, intScroll));
          this._callService('mouse_scroll', { amount: clamped });
          accumY -= intScroll;
        }
        lastScrollY = y;
        moved = true;
        return;
      }

      const rawDx = (x - lastX) * sensitivity;
      const rawDy = (y - lastY) * sensitivity;

      accumX += rawDx;
      accumY += rawDy;

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
    };

    const onEnd = () => {
      if (!tracking) return;
      tracking = false;
      pad.classList.remove('active');

      // Tap-to-click: short tap with minimal movement
      if (this._config.tap_to_click && !moved) {
        const elapsed = Date.now() - startTime;
        if (elapsed < 250) {
          this._callService('mouse_click', { buttons: 1 });
        }
      }
    };

    // Mouse events
    pad.addEventListener('mousedown', (e) => {
      e.preventDefault();
      onStart(e.clientX, e.clientY, 1);
    });
    window.addEventListener('mousemove', (e) => onMove(e.clientX, e.clientY));
    window.addEventListener('mouseup', () => onEnd());

    // Touch events
    pad.addEventListener('touchstart', (e) => {
      e.preventDefault();
      const t = e.touches[0];
      onStart(t.clientX, t.clientY, e.touches.length);
    }, { passive: false });

    pad.addEventListener('touchmove', (e) => {
      e.preventDefault();
      const t = e.touches[0];
      // Update finger count live for two-finger scroll detection
      if (e.touches.length > fingers) fingers = e.touches.length;
      onMove(t.clientX, t.clientY);
    }, { passive: false });

    pad.addEventListener('touchend', (e) => {
      e.preventDefault();
      if (e.touches.length === 0) onEnd();
    }, { passive: false });
  }

  // ── Click buttons ────────────────────────────────────────────────

  _setupButtons() {
    const btnMap = {
      'btn-left': 1,
      'btn-middle': 4,
      'btn-right': 2,
    };
    for (const [id, button] of Object.entries(btnMap)) {
      const el = this.shadowRoot.getElementById(id);
      el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        el.classList.add('pressed');
        this._callService('mouse_click', { buttons: button });
      });
      el.addEventListener('pointerup', () => el.classList.remove('pressed'));
      el.addEventListener('pointerleave', () => el.classList.remove('pressed'));
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
        startScroll(amount);
      });
      btn.addEventListener('pointerup', stopScroll);
      btn.addEventListener('pointerleave', stopScroll);
    }
  }

  getCardSize() {
    return 4;
  }

  static getStubConfig() {
    return { device: 'bluetooth_keyboard' };
  }
}

customElements.define('ble-mouse-card', BleMouseCard);

window.customCards = window.customCards || [];
window.customCards.push({
  type: 'ble-mouse-card',
  name: 'BLE Mouse Control',
  description: 'Touchpad, mouse buttons, and scroll for BLE Keyboard component',
  preview: true,
});
