#include "Board/Config.h"
#include "OGXMini/Board/Standard.h"
#if ((OGXM_BOARD == PI_PICO) || (OGXM_BOARD == RP2040_ZERO) || (OGXM_BOARD == ADAFRUIT_FEATHER))

#include <cstddef>

#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/uart.h>

#include "tusb.h"
#include "bsp/board_api.h"

#include "USBDevice/DeviceManager.h"
#include "TaskQueue/TaskQueue.h"
#include "Gamepad/Gamepad.h"
#include "Board/board_api.h"
#include "UserSettings/UserSettings.h"

namespace {

constexpr uint32_t UART_BAUD_RATE = 115200;
constexpr uint8_t FRAME_START = 0xA5;

constexpr uart_inst_t* HOST_UART = uart0;

constexpr uint HOST_UART_TX_PIN =
#if defined(PICO_DEFAULT_UART_TX_PIN)
    PICO_DEFAULT_UART_TX_PIN;
#else
    0;
#endif

constexpr uint HOST_UART_RX_PIN =
#if defined(PICO_DEFAULT_UART_RX_PIN)
    PICO_DEFAULT_UART_RX_PIN;
#else
    1;
#endif

Gamepad gamepads[MAX_GAMEPADS];

uint8_t compute_checksum(const Gamepad::PadIn& pad_in) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&pad_in);
    uint8_t checksum = 0;
    for (size_t i = 0; i < sizeof(Gamepad::PadIn); ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

bool read_pad_in(Gamepad::PadIn& pad_in) {
    uint8_t header = 0;

    while (true) {
        uart_read_blocking(HOST_UART, &header, 1);
        if (header == FRAME_START) {
            break;
        }
    }

    uart_read_blocking(HOST_UART, reinterpret_cast<uint8_t*>(&pad_in), sizeof(Gamepad::PadIn));

    uint8_t checksum = 0;
    uart_read_blocking(HOST_UART, &checksum, 1);

    return checksum == compute_checksum(pad_in);
}

void configure_uart() {
    uart_init(HOST_UART, UART_BAUD_RATE);
    gpio_set_function(HOST_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(HOST_UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(HOST_UART, false, false);
    uart_set_format(HOST_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(HOST_UART, true);
}

void core1_task() {
    configure_uart();

    for (size_t i = 0; i < MAX_GAMEPADS; ++i) {
        gamepads[i].set_analog_host(true);
    }

    bool link_active = false;

    while (true) {
        Gamepad::PadIn pad_in;
        if (read_pad_in(pad_in)) {
            gamepads[0].set_pad_in(pad_in);
            if (!link_active) {
                board_api::set_led(true);
                link_active = true;
            }
        }
    }
}

} // namespace

void standard::initialize() {
    board_api::init_board();
    board_api::set_led(false);

    UserSettings& user_settings = UserSettings::get_instance();
    user_settings.initialize_flash();

    for (uint8_t i = 0; i < MAX_GAMEPADS; ++i) {
        gamepads[i].set_profile(user_settings.get_profile_by_index(i));
    }

    DeviceManager::get_instance().initialize_driver(user_settings.get_current_driver(), gamepads);
}

void standard::run() {
    multicore_reset_core1();
    multicore_launch_core1(core1_task);

    tud_init(BOARD_TUD_RHPORT);

    DeviceDriver* device_driver = DeviceManager::get_instance().get_driver();

    while (true) {
        TaskQueue::Core0::process_tasks();

        for (uint8_t i = 0; i < MAX_GAMEPADS; ++i) {
            device_driver->process(i, gamepads[i]);
        }

        tud_task();
        sleep_ms(1);
    }
}

#endif // OGXM_BOARD == PI_PICO || OGXM_BOARD == RP2040_ZERO || OGXM_BOARD == ADAFRUIT_FEATHER
