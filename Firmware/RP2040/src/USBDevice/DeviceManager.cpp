#include "USBDevice/DeviceDriver/PS3/PS3.h"
#include "USBDevice/DeviceManager.h"

void DeviceManager::initialize_driver(  DeviceDriverType driver_type,
                                        Gamepad(&gamepads)[MAX_GAMEPADS]) {
    (void)driver_type;

    device_driver_ = std::make_unique<PS3Device>();

    for (size_t i = 0; i < MAX_GAMEPADS; ++i) {
        gamepads[i].set_analog_device(true);
    }

    device_driver_->initialize();
}