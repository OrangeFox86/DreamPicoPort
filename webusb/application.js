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
      let data = [addr, cmd, ...payload];
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

    function send(pkt) {
      const pktData = new Uint8Array(pkt);
      console.info("SND: " + Array.from(pktData).map(b => b.toString(16).padStart(2, '0')).join(' '));
      port.send(pktData);
    }

    function connect() {
      port.connect().then(() => {
        statusDisplay.textContent = '';
        connectButton.textContent = 'Disconnect';

        port.onReceive = data => {
           console.info("RCV: " + Array.from(new Uint8Array(data.buffer)).map(b => b.toString(16).padStart(2, '0')).join(' '));
          // let textDecoder = new TextDecoder();
          // // let receiveTime = performance.now();
          // // bs += data.buffer.byteLength;
          // // if (sendTime && bs >= 536) {
          // //   alert(`Round-trip time: ${(receiveTime - sendTime).toFixed(2)} ms; ${bs} bytes`);
          // //   sendTime = null;
          // // }
          //   console.log(Array.from(new Uint8Array(data.buffer)).map(b => b.toString(16).padStart(2, '0')).join(' '));
          // if (data.getInt8() === 13) {
          //   currentReceiverLine = null;
          // } else {
          //   appendLines('receiver_lines', Array.from(new Uint8Array(data.buffer)).map(b => b.toString(16).padStart(2, '0')).join(' '));
          // }
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
      const pkt1 = makePkt(0, 'S'.charCodeAt(0), [77, mscValue]);
      send(pkt1);

      const pkt2 = makePkt(0, 'S'.charCodeAt(0), [83]);
      send(pkt2);

      // const pkt = makePkt(0, 'X'.charCodeAt(0), [63, 0]);
      // send(pkt);
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
