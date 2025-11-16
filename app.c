#include "sl_btmesh_api.h"
#include "sl_simple_button_instances.h"
#include "sl_sleeptimer.h"
#include "my_model_def.h"
#include "app.h"
#include "app_log.h"

// Vendor model info
static uint16_t elem_index = 0;
static uint16_t vendor_id = MY_COMPANY_ID;
static uint16_t model_id  = MY_CLIENT_MODEL_ID;

// LED state 2-bit
static uint8_t led0 = 0;
static uint8_t led1 = 0;

// =====================================================
// KHỞI TẠO CLIENT
// =====================================================
void app_init(void)
{
    elem_index = 0;  // Element mặc định
    app_log("Client initialized OK\r\n");
}

// =====================================================
// HÀM GỬI MSSV 4 BẠN (BTN0)
// =====================================================
void client_send_mssv(void)
{
    // MSSV 4 bạn → tổng 16 bytes ASCII
    uint8_t mssv[16] = {
        '2','2','2','0','0','1','1','4',
        '2','2','2','0','0','1','3','1',
        '2','2','2','0','0','1','4','4',
        '2','2','2','0','0','1','6','6'
    };

    sl_btmesh_vendor_model_set_publication(elem_index,
                                           vendor_id,
                                           model_id,
                                           OPCODE_MSSV,
                                           0,
                                           sizeof(mssv),
                                           mssv);

    sl_btmesh_vendor_model_publish(elem_index, vendor_id, model_id);

    app_log("Sent MSSV group payload (16 bytes)\r\n");
}

// =====================================================
// HÀM GỬI UPTIME (BTN1)
// =====================================================
void client_send_uptime(void)
{
    uint32_t uptime_s = sl_sleeptimer_get_tick_count64() / 32768;

    uint8_t data[4];
    data[0] = uptime_s >> 24;
    data[1] = uptime_s >> 16;
    data[2] = uptime_s >> 8;
    data[3] = uptime_s;

    sl_btmesh_vendor_model_set_publication(elem_index,
                                           vendor_id,
                                           model_id,
                                           OPCODE_UPTIME,
                                           0,
                                           4,
                                           data);

    sl_btmesh_vendor_model_publish(elem_index, vendor_id, model_id);

    app_log("Sent uptime: %lu s\r\n", uptime_s);
}

// =====================================================
// HÀM GỬI LED STATE (BTN1)
// =====================================================
void client_send_led_state(void)
{
    // Toggle 2 bit LED mỗi lần nhấn
    led0 ^= 1;
    led1 ^= 1;

    uint8_t led_state = (led0 << 1) | (led1);

    sl_btmesh_vendor_model_set_publication(elem_index,
                                           vendor_id,
                                           model_id,
                                           OPCODE_LED,
                                           0,
                                           1,
                                           &led_state);

    sl_btmesh_vendor_model_publish(elem_index, vendor_id, model_id);

    app_log("Sent LED state: %d%d\r\n", led0, led1);
}

// =====================================================
// BUTTON HANDLER
// =====================================================
#include "app_button_press.h"

void app_button_press_on_change(uint8_t button, uint8_t pressed)
{
    if (!pressed) return;   // chỉ xử lý khi nhấn xuống

    switch(button)
    {
        case 0:     // BTN0
            client_send_mssv();
            break;

        case 1:     // BTN1
            client_send_uptime();
            client_send_led_state();
            break;
    }
}
