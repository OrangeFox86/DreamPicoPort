#pragma once

#include <hal/System/SystemIdentification.hpp>
#include "pico/unique_id.h"

class PicoIdentification : public SystemIdentification
{
    public:
        PicoIdentification() = default;
        virtual ~PicoIdentification() = default;

        std::uint32_t getSerialSize() override;
        void getSerial(char* buffer, std::uint32_t bufflen) override;
};
