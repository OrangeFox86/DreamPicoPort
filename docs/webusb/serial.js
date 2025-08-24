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
    this.serial = device.serialNumber;
    this.major = device.deviceVersionMajor;
    this.minor = device.deviceVersionMinor;
    this.patch = device.deviceVersionSubminor;
  };

  serial.Port.prototype.connect = function() {
    let readLoop = () => {
      this.device_.transferIn(this.endpointIn, 64).then(result => {
        this.onReceive(result.data);
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
    return this.device_.controlTransferOut({
            'requestType': 'class',
            'recipient': 'interface',
            'request': 0x22,
            'value': 0x00,
            'index': this.interfaceNumber})
        .then(() => this.device_.close());
  };

  serial.Port.prototype.send = function(data) {
    return this.device_.transferOut(this.endpointOut, data);
  };
})();
