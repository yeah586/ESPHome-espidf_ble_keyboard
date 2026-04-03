/**
 * BLE Media Remote Card for Home Assistant
 *
 * A custom Lovelace card that provides a modern media remote control
 * for the ESPHome BLE Keyboard component. Includes power, navigation,
 * volume, media playback, and app launch buttons.
 *
 * Installation:
 *   1. Copy this file to your HA config/www/ folder.
 *   2. Add the resource in HA:
 *        Settings -> Dashboards -> Resources -> Add Resource
 *        URL: /local/remote-card.js   Type: JavaScript Module
 *   3. Add ESPHome services to your device YAML (see README).
 *   4. Add the card to a dashboard via the UI or YAML.
 *
 * Card YAML:
 *   type: custom:ble-remote-card
 *   device: bluetooth_keyboard    # your ESPHome device name
 *   # Optional overrides:
 *   # name: Media Remote           # card title (auto from HA if omitted)
 *   # show_numpad: true            # show number pad (default false)
 *   # show_apps: true              # show app launch row (default true)
 *   # show_color: true             # show color buttons (default false)
 *
 * Full example with overrides:
 *   type: custom:ble-remote-card
 *   device: bluetooth_keyboard
 *   name: Living Room Remote
 *   show_numpad: true
 *   show_apps: true
 *   show_color: true
 */

class BleRemoteCard extends HTMLElement {
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
      show_numpad: config.show_numpad === true,
      show_apps: config.show_apps !== false,
      show_color: config.show_color === true,
    };
  }

  _initialize() {
    if (this._initialized) return;
    this._initialized = true;

    const shadow = this.attachShadow({ mode: 'open' });
    shadow.innerHTML = `
      <style>
        :host { display: block; }
        .card {
          background: var(--ha-card-background, var(--card-background-color, #fff));
          border-radius: var(--ha-card-border-radius, 12px);
          box-shadow: var(--ha-card-box-shadow, 0 2px 6px rgba(0,0,0,.15));
          padding: 16px;
          overflow: hidden;
        }
        .header {
          display: flex; align-items: center; gap: 8px;
          font-size: 16px; font-weight: 600; margin-bottom: 14px;
          color: var(--primary-text-color, #333);
        }
        .header svg { width: 22px; height: 22px; fill: var(--primary-color, #03a9f4); }

        /* Button grid sections */
        .section { margin-bottom: 12px; }
        .section:last-child { margin-bottom: 0; }
        .row { display: flex; justify-content: center; gap: 8px; margin-bottom: 8px; }
        .row:last-child { margin-bottom: 0; }

        /* Standard round button */
        .btn {
          width: 52px; height: 52px;
          border: 1px solid var(--divider-color, #e0e0e0);
          border-radius: 50%;
          background: var(--secondary-background-color, #f5f5f5);
          color: var(--primary-text-color, #333);
          font-size: 13px; font-weight: 500;
          cursor: pointer; touch-action: manipulation;
          display: flex; align-items: center; justify-content: center;
          transition: background 0.1s, transform 0.1s;
          user-select: none; -webkit-user-select: none;
        }
        .btn:active, .btn.p {
          background: var(--primary-color, #03a9f4);
          color: #fff; transform: scale(0.93);
        }
        .btn svg { width: 22px; height: 22px; fill: currentColor; pointer-events: none; }

        /* Power button */
        .btn.power { background: #c62828; color: #fff; border-color: #c62828; }
        .btn.power:active, .btn.power.p { background: #e53935; }

        /* Color buttons */
        .btn.red { background: #e53935; color: #fff; border: none; width: 44px; height: 44px; }
        .btn.green { background: #43a047; color: #fff; border: none; width: 44px; height: 44px; }
        .btn.yellow { background: #fdd835; color: #333; border: none; width: 44px; height: 44px; }
        .btn.blue { background: #1e88e5; color: #fff; border: none; width: 44px; height: 44px; }

        /* D-pad */
        .dpad { display: grid; grid-template-columns: 52px 52px 52px; grid-template-rows: 52px 52px 52px; gap: 4px; justify-content: center; margin: 8px 0; }
        .dpad .btn { border-radius: 12px; }
        .dpad .center { background: var(--primary-color, #03a9f4); color: #fff; border-color: var(--primary-color, #03a9f4); font-size: 11px; font-weight: 700; border-radius: 50%; }
        .dpad .center:active { background: var(--accent-color, #ff9800); }
        .dpad .empty { visibility: hidden; }

        /* Wide buttons */
        .btn.wide { width: auto; border-radius: 26px; padding: 0 18px; font-size: 12px; }

        /* Volume/channel strip */
        .strip { display: flex; align-items: center; justify-content: center; gap: 16px; }
        .strip-group { display: flex; flex-direction: column; align-items: center; gap: 4px; }
        .strip-label { font-size: 10px; color: var(--secondary-text-color, #888); font-weight: 600; text-transform: uppercase; }

        /* Media controls */
        .media-row { display: flex; justify-content: center; gap: 10px; }
        .btn.media { width: 46px; height: 46px; }

        /* Number pad */
        .numpad { display: grid; grid-template-columns: repeat(3, 52px); gap: 6px; justify-content: center; }

        /* App row */
        .app-row { display: flex; justify-content: center; gap: 8px; flex-wrap: wrap; }
        .btn.app { width: auto; border-radius: 26px; padding: 0 14px; height: 38px; font-size: 11px; }

        /* Divider */
        .divider { height: 1px; background: var(--divider-color, #e0e0e0); margin: 12px 0; }
      </style>
      <div class="card">
        <div class="header">
          <svg viewBox="0 0 24 24"><path d="M18 7V4c0-1.1-.9-2-2-2H8c-1.1 0-2 .9-2 2v3H2v15h20V7h-4zM8 4h8v3H8V4zm10 16H6V9h12v11zm-6-7c1.1 0 2-.9 2-2s-.9-2-2-2-2 .9-2 2 .9 2 2 2z"/></svg>
          <span class="header-name">${this._config.name || 'Media Remote'}</span>
        </div>

        <!-- Power & Back row -->
        <div class="section">
          <div class="row">
            <button class="btn power" id="power" title="Power">
              <svg viewBox="0 0 24 24"><path d="M13 3h-2v10h2V3zm4.83 2.17l-1.42 1.42A6.92 6.92 0 0119 12c0 3.87-3.13 7-7 7s-7-3.13-7-7c0-2.04.88-3.88 2.29-5.16L5.88 5.46A8.96 8.96 0 003 12c0 4.97 4.03 9 9 9s9-4.03 9-9c0-2.72-1.21-5.15-3.17-6.83z"/></svg>
            </button>
            <div style="flex:1"></div>
            <button class="btn" id="search" title="Search">
              <svg viewBox="0 0 24 24"><path d="M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/></svg>
            </button>
            <button class="btn" id="info" title="Info / Guide">
              <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/></svg>
            </button>
            <button class="btn" id="mute" title="Mute">
              <svg viewBox="0 0 24 24"><path d="M16.5 12A4.5 4.5 0 0014 7.97v2.21l2.45 2.45c.03-.2.05-.41.05-.63zm2.5 0c0 .94-.2 1.82-.54 2.64l1.51 1.51A8.796 8.796 0 0021 12c0-4.28-2.99-7.86-7-8.77v2.06c2.89.86 5 3.54 5 6.71zM4.27 3L3 4.27 7.73 9H3v6h4l5 5v-6.73l4.25 4.25c-.67.52-1.42.93-2.25 1.18v2.06a8.99 8.99 0 003.69-1.81L19.73 21 21 19.73l-9-9L4.27 3zM12 4L9.91 6.09 12 8.18V4z"/></svg>
            </button>
            <button class="btn" id="home" title="Home (Win)">
              <svg viewBox="0 0 24 24"><path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/></svg>
            </button>
            <button class="btn" id="back" title="Back (Escape)">
              <svg viewBox="0 0 24 24"><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg>
            </button>
          </div>
        </div>

        <!-- D-Pad Navigation -->
        <div class="section">
          <div class="dpad">
            <div class="empty"></div>
            <button class="btn" id="up" title="Up">
              <svg viewBox="0 0 24 24"><path d="M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z"/></svg>
            </button>
            <div class="empty"></div>
            <button class="btn" id="left" title="Left">
              <svg viewBox="0 0 24 24"><path d="M15.41 7.41L14 6l-6 6 6 6 1.41-1.41L10.83 12z"/></svg>
            </button>
            <button class="btn center" id="ok">OK</button>
            <button class="btn" id="right" title="Right">
              <svg viewBox="0 0 24 24"><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg>
            </button>
            <div class="empty"></div>
            <button class="btn" id="down" title="Down">
              <svg viewBox="0 0 24 24"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z"/></svg>
            </button>
            <div class="empty"></div>
          </div>
        </div>

        <!-- Volume & Channel -->
        <div class="section">
          <div class="strip">
            <div class="strip-group">
              <span class="strip-label">Vol</span>
              <button class="btn" id="vol_up" title="Volume Up">
                <svg viewBox="0 0 24 24"><path d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"/></svg>
              </button>
              <button class="btn" id="vol_dn" title="Volume Down">
                <svg viewBox="0 0 24 24"><path d="M19 13H5v-2h14v2z"/></svg>
              </button>
            </div>
            <div style="width:40px"></div>
            <div class="strip-group">
              <span class="strip-label">Ch</span>
              <button class="btn" id="pg_up" title="Page Up (Channel Up)">
                <svg viewBox="0 0 24 24"><path d="M7.41 15.41L12 10.83l4.59 4.58L18 14l-6-6-6 6z"/></svg>
              </button>
              <button class="btn" id="pg_dn" title="Page Down (Channel Down)">
                <svg viewBox="0 0 24 24"><path d="M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6z"/></svg>
              </button>
            </div>
          </div>
        </div>

        <div class="divider"></div>

        <!-- Media Controls -->
        <div class="section">
          <div class="media-row">
            <button class="btn media" id="prev" title="Previous Track">
              <svg viewBox="0 0 24 24"><path d="M6 6h2v12H6zm3.5 6l8.5 6V6z"/></svg>
            </button>
            <button class="btn media" id="rew" title="Rewind">
              <svg viewBox="0 0 24 24"><path d="M11 18V6l-8.5 6 8.5 6zm.5-6l8.5 6V6l-8.5 6z"/></svg>
            </button>
            <button class="btn media" id="play" title="Play / Pause">
              <svg viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg>
            </button>
            <button class="btn media" id="stop" title="Stop">
              <svg viewBox="0 0 24 24"><path d="M6 6h12v12H6z"/></svg>
            </button>
            <button class="btn media" id="fwd" title="Fast Forward">
              <svg viewBox="0 0 24 24"><path d="M4 18l8.5-6L4 6v12zm9-12v12l8.5-6L13 6z"/></svg>
            </button>
            <button class="btn media" id="next" title="Next Track">
              <svg viewBox="0 0 24 24"><path d="M6 18l8.5-6L6 6v12zM16 6v12h2V6h-2z"/></svg>
            </button>
          </div>
        </div>

        <!-- Color Buttons (optional) -->
        <div class="section" id="color-section" style="display:none">
          <div class="divider"></div>
          <div class="row">
            <button class="btn red" id="c_red"></button>
            <button class="btn green" id="c_green"></button>
            <button class="btn yellow" id="c_yellow"></button>
            <button class="btn blue" id="c_blue"></button>
          </div>
        </div>

        <!-- Number Pad (optional) -->
        <div class="section" id="numpad-section" style="display:none">
          <div class="divider"></div>
          <div class="numpad" id="numpad"></div>
        </div>

        <!-- App Launch Row (optional) -->
        <div class="section" id="app-section" style="display:none">
          <div class="divider"></div>
          <div class="app-row" id="app-row"></div>
        </div>
      </div>
    `;

    // Show optional sections
    if (this._config.show_color) {
      shadow.getElementById('color-section').style.display = '';
    }

    if (this._config.show_numpad) {
      const section = shadow.getElementById('numpad-section');
      section.style.display = '';
      const numpad = shadow.getElementById('numpad');
      for (let i = 1; i <= 9; i++) {
        const btn = document.createElement('button');
        btn.className = 'btn';
        btn.textContent = i;
        btn.addEventListener('pointerdown', (e) => { 
          e.preventDefault(); 
          this._press(btn); 
          this._sendString(String(i)); 
        });
        numpad.appendChild(btn);
      }
      // Bottom row: empty, 0, empty
      const spacer1 = document.createElement('div');
      const zero = document.createElement('button');
      zero.className = 'btn';
      zero.textContent = '0';
      zero.addEventListener('pointerdown', (e) => { 
        e.preventDefault(); 
        this._press(zero); 
        this._sendString('0'); 
      });
      const spacer2 = document.createElement('div');
      numpad.appendChild(spacer1);
      numpad.appendChild(zero);
      numpad.appendChild(spacer2);
    }

    if (this._config.show_apps) {
      const section = shadow.getElementById('app-section');
      section.style.display = '';
      const row = shadow.getElementById('app-row');
      const apps = [
        { name: 'Explorer', consumer: 0x0194 },
        { name: 'Browser', consumer: 0x0223 },
        { name: 'Email', consumer: 0x018A },
        { name: 'Calc', consumer: 0x0192 },
        { name: 'Search', consumer: 0x0221 },
      ];
      apps.forEach(app => {
        const btn = document.createElement('button');
        btn.className = 'btn app';
        btn.textContent = app.name;
        btn.addEventListener('pointerdown', (e) => {
          e.preventDefault();
          this._press(btn);
          this._sendConsumer(app.consumer);
        });
        row.appendChild(btn);
      });
    }

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

    // Wire up all buttons
    this._wireButtons(shadow);
  }

  _press(btn) {
    btn.classList.add('p');
    setTimeout(() => btn.classList.remove('p'), 150);
  }

  _wireButtons(shadow) {
    // Button action map: id -> handler
    const actions = {
      // Power & navigation
      power:  () => this._sendConsumer(0x0030),    // HID Power
      back:   () => this._sendConsumer(0x0224),    // AC Back
      home:   () => this._sendConsumer(0x0223),    // AC Home
      mute:   () => this._sendConsumer(0x00E2),    // Mute

      search: () => this._sendConsumer(0x0221),    // AC Search
      info:   () => this._sendConsumer(0x0209),    // AC More Info / Guide

      // D-pad (arrow keys + Enter)
      up:     () => this._sendConsumer(0x0042),    // Menu Up
      down:   () => this._sendConsumer(0x0043),    // Menu Down
      left:   () => this._sendConsumer(0x0044),    // Menu Left
      right:  () => this._sendConsumer(0x0045),    // Menu Right
      ok:     () => this._sendConsumer(0x0041),    // Menu Pick (OK)

      // Volume
      vol_up: () => this._sendConsumer(0x00E9),
      vol_dn: () => this._sendConsumer(0x00EA),

      // Channel (Page Up/Down)
      pg_up:  () => this._sendKey(0, 0x4B),        // Page Up
      pg_dn:  () => this._sendKey(0, 0x4E),        // Page Down

      // Media
      play:   () => this._sendConsumer(0x00CD),     // Play/Pause
      stop:   () => this._sendConsumer(0x00B7),     // Stop
      prev:   () => this._sendConsumer(0x00B6),     // Previous
      next:   () => this._sendConsumer(0x00B5),     // Next
      rew:    () => this._sendConsumer(0x00B4),     // Rewind
      fwd:    () => this._sendConsumer(0x00B3),     // Fast Forward

      // Color buttons (F1-F4, common for media apps)
      c_red:    () => this._sendKey(0, 0x3A),       // F1
      c_green:  () => this._sendKey(0, 0x3B),       // F2
      c_yellow: () => this._sendKey(0, 0x3C),       // F3
      c_blue:   () => this._sendKey(0, 0x3D),       // F4
    };

    for (const [id, handler] of Object.entries(actions)) {
      const el = shadow.getElementById(id);
      if (!el) continue;

      // Support hold-to-repeat for volume and channel
      if (['vol_up', 'vol_dn', 'pg_up', 'pg_dn'].includes(id)) {
        let interval = null;
        const start = (e) => {
          e.preventDefault();
          this._press(el);
          handler();
          interval = setInterval(() => handler(), 180);
        };
        const stop = () => { if (interval) { clearInterval(interval); interval = null; } };
        el.addEventListener('pointerdown', start);
        el.addEventListener('pointerup', stop);
        el.addEventListener('pointerleave', stop);
        el.addEventListener('pointercancel', stop);
      } else {
        el.addEventListener('pointerdown', (e) => {
          e.preventDefault();
          this._press(el);
          handler();
        });
      }
    }
  }

  _sendKey(modifier, keycode) {
    if (!this._hass) return;
    this._hass.callService('esphome', `${this._config.device}_send_key`, { modifier, keycode });
  }

  _sendString(text) {
    if (!this._hass) return;
    this._hass.callService('esphome', `${this._config.device}_send_string`, { keys: text });
  }

  _sendConsumer(code) {
    if (!this._hass) return;
    this._hass.callService('esphome', `${this._config.device}_send_consumer`, { code });
  }

  getCardSize() {
    return 6;
  }
}

customElements.define('ble-remote-card', BleRemoteCard);

window.customCards = window.customCards || [];
window.customCards.push({
  type: 'ble-remote-card',
  name: 'BLE Media Remote',
  description: 'Media remote control for ESPHome BLE Keyboard',
});
