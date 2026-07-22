/**
 * HACS entry point for the ESPHome BLE Keyboard dashboard cards.
 *
 * HACS matches the .js file named after the repository, downloads every .js
 * beside it in dist/, but registers only this one as a dashboard resource — so
 * it has to pull the others in. The relative imports resolve inside
 * /hacsfiles/ESPHome-espidf_ble_keyboard/, where HACS places the files.
 *
 * Each card registers itself on import (customElements.define + customCards),
 * so importing for side effects is all that's needed — no build step, and the
 * cards stay separate readable files.
 *
 * Installing by hand instead? You can ignore this file and add whichever cards
 * you want as individual resources — see the README.
 */
import './keyboard-card.js';
import './mouse-card.js';
import './remote-card.js';
