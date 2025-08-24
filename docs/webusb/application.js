(function() {
  'use strict';

  if (!('usb' in navigator)) {
    window.addEventListener('DOMContentLoaded', () => {
      const warning = document.createElement('div');
      warning.style.background = '#ffdddd';
      warning.style.color = '#a00';
      warning.style.padding = '1em';
      warning.style.margin = '1em 0';
      warning.style.border = '1px solid #a00';
      warning.style.fontWeight = 'bold';
      warning.textContent = 'This page is not supported by your browser. Use a Chromium-based browser such as Chrome, Edge, or Opera.';
      document.body.insertBefore(warning, document.body.firstChild);
    });
    return;
  }

  document.addEventListener('DOMContentLoaded', event => {
    let selectButton = document.querySelector("#select");
    let selectedDevice = document.querySelector('#selected-device')
    let statusDisplay = document.querySelector('#status');
    let saveButton = document.querySelector('#save');
    let mscCheckbox = document.querySelector('#enable-msc-check');
    let player1 = document.querySelector('#player1-select');
    let player2 = document.querySelector('#player2-select');
    let player3 = document.querySelector('#player3-select');
    let player4 = document.querySelector('#player4-select');
    let port;
    let selectedSerial = null;
    let receiveSm = null; // must define start(), process(), and timeout()
    let receiveSmTimeoutId = -1;
    let offlineHint = document.querySelector('#offline-hint');

    if (window.location.protocol !== "file:") {
      offlineHint.innerHTML = "<strong>Hint:</strong> Press ctrl+s or cmd+s to save this page locally for offline usage.";
    } else {
      offlineHint.innerHTML = "";
    }

    // CRC16-CCITT (XModem) implementation
    function crc16(arr) {
      let crc = 0xffff;
      for (let b of arr) {
        crc ^= (b << 8);
        for (let i = 0; i < 8; i++) {
          if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
          } else {
            crc = crc << 1;
          }
          crc &= 0xFFFF;
        }
      }
      return crc;
    }

    function send(addr, cmd, payload) {
      console.info(`SND ADDR: 0x${addr.toString(16)}, CMD: 0x${cmd.toString(16)}, PAYLOAD: [${Array.from(payload).map(b => '0x' + b.toString(16).padStart(2, '0')).join(', ')}]`);

      // Pack addr as variable-length 64-bit unsigned integer, 7 bits per byte, MSb=1 if more bytes follow
      let tmp = BigInt(addr);
      let addrBytes = [];
      // Extract 7 bits at a time, LSB first
      do {
        let currentByte = 0;
        if (addrBytes.length == 8)
        {
          // Last acceptable byte, MSb may be used for data
          currentByte = Number(tmp & BigInt(0xFF));
          tmp = 0;
        }
        else
        {
          currentByte = Number(tmp & BigInt(0x7F));
          tmp = tmp >> 7n;
          if (tmp > 0n)
          {
            // Another byte expected
            currentByte |= 0x80;
          }
        }
        addrBytes.push(currentByte);
      } while (tmp > 0n);

      let data = [...addrBytes, cmd, ...payload];
      const crc = crc16(data);
      // Append CRC16 (big-endian) to data
      data.push((crc >> 8) & 0xFF, crc & 0xFF);

      let len = data.length;
      let lenXor = len ^ 0xFFFF;
      const pkt = [0xDB, 0x8B, 0xAF, 0xD5];
      pkt.push((len >> 8) & 0xFF, len & 0xFF);
      pkt.push((lenXor >> 8) & 0xFF, lenXor & 0xFF);
      pkt.push(...data);

      let pktData = new Uint8Array(pkt)

      if (port && typeof port.send === 'function') {
        port.send(pktData);
      } else {
        console.error("Port is not connected or does not support send().");
      }
    }

    function smTimeout() {
      if (receiveSm) {
        receiveSm.timeout();
        receiveSm = null;
      }
    }

    function stopSmTimeout() {
      if (receiveSmTimeoutId >= 0) {
        clearTimeout(receiveSmTimeoutId);
        receiveSmTimeoutId = -1;
      }
    }

    function resetSmTimeout() {
      stopSmTimeout();
      receiveSmTimeoutId = setTimeout(smTimeout, 150);
    }

    function connectionComplete() {
      if (receiveSm) {
        receiveSm.start();
        resetSmTimeout();
      }
    }

    function connectionFailed(reason = 'Failed to connect to device') {
      stopSmTimeout();
      if (receiveSm) {
        receiveSm = null;
        disconnect(reason);
      }
    }

    function startSm(sm, selectedPort = null) {
      if (receiveSm) {
        stopSm('Operation canceled');
      } else {
        stopSmTimeout();
      }
      receiveSm = sm;
      if (!selectedSerial && !selectedPort) {
        statusDisplay.textContent = 'Please select a device first';
      }
      let portOrSerialNumber = selectedSerial;
      if (selectedPort) {
        portOrSerialNumber = selectedPort;
      }
      if (startConnect(portOrSerialNumber)) {
        resetSmTimeout();
      } else {
        disconnect('Failed to connect to device');
        receiveSm = null;
      }
    }

    function stopSm(reason) {
      disconnect(reason);
      receiveSm = null;
      stopSmTimeout();
    }

    // Starts the state machine which loads the settings from the device
    function startLoadSm(selectedPort) {
      selectedSerial = selectedPort.serial;
      selectedDevice.textContent = `Selected device: ${selectedPort.serial} v${selectedPort.major}.${selectedPort.minor}.${selectedPort.patch}`;
      statusDisplay.textContent = "Loading settings..."
      const GET_SETTINGS_ADDR = 123;
      var loadSm = {};
      loadSm.timeout = function() {
        disconnect('Load failed');
      };
      loadSm.start = function() {
        // Load the currently staged settings, not the settings on flash
        send(GET_SETTINGS_ADDR, 'S'.charCodeAt(0), [103]);
      }
      loadSm.process = function(addr, cmd, payload) {
        if (addr == GET_SETTINGS_ADDR) {
          if (cmd == 0x0a && payload.length >= 6) {
            // Retrieved settings
            mscCheckbox.checked = (payload[1] !== 0);
            player1.value = payload[2];
            player2.value = payload[3];
            player3.value = payload[4];
            player4.value = payload[5];
          }
        }
        stopSm('Settings loaded');
      }

      startSm(loadSm, selectedPort);
    }

    // Starts the state machine which saves the general settings
    function startSaveSm() {
      statusDisplay.textContent = "Saving...";
      const SEND_MSC_ADDR = 10;
      const SEND_CONTROLLER_A_ADDR = 11;
      const SEND_CONTROLLER_B_ADDR = 12;
      const SEND_CONTROLLER_C_ADDR = 13;
      const SEND_CONTROLLER_D_ADDR = 14;
      const SEND_SAVE_AND_RESTART_ADDR = 15;

      var saveSm = {};
      saveSm.timeout = function() {
        disconnect('Save failed');
      };
      saveSm.start = function() {
        const mscValue = mscCheckbox.checked ? 1 : 0;
        send(SEND_MSC_ADDR, 'S'.charCodeAt(0), [77, mscValue]);
      }
      saveSm.process = function(addr, cmd, payload) {
        if (addr == SEND_MSC_ADDR) {
          send(SEND_CONTROLLER_A_ADDR, 'S'.charCodeAt(0), [80, 0, player1.value]);
        } else if (addr == SEND_CONTROLLER_A_ADDR) {
          send(SEND_CONTROLLER_B_ADDR, 'S'.charCodeAt(0), [80, 1, player2.value]);
        } else if (addr == SEND_CONTROLLER_B_ADDR) {
          send(SEND_CONTROLLER_C_ADDR, 'S'.charCodeAt(0), [80, 2, player3.value]);
        } else if (addr == SEND_CONTROLLER_C_ADDR) {
          send(SEND_CONTROLLER_D_ADDR, 'S'.charCodeAt(0), [80, 3, player4.value]);
        } else if (addr == SEND_CONTROLLER_D_ADDR) {
          send(SEND_SAVE_AND_RESTART_ADDR, 'S'.charCodeAt(0), [83]);
        } else if (addr == SEND_SAVE_AND_RESTART_ADDR) {
          stopSm('Save successful!');
          return;
        }
        resetSmTimeout();
      }

      startSm(saveSm);
    }

    function handleIncomingMsg(addr, cmd, payload) {
      console.info(`RCV ADDR: 0x${addr.toString(16)}, CMD: 0x${cmd.toString(16)}, PAYLOAD: [${Array.from(payload).map(b => '0x' + b.toString(16).padStart(2, '0')).join(', ')}]`);
      // receiveSm
      if (receiveSm) {
        receiveSm.process(addr, cmd, payload);
      }
    }

    function disconnect(reason = '') {
        port.disconnect();
        statusDisplay.textContent = reason;
        port = null;
    }

    function setPortAndConnect(p) {
      port = p;
      port.ready = function () {
        connectionComplete();
      }
      return connect();
    }

    function startConnect(portOrSerialNumber){
      if (typeof(portOrSerialNumber) === 'string') {
        serial.getPorts().then(ports => {
          port = null;
          for (let i = 0; i < ports.length; i++) {
            if (ports[i].serial == portOrSerialNumber) {
              if (!setPortAndConnect(ports[i]))
              {
                connectionFailed();
              }
              return;
            }
          }
          connectionFailed('Could not find selected device');
        });
        return true;
      } else {
        return setPortAndConnect(portOrSerialNumber);
      }
    }

    function connect() {
      port.connect().then(() => {
        let receiveBuffer = new Uint8Array(0);

        function processPackets() {
          while (receiveBuffer.length >= 8) {
            // Check magic
            if (
              receiveBuffer[0] !== 0xDB ||
              receiveBuffer[1] !== 0x8B ||
              receiveBuffer[2] !== 0xAF ||
              receiveBuffer[3] !== 0xD5
            ) {
              // Invalid magic, discard first byte and retry
              receiveBuffer = receiveBuffer.slice(1);
              continue;
            }
            // Read size and size inverse
            const size = (receiveBuffer[4] << 8) | receiveBuffer[5];
            const sizeInv = (receiveBuffer[6] << 8) | receiveBuffer[7];
            if ((size ^ sizeInv) !== 0xFFFF) {
              // Invalid size inverse, discard first byte and retry
              receiveBuffer = receiveBuffer.slice(1);
              continue;
            }
            // Check if full payload is available
            if (receiveBuffer.length < 8 + size) {
              // Wait for more data
              break;
            }
            // Extract payload
            const payload = receiveBuffer.slice(8, 8 + size);

            if (payload.length < 4) {
              console.warn("Payload too short");
              receiveBuffer = receiveBuffer.slice(8 + size);
              continue;
            }

            // Verify CRC16
            const payloadWithoutCrc = payload.slice(0, -2);
            const receivedCrc = (payload[payload.length - 2] << 8) | payload[payload.length - 1];
            const computedCrc = crc16(payloadWithoutCrc);

            //console.info("PKT: " + Array.from(payload).map(b => b.toString(16).padStart(2, '0')).join(' '));

            if (receivedCrc !== computedCrc) {
              console.warn(`CRC mismatch: received ${receivedCrc.toString(16)}, computed ${computedCrc.toString(16)}`);
              receiveBuffer = receiveBuffer.slice(8 + size);
              continue;
            }

            // Parse address (variable-length, 7 bits per byte, MSb=1 if more bytes follow)
            let addr = 0n;
            let addrLen = 0;
            for (let i = 0; i < Math.min(9, payloadWithoutCrc.length); i++) {
              let mask = 0x7f;
              if (i == 8)
              {
                mask = 0xff;
              }
              addr |= BigInt(payloadWithoutCrc[i] & mask) << BigInt(7 * i);
              addrLen++;
              if ((payloadWithoutCrc[i] & 0x80) === 0) break;
            }
            if (addrLen === 0 || addrLen >= payloadWithoutCrc.length) {
              console.warn("Invalid address length");
              receiveBuffer = receiveBuffer.slice(8 + size);
              continue;
            }

            // Parse command
            const cmd = payloadWithoutCrc[addrLen];

            // Parse data payload
            const dataPayload = payloadWithoutCrc.slice(addrLen + 1);

            handleIncomingMsg(addr, cmd, dataPayload);

            // Remove processed packet from buffer
            receiveBuffer = receiveBuffer.slice(8 + size);
          }
        }

        port.onReceive = data => {
          // Append new data to buffer
          const newData = new Uint8Array(data.buffer);
          let tmp = new Uint8Array(receiveBuffer.length + newData.length);
          tmp.set(receiveBuffer, 0);
          tmp.set(newData, receiveBuffer.length);
          receiveBuffer = tmp;
          processPackets();
        };

        port.onReceiveError = error => {
          console.error(error);
          if (error && (error.name === 'NetworkError' || error.message === 'NetworkError')) {
            if (port && typeof port.disconnect === 'function') {
              disconnect('Lost connection with device');
            }
          }
        };
      }, error => {
        statusDisplay.textContent = error;
        return false;
      });

      return true;
    }

    selectButton.addEventListener('click', function (){
      statusDisplay.textContent = '';
      serial.requestPort().then(selectedPort => {
        if (selectedPort) {
          startLoadSm(selectedPort);
        }
      }).catch(error => {
        if (!(error instanceof NotFoundError))
        {
          statusDisplay.textContent = error;
        }
      });
    });

    saveButton.addEventListener('click', function() {
      startSaveSm();
    });

    // Select first device on load
    serial.getPorts().then(ports => {
      if (ports.length > 0) {
        startLoadSm(ports[0]);
      }
    });

  });
})();
