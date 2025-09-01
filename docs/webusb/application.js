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
    let enableWebusbAnnounceCheck = document.querySelector('#enable-webusb-announce-check');
    let player1 = document.querySelector('#player1-select');
    let player2 = document.querySelector('#player2-select');
    let player3 = document.querySelector('#player3-select');
    let player4 = document.querySelector('#player4-select');
    let port;
    let selectedSerial = null;
    let receiveSm = null; // must define cancel(), start(), process(), and timeout() in the object
    let receiveSmExpectReboot = false; // true if reboot may occur before next event
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
    let vmuMemoryCancelButton = document.querySelector('#vmu-memory-cancel')
    let gpioAText = document.querySelector('#gpio-a');
    let gpioBText = document.querySelector('#gpio-b');
    let gpioCText = document.querySelector('#gpio-c');
    let gpioDText = document.querySelector('#gpio-d');
    let gpioABSpan = document.querySelector('#gpio-a-sdck-b');
    let gpioBBSpan = document.querySelector('#gpio-b-sdck-b');
    let gpioCBSpan = document.querySelector('#gpio-c-sdck-b');
    let gpioDBSpan = document.querySelector('#gpio-d-sdck-b');
    let gpioADirText = document.querySelector('#gpio-dir-a');
    let gpioBDirText = document.querySelector('#gpio-dir-b');
    let gpioCDirText = document.querySelector('#gpio-dir-c');
    let gpioDDirText = document.querySelector('#gpio-dir-d');
    let gpioADirOutHighCheckbox = document.querySelector('#dir-out-high-a');
    let gpioBDirOutHighCheckbox = document.querySelector('#dir-out-high-b');
    let gpioCDirOutHighCheckbox = document.querySelector('#dir-out-high-c');
    let gpioDDirOutHighCheckbox = document.querySelector('#dir-out-high-d');
    let gpioLedText = document.querySelector('#gpio-led');
    let gpioSimpleLedText = document.querySelector('#gpio-simple-led');
    let saveGpioButton = document.querySelector('#save-advanced');
    let resetSettingsButton = document.querySelector('#reset-settings');
    const CMD_OK = 0x0A; // Command success
    const CMD_ATTENTION = 0x0B; // Command success, but attention is needed
    const CMD_FAIL = 0x0F; // Command failed - data was parsed but execution failed
    const CMD_INVALID = 0xFE; // Command was missing data
    const CMD_BAD = 0xFF; // Command had to target

    const CANCEL_REASON_USER = 0;
    const CANCEL_REASON_DISCONNECT = 1;

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
        // console.debug("PKT: " + Array.from(pktData).map(b => b.toString(16).padStart(2, '0')).join(' '));
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
      receiveSmExpectReboot = false;
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

    function cancelSm(reason) {
      if (receiveSm) {
        receiveSm.cancel(reason);
      }
      receiveSm = null;
      stopSmTimeout();
      disconnect();
    }

    function enableAllControls() {
      let tabcontent = document.getElementsByClassName("tabcontent");
      for (let i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.pointerEvents = 'auto';
        tabcontent[i].style.opacity = 1.0;
      }
    }

    function setSettingsFromPayload(payload) {
      if (payload.length < 38) {
        return false;
      }

      // Retrieved settings
      mscCheckbox.checked = (payload[1] !== 0);
      enableWebusbAnnounceCheck.checked = (payload[2] !== 0);

      const controllerADetection = payload[3];
      player1.value = controllerADetection;
      const controllerBDetection = payload[4];
      player2.value = controllerBDetection;
      const controllerCDetection = payload[5];
      player3.value = controllerCDetection;
      const controllerDDetection = payload[6];
      player4.value = controllerDDetection;

      const gpioA = (payload[7] << 24 | payload[8] << 16 | payload[9] << 8 | payload[10]) | 0;
      if (gpioA >= 0) {
        gpioAText.value = gpioA.toString(10);
        gpioABSpan.textContent = gpioA + 1;
      } else {
        gpioAText.value = "";
        gpioABSpan.textContent = "Disabled";
      }

      const gpioB = (payload[11] << 24 | payload[12] << 16 | payload[13] << 8 | payload[14]) | 0;
      if (gpioB >= 0) {
        gpioBText.value = gpioB.toString(10);
        gpioBBSpan.textContent = gpioB + 1;
      } else {
        gpioBText.value = "";
        gpioBBSpan.textContent = "Disabled";
      }

      const gpioC = (payload[15] << 24 | payload[16] << 16 | payload[17] << 8 | payload[18]) | 0;
      if (gpioC >= 0) {
        gpioCText.value = gpioC.toString(10);
        gpioCBSpan.textContent = gpioC + 1;
      } else {
        gpioCText.value = "";
        gpioCBSpan.textContent = "Disabled";
      }

      const gpioD = (payload[19] << 24 | payload[20] << 16 | payload[21] << 8 | payload[22]) | 0;
      if (gpioD >= 0) {
        gpioDText.value = gpioD.toString(10);
        gpioDBSpan.textContent = gpioD + 1;
      } else {
        gpioDText.value = "";
        gpioDBSpan.textContent = "Disabled";
      }

      const gpioDirA = (payload[23] << 24 | payload[24] << 16 | payload[25] << 8 | payload[26]) | 0;
      if (gpioDirA >= 0) {
        gpioADirText.value = gpioDirA.toString(10);
      } else {
        gpioADirText.value = "";
      }

      const gpioDirB = (payload[27] << 24 | payload[28] << 16 | payload[29] << 8 | payload[30]) | 0;
      if (gpioDirB >= 0) {
        gpioBDirText.value = gpioDirB.toString(10);
      } else {
        gpioBDirText.value = "";
      }

      const gpioDirC = (payload[31] << 24 | payload[32] << 16 | payload[33] << 8 | payload[34]) | 0;
      if (gpioDirC >= 0) {
        gpioCDirText.value = gpioDirC.toString(10);
      } else {
        gpioCDirText.value = "";
      }

      const gpioDirD = (payload[35] << 24 | payload[36] << 16 | payload[37] << 8 | payload[38]) | 0;
      if (gpioDirD >= 0) {
        gpioDDirText.value = gpioDirD.toString(10);
      } else {
        gpioDDirText.value = "";
      }

      gpioADirOutHighCheckbox.checked = (payload[39] != 0);
      gpioBDirOutHighCheckbox.checked = (payload[40] != 0);
      gpioCDirOutHighCheckbox.checked = (payload[41] != 0);
      gpioDDirOutHighCheckbox.checked = (payload[42] != 0);

      const gpioLed = (payload[43] << 24 | payload[44] << 16 | payload[45] << 8 | payload[46]) | 0;
      if (gpioLed >= 0) {
        gpioLedText.value = gpioLed;
      } else {
        gpioLedText.value = "";
      }

      const gpioSimpleLed = (payload[47] << 24 | payload[48] << 16 | payload[49] << 8 | payload[50]) | 0;
      if (gpioSimpleLed >= 0) {
        gpioSimpleLedText.value = gpioSimpleLed;
      } else {
        gpioSimpleLedText.value = "";
      }

      return true;
    }

    // Starts the state machine which loads the settings from the device
    function startLoadSm(selectedPort) {
      selectedSerial = selectedPort.serial;
      let deviceVersion = `v${selectedPort.major}.${selectedPort.minor}.${selectedPort.patch}`;
      let selectedDeviceText = `Selected device: ${selectedPort.name}`;
      if (!selectedPort.name.includes(selectedSerial)) {
        selectedDeviceText += `, serial: ${selectedSerial}`;
      }
      if (!selectedPort.name.includes(deviceVersion)) {
        selectedDeviceText += `, ${deviceVersion}`;
      }
      selectedDevice.textContent = selectedDeviceText;
      enableAllControls();
      setStatus("Loading settings...");
      const GET_SETTINGS_ADDR = 123;
      var loadSm = {};
      loadSm.cancel = function(reason) {
        setStatus('Load canceled', 'red', 'bold');
      };
      loadSm.timeout = function() {
        disconnect('Load failed', 'red', 'bold');
      };
      loadSm.start = function() {
        // Load the currently staged settings, not the settings on flash
        send(GET_SETTINGS_ADDR, 'S'.charCodeAt(0), [103]);
      }
      loadSm.process = function(addr, cmd, payload) {
        if (addr == GET_SETTINGS_ADDR) {
          if (cmd == CMD_OK && setSettingsFromPayload(payload)) {
            // Retrieved settings
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
      const SEND_WEBUSB_ANNOUNCE_ADDR = 11;
      const SEND_CONTROLLER_A_ADDR = 12;
      const SEND_CONTROLLER_B_ADDR = 13;
      const SEND_CONTROLLER_C_ADDR = 14;
      const SEND_CONTROLLER_D_ADDR = 15;
      const SEND_VALIDATE_ADDR = 16;
      const SEND_SAVE_AND_RESTART_ADDR = 17;

      var saveSm = {};
      saveSm.disconnectExpected = false;
      saveSm.cancel = function(reason) {
        if (reason == CANCEL_REASON_DISCONNECT && saveSm.disconnectExpected) {
          // Didn't get a chance to get response before reboot occurred
          setStatus('Save successful!');
        } else {
          setStatus('Save canceled', 'red', 'bold');
        }
      };
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
            const webusbAnnounceFlag = enableWebusbAnnounceCheck.checked ? 1 : 0;
            send(SEND_WEBUSB_ANNOUNCE_ADDR, 'S'.charCodeAt(0), [87, webusbAnnounceFlag]);
          } else {
            stopSm('Failed to set WebUSB announcement flag', 'red', 'bold');
          }
        } else if (addr == SEND_WEBUSB_ANNOUNCE_ADDR) {
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
            send(SEND_VALIDATE_ADDR, 'S'.charCodeAt(0), [115]);
          } else {
            stopSm('Failed to set controller D setting', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_VALIDATE_ADDR) {
          if (cmd == CMD_OK || cmd == CMD_ATTENTION) {
            setSettingsFromPayload(payload);
            send(SEND_SAVE_AND_RESTART_ADDR, 'S'.charCodeAt(0), [83]);
            // Reboot may occur before the next event
            saveSm.disconnectExpected = true;
            return;
          } else {
            stopSm('Failed to validate settings', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_SAVE_AND_RESTART_ADDR) {
          if (cmd == CMD_OK) {
            stopSm('Save successful!');
          } else {
            stopSm('Failed to save settings', 'red', 'bold');
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
    function startReadVmuSm(controllerIdx, vmuIndex) {
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

      readVmuSm.cancel = function(reason) {
        setStatus('VMU read canceled', 'red', 'bold');
        resetProgressBar();
      };

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
            a.download = `vmu_save_${String.fromCharCode(65 + readVmuSm.controllerIdx)}${readVmuSm.vmuIndex + 1}.bin`;
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
    function startWriteVmuSm(controllerIdx, vmuIdx, fileData) {
      setStatus("Writing VMU...");
      const WRITE_ADDR = 0x55;
      const NUM_PHASES_PER_BLOCK = 4;
      const BLOCK_SIZE = 512;
      const PHASE_SIZE = BLOCK_SIZE / NUM_PHASES_PER_BLOCK;
      const TOTAL_BLOCKS = 256;

      var writeVmuSm = {};
      writeVmuSm.retries = 0;
      writeVmuSm.errorCount = 0;
      writeVmuSm.controllerIdx = controllerIdx;
      writeVmuSm.vmuIndex = vmuIdx;
      writeVmuSm.hostAddr = getMapleHostAddr(controllerIdx);
      writeVmuSm.destAddr = writeVmuSm.hostAddr | (1 << vmuIdx);
      writeVmuSm.currentBlock = 0;
      writeVmuSm.currentPhase = 0; // [0,4] // commit on 4
      writeVmuSm.writeDelayMs = 10; // delay between writes, grows on each retry

      writeVmuSm.cancel = function(reason) {
        setStatus('VMU write canceled, VMU memory may be left in a corrupted state', 'red', 'bold');
        resetProgressBar();
      };

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
        ++writeVmuSm.errorCount;
        if (++writeVmuSm.retries > 2) {
          console.error("Write failed, retrying");
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
              console.log(`VMU read completed in ${readTime}ms with ${writeVmuSm.errorCount} retried packets`);
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
            // Try to speed things up
            --writeVmuSm.writeDelayMs;
            if (writeVmuSm.writeDelayMs < 10) {
              writeVmuSm.writeDelayMs = 10;
            }

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

    function int32ToBytes(val) {
      const buffer = new ArrayBuffer(4);
      const view = new DataView(buffer);
      view.setInt32(0, Number(val), false); // false = big endian
      return [view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3)];
    }

    function startWriteGpioSm() {
      setStatus("Writing GPIO Settings...");

      const SEND_CONTROLLER_A_ADDR = 20;
      const SEND_CONTROLLER_B_ADDR = 21;
      const SEND_CONTROLLER_C_ADDR = 22;
      const SEND_CONTROLLER_D_ADDR = 23;
      const SEND_LED_ADDR = 24;
      const SEND_SIMPLE_LED_ADDR = 25;
      const SEND_VALIDATE_SETTINGS = 26;
      const SEND_SAVE_AND_RESTART_ADDR = 27;

      const CMD_SETTINGS = 'S'.charCodeAt(0);

      var writeGpioSm = {};
      writeGpioSm.disconnectExpected = false;

      writeGpioSm.cancel = function(reason) {
        if (reason == CANCEL_REASON_DISCONNECT && writeGpioSm.disconnectExpected) {
          // Didn't get a chance to get response before reboot occurred
          setStatus('GPIO settings saved');
        } else {
          setStatus('Write GPIO canceled', 'red', 'bold');
        }
      };

      writeGpioSm.timeout = function() {
        setStatus('Write GPIO failed', 'red', 'bold');
      };

      writeGpioSm.start = function() {
        // I[0-3][GPIO A (4 byte)][GPIO DIR (4 byte)][DIR Output HIGH (1 byte)]
        const gpioA = (gpioAText.value === "") ? -1 : Number(gpioAText.value);
        const gpioDir = (gpioADirText.value === "") ? -1 : Number(gpioADirText.value);
        const gpioDirOutHigh = gpioADirOutHighCheckbox.checked ? 1 : 0
        send(SEND_CONTROLLER_A_ADDR, CMD_SETTINGS, ['I'.charCodeAt(0), 0, ...int32ToBytes(gpioA), ...int32ToBytes(gpioDir), gpioDirOutHigh])
      };

      writeGpioSm.process = function(addr, cmd, payload) {
        if (addr == SEND_CONTROLLER_A_ADDR) {
          if (cmd == CMD_OK) {
            const gpioA = (gpioBText.value === "") ? -1 : Number(gpioBText.value);
            const gpioDir = (gpioBDirText.value === "") ? -1 : Number(gpioBDirText.value);
            const gpioDirOutHigh = gpioBDirOutHighCheckbox.checked ? 1 : 0
            send(SEND_CONTROLLER_B_ADDR, CMD_SETTINGS, ['I'.charCodeAt(0), 1, ...int32ToBytes(gpioA), ...int32ToBytes(gpioDir), gpioDirOutHigh])
          } else {
            stopSm('Failed to set controller A GPIO - ensure they are valid', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_CONTROLLER_B_ADDR) {
          if (cmd == CMD_OK) {
            const gpioA = (gpioCText.value === "") ? -1 : Number(gpioCText.value);
            const gpioDir = (gpioCDirText.value === "") ? -1 : Number(gpioCDirText.value);
            const gpioDirOutHigh = gpioCDirOutHighCheckbox.checked ? 1 : 0
            send(SEND_CONTROLLER_C_ADDR, CMD_SETTINGS, ['I'.charCodeAt(0), 2, ...int32ToBytes(gpioA), ...int32ToBytes(gpioDir), gpioDirOutHigh])
          } else {
            stopSm('Failed to set controller B GPIO - ensure they are valid', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_CONTROLLER_C_ADDR) {
          if (cmd == CMD_OK) {
            const gpioA = (gpioDText.value === "") ? -1 : Number(gpioDText.value);
            const gpioDir = (gpioDDirText.value === "") ? -1 : Number(gpioDDirText.value);
            const gpioDirOutHigh = gpioDDirOutHighCheckbox.checked ? 1 : 0
            send(SEND_CONTROLLER_D_ADDR, CMD_SETTINGS, ['I'.charCodeAt(0), 3, ...int32ToBytes(gpioA), ...int32ToBytes(gpioDir), gpioDirOutHigh])
          } else {
            stopSm('Failed to set controller C GPIO - ensure they are valid', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_CONTROLLER_D_ADDR) {
          if (cmd == CMD_OK) {
            // LED setting L[LED Pin (4 byte)]
            const gpioLed = (gpioLedText.value === "") ? -1 : Number(gpioLedText.value);
            send(SEND_LED_ADDR, CMD_SETTINGS, ['L'.charCodeAt(0), ...int32ToBytes(gpioLed)]);
          } else {
            stopSm('Failed to set controller D GPIO - ensure they are valid', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_LED_ADDR) {
          if (cmd == CMD_OK) {
            // Simple LED setting l[LED Pin (4 byte)]
            const gpioLed = (gpioSimpleLedText.value === "") ? -1 : Number(gpioSimpleLedText.value);
            send(SEND_SIMPLE_LED_ADDR, CMD_SETTINGS, ['l'.charCodeAt(0), ...int32ToBytes(gpioLed)]);
          } else {
            stopSm('Failed to set LED GPIO - ensure it is valid', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_SIMPLE_LED_ADDR) {
          if (cmd == CMD_OK) {
            send(SEND_VALIDATE_SETTINGS, CMD_SETTINGS, ['s'.charCodeAt(0)]);
          } else {
            stopSm('Failed to set simple LED GPIO - ensure it is valid', 'red', 'bold');
            return;
          }
        } else if (addr == SEND_VALIDATE_SETTINGS) {
          if (cmd == CMD_ATTENTION) {
            setSettingsFromPayload(payload)
            stopSm('GPIO settings need adjustment to prevent overlap - please review changes', 'red', 'bold');
          } else if (cmd == CMD_OK) {
            send(SEND_SAVE_AND_RESTART_ADDR, CMD_SETTINGS, ['S'.charCodeAt(0)]);
            // Reboot may occur before the next event
            writeGpioSm.disconnectExpected = true;
          } else {
            stopSm('Failed to validate settings', 'red', 'bold');
          }
        } else if (addr == SEND_SAVE_AND_RESTART_ADDR) {
          if (cmd == CMD_OK) {
            if (setSettingsFromPayload(payload))
            {
              stopSm('GPIO settings saved with adjustments due to overlapping GPIO - please review changes', 'orange', 'bold')
            }
            else
            {
              stopSm('GPIO settings saved')
            }
          } else {
            stopSm('Failed to save settings', 'red', 'bold');
          }
          return;
        }
        resetSmTimeout();
      };

      startSm(writeGpioSm);
    }

    function startResetSettingsSm() {
      setStatus("Resetting Settings...");

      const SEND_GET_DEFAULTS_ADDR = 30;
      const SEND_RESET_AND_RESTART_ADDR = 31;

      const CMD_SETTINGS = 'S'.charCodeAt(0);

      var resetSettingsSm = {};
      resetSettingsSm.disconnectExpected = false;

      resetSettingsSm.cancel = function(reason) {
        if (reason == CANCEL_REASON_DISCONNECT && resetSettingsSm.disconnectExpected) {
          // Didn't get a chance to get response before reboot occurred
          setStatus('Settings reset');
        } else {
          setStatus('Settings reset canceled', 'red', 'bold');
        }
      };

      resetSettingsSm.timeout = function() {
        setStatus('Settings reset failed', 'red', 'bold');
      };

      resetSettingsSm.start = function() {
        send(SEND_GET_DEFAULTS_ADDR, CMD_SETTINGS, ['x'.charCodeAt(0)]);
      };

      resetSettingsSm.process = function(addr, cmd, payload) {
        if (addr == SEND_GET_DEFAULTS_ADDR) {
          if (cmd == CMD_OK) {
            setSettingsFromPayload(payload);
            send(SEND_RESET_AND_RESTART_ADDR, CMD_SETTINGS, ['X'.charCodeAt(0)]);
            // Reboot may occur before the next event
            resetSettingsSm.disconnectExpected = true;
          } else {
            stopSm('Failed to retrieve defaults', 'red', 'bold');
          }
        } else if (addr == SEND_RESET_AND_RESTART_ADDR) {
          if (cmd == CMD_OK) {
            setSettingsFromPayload(payload);
            stopSm('Settings reset');
          } else {
            stopSm('Failed to reset settings', 'red', 'bold');
          }
          return;
        }
        resetSmTimeout();
      };

      startSm(resetSettingsSm);
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

    function disconnect(reason = null, color = 'black', fontWeight = 'normal') {
      if (port) {
        port.disconnect();
      }
      if (reason !== null) {
        setStatus(reason, color, fontWeight);
      }
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

            // console.debug("PKT: " + Array.from(payload).map(b => b.toString(16).padStart(2, '0')).join(' '));

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
          if (receiveSm) {
            cancelSm(CANCEL_REASON_DISCONNECT);
          } else if (port) {
            console.error(error);
            disconnect('Lost connection with device', 'red', 'bold');
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
      startReadVmuSm(controllerIdx, vmuIdx);
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
            startWriteVmuSm(controllerIdx, vmuIdx, fileData);
          };
          reader.readAsArrayBuffer(file);
        }
      });

      document.body.appendChild(fileInput);
      fileInput.click();
      document.body.removeChild(fileInput);
    });

    vmuMemoryCancelButton.addEventListener('click', function () {
      cancelSm(CANCEL_REASON_USER);
    });

    saveGpioButton.addEventListener('click', function() {
      startWriteGpioSm();
    });

    resetSettingsButton.addEventListener('click', function() {
      startResetSettingsSm();
    });

  });
})();
