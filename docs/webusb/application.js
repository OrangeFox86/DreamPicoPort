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
    let receiveSm = null; // must define start(), process(), and timeout() in the object
    let receiveSmTimeoutId = -1;
    let offlineHint = document.querySelector('#offline-hint');
    let vmu1ARadio = document.querySelector('#vmu1-a-radio');
    let vmu2ARadio = document.querySelector('#vmu2-a-radio');
    let vmu1BRadio = document.querySelector('#vmu1-b-radio');
    let vmu2BRadio = document.querySelector('#vmu2-b-radio');
    let vmu1CRadio = document.querySelector('#vmu1-c-radio');
    let vmu2CRadio = document.querySelector('#vmu2-c-radio');
    let vmu1DRadio = document.querySelector('#vmu1-d-radio');
    let vmu2DRadio = document.querySelector('#vmu2-d-radio');
    let readVmuButton = document.querySelector('#read-vmu');
    let writeVmuButton = document.querySelector('#write-vmu');
    let vmuProgressContainer = document.querySelector('#vmu-progress-container');
    let vmuProgress = document.querySelector('#vmu-progress')
    let vmuProgressText = document.querySelector('#vmu-progress-label')
    const CMD_OK = 0x0A; // Command success
    const CMD_FAIL = 0x0F; // Command failed - data was parsed but execution failed
    const CMD_INVALID = 0xFE; // Command was missing data
    const CMD_BAD = 0xFF; // Command had to target

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
      if (receiveSm) {
        receiveSmTimeoutId = setTimeout(smTimeout, 150);
      }
    }

    function connectionComplete() {
      if (receiveSm) {
        receiveSm.start();
        resetSmTimeout();
      }
    }

    function connectionFailed(reason = 'Operation failed: failed to connect to device') {
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
        setStatus('Please select a device first');
      }
      let portOrSerialNumber = selectedSerial;
      if (selectedPort) {
        portOrSerialNumber = selectedPort;
      }
      if (startConnect(portOrSerialNumber)) {
        resetSmTimeout();
      } else {
        disconnect('Failed to connect to device', 'red', 'bold');
        receiveSm = null;
      }
    }

    function stopSm(reason, color = 'black', fontWeight = 'normal') {
      disconnect(reason, color, fontWeight);
      receiveSm = null;
      stopSmTimeout();
    }

    function enableAllControls() {
      let tabcontent = document.getElementsByClassName("tabcontent");
      for (let i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.pointerEvents = 'auto';
        tabcontent[i].style.opacity = 1.0;
      }
    }

    // Starts the state machine which loads the settings from the device
    function startLoadSm(selectedPort) {
      selectedSerial = selectedPort.serial;
      selectedDevice.textContent = `Selected device: ${selectedPort.serial} v${selectedPort.major}.${selectedPort.minor}.${selectedPort.patch}`;
      enableAllControls();
      setStatus("Loading settings...");
      const GET_SETTINGS_ADDR = 123;
      var loadSm = {};
      loadSm.timeout = function() {
        disconnect('Load failed', 'red', 'bold');
      };
      loadSm.start = function() {
        // Load the currently staged settings, not the settings on flash
        send(GET_SETTINGS_ADDR, 'S'.charCodeAt(0), [103]);
      }
      loadSm.process = function(addr, cmd, payload) {
        if (addr == GET_SETTINGS_ADDR) {
          if (cmd == CMD_OK && payload.length >= 6) {
            // Retrieved settings
            mscCheckbox.checked = (payload[1] !== 0);
            player1.value = payload[2];
            player2.value = payload[3];
            player3.value = payload[4];
            player4.value = payload[5];
            stopSm('Settings loaded');
            return;
          }
        }
        stopSm('Failed to load settings', 'red', 'bold')
      }

      startSm(loadSm, selectedPort);
    }

    // Starts the state machine which saves the general settings
    function startSaveSm() {
      setStatus("Saving...");
      const SEND_MSC_ADDR = 10;
      const SEND_CONTROLLER_A_ADDR = 11;
      const SEND_CONTROLLER_B_ADDR = 12;
      const SEND_CONTROLLER_C_ADDR = 13;
      const SEND_CONTROLLER_D_ADDR = 14;
      const SEND_SAVE_AND_RESTART_ADDR = 15;

      var saveSm = {};
      saveSm.timeout = function() {
        disconnect('Save failed', 'red', 'bold');
      };
      saveSm.start = function() {
        const mscValue = mscCheckbox.checked ? 1 : 0;
        send(SEND_MSC_ADDR, 'S'.charCodeAt(0), [77, mscValue]);
      }
      saveSm.process = function(addr, cmd, payload) {
        if (addr == SEND_MSC_ADDR) {
          if (cmd == CMD_OK) {
            send(SEND_CONTROLLER_A_ADDR, 'S'.charCodeAt(0), [80, 0, player1.value]);
          } else {
            stopSm('Failed to set Mass Storage Class setting', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_CONTROLLER_A_ADDR) {
          if (cmd == CMD_OK) {
            send(SEND_CONTROLLER_B_ADDR, 'S'.charCodeAt(0), [80, 1, player2.value]);
          } else {
            stopSm('Failed to set controller A setting', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_CONTROLLER_B_ADDR) {
          if (cmd == CMD_OK) {
            send(SEND_CONTROLLER_C_ADDR, 'S'.charCodeAt(0), [80, 2, player3.value]);
          } else {
            stopSm('Failed to set controller B setting', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_CONTROLLER_C_ADDR) {
          if (cmd == CMD_OK) {
            send(SEND_CONTROLLER_D_ADDR, 'S'.charCodeAt(0), [80, 3, player4.value]);
          } else {
            stopSm('Failed to set controller C setting', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_CONTROLLER_D_ADDR) {
          if (cmd == CMD_OK) {
            send(SEND_SAVE_AND_RESTART_ADDR, 'S'.charCodeAt(0), [83]);
          } else {
            stopSm('Failed to set controller D setting', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_SAVE_AND_RESTART_ADDR) {
          if (cmd == CMD_OK) {
            stopSm('Save successful!');
            return;
          } else {
            stopSm('Failed save settings', 'red', 'bold');
            return;
          }
        }
        resetSmTimeout();
      }

      startSm(saveSm);
    }

    function resetProgressBar() {
      vmuProgress.value = 0;
      vmuProgressContainer.style.display = 'none';
      vmuProgressText.textContent = '0%';
    }

    function getMapleHostAddr(controllerIdx) {
      if (controllerIdx == 3) {
        return 0xC0;
      } else if (controllerIdx == 2) {
        return 0x80;
      } else if (controllerIdx == 1) {
        return 0x40;
      } else {
        return 0x00;
      }
    }

    // Starts the state machine which reads all VMU data
    function startReadVmu(controllerIdx, vmuIndex) {
      setStatus("Reading VMU...");
      const READ_ADDR = 0xAA;

      var readVmuSm = {};
      readVmuSm.retries = 0;
      readVmuSm.controllerIdx = controllerIdx;
      readVmuSm.vmuIndex = vmuIndex;
      readVmuSm.vmuData = new Uint8Array(128 * 1024); // 128KB VMU storage
      readVmuSm.dataOffset = 0;
      readVmuSm.hostAddr = getMapleHostAddr(controllerIdx);
      readVmuSm.destAddr = readVmuSm.hostAddr | (1 << vmuIndex);
      readVmuSm.currentBlock = 0;
      function sendCurrentBlock() {
        send(READ_ADDR, '0'.charCodeAt(0), [0x0B, readVmuSm.destAddr, readVmuSm.hostAddr, 2, 0, 0, 0, 2, 0, 0, 0, readVmuSm.currentBlock]);
      };
      readVmuSm.timeout = function() {
        if (readVmuSm.retries++ < 2) {
            // Re-read current block
            sendCurrentBlock();
        } else {
          disconnect('VMU read failed - timeout', 'red', 'bold');
          resetProgressBar();
        }
      };
      readVmuSm.start = function() {
        vmuProgressContainer.style.display = 'block';
        vmuProgressContainer.style.visibility = 'visible';
        vmuProgress.value = 0;
        vmuProgressText.textContent = '0%';
        readVmuSm.startTime = Date.now();
        sendCurrentBlock();
      };
      readVmuSm.process = function(addr, cmd, payload) {
        let fnType = Number(addr & BigInt(0xff));
        let receivedBlock = -1;
        if (payload.length >= 12)
        {
          receivedBlock = payload[11];
        }
        if (fnType == READ_ADDR && receivedBlock == readVmuSm.currentBlock && cmd == CMD_OK && payload.length == 524) {
          // Copy received data to VMU buffer
          // TODO: verify maple packet header
          readVmuSm.vmuData.set(payload.slice(12, payload.length), readVmuSm.dataOffset);
          readVmuSm.dataOffset += payload.length - 12;
          readVmuSm.currentBlock++;
          readVmuSm.retries = 0;

          // Update progress
          const progress = (readVmuSm.currentBlock / 256) * 100;
          vmuProgress.value = progress;
          vmuProgressText.textContent = Math.round(progress) + '%';

          if (readVmuSm.currentBlock < 256) {
            // Read next block
            sendCurrentBlock();
            resetSmTimeout();
          } else {
            // Reading complete, save file
            const blob = new Blob([readVmuSm.vmuData], { type: 'application/octet-stream' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `vmu_controller${String.fromCharCode(65 + readVmuSm.controllerIdx)}_slot${readVmuSm.vmuIndex + 1}.bin`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);

            stopSm('VMU read complete');
            resetProgressBar();
            const readTime = Date.now() - readVmuSm.startTime;
            console.log(`VMU read completed in ${readTime}ms`);
          }
        } else if (readVmuSm.retries++ < 2) {
            // Re-read current block
            sendCurrentBlock();
            resetSmTimeout();
        } else {
          stopSm('VMU read failed', 'red', 'bold');
          resetProgressBar();
        }
      };

      startSm(readVmuSm);
    }

    // Starts the state machine which writes to a selected VMU
    function startWriteVmu(controllerIdx, vmuIdx, fileData) {
      setStatus("Writing VMU...");
      const WRITE_ADDR = 0x55;
      const NUM_PHASES_PER_BLOCK = 4;
      const BLOCK_SIZE = 512;
      const PHASE_SIZE = BLOCK_SIZE / NUM_PHASES_PER_BLOCK;
      const TOTAL_BLOCKS = 256;

      var writeVmuSm = {};
      writeVmuSm.retries = 0;
      writeVmuSm.controllerIdx = controllerIdx;
      writeVmuSm.vmuIndex = vmuIdx;
      writeVmuSm.hostAddr = getMapleHostAddr(controllerIdx);
      writeVmuSm.destAddr = writeVmuSm.hostAddr | (1 << vmuIdx);
      writeVmuSm.currentBlock = 0;
      writeVmuSm.currentPhase = 0; // [0,4] // commit on 4
      writeVmuSm.writeDelayMs = 10; // delay between writes, grows on each retry

      function writeCurrentPhase() {
        const locationWord = [0, writeVmuSm.currentPhase, 0, writeVmuSm.currentBlock];
        const dataOffset = (writeVmuSm.currentBlock * BLOCK_SIZE) + (writeVmuSm.currentPhase * PHASE_SIZE);
        const dataChunk = fileData.slice(dataOffset, dataOffset + PHASE_SIZE);
        send(WRITE_ADDR | (writeVmuSm.currentBlock << 8) | (writeVmuSm.currentPhase << 16), '0'.charCodeAt(0), [0x0C, writeVmuSm.destAddr, writeVmuSm.hostAddr, 34, 0, 0, 0, 2, ...locationWord, ...dataChunk]);
      }

      function writeCommit() {
        const locationWord = [0, writeVmuSm.currentPhase, 0, writeVmuSm.currentBlock];
        send(WRITE_ADDR | (writeVmuSm.currentBlock << 8) | (writeVmuSm.currentPhase << 16), '0'.charCodeAt(0), [0x0D, writeVmuSm.destAddr, writeVmuSm.hostAddr, 2, 0, 0, 0, 2, ...locationWord]);
      }

      function retry() {
        if (++writeVmuSm.retries > 2) {
          return false;
        }

        resetSmTimeout();
        writeVmuSm.currentPhase = 0;
        writeVmuSm.writeDelayMs += 5;
        if (writeVmuSm.writeDelayMs > 30) {
          writeVmuSm.writeDelayMs = 30;
        }

        setTimeout(() => {
          writeCurrentPhase();
        }, writeVmuSm.writeDelayMs);

        return true;
      }

      writeVmuSm.timeout = function() {
        if (!retry()) {
          disconnect('VMU write failed - timeout', 'red', 'bold');
          resetProgressBar();
        }
      };

      writeVmuSm.start = function() {
        vmuProgressContainer.style.display = 'block';
        vmuProgressContainer.style.visibility = 'visible';
        vmuProgress.value = 0;
        vmuProgressText.textContent = '0%';
        writeVmuSm.startTime = Date.now();
        writeCurrentPhase();
      };

      writeVmuSm.process = function(addr, cmd, payload) {
        let fnType = Number(addr & BigInt(0xff));
        let forBlock = Number((addr >> BigInt(8)) & BigInt(0xff));
        let forPhase = Number((addr >> BigInt(16)) & BigInt(0xff));

        if (
          fnType == WRITE_ADDR &&
          cmd == CMD_OK &&
          forBlock == writeVmuSm.currentBlock &&
          forPhase == writeVmuSm.currentPhase &&
          payload.length >= 1 &&
          payload[0] == 0x07 // Acknowledge from Maple Bus
        ) {
          if (++writeVmuSm.currentPhase > 4)
          {
            writeVmuSm.currentPhase = 0;
            if (++writeVmuSm.currentBlock >= TOTAL_BLOCKS)
            {
              // Done!
              stopSm('VMU write complete');
              resetProgressBar();
              const readTime = Date.now() - writeVmuSm.startTime;
              console.log(`VMU read completed in ${readTime}ms`);
              return;
            }
          }

          // Update progress
          writeVmuSm.retries = 0;
          const progress = (writeVmuSm.currentBlock / TOTAL_BLOCKS) * 100;
          vmuProgress.value = progress;
          vmuProgressText.textContent = Math.round(progress) + '%';

          if (writeVmuSm.currentPhase > 3)
          {
            resetSmTimeout();
            setTimeout(() => {
              writeCommit();
            }, writeVmuSm.writeDelayMs);
          } else {
            resetSmTimeout();
            setTimeout(() => {
              writeCurrentPhase();
            }, writeVmuSm.writeDelayMs);
          }
        } else {
          if (!retry()) {
            stopSm('VMU write failed', 'red', 'bold');
            resetProgressBar();
          }
        }
      };

      startSm(writeVmuSm);
    }

    function handleIncomingMsg(addr, cmd, payload) {
      console.info(`RCV ADDR: 0x${addr.toString(16)}, CMD: 0x${cmd.toString(16)}, PAYLOAD: [${Array.from(payload).map(b => '0x' + b.toString(16).padStart(2, '0')).join(', ')}]`);
      // receiveSm
      if (receiveSm) {
        receiveSm.process(addr, cmd, payload);
      }
    }

    function setStatus(str, color = 'black', fontWeight = 'normal') {
      statusDisplay.textContent = str;
      statusDisplay.style.color = color;
      statusDisplay.style.fontWeight = fontWeight;
    }

    function disconnect(reason = '', color = 'black', fontWeight = 'normal') {
      if (port) {
        port.disconnect();
      }
      setStatus(reason, color, fontWeight);
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
          connectionFailed('Operation failed: could not find selected device');
        });
        return true;
      } else {
        return setPortAndConnect(portOrSerialNumber);
      }
    }

    function connect() {
      port.connect().then(() => {
        let receiveBuffer = new Uint8Array(0);

        function printPacket(pkt) {
          console.info(`RCV PKT: [${Array.from(pkt).map(b => '0x' + b.toString(16).padStart(2, '0')).join(', ')}]`);
        }

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
              printPacket(receiveBuffer.slice(0, 8 + size));
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
              printPacket(receiveBuffer.slice(0, 8 + size));
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
              printPacket(receiveBuffer.slice(0, 8 + size));
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
              disconnect('Lost connection with device', 'red', 'bold');
            }
          }
        };
      }, error => {
        setStatus(error, 'red', 'bold');
        return false;
      });

      return true;
    }

    selectButton.addEventListener('click', function (){
      setStatus('');
      // This can only be activated as a response to a user action
      serial.requestPort().then(selectedPort => {
        if (selectedPort) {
          startLoadSm(selectedPort);
        }
      }).catch(error => {
        if (error.name !== 'NotFoundError')
        {
          setStatus(error, 'red', 'bold');
        }
      });
    });

    saveButton.addEventListener('click', function() {
      startSaveSm();
    });

    function getSelectedVmuIndices() {
      let controllerIdx = 0;
      let vmuIdx = 0;

      if (vmu1ARadio.checked) {
        controllerIdx = 0;
        vmuIdx = 0;
      } else if (vmu2ARadio.checked) {
        controllerIdx = 0;
        vmuIdx = 1;
      } else if (vmu1BRadio.checked) {
        controllerIdx = 1;
        vmuIdx = 0;
      } else if (vmu2BRadio.checked) {
        controllerIdx = 1;
        vmuIdx = 1;
      } else if (vmu1CRadio.checked) {
        controllerIdx = 2;
        vmuIdx = 0;
      } else if (vmu2CRadio.checked) {
        controllerIdx = 2;
        vmuIdx = 1;
      } else if (vmu1DRadio.checked) {
        controllerIdx = 3;
        vmuIdx = 0;
      } else if (vmu2DRadio.checked) {
        controllerIdx = 3;
        vmuIdx = 1;
      }

      return { controllerIdx, vmuIdx };
    }

    readVmuButton.addEventListener('click', function() {
      const { controllerIdx, vmuIdx } = getSelectedVmuIndices();
      startReadVmu(controllerIdx, vmuIdx);
    });

    writeVmuButton.addEventListener('click', function() {
      const { controllerIdx, vmuIdx } = getSelectedVmuIndices();

      // Create file input dialog
      const fileInput = document.createElement('input');
      fileInput.type = 'file';
      fileInput.accept = '.bin';
      fileInput.style.display = 'none';

      fileInput.addEventListener('change', function(event) {
        const file = event.target.files[0];
        if (file) {
          const reader = new FileReader();
          reader.onload = function(e) {
            const fileData = new Uint8Array(e.target.result);
            if (fileData.length !== 128 * 1024) {
              setStatus('Invalid file size. VMU files must be exactly 128KB.', 'red', 'bold');
              return;
            }
            // Start write VMU process with the loaded file data
            startWriteVmu(controllerIdx, vmuIdx, fileData);
          };
          reader.readAsArrayBuffer(file);
        }
      });

      document.body.appendChild(fileInput);
      fileInput.click();
      document.body.removeChild(fileInput);
    });
  });
})();
