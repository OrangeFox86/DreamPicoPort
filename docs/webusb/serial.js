var serial = {};

(function() {
  'use strict';

  serial.getPorts = function() {
    return navigator.usb.getDevices().then(devices => {
      return devices.map(device => new serial.Port(device));
    });
  };

  serial.requestPort = function() {
    const filters = [
      { 'vendorId': 0x1209, 'productId': 0x2f07 }, // DreamPicoPort
    ];
    return navigator.usb.requestDevice({ 'filters': filters }).then(
      device => new serial.Port(device)
    );
  }

  serial.Port = function(device) {
    this.device_ = device;
    this.interfaceNumber = 0;
    this.endpointIn = 0;
    this.endpointOut = 0;
    this.name = device.productName;
    this.serial = device.serialNumber;
    this.major = device.deviceVersionMajor;
    this.minor = device.deviceVersionMinor;
    this.patch = device.deviceVersionSubminor;
  };

  serial.Port.prototype.connect = function() {
    let readLoop = () => {
      // 2048 is about twice as much as actually required
      this.device_.transferIn(this.endpointIn, 2048).then(result => {
        if (result.data && result.data.byteLength > 0) {
          this.onReceive(result.data);
        }
        readLoop();
      }, error => {
        this.onReceiveError(error);
      });
    };

    return this.device_.open()
        .then(() => {
          if (this.device_.configuration === null) {
            return this.device_.selectConfiguration(1);
          }
        })
        .then(() => {
          // This will select the last interface which implements the VENDOR class
          var interfaces = this.device_.configuration.interfaces;
          interfaces.forEach(element => {
            element.alternates.forEach(elementalt => {
              if (elementalt.interfaceClass==0xFF) {
                this.interfaceNumber = element.interfaceNumber;
                elementalt.endpoints.forEach(elementendpoint => {
                  if (elementendpoint.direction == "out") {
                    this.endpointOut = elementendpoint.endpointNumber;
                  }
                  if (elementendpoint.direction=="in") {
                    this.endpointIn =elementendpoint.endpointNumber;
                  }
                })
              }
            })
          })
        })
        // .then(() => {
        //   alert(`receiving from ${this.interfaceNumber}`);
        // })
        .then(() => this.device_.claimInterface(this.interfaceNumber))
        .then(() => this.device_.selectAlternateInterface(this.interfaceNumber, 0))
        .then(() => this.device_.controlTransferOut({
            'requestType': 'class',
            'recipient': 'interface',
            'request': 0x22,
            'value': 0x01,
            'index': this.interfaceNumber}))
        .then(() => this.ready())
        .then(() => {
          readLoop();
        });
  };

  serial.Port.prototype.disconnect = function() {
    let value = 0; // value to reset interface
    if (window.navigator.userAgent.indexOf("Mac") !== -1) {
      // Workaround for MacOS: force device reboot by sending special case reboot value
      value = 0xFFFE;
    }

    return this.device_.controlTransferOut({
            'requestType': 'class',
            'recipient': 'interface',
            'request': 0x22,
            'value': value,
            'index': this.interfaceNumber})
        .catch(() => {})
        .finally(() => {
          return this.device_.reset().catch(() => {});
        })
        .finally(() => {
          return this.device_.close().catch(() => {});
        });
  };

  serial.Port.prototype.send = function(data) {
    return this.device_.transferOut(this.endpointOut, data);
  };
})();
