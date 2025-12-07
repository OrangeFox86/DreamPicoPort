(function() {
  'use strict';

  // Show a banner when the current browser is not compatible with WebUSB functions
  if (!('usb' in navigator)) {
    window.addEventListener('DOMContentLoaded', () => {
      const warning = document.createElement('div');
      warning.style.background = '#ffdddd';
      warning.style.color = '#a00';
      warning.style.padding = '1em';
      warning.style.margin = '1em 0';
      warning.style.border = '1px solid #a00';
      warning.style.fontWeight = 'bold';
      warning.textContent = 'This page is not supported by your browser. Use a Chromium-based browser such as Chrome, Edge, Opera, or Brave.';
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
    let receiveSm = null; // must define name, start(), and process() in the object; optionally define done()
    let receiveSmTimeout = 150;
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

    // Tests tab
    let testsProfileButton = document.querySelector('#tests-profile');
    let testsBasicButton = document.querySelector('#tests-basic');
    let testsStressButton = document.querySelector('#tests-stress');
    let testsStatusDisplay = document.querySelector('#tests-status');

    const CMD_OK = 0x0A; // Command success
    const CMD_ATTENTION = 0x0B; // Command success, but attention is needed
    const CMD_FAIL = 0x0F; // Command failed - data was parsed but execution failed
    const CMD_INVALID = 0xFE; // Command was missing data
    const CMD_BAD = 0xFF; // Command had to target

    const SM_DONE_REASON_NONE = 0;
    const SM_DONE_CONNECT_FAILED = 1;
    const SM_DONE_REASON_CANCELED_USER = 2;
    const SM_DONE_REASON_DISCONNECT = 3;
    const SM_DONE_REASON_TIMEOUT = 4;

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

    // Send a formatted command to the DreamPicoPort
    // addr: host-defined address, up to 64-bits, which is returned within the response
    // cmd: the command byte which selects the command parser to route the payload to
    // payload: the payload for the command
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

    // State machine function: Called when state machine operation is complete
    function smDone(reason) {
      if (receiveSm) {
        if (typeof receiveSm.done === 'function') {
          receiveSm.done(reason);
        } else {
          receiveSm.defaultDone(reason);
        }
      }
      receiveSm = null;
      receiveSmTimeout = 150;
    }

    // State machine function: Called when the state machine experienced a timeout
    function smTimeout() {
      receiveSmTimeoutId = -1;
      if (receiveSm) {
        smDone(SM_DONE_REASON_TIMEOUT);
        if (receiveSmTimeoutId == -1) {
          disconnect();
        }
      }
    }

    // State machine function: Cancels the state machine timeout
    function stopSmTimeout() {
      if (receiveSmTimeoutId >= 0) {
        clearTimeout(receiveSmTimeoutId);
        receiveSmTimeoutId = -1;
      }
    }

    // State machine function: Stops then restarts the state machine timeout to 150 ms
    function resetSmTimeout(timeout = null) {
      if (timeout == null) {
        timeout = receiveSmTimeout;
      } else {
        receiveSmTimeout = timeout;
      }
      stopSmTimeout();
      if (receiveSm) {
        receiveSmTimeoutId = setTimeout(smTimeout, timeout);
      }
    }

    // State machine function: Called when connection phase has been successfully completed
    function connectionComplete() {
      if (receiveSm) {
        receiveSm.start();
        resetSmTimeout();
      }
    }

    // State machine function: Called when connection could not be established
    function connectionFailed(reason = 'Operation failed: failed to connect to device', disableControl = false) {
      stopSmTimeout();
      if (receiveSm) {
        smDone(SM_DONE_CONNECT_FAILED);
        disconnect(reason, 'red', 'bold');
      }
      if (disableControl) {
        disableAllControls();
      }
    }

    // State machine function: Starts a given state machine
    // sm: The state machine object to start
    // selectedPort: A specific DreamPicoPort device to connect to
    function startSm(sm, selectedPort = null) {
      if (receiveSm) {
        stopSm('Operation canceled');
      } else {
        stopSmTimeout();
      }
      receiveSm = sm;
      receiveSm.defaultDone = function(reason) {
        if (reason == SM_DONE_REASON_CANCELED_USER || reason == SM_DONE_REASON_DISCONNECT) {
          setStatus(`${receiveSm.name} canceled`, 'red', 'bold');
        } else if (reason == SM_DONE_REASON_TIMEOUT) {
          disconnect(`${receiveSm.name} failed`, 'red', 'bold');
        }
      };
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
        smDone(SM_DONE_CONNECT_FAILED);
        disconnect('Failed to connect to device', 'red', 'bold');
      }
    }

    // State machine function: Stops the currently running state machine
    // reason: The reason string to display on disconnection
    // color: The color to set the reason string
    // fontWeight: The font weight to set the reason string
    function stopSm(reason, color = 'black', fontWeight = 'normal', doneReason = SM_DONE_REASON_NONE) {
      smDone(doneReason);
      disconnect(reason, color, fontWeight);
      stopSmTimeout();
    }

    // State machine function: Cancels a state machine due to external event
    // reason: One of {SM_DONE_REASON_CANCELED_USER, SM_DONE_REASON_DISCONNECT}
    function cancelSm(reason) {
      smDone(reason);
      stopSmTimeout();
      disconnect();
    }

    // Enable all controls within the form
    function enableAllControls() {
      let tabcontent = document.getElementsByClassName("tabcontent");
      for (let i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.pointerEvents = 'auto';
        tabcontent[i].style.opacity = 1.0;
      }
    }

    // Disable all controls in within the form besides the Select Device button
    function disableAllControls() {
      let tabcontent = document.getElementsByClassName("tabcontent");
      for (let i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.pointerEvents = 'none';
        tabcontent[i].style.opacity = 0.7;
      }
    }

    // Set the settings on the form from a payload which is known to contain settings
    // payload: bytestream retrieved from the device
    // Returns true iff the payload contained enough data to parse
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
      startSm(new LoadStateMachine(), selectedPort);
    }

    // Starts the state machine which saves the general settings
    function startSaveSm() {
      setStatus("Saving...");
      startSm(new SaveStateMachine());
    }

    // Resets the progress bar on the VMU Memory tab
    function resetProgressBar() {
      vmuProgress.value = 0;
      vmuProgressContainer.style.display = 'none';
      vmuProgressText.textContent = '0%';
    }

    // Converts a controller index [0,3] to the Maple Bus host address
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

    // Handles incoming message from the DreamPicoPort
    // addr: The return address which matches what was sent in send()
    // cmd: The result command
    // payload: The payload of the result
    function handleIncomingMsg(addr, cmd, payload) {
      console.info(`RCV ADDR: 0x${addr.toString(16)}, CMD: 0x${cmd.toString(16)}, PAYLOAD: [${Array.from(payload).map(b => '0x' + b.toString(16).padStart(2, '0')).join(', ')}]`);
      // receiveSm
      if (receiveSm) {
        receiveSm.process(addr, cmd, payload);
        // If the state machine is not stopped by above, reset the timeout
        if (receiveSm) {
          resetSmTimeout();
        }
      }
    }

    // Sets the status display to a short string
    // str: The string to set
    // color: The color to set the status to
    // fontWeight: The font weight to set the status to
    function setStatus(str, color = 'black', fontWeight = 'normal') {
      statusDisplay.textContent = str;
      statusDisplay.style.color = color;
      statusDisplay.style.fontWeight = fontWeight;
    }

    // Disconnects the currently connected port and optionally sets the status
    // reason: The reason for disconnection (optional)
    // color: The color to set the status to
    // fontWeight: The font weight to set the status to
    function disconnect(reason = null, color = 'black', fontWeight = 'normal') {
      if (port) {
        port.disconnect();
      }
      if (reason !== null) {
        setStatus(reason, color, fontWeight);
      }
      port = null;
    }

    // Sets the selected port and begins the connection process
    function setPortAndConnect(p) {
      port = p;
      port.ready = function () {
        connectionComplete();
      }
      return connect();
    }

    // Start the connection process with either a port object or serial number of desired port
    // portOrSerialNumber: Either the port object or serial number string
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

          let failureMsg = 'Operation failed: could not find selected device';
          let disableControl = false;
          if (window.navigator.userAgent.indexOf("Android") !== -1) {
            // Android revokes permission when device detaches, so the user needs to provide permission again
            failureMsg = 'Operation failed: please click Select Device and reselect the device to grant access again';
            disableControl = true;
          }
          connectionFailed(failureMsg, disableControl);
        });
        return true;
      } else {
        return setPortAndConnect(portOrSerialNumber);
      }
    }

    // Connect to the selected port and setup callbacks
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
            cancelSm(SM_DONE_REASON_DISCONNECT);
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

    // Select Button - click handler
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

    // Returns the controller index and VMU index for the selected device under the VMU Memory tab
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

    // Read VMU Button - click handler
    readVmuButton.addEventListener('click', function() {
      const { controllerIdx, vmuIdx } = getSelectedVmuIndices();
      startReadVmuSm(controllerIdx, vmuIdx);
    });

    // Write VMU Button - click handler
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
            if (!confirm('This will overwrite all memory on the selected VMU. Would you like to proceed?')) {
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

    // Converts an int32 value to a byte stream
    function int32ToBytes(val) {
      const buffer = new ArrayBuffer(4);
      const view = new DataView(buffer);
      view.setInt32(0, Number(val), false); // false = big endian
      return [view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3)];
    }

    // *************************************************************************
    // Start State Machine Definitions
    // *************************************************************************

    // Base class of state machine which may be set for receiveSm
    class StateMachine {
      constructor(name) {
        this.name = name;
      }

      start() {
        console.error(`State machine ${this.name} has no start() defined`);
      }

      process(addr, cmd, payload) {
        console.error(`State machine ${this.name} has no process() defined`);
      }

      done(reason) {
        if (reason == SM_DONE_REASON_CANCELED_USER || reason == SM_DONE_REASON_DISCONNECT) {
          setStatus(`${this.name} canceled`, 'red', 'bold');
        } else if (reason == SM_DONE_REASON_TIMEOUT) {
          setStatus(`${this.name} failed`, 'red', 'bold');
        }
      }

      stop(reason = null, color = 'black', fontWeight = 'normal', doneReason = SM_DONE_REASON_NONE) {
        if (receiveSm === this) {
          stopSm(reason, color, fontWeight, doneReason);
        }
      }
    }

    // State machine which loads current settings onto the form
    class LoadStateMachine extends StateMachine {
      static GET_SETTINGS_ADDR = 123;

      constructor() {
        super("Load");
      }

      start() {
        // Load the currently staged settings, not the settings on flash
        send(LoadStateMachine.GET_SETTINGS_ADDR, 'S'.charCodeAt(0), [103]);
      }

      process(addr, cmd, payload) {
        if (addr == LoadStateMachine.GET_SETTINGS_ADDR) {
          if (cmd == CMD_OK && setSettingsFromPayload(payload)) {
            // Retrieved settings
            this.stop('Settings loaded');
            return;
          }
        }
        this.stop('Failed to load settings', 'red', 'bold')
      }
    }

    // State machine which saves the general settings
    class SaveStateMachine extends StateMachine {
      static SEND_MSC_ADDR = 10;
      static SEND_WEBUSB_ANNOUNCE_ADDR = 11;
      static SEND_CONTROLLER_A_ADDR = 12;
      static SEND_CONTROLLER_B_ADDR = 13;
      static SEND_CONTROLLER_C_ADDR = 14;
      static SEND_CONTROLLER_D_ADDR = 15;
      static SEND_VALIDATE_ADDR = 16;
      static SEND_SAVE_AND_RESTART_ADDR = 17;

      constructor() {
        super("Save");
        this.disconnectExpected = false;
      }

      done(reason) {
        if (reason == SM_DONE_REASON_DISCONNECT && this.disconnectExpected) {
          // Didn't get a chance to get response before reboot occurred
          setStatus('Save successful!');
        } else {
          super.done(reason);
        }
      }

      start() {
        const mscValue = mscCheckbox.checked ? 1 : 0;
        send(SaveStateMachine.SEND_MSC_ADDR, 'S'.charCodeAt(0), [77, mscValue]);
      }

      process(addr, cmd, payload) {
        if (addr == SaveStateMachine.SEND_MSC_ADDR) {
          if (cmd == CMD_OK) {
            const webusbAnnounceFlag = enableWebusbAnnounceCheck.checked ? 1 : 0;
            send(SaveStateMachine.SEND_WEBUSB_ANNOUNCE_ADDR, 'S'.charCodeAt(0), [87, webusbAnnounceFlag]);
          } else {
            this.stop('Failed to set WebUSB announcement flag', 'red', 'bold');
          }
        } else if (addr == SaveStateMachine.SEND_WEBUSB_ANNOUNCE_ADDR) {
          if (cmd == CMD_OK) {
            send(SaveStateMachine.SEND_CONTROLLER_A_ADDR, 'S'.charCodeAt(0), [80, 0, player1.value]);
          } else {
            this.stop('Failed to set Mass Storage Class setting', 'red', 'bold');
          }
        } else if (addr == SaveStateMachine.SEND_CONTROLLER_A_ADDR) {
          if (cmd == CMD_OK) {
            send(SaveStateMachine.SEND_CONTROLLER_B_ADDR, 'S'.charCodeAt(0), [80, 1, player2.value]);
          } else {
            this.stop('Failed to set controller A setting', 'red', 'bold');
          }
        } else if (addr == SaveStateMachine.SEND_CONTROLLER_B_ADDR) {
          if (cmd == CMD_OK) {
            send(SaveStateMachine.SEND_CONTROLLER_C_ADDR, 'S'.charCodeAt(0), [80, 2, player3.value]);
          } else {
            this.stop('Failed to set controller B setting', 'red', 'bold');
          }
        } else if (addr == SaveStateMachine.SEND_CONTROLLER_C_ADDR) {
          if (cmd == CMD_OK) {
            send(SaveStateMachine.SEND_CONTROLLER_D_ADDR, 'S'.charCodeAt(0), [80, 3, player4.value]);
          } else {
            this.stop('Failed to set controller C setting', 'red', 'bold');
          }
        } else if (addr == SaveStateMachine.SEND_CONTROLLER_D_ADDR) {
          if (cmd == CMD_OK) {
            send(SaveStateMachine.SEND_VALIDATE_ADDR, 'S'.charCodeAt(0), [115]);
          } else {
            this.stop('Failed to set controller D setting', 'red', 'bold');
          }
        } else if (addr == SaveStateMachine.SEND_VALIDATE_ADDR) {
          if (cmd == CMD_OK || cmd == CMD_ATTENTION) {
            setSettingsFromPayload(payload);
            send(SaveStateMachine.SEND_SAVE_AND_RESTART_ADDR, 'S'.charCodeAt(0), [83]);
            // Reboot may occur before the next event
            this.disconnectExpected = true;
          } else {
            this.stop('Failed to validate settings', 'red', 'bold');
          }
        } else if (addr == SaveStateMachine.SEND_SAVE_AND_RESTART_ADDR) {
          if (cmd == CMD_OK) {
            this.stop('Save successful!');
          } else {
            this.stop('Failed to save settings', 'red', 'bold');
          }
        }
      }
    }

    // State machine which reads a VMU
    class ReadVmuStateMachine extends StateMachine {
      static READ_ADDR = 170;

      constructor(controllerIdx, vmuIndex) {
        super("VMU read");
        this.retries = 0;
        this.controllerIdx = controllerIdx;
        this.vmuIndex = vmuIndex;
        this.vmuData = new Uint8Array(128 * 1024); // 128KB VMU storage
        this.dataOffset = 0;
        this.hostAddr = getMapleHostAddr(controllerIdx);
        this.destAddr = this.hostAddr | (1 << vmuIndex);
        this.currentBlock = 0;
      }

      sendCurrentBlock() {
        send(ReadVmuStateMachine.READ_ADDR, '0'.charCodeAt(0), [0x0B, this.destAddr, this.hostAddr, 2, 0, 0, 0, 2, 0, 0, 0, this.currentBlock]);
      }

      done(reason) {
        if (reason == SM_DONE_REASON_TIMEOUT) {
          if (this.retries++ < 2) {
            // Re-read current block
            resetSmTimeout();
            this.sendCurrentBlock();
          } else {
            setStatus('VMU read failed - timeout', 'red', 'bold');
            resetProgressBar();
          }
        } else {
          this.defaultDone(reason);
          resetProgressBar();
        }
      }

      start() {
        vmuProgressContainer.style.display = 'block';
        vmuProgressContainer.style.visibility = 'visible';
        vmuProgress.value = 0;
        vmuProgressText.textContent = '0%';
        this.startTime = Date.now();
        this.sendCurrentBlock();
      }

      process(addr, cmd, payload) {
        let fnType = Number(addr & BigInt(0xff));
        let receivedBlock = -1;
        if (payload.length >= 12)
        {
          receivedBlock = payload[11];
        }
        if (fnType == ReadVmuStateMachine.READ_ADDR && receivedBlock == this.currentBlock && cmd == CMD_OK && payload.length == 524) {
          // Copy received data to VMU buffer
          // TODO: verify maple packet header
          this.vmuData.set(payload.slice(12, payload.length), this.dataOffset);
          this.dataOffset += payload.length - 12;
          this.currentBlock++;
          this.retries = 0;

          // Update progress
          const progress = (this.currentBlock / 256) * 100;
          vmuProgress.value = progress;
          vmuProgressText.textContent = Math.round(progress) + '%';

          if (this.currentBlock < 256) {
            // Read next block
            this.sendCurrentBlock();
          } else {
            // Reading complete, save file
            const blob = new Blob([this.vmuData], { type: 'application/octet-stream' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `vmu_save_${String.fromCharCode(65 + this.controllerIdx)}${this.vmuIndex + 1}.bin`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);

            this.stop('VMU read complete');
            resetProgressBar();
            const readTime = Date.now() - this.startTime;
            console.log(`VMU read completed in ${readTime}ms`);
          }
        } else if (this.retries++ < 2) {
            // Re-read current block
            this.sendCurrentBlock();
        } else {
          this.stop('VMU read failed', 'red', 'bold');
          resetProgressBar();
        }
      }
    }

    // State machine which writes a VMU
    class WriteVmuStateMachine extends StateMachine {
      static WRITE_ADDR = 171;
      static NUM_PHASES_PER_BLOCK = 4;
      static BLOCK_SIZE = 512;
      static PHASE_SIZE = WriteVmuStateMachine.BLOCK_SIZE / WriteVmuStateMachine.NUM_PHASES_PER_BLOCK;
      static TOTAL_BLOCKS = 256;

      constructor(controllerIdx, vmuIdx, fileData) {
        super("Write VMU");
        this.retries = 0;
        this.errorCount = 0;
        this.controllerIdx = controllerIdx;
        this.vmuIndex = vmuIdx;
        this.hostAddr = getMapleHostAddr(controllerIdx);
        this.destAddr = this.hostAddr | (1 << vmuIdx);
        this.currentBlock = 0;
        this.currentPhase = 0; // [0,4] // commit on 4
        this.writeDelayMs = 10; // delay between writes, grows on each retry
        this.fileData = fileData;
      }

      done(reason) {
        if (reason == SM_DONE_REASON_TIMEOUT) {
          if (!this.retry()) {
            setStatus('VMU write failed - timeout', 'red', 'bold');
            resetProgressBar();
          }
        } else if (reason == SM_DONE_CONNECT_FAILED || reason == SM_DONE_REASON_CANCELED_USER) {
          setStatus('VMU write canceled, VMU memory may be left in a corrupted state', 'red', 'bold');
          resetProgressBar();
        } else {
          super.done(reason);
          resetProgressBar();
        }
      };

      writeCurrentPhase() {
        const locationWord = [0, this.currentPhase, 0, this.currentBlock];
        const dataOffset = (this.currentBlock * WriteVmuStateMachine.BLOCK_SIZE) + (this.currentPhase * WriteVmuStateMachine.PHASE_SIZE);
        const dataChunk = this.fileData.slice(dataOffset, dataOffset + WriteVmuStateMachine.PHASE_SIZE);
        send(WriteVmuStateMachine.WRITE_ADDR | (this.currentBlock << 8) | (this.currentPhase << 16), '0'.charCodeAt(0), [0x0C, this.destAddr, this.hostAddr, 34, 0, 0, 0, 2, ...locationWord, ...dataChunk]);
      }

      writeCommit() {
        const locationWord = [0, this.currentPhase, 0, this.currentBlock];
        send(WriteVmuStateMachine.WRITE_ADDR | (this.currentBlock << 8) | (this.currentPhase << 16), '0'.charCodeAt(0), [0x0D, this.destAddr, this.hostAddr, 2, 0, 0, 0, 2, ...locationWord]);
      }

      retry() {
        ++this.errorCount;
        if (++this.retries > 2) {
          return false;
        }

        console.error("Write failed, retrying");

        resetSmTimeout();
        this.currentPhase = 0;
        this.writeDelayMs += 5;
        if (this.writeDelayMs > 30) {
          this.writeDelayMs = 30;
        }

        setTimeout(() => {
          this.writeCurrentPhase();
        }, this.writeDelayMs);

        return true;
      }

      start() {
        vmuProgressContainer.style.display = 'block';
        vmuProgressContainer.style.visibility = 'visible';
        vmuProgress.value = 0;
        vmuProgressText.textContent = '0%';
        this.startTime = Date.now();
        this.writeCurrentPhase();
      };

      process(addr, cmd, payload) {
        let fnType = Number(addr & BigInt(0xff));
        let forBlock = Number((addr >> BigInt(8)) & BigInt(0xff));
        let forPhase = Number((addr >> BigInt(16)) & BigInt(0xff));

        if (
          fnType == WriteVmuStateMachine.WRITE_ADDR &&
          cmd == CMD_OK &&
          forBlock == this.currentBlock &&
          forPhase == this.currentPhase &&
          payload.length >= 1 &&
          payload[0] == 0x07 // Acknowledge from Maple Bus
        ) {
          if (++this.currentPhase > 4)
          {
            this.currentPhase = 0;
            if (++this.currentBlock >= WriteVmuStateMachine.TOTAL_BLOCKS)
            {
              // Done!
              this.stop('VMU write complete');
              resetProgressBar();
              const readTime = Date.now() - this.startTime;
              console.log(`VMU read completed in ${readTime}ms with ${this.errorCount} retried packets`);
              return;
            }
          }

          // Update progress
          this.retries = 0;
          const progress = (this.currentBlock / WriteVmuStateMachine.TOTAL_BLOCKS) * 100;
          vmuProgress.value = progress;
          vmuProgressText.textContent = Math.round(progress) + '%';

          if (this.currentPhase > 3)
          {
            // Try to speed things up
            --this.writeDelayMs;
            if (this.writeDelayMs < 10) {
              this.writeDelayMs = 10;
            }

            setTimeout(() => {
              this.writeCommit();
            }, this.writeDelayMs);
          } else {
            setTimeout(() => {
              this.writeCurrentPhase();
            }, this.writeDelayMs);
          }
        } else {
          if (!this.retry()) {
            this.stop('VMU write failed', 'red', 'bold');
            resetProgressBar();
          }
        }
      };
    }

    // State machine which profiles all connected peripherals
    class ProfilingStateMachine extends StateMachine {
      // 1. GetConnectedGamepads(), and then for each controller:
      //    A. GetDcSummary()
      //    B. Maple command: extended device info request
      // 2. Display all data

      static GET_CONNECTED_GAMEPADS_ADDR = 40;
      static GET_DC_SUMMARY_BASE_ADDR = 41;
      static GET_DEVICE_INFO_BASE_ADDR = 45;

      constructor(name = null) {
        if (name == null) {
          super("Get device information");
          this.stopOnComplete = true;
        } else {
          // Extension class
          super(name);
          this.stopOnComplete = false;
        }
        this.currentIdx = -1;
        this.connectionStates = [];
        this.currentPeripheralSummary = [];
        this.allData = {};
        this.currentPeripheralIdx = -1;
        this.sentBasicInfoReq = false;
      }

      nextIdx() {
        while (this.connectionStates.length > ++this.currentIdx) {
          let idx = this.currentIdx;
          let connectionState = this.connectionStates[idx];
          if (connectionState == 2) {
            // Connected
            return idx;
          }
        }
        return -1;
      }

      loadDcSummary(payload) {
        this.currentPeripheralSummary = [];
        this.currentPeripheralIdx = -1;
        let pidx = 0;
        while (pidx < payload.length) {
          let currentPeriph = [];
          let arr = [];
          let aidx = 0;
          // Pipe means that 4-byte function data should follow (should be in pairs)
          while (pidx < payload.length && payload[pidx] === 124) { // 124 is '|'
            pidx++; // skip past pipe
            if (pidx + 4 <= payload.length) {
              // Convert 4 bytes to uint32 (big-endian)
              const val = (payload[pidx] << 24) | (payload[pidx + 1] << 16) | (payload[pidx + 2] << 8) | payload[pidx + 3];
              arr[aidx++] = val;
              if (aidx >= 2) {
                currentPeriph.push([...arr]);
                arr = [];
                aidx = 0;
              }
              pidx += 4;
            } else {
              // Not enough data - skip to the end
              pidx = payload.length;
            }
          }
          // Add the accumulated peripheral data
          this.currentPeripheralSummary.push([...currentPeriph]);
          if (pidx < payload.length) {
            // This is assumed to be a semicolon which terminates the current peripheral
            pidx++;
          }
        }
      }

      getDestAddr(hostAddr) {
        let destAddr = hostAddr;
        if (this.currentPeripheralIdx == 0) {
          destAddr |= 0x20;
        } else {
          destAddr |= (1 << (this.currentPeripheralIdx - 1));
        }
        return destAddr;
      }

      complete() {
        if (this.allData.size) {
          testsStatusDisplay.innerHTML = "No peripherals detected";
        } else {
          let innerHTML = "<table style='border: 1px solid black; border-collapse: collapse;'>";
          for (const busIdxStr of Object.keys(this.allData)) {
            let busIdx = Number(busIdxStr);
            let busPeripherals = this.allData[busIdx];
            innerHTML += "<tr><td style='border: 1px solid black;text-align: center;'><br><b>Port " + String.fromCharCode("A".charCodeAt(0) + busIdx);
            innerHTML += "</b></td><td style='border: 1px solid black;'><table style='border-collapse: collapse;'>";
            for (const peripheralIdx of Object.keys(busPeripherals)) {
              let peripheralData = busPeripherals[peripheralIdx];
              let peripheralDesc = "";
              if (peripheralIdx == 0) {
                peripheralDesc = "Device";
              } else {
                peripheralDesc = `Slot ${peripheralIdx}`;
              }
              innerHTML += "<tr><td style='border: 1px dotted black;text-align: center;'><b>" + peripheralDesc;
              innerHTML += "</b></td><td style='border: 1px dotted black;padding: 10px;'><table style='border-collapse: collapse;'>";

              for (const key of Object.keys(peripheralData)) {
                let value = peripheralData[key];
                innerHTML += `<tr><td style="text-align: right;width: 1%;white-space: nowrap;"><b>${key}:&nbsp;</b></td>`;
                if (key == "functions") {
                  let fns = "";
                  for (let i = 0; i < value.length; i++) {
                    if (fns.length > 0) {
                      fns += "; ";
                    }
                    let d = value[i];
                    let name = d["name"];
                    let codeStr = "0x" + d["code"].toString(16).padStart(8, '0');
                    fns += `${name}(${codeStr})`;
                  }
                  innerHTML += `<td>${fns}</td>`;
                } else if (key == "minCurrent" || key == "maxCurrent") {
                  innerHTML += `<td>${value} mA</td>`;
                } else {
                  innerHTML += `<td>${value}</td>`;
                }
                innerHTML += "</tr>";
              }

              innerHTML += "</table></td></tr>";
            }
            innerHTML += "</table></td></tr>";
          }
          innerHTML += "</table>";
          testsStatusDisplay.innerHTML = innerHTML;
        }
        if (this.stopOnComplete) {
          this.stop('Get device information complete');
        }
      };

      start() {
        // Load the currently staged settings, not the settings on flash
        send(ProfilingStateMachine.GET_CONNECTED_GAMEPADS_ADDR, 'X'.charCodeAt(0), ['O'.charCodeAt(0)]);
      }

      sendNext() {
        if (this.currentIdx >= 0) {
          while (this.currentPeripheralSummary.length > ++this.currentPeripheralIdx) {
            if (this.currentPeripheralSummary[this.currentPeripheralIdx].length > 0) {
              let hostAddr = getMapleHostAddr(this.currentIdx);
              let destAddr = this.getDestAddr(hostAddr);
              this.sentBasicInfoReq = false;
              send(ProfilingStateMachine.GET_DEVICE_INFO_BASE_ADDR + this.currentIdx, '0'.charCodeAt(0), [0x02, destAddr, hostAddr, 0]);
              return true;
            }
          }
        }
        let nextIdx = this.nextIdx();
        if (nextIdx >= 0) {
          send(ProfilingStateMachine.GET_DC_SUMMARY_BASE_ADDR + nextIdx, 'X'.charCodeAt(0), ['?'.charCodeAt(0), nextIdx]);
          return true;
        }
        return false;
      }

      sendBasicInfoReq() {
        this.sentBasicInfoReq = true;
        let hostAddr = getMapleHostAddr(this.currentIdx);
        let destAddr = this.getDestAddr(hostAddr);
        send(ProfilingStateMachine.GET_DEVICE_INFO_BASE_ADDR + this.currentIdx, '0'.charCodeAt(0), [0x01, destAddr, hostAddr, 0]);
      }

      process(addr, cmd, payload) {
        if (addr == ProfilingStateMachine.GET_CONNECTED_GAMEPADS_ADDR) {
          if (cmd == CMD_OK) {
            if (payload.length >= 1) {
              // Retrieved connected gamepads
              this.connectionStates = [...payload];
              if (this.sendNext()) {
                return;
              }
            }

            this.complete();
            return;
          }
        } else if (addr >= ProfilingStateMachine.GET_DC_SUMMARY_BASE_ADDR && addr < ProfilingStateMachine.GET_DC_SUMMARY_BASE_ADDR + 4) {
          if (cmd == CMD_OK) {
            this.allData[this.currentIdx] = {};
            this.loadDcSummary(payload);
            if (!this.sendNext()) {
              this.complete();
            }
            return;
          }
        } else {
          if (payload.length < 52 && !this.sentBasicInfoReq) {
            // Some 3rd party devices don't support the extended device info - try getting the basic info
            this.sendBasicInfoReq();
            return;
          }
          let dataDict = {};
          let description = "";
          if (payload.length >= 52) {
            description = String.fromCharCode(...payload.slice(22, 52)).trim();
            dataDict["description"] = description;
          }
          let producer = "";
          if (payload.length >= 112) {
            producer = String.fromCharCode(...payload.slice(52, 112)).trim();
            dataDict["producer"] = producer;
          }
          let capabilities = "";
          if (payload.length >= 192) {
            capabilities = String.fromCharCode(...payload.slice(116, 192)).trim();
            dataDict["capabilities"] = capabilities;
          }
          let currentStr = "";
          if (payload.length >= 116) {
            let minCurrent = (payload[113] << 8 | payload[112]) / 10.0;
            let maxCurrent = (payload[115] << 8 | payload[114]) / 10.0;
            dataDict["minCurrent"] = minCurrent;
            dataDict["maxCurrent"] = maxCurrent;
            currentStr = `${minCurrent} to ${maxCurrent} mA`
          }
          let profileData = this.currentPeripheralSummary[this.currentPeripheralIdx];
          if (profileData.length > 0) {
            let fnsData = [];
            let idx = 0;
            while (idx < profileData.length) {
              let fn = profileData[idx][0];
              let fnName = "";
              if (fn == 0x00000001) {
                fnName = "Controller";
              } else if (fn == 0x00000002) {
                fnName = "Storage";
              } else if (fn == 0x00000004) {
                fnName = "Screen";
              } else if (fn == 0x00000008) {
                fnName = "Timer";
              } else if (fn == 0x00000010) {
                fnName = "Audio Input";
              } else if (fn == 0x00000020) {
                fnName = "AR Gun";
              } else if (fn == 0x00000040) {
                fnName = "Keyboard";
              } else if (fn == 0x00000080) {
                fnName = "Gun";
              } else if (fn == 0x00000100) {
                fnName = "Vibration";
              } else if (fn == 0x00000200) {
                fnName = "Mouse";
              }
              let code = profileData[idx][1];
              fnsData.push({"name": fnName, "id": fn, "code": code});
              ++idx;
            }
            dataDict["functions"] = fnsData;
          }
          this.allData[this.currentIdx][this.currentPeripheralIdx] = dataDict;
          if (!this.sendNext()) {
            this.complete();
          }
          return;
        }
        this.stop('Failed to get device information', 'red', 'bold')
      }
    }

    class BasicTestStateMachine extends ProfilingStateMachine {
      static START_ADDR = 50;
      static SET_SCREEN_ADDR = BasicTestStateMachine.START_ADDR;
      static VIBRATE_START_ADDR = 51;
      static VIBRATE_END_ADDR = 52;

      constructor() {
        super("Basic Test");
        this.timeoutId = -1;
        this.retries = 0;
        this.resendFn = null;
      }

      done(reason) {
        if (this.timeoutId >= 0) {
          clearTimeout(this.timeoutId);
          this.timeoutId = -1;
        }
        super.done(reason);
      }

      sendScreenCmd() {
        this.resendFn = this.sendScreenCmd.bind(this);
        let hostAddr = getMapleHostAddr(this.currentIdx);
        let destAddr = hostAddr | (1 << (this.currentPeripheralIdx - 1));
        send(BasicTestStateMachine.SET_SCREEN_ADDR, '0'.charCodeAt(0), [
          0x0C, destAddr, hostAddr, 0x32, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0xC0,  0x00, 0x00, 0x00, 0x00,  0x00, 0xC0, 0x18, 0x00,
          0x00, 0x00, 0x00, 0xC0,  0x1F, 0x00, 0x00, 0x00,  0x00, 0xC0, 0x0F, 0xFF,  0xC0, 0x00, 0x00, 0xC0,
          0x01, 0xFF, 0xE0, 0x00,  0x00, 0xC0, 0x00, 0x00,  0x70, 0x00, 0x00, 0xC0,  0x00, 0x00, 0x38, 0x1C,
          0x00, 0xC0, 0x00, 0x00,  0x18, 0x38, 0x00, 0xC0,  0x00, 0x00, 0x1D, 0xF0,  0x00, 0xC0, 0x07, 0x80,
          0x0F, 0xE0, 0x00, 0xFE,  0x3F, 0x80, 0x07, 0x00,  0x1F, 0xFC, 0x7C, 0x00,  0x00, 0x00, 0x0F, 0xC0,
          0xE0, 0x0F, 0xE0, 0x00,  0x00, 0x00, 0xE0, 0x07,  0xE0, 0x00, 0x00, 0x00,  0x60, 0x00, 0x60, 0x00,
          0x00, 0x00, 0x7C, 0x00,  0x61, 0x80, 0x00, 0x00,  0x3E, 0x01, 0xE1, 0x80,  0x00, 0x00, 0x07, 0x01,
          0xE1, 0x80, 0x00, 0x00,  0x07, 0x00, 0x61, 0x80,  0x00, 0x00, 0xFE, 0x00,  0x61, 0x80, 0x1F, 0xE0,
          0x7C, 0x0F, 0xE1, 0x80,  0x0F, 0xF0, 0x30, 0x07,  0xC1, 0x80, 0x00, 0x38,  0x00, 0x00, 0x01, 0x80,
          0x00, 0x18, 0x00, 0x00,  0x01, 0x80, 0x00, 0x18,  0x00, 0x00, 0x01, 0x80,  0x00, 0x1C, 0x00, 0x00,
          0x7F, 0xFC, 0x00, 0x0E,  0x00, 0x00, 0x3F, 0xF8,  0x00, 0x07, 0xF0, 0x00,  0x00, 0x00, 0x00, 0x01,
          0xE0, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00
        ]);
      }

      sendVibeStart() {
        this.resendFn = this.sendVibeStart.bind(this);
        let hostAddr = getMapleHostAddr(this.currentIdx);
        let destAddr = hostAddr | (1 << (this.currentPeripheralIdx - 1));
        const VIBE_POWER = 4; // 1 to 7
        send(BasicTestStateMachine.VIBRATE_START_ADDR, '0'.charCodeAt(0), [0x0E, destAddr, hostAddr, 2, 0x00, 0x00, 0x01, 0x00, 0x11, (VIBE_POWER << 4), 59, 0x00]);
      }

      sendVibeEnd() {
        this.resendFn = this.sendVibeEnd.bind(this);
        let hostAddr = getMapleHostAddr(this.currentIdx);
        let destAddr = hostAddr | (1 << (this.currentPeripheralIdx - 1));
        send(BasicTestStateMachine.VIBRATE_END_ADDR, '0'.charCodeAt(0), [0x0E, destAddr, hostAddr, 2, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 59, 0x00]);
      }

      sendNext() {
        if (this.currentIdx < 0) {
          while (!(++this.currentIdx in this.allData) && this.currentIdx < 4) {}
          if (this.currentIdx >= 4) {
            this.stop("No peripherals to test");
            return;
          }
        }

        let dataDict = this.allData[this.currentIdx];

        while (true) {
          while ((!((++this.currentPeripheralIdx) in dataDict) && this.currentPeripheralIdx < 6) || this.currentPeripheralIdx <= 0) {}
          if (this.currentPeripheralIdx >= 6) {
            while (!(++this.currentIdx in this.allData) && this.currentIdx < 4) {}
            if (this.currentIdx >= 4) {
              this.stop("Basic test complete");
              return;
            }
            dataDict = this.allData[this.currentIdx];
            this.currentPeripheralIdx = -1;
          } else {
            // Valid next peripheral
            let periphData = dataDict[this.currentPeripheralIdx];
            if ("functions" in periphData) {
              let fns = periphData["functions"];
              for (let i = 0; i < fns.length; i++) {
                let fnId = fns[i]["id"];
                if (fnId == 0x00000004) {
                  this.sendScreenCmd();
                  return;
                } else if (fnId == 0x00000100) {
                  this.sendVibeStart();
                  return;
                }
              }
            }
          }
        }
      }

      complete() {
        super.complete();
        this.currentIdx = -1;
        this.currentPeripheralIdx = -1;
        this.sendNext();
      }

      process(addr, cmd, payload) {
        if (addr < BasicTestStateMachine.START_ADDR) {
          super.process(addr, cmd, payload);
        } else if (cmd != CMD_OK){
          if (++this.retries <= 2 && this.resendFn != null) {
            this.resendFn();
            return;
          }
          this.stop('Basic test failed: maple command failure', 'red', 'bold')
        } else {
          this.retries = 0;
          if (addr == BasicTestStateMachine.SET_SCREEN_ADDR || addr == BasicTestStateMachine.VIBRATE_END_ADDR) {
            resetSmTimeout(150);
            this.sendNext();
          } else if (addr = BasicTestStateMachine.VIBRATE_START_ADDR) {
            const VIBE_TIME_MS = 250;
            this.timeoutId = setTimeout(this.sendVibeEnd.bind(this), VIBE_TIME_MS);
            resetSmTimeout(VIBE_TIME_MS + 150);
          }
        }
      }
    }

    class StressTestStateMachine extends ProfilingStateMachine {
      static START_ADDR = 60;
      static SET_SCREEN_ADDR = BasicTestStateMachine.START_ADDR;
      static VIBRATE_START_ADDR = 61;
      static VIBRATE_END_ADDR = 62;

      constructor() {
        super("Stress Test");
        this.timeoutId = -1;
        this.vibrationAddrs = [];
        this.retries = 0;
        this.resendFn = null;
      }

      done(reason) {
        if (this.timeoutId >= 0) {
          clearTimeout(this.timeoutId);
          this.timeoutId = -1;
        }
        super.done(reason);
      }

      sendNext() {
        if (this.currentIdx < 0) {
          while (!(++this.currentIdx in this.allData) && this.currentIdx < 4) {}
          if (this.currentIdx >= 4) {
            this.stop("No peripherals to test");
            return;
          }
        }

        let dataDict = this.allData[this.currentIdx];

        while (true) {
          while ((!((++this.currentPeripheralIdx) in dataDict) && this.currentPeripheralIdx < 6) || this.currentPeripheralIdx <= 0) {}
          if (this.currentPeripheralIdx >= 6) {
            while (!(++this.currentIdx in this.allData) && this.currentIdx < 4) {}
            if (this.currentIdx >= 4) {
              if (this.vibrationAddrs.length == 0) {
                this.stop("Stress test complete");
              } else {
                const VIBE_TIME_MS = 5000;
                this.timeoutId = setTimeout(this.vibrationDone.bind(this), VIBE_TIME_MS);
                resetSmTimeout(VIBE_TIME_MS + 150);
              }
              return;
            }
            dataDict = this.allData[this.currentIdx];
            this.currentPeripheralIdx = -1;
          } else {
            // Valid next peripheral
            let periphData = dataDict[this.currentPeripheralIdx];
            if ("functions" in periphData) {
              let fns = periphData["functions"];
              for (let i = 0; i < fns.length; i++) {
                let fnId = fns[i]["id"];
                let hostAddr = getMapleHostAddr(this.currentIdx);
                let destAddr = hostAddr | (1 << (this.currentPeripheralIdx - 1));
                if (fnId == 0x00000004) {
                  // Send screen update
                  send(StressTestStateMachine.SET_SCREEN_ADDR, '0'.charCodeAt(0), [
                    0x0C, destAddr, hostAddr, 0x32, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0xC0,  0x00, 0x00, 0x00, 0x00,  0x00, 0xC0, 0x18, 0x00,
                    0x00, 0x00, 0x00, 0xC0,  0x1F, 0x00, 0x00, 0x00,  0x00, 0xC0, 0x0F, 0xFF,  0xC0, 0x00, 0x00, 0xC0,
                    0x01, 0xFF, 0xE0, 0x00,  0x00, 0xC0, 0x00, 0x00,  0x70, 0x00, 0x00, 0xC0,  0x00, 0x00, 0x38, 0x1C,
                    0x00, 0xC0, 0x00, 0x00,  0x18, 0x38, 0x00, 0xC0,  0x00, 0x00, 0x1D, 0xF0,  0x00, 0xC0, 0x07, 0x80,
                    0x0F, 0xE0, 0x00, 0xFE,  0x3F, 0x80, 0x07, 0x00,  0x1F, 0xFC, 0x7C, 0x00,  0x00, 0x00, 0x0F, 0xC0,
                    0xE0, 0x0F, 0xE0, 0x00,  0x00, 0x00, 0xE0, 0x07,  0xE0, 0x00, 0x00, 0x00,  0x60, 0x00, 0x60, 0x00,
                    0x00, 0x00, 0x7C, 0x00,  0x61, 0x80, 0x00, 0x00,  0x3E, 0x01, 0xE1, 0x80,  0x00, 0x00, 0x07, 0x01,
                    0xE1, 0x80, 0x00, 0x00,  0x07, 0x00, 0x61, 0x80,  0x00, 0x00, 0xFE, 0x00,  0x61, 0x80, 0x1F, 0xE0,
                    0x7C, 0x0F, 0xE1, 0x80,  0x0F, 0xF0, 0x30, 0x07,  0xC1, 0x80, 0x00, 0x38,  0x00, 0x00, 0x01, 0x80,
                    0x00, 0x18, 0x00, 0x00,  0x01, 0x80, 0x00, 0x18,  0x00, 0x00, 0x01, 0x80,  0x00, 0x1C, 0x00, 0x00,
                    0x7F, 0xFC, 0x00, 0x0E,  0x00, 0x00, 0x3F, 0xF8,  0x00, 0x07, 0xF0, 0x00,  0x00, 0x00, 0x00, 0x01,
                    0xE0, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00
                  ]);
                } else if (fnId == 0x00000100) {
                  // Send vibration update
                  const VIBE_POWER = 7; // 1 to 7
                  send(StressTestStateMachine.VIBRATE_START_ADDR, '0'.charCodeAt(0), [0x0E, destAddr, hostAddr, 2, 0x00, 0x00, 0x01, 0x00, 0x11, (VIBE_POWER << 4), 59, 0x00]);
                  this.vibrationAddrs.push([destAddr, hostAddr])
                }
              }
            }
          }
        }
      }

      delayToDone() {
        this.stop("Stress test complete");
      }

      vibrationDone() {
        for (let i = 0; i < this.vibrationAddrs.length; ++i) {
          let addrs = this.vibrationAddrs[i];
          let hostAddr = addrs[1];
          let destAddr = addrs[0];
          send(StressTestStateMachine.VIBRATE_END_ADDR, '0'.charCodeAt(0), [0x0E, destAddr, hostAddr, 2, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 59, 0x00]);
        }
        const DELAY_TIME_MS = 100;
        this.timeoutId = setTimeout(this.delayToDone.bind(this), DELAY_TIME_MS);
        resetSmTimeout(DELAY_TIME_MS + 150);
      }

      complete() {
        super.complete();
        this.currentIdx = -1;
        this.currentPeripheralIdx = -1;
        this.sendNext();
      }

      process(addr, cmd, payload) {
        if (addr < BasicTestStateMachine.START_ADDR) {
          super.process(addr, cmd, payload);
        } else if (cmd != CMD_OK){
          this.stop('Stress test failed: maple command failure', 'red', 'bold')
        }
      }
    }

    // State machine which writes GPIO settings
    class WriteGpioStateMachine extends StateMachine {
      static SEND_CONTROLLER_A_ADDR = 20;
      static SEND_CONTROLLER_B_ADDR = 21;
      static SEND_CONTROLLER_C_ADDR = 22;
      static SEND_CONTROLLER_D_ADDR = 23;
      static SEND_LED_ADDR = 24;
      static SEND_SIMPLE_LED_ADDR = 25;
      static SEND_VALIDATE_SETTINGS = 26;
      static SEND_SAVE_AND_RESTART_ADDR = 27;

      static CMD_SETTINGS = 'S'.charCodeAt(0);

      constructor() {
        super("Write GPIO");
        this.disconnectExpected = false;
      }

      done(reason) {
        if (reason == SM_DONE_REASON_DISCONNECT && this.disconnectExpected) {
          // Didn't get a chance to get response before reboot occurred
          setStatus('GPIO settings saved');
        } else {
          super.done(reason);
        }
      };

      start() {
        // I[0-3][GPIO A (4 byte)][GPIO DIR (4 byte)][DIR Output HIGH (1 byte)]
        const gpioA = (gpioAText.value === "") ? -1 : Number(gpioAText.value);
        const gpioDir = (gpioADirText.value === "") ? -1 : Number(gpioADirText.value);
        const gpioDirOutHigh = gpioADirOutHighCheckbox.checked ? 1 : 0
        send(WriteGpioStateMachine.SEND_CONTROLLER_A_ADDR, WriteGpioStateMachine.CMD_SETTINGS, ['I'.charCodeAt(0), 0, ...int32ToBytes(gpioA), ...int32ToBytes(gpioDir), gpioDirOutHigh])
      };

      process(addr, cmd, payload) {
        if (addr == WriteGpioStateMachine.SEND_CONTROLLER_A_ADDR) {
          if (cmd == CMD_OK) {
            const gpioA = (gpioBText.value === "") ? -1 : Number(gpioBText.value);
            const gpioDir = (gpioBDirText.value === "") ? -1 : Number(gpioBDirText.value);
            const gpioDirOutHigh = gpioBDirOutHighCheckbox.checked ? 1 : 0
            send(WriteGpioStateMachine.SEND_CONTROLLER_B_ADDR, WriteGpioStateMachine.CMD_SETTINGS, ['I'.charCodeAt(0), 1, ...int32ToBytes(gpioA), ...int32ToBytes(gpioDir), gpioDirOutHigh])
          } else {
            this.stop('Failed to set controller A GPIO - ensure they are valid', 'red', 'bold');
          }
        } else if (addr == WriteGpioStateMachine.SEND_CONTROLLER_B_ADDR) {
          if (cmd == CMD_OK) {
            const gpioA = (gpioCText.value === "") ? -1 : Number(gpioCText.value);
            const gpioDir = (gpioCDirText.value === "") ? -1 : Number(gpioCDirText.value);
            const gpioDirOutHigh = gpioCDirOutHighCheckbox.checked ? 1 : 0
            send(WriteGpioStateMachine.SEND_CONTROLLER_C_ADDR, WriteGpioStateMachine.CMD_SETTINGS, ['I'.charCodeAt(0), 2, ...int32ToBytes(gpioA), ...int32ToBytes(gpioDir), gpioDirOutHigh])
          } else {
            this.stop('Failed to set controller B GPIO - ensure they are valid', 'red', 'bold');
          }
        } else if (addr == WriteGpioStateMachine.SEND_CONTROLLER_C_ADDR) {
          if (cmd == CMD_OK) {
            const gpioA = (gpioDText.value === "") ? -1 : Number(gpioDText.value);
            const gpioDir = (gpioDDirText.value === "") ? -1 : Number(gpioDDirText.value);
            const gpioDirOutHigh = gpioDDirOutHighCheckbox.checked ? 1 : 0
            send(WriteGpioStateMachine.SEND_CONTROLLER_D_ADDR, WriteGpioStateMachine.CMD_SETTINGS, ['I'.charCodeAt(0), 3, ...int32ToBytes(gpioA), ...int32ToBytes(gpioDir), gpioDirOutHigh])
          } else {
            this.stop('Failed to set controller C GPIO - ensure they are valid', 'red', 'bold');
          }
        } else if (addr == WriteGpioStateMachine.SEND_CONTROLLER_D_ADDR) {
          if (cmd == CMD_OK) {
            // LED setting L[LED Pin (4 byte)]
            const gpioLed = (gpioLedText.value === "") ? -1 : Number(gpioLedText.value);
            send(WriteGpioStateMachine.SEND_LED_ADDR, WriteGpioStateMachine.CMD_SETTINGS, ['L'.charCodeAt(0), ...int32ToBytes(gpioLed)]);
          } else {
            this.stop('Failed to set controller D GPIO - ensure they are valid', 'red', 'bold');
          }
        } else if (addr == WriteGpioStateMachine.SEND_LED_ADDR) {
          if (cmd == CMD_OK) {
            // Simple LED setting l[LED Pin (4 byte)]
            const gpioLed = (gpioSimpleLedText.value === "") ? -1 : Number(gpioSimpleLedText.value);
            send(WriteGpioStateMachine.SEND_SIMPLE_LED_ADDR, WriteGpioStateMachine.CMD_SETTINGS, ['l'.charCodeAt(0), ...int32ToBytes(gpioLed)]);
          } else {
            this.stop('Failed to set LED GPIO - ensure it is valid', 'red', 'bold');
          }
        } else if (addr == WriteGpioStateMachine.SEND_SIMPLE_LED_ADDR) {
          if (cmd == CMD_OK) {
            send(WriteGpioStateMachine.SEND_VALIDATE_SETTINGS, WriteGpioStateMachine.CMD_SETTINGS, ['s'.charCodeAt(0)]);
          } else {
            this.stop('Failed to set simple LED GPIO - ensure it is valid', 'red', 'bold');
          }
        } else if (addr == WriteGpioStateMachine.SEND_VALIDATE_SETTINGS) {
          if (cmd == CMD_ATTENTION) {
            setSettingsFromPayload(payload)
            this.stop('GPIO settings need adjustment to prevent overlap - please review changes', 'red', 'bold');
          } else if (cmd == CMD_OK) {
            send(WriteGpioStateMachine.SEND_SAVE_AND_RESTART_ADDR, WriteGpioStateMachine.CMD_SETTINGS, ['S'.charCodeAt(0)]);
            // Reboot may occur before the next event
            this.disconnectExpected = true;
          } else {
            this.stop('Failed to validate settings', 'red', 'bold');
          }
        } else if (addr == WriteGpioStateMachine.SEND_SAVE_AND_RESTART_ADDR) {
          if (cmd == CMD_OK) {
            if (setSettingsFromPayload(payload))
            {
              this.stop('GPIO settings saved with adjustments due to overlapping GPIO - please review changes', 'orange', 'bold')
            }
            else
            {
              this.stop('GPIO settings saved')
            }
          } else {
            this.stop('Failed to save settings', 'red', 'bold');
          }
        }
      };
    }

    // State machine which resets all settings
    class ResetSettingsStateMachine extends StateMachine {
      static SEND_GET_DEFAULTS_ADDR = 30;
      static SEND_RESET_AND_RESTART_ADDR = 31;

      static CMD_SETTINGS = 'S'.charCodeAt(0);

      constructor() {
        super("Settings reset");
        this.disconnectExpected = false;
      }

      resetSuccessful() {
        // It's easier to just disable control and force user to select the device again - settings may automatically
        // change on next boot if a controller is connected. This helps ensure loaded settings will match reality.
        setStatus('Settings reset - please select device again to continue');
        disableAllControls();
      }

      done(reason) {
        if (reason == SM_DONE_REASON_DISCONNECT && this.disconnectExpected) {
          // Didn't get a chance to get response before reboot occurred
          this.resetSuccessful();
        } else {
          super.done(reason);
        }
      };

      start() {
        send(ResetSettingsStateMachine.SEND_GET_DEFAULTS_ADDR, ResetSettingsStateMachine.CMD_SETTINGS, ['x'.charCodeAt(0)]);
      };

      process(addr, cmd, payload) {
        if (addr == ResetSettingsStateMachine.SEND_GET_DEFAULTS_ADDR) {
          if (cmd == CMD_OK) {
            setSettingsFromPayload(payload);
            send(ResetSettingsStateMachine.SEND_RESET_AND_RESTART_ADDR, ResetSettingsStateMachine.CMD_SETTINGS, ['X'.charCodeAt(0)]);
            // Reboot may occur before the next event
            this.disconnectExpected = true;
          } else {
            this.stop('Failed to retrieve defaults', 'red', 'bold');
          }
        } else if (addr == ResetSettingsStateMachine.SEND_RESET_AND_RESTART_ADDR) {
          if (cmd == CMD_OK) {
            setSettingsFromPayload(payload);
            this.resetSuccessful();
            this.stop();
          } else {
            this.stop('Failed to reset settings', 'red', 'bold');
          }
        }
      };
    }

    // *************************************************************************
    // End State Machine Definitions
    // *************************************************************************


    // Starts the state machine which reads all VMU data
    function startReadVmuSm(controllerIdx, vmuIndex) {
      setStatus("Reading VMU...");
      startSm(new ReadVmuStateMachine(controllerIdx, vmuIndex));
    }

    // Starts the state machine which writes to a selected VMU
    function startWriteVmuSm(controllerIdx, vmuIdx, fileData) {
      setStatus("Writing VMU...");
      startSm(new WriteVmuStateMachine(controllerIdx, vmuIdx, fileData));
    }

    // Starts the state machine which gets all device information strings
    function startProfilingSm() {
      setStatus("Getting device information...");
      testsStatusDisplay.innerHTML = "";
      startSm(new ProfilingStateMachine());
    }

    // Starts the state machine which runs basic tests
    function startBasicTest() {
      setStatus("Running basic test...");
      startSm(new BasicTestStateMachine());
    }

    // Starts the state machine which runs stress tests
    function startStressTest() {
      setStatus("Running stress test...");
      startSm(new StressTestStateMachine());
    }

    // Starts the state machine which writes the current settings within the GPIO tab
    function startWriteGpioSm() {
      setStatus("Writing GPIO Settings...");
      startSm(new WriteGpioStateMachine());
    }

    // Starts the state machine which resets the sector in flash that settings reside on the DreamPicoPort
    function startResetSettingsSm() {
      setStatus("Resetting Settings...");
      startSm(new ResetSettingsStateMachine());
    }

    // General Settings Save Button - click handler
    saveButton.addEventListener('click', function() {
      startSaveSm();
    });

    // VMU Memory Cancel Button - click handler
    vmuMemoryCancelButton.addEventListener('click', function () {
      cancelSm(SM_DONE_REASON_CANCELED_USER);
    });

    // Test: Profiling
    testsProfileButton.addEventListener('click', function() {
      startProfilingSm();
    });

    // Test: Basic
    testsBasicButton.addEventListener('click', function() {
      startBasicTest();
    });

    // Test: Stress
    testsStressButton.addEventListener('click', function() {
      startStressTest();
    });

    // Save GPIO Settings Button - click handler
    saveGpioButton.addEventListener('click', function() {
      startWriteGpioSm();
    });

    // Reset Settings Button - click handler
    resetSettingsButton.addEventListener('click', function() {
      startResetSettingsSm();
    });

  });
})();
