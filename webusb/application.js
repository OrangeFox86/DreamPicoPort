(function() {
  'use strict';

  document.addEventListener('DOMContentLoaded', event => {
    let connectButton = document.querySelector("#connect");
    let statusDisplay = document.querySelector('#status');
    let saveButton = document.querySelector('#save');
    let mscCheckbox = document.querySelector('#enable-msc-check');
    let port;

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

    function makePkt(addr, cmd, payload) {
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

      return pkt
    }

    function send(addr, cmd, payload) {
      console.info(`SND ADDR: 0x${addr.toString(16)}, CMD: 0x${cmd.toString(16)}, PAYLOAD: [${Array.from(payload).map(b => '0x' + b.toString(16).padStart(2, '0')).join(', ')}]`);
      const pktData = new Uint8Array(makePkt(addr, cmd, payload));
      if (port && typeof port.send === 'function') {
        port.send(pktData);
      } else {
        console.error("Port is not connected or does not support send().");
      }
    }

    function connect() {
      port.connect().then(() => {
        statusDisplay.textContent = '';
        connectButton.textContent = 'Disconnect';
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

            console.info(`RCV ADDR: 0x${addr.toString(16)}, CMD: 0x${cmd.toString(16)}, PAYLOAD: [${Array.from(dataPayload).map(b => '0x' + b.toString(16).padStart(2, '0')).join(', ')}]`);

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

        // // Data to send
        // const pkt = makePkt(0, 'X'.charCodeAt(0), [63, 0]);

        // alert("Sending: " + Array.from(new Uint8Array(pkt)).map(b => b.toString(16).padStart(2, '0')).join(' '));

        // sendTime = performance.now();
        // port.send(new Uint8Array(pkt));

        port.onReceiveError = error => {
          console.error(error);
        };
      }, error => {
        statusDisplay.textContent = error;
      });
    }

    connectButton.addEventListener('click', function() {
      if (port) {
        port.disconnect();
        connectButton.textContent = 'Connect';
        statusDisplay.textContent = '';
        port = null;
      } else {
        serial.requestPort().then(selectedPort => {
          port = selectedPort;
          connect();
        }).catch(error => {
          statusDisplay.textContent = error;
        });
      }
    });

    saveButton.addEventListener('click', function() {
      const mscValue = mscCheckbox.checked ? 1 : 0;
      send(0, 'S'.charCodeAt(0), [77, mscValue]);

      send(0, 'S'.charCodeAt(0), [83]);

      // send(0, 'X'.charCodeAt(0), [63, 0]);
    });

    // serial.getPorts().then(ports => {
    //   if (ports.length === 0) {
    //     statusDisplay.textContent = 'No device found.';
    //   } else {
    //     statusDisplay.textContent = 'Connecting...';
    //     port = ports[0];
    //     connect();
    //   }
    // });


    // let commandLine = document.getElementById("command_line");

    // commandLine.addEventListener("keypress", function(event) {
    //   if (event.keyCode === 13) {
    //     if (commandLine.value.length > 0) {
    //       addLine('sender_lines', commandLine.value);
    //       commandLine.value = '';
    //     }
    //   }

    //   port.send(new TextEncoder('utf-8').encode(String.fromCharCode(event.which || event.keyCode)));
    // });
  });
})();
