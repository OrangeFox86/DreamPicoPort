#include "DreamcastLightgun.hpp"

namespace client
{

DreamcastLightgun::DreamcastLightgun() :
    DreamcastPeripheralFunction(DEVICE_FN_GUN)
{}

//! Handle packet meant for this peripheral function
//! @param[in] in  The packet read from the Maple Bus
//! @param[out] out  The packet to write to the Maple Bus when true is returned
//! @returns true iff the packet was handled
bool DreamcastLightgun::handlePacket(const MaplePacket& in, MaplePacket& out)
{
    // Light gun defines no other packets besides the standard one -
    // it is just here to tell Maple Bus that we support the light gun packet
    return false;
}

//! Called when player index changed or timeout occurred
void DreamcastLightgun::reset()
{
}

//! @returns the function definition for this peripheral function
uint32_t DreamcastLightgun::getFunctionDefinition()
{
    return 0;
}

}
