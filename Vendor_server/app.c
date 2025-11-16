/***************************************************************************//**
 * @file app.c
 * @brief Core application logic for the vendor server node.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************
 * # Experimental Quality
 * This code has not been formally tested and is provided as-is. It is not
 * suitable for production environments. In addition, this code will not be
 * maintained and there may be no bug maintenance planned for these resources.
 * Silicon Labs may update projects from time to time.
 ******************************************************************************/
#include <stdio.h>
#include "em_common.h"
#include "app_assert.h"
#include "app_log.h"
#include "sl_status.h"
#include "app.h"
#include "gatt_db.h"

#include "sl_btmesh_api.h"
#include "sl_bt_api.h"
#include "app_timer.h"

#include "em_cmu.h"
#include "em_gpio.h"
#include "em_rtcc.h"

#include "my_model_def.h"

#include "app_button_press.h"
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"

#define EX_B0_PRESS                                 ((1) << 5)
#define EX_B1_PRESS                                 ((1) << 6)

// Advertising Provisioning Bearer
#define PB_ADV                                      0x1
// GATT Provisioning Bearer
#define PB_GATT                                     0x2

/// Length of the display name buffer
#define NAME_BUF_LEN                   20

/// Length of device's uuid
#define BLE_MESH_UUID_LEN_BYTE (16)

#ifdef SL_CATALOG_BTMESH_WSTK_LCD_PRESENT
#include "sl_btmesh_wstk_lcd.h"
#endif // SL_CATALOG_BTMESH_WSTK_LCD_PRESENT

#ifdef SL_CATALOG_BTMESH_WSTK_LCD_PRESENT
#define lcd_print(...) sl_btmesh_LCD_write(__VA_ARGS__)
#else
#define lcd_print(...)
#endif // SL_CATALOG_BTMESH_WSTK_LCD_PRESENT

my_model_t my_model = {
  .elem_index = PRIMARY_ELEMENT,
  .vendor_id = VENDOR_ID,
  .model_id = MY_VENDOR_SERVER_ID,
  .publish = 1,
  .opcodes_len = NUMBER_OF_OPCODES,
  .opcodes_data[0] = sensor_status
};
static uint8_t cache_data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static uint16_t my_address = 0;
static uint64_t store_data[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // Storing variable
uint8_t store_state = 0;

static void factory_reset(void);
static void delay_reset_ms(uint32_t ms);
static void initialize_server_settings(void);

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  app_log("=================\r\n");
  app_log("Server Device\r\n");
  app_button_press_enable();
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/***************************************************************************//**
 * Set device name in the GATT database. A unique name is generated using
 * the two last bytes from the Bluetooth address of this device. Name is also
 * displayed on the LCD.
 *
 * @param[in] addr  Pointer to Bluetooth address.
 ******************************************************************************/
static void set_device_name(bd_addr *addr)
{
  char name[NAME_BUF_LEN];
  sl_status_t result;

  // Create unique device name using the last two bytes of the Bluetooth address
  snprintf(name, NAME_BUF_LEN, "Server %02x:%02x",
           addr->addr[1], addr->addr[0]);

  app_log("Device name: '%s'\r\n", name);

  result = sl_bt_gatt_server_write_attribute_value(gattdb_device_name,
                                                   0,
                                                   strlen(name),
                                                   (uint8_t *)name);
  if(result) {
    app_log("sl_bt_gatt_server_write_attribute_value() failed, code %lx\r\n", result);
  }

  // Show device name on the LCD
  lcd_print(name, SL_BTMESH_WSTK_LCD_ROW_NAME_CFG_VAL);
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(struct sl_bt_msg *evt)
{
  sl_status_t sc;
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
      // Factory reset the device if Button 0 or 1 is being pressed during reset
      if((sl_simple_button_get_state(&sl_button_btn0) == SL_SIMPLE_BUTTON_PRESSED) || (sl_simple_button_get_state(&sl_button_btn1) == SL_SIMPLE_BUTTON_PRESSED)) {
          factory_reset();
          break;
      }
      // Initialize Mesh stack in Node operation mode,
      // wait for initialized event
      app_log("Node init\r\n");
      sc = sl_btmesh_node_init();

      switch (sc) {
        case 0x00: break;
        case 0x02: app_log("Node already initialized\r\n"); break;
        default: app_assert_status_f(sc, "Failed to init node\r\n");
      }
      break;

    // -------------------------------
    // Handle Button Presses
    case sl_bt_evt_system_external_signal_id: {
    }
    break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

/**************************************************************************//**
 * Bluetooth Mesh stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth Mesh stack.
 *****************************************************************************/
void sl_btmesh_on_event(sl_btmesh_msg_t *evt)
{
  sl_status_t sc;
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_btmesh_evt_node_initialized_id:
      app_log("Node initialized ...\r\n");
      sc = sl_btmesh_vendor_model_init(my_model.elem_index,
                                       my_model.vendor_id,
                                       my_model.model_id,
                                       my_model.publish,
                                       my_model.opcodes_len,
                                       my_model.opcodes_data);
      app_assert_status_f(sc, "Failed to initialize vendor model\r\n");
      bd_addr address;
      uint8_t address_type;
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status_f(sc, "Failed to get Bluetooth address\r\n");
      set_device_name(&address);

      if(evt->data.evt_node_initialized.provisioned) {
        app_log("Node already provisioned.\r\n");
        initialize_server_settings();
        lcd_print("Node ready", SL_BTMESH_WSTK_LCD_ROW_STATUS_CFG_VAL);
      } else {
        app_log("Node unprovisioned\r\n");
        // Start unprovisioned Beaconing using PB-ADV and PB-GATT Bearers
        app_log("Send unprovisioned beacons.\r\n");
        lcd_print("Node unprovisioned", SL_BTMESH_WSTK_LCD_ROW_STATUS_CFG_VAL);
//        sc = sl_btmesh_node_start_unprov_beaconing(PB_ADV | PB_GATT);
//        app_assert_status_f(sc, "Failed to start unprovisioned beaconing\r\n");
      }
      break;

    // -------------------------------
    // Provisioning Events
    case sl_btmesh_evt_node_provisioned_id:
      app_log("Provisioning done. Address: 0x%04x, IV Index: 0x%lx\r\n",
              evt->data.evt_node_provisioned.address,
              evt->data.evt_node_provisioned.iv_index);
      initialize_server_settings();
      lcd_print("Provisioning done", SL_BTMESH_WSTK_LCD_ROW_STATUS_CFG_VAL);
      break;

    case sl_btmesh_evt_node_provisioning_failed_id:
      app_log("Provisioning failed. Result = 0x%04x\r\n",
              evt->data.evt_node_provisioning_failed.result);
      lcd_print("Provisioning failed", SL_BTMESH_WSTK_LCD_ROW_STATUS_CFG_VAL);
      break;

    case sl_btmesh_evt_node_provisioning_started_id:
      app_log("Provisioning started.\r\n");
      lcd_print("Provisioning...", SL_BTMESH_WSTK_LCD_ROW_STATUS_CFG_VAL);
      break;

    case sl_btmesh_evt_node_key_added_id:
      app_log("Got new %s key with index %x\r\n",
              evt->data.evt_node_key_added.type == 0 ? "network " : "application ",
              evt->data.evt_node_key_added.index);
      break;

    case sl_btmesh_evt_node_config_set_id:
      app_log("evt_node_config_set_id\r\n\t");
      break;

    case sl_btmesh_evt_node_model_config_changed_id:
      app_log("Model config changed, type: %d, elem_addr: %x, model_id: %x, vendor_id: %x\r\n",
              evt->data.evt_node_model_config_changed.node_config_state,
              evt->data.evt_node_model_config_changed.element_address,
              evt->data.evt_node_model_config_changed.model_id,
              evt->data.evt_node_model_config_changed.vendor_id);
      break;

    // -------------------------------
    // Handle vendor model messages
    case sl_btmesh_evt_vendor_model_receive_id: {
      sl_btmesh_evt_vendor_model_receive_t *rx_evt = (sl_btmesh_evt_vendor_model_receive_t *)&evt->data;
        // Check if payload is duplicate
        bool is_duplicate = true;
        if (rx_evt->payload.len == 8) {
            for (int i = 0; i < 8; i++) {
                if (rx_evt->payload.data[i] != cache_data[i]) {
                    is_duplicate = false;
                    break;
                }
            }
        } else {
            is_duplicate = false;
        }

        if (is_duplicate) {
            app_log("Duplicate payload detected, skipping processing.\r\n");
            break;
        }

        // Update cache with new payload
        if (rx_evt->payload.len == 8) {
            for (int i = 0; i < 8; i++) {
                cache_data[i] = rx_evt->payload.data[i];
            }
        }
        // Store data
        uint64_t data_value = 0;
        for (int i = 0; i < 8; i++) {
            data_value = (data_value << 8) | cache_data[i];
        }
        store_data[store_state] = data_value;
        if (store_state < 7) store_state++;
        else store_state = 0;
        app_log("New data stored.\r\n");

        app_log("Vendor model data received.\r\n\t"
                "Element index = %d\r\n\t"
                "Vendor id = 0x%04X\r\n\t"
                "Model id = 0x%04X\r\n\t"
                "Source address = 0x%04X\r\n\t"
                "Destination address = 0x%04X\r\n\t"
                "Destination label UUID index = 0x%02X\r\n\t"
                "App key index = 0x%04X\r\n\t"
                "Non-relayed = 0x%02X\r\n\t"
                "Opcode = 0x%02X\r\n\t"
                "Final = 0x%04X\r\n\t"
                "Payload: ",
                rx_evt->elem_index,
                rx_evt->vendor_id,
                rx_evt->model_id,
                rx_evt->source_address,
                rx_evt->destination_address,
                rx_evt->va_index,
                rx_evt->appkey_index,
                rx_evt->nonrelayed,
                rx_evt->opcode,
                rx_evt->final);
        for(int i = 0; i < rx_evt->payload.len; i++) {
            app_log("%x ", rx_evt->payload.data[i]);
        }
        app_log("\r\n");

        switch (rx_evt->opcode) {
          case sensor_status:
            int32_t temperature = 0;
            uint32_t humidity = 0;
            for (int8_t i = 7; i >= 4; i--) {
                uint8_t temp = rx_evt->payload.data[i];
                temperature = (temperature << 8) | temp;
            }
            for (int8_t i = 3; i >= 0; i--) {
                uint8_t temp = rx_evt->payload.data[i];
                humidity = (humidity << 8) | temp;
            }
            app_log("Temperature = %ld.%1ld Celsius\r\n",
                    temperature / 1000,
                    temperature % 1000);

            float temp = (float) (temperature / 1000);
            temp = temp * 1.8 + 32;
            temperature = (int32_t) (temp * 1000);
            app_log("Temperature = %ld.%1ld Fahrenheit\r\n",
                    temperature / 1000,
                    temperature % 1000);

            app_log("Humidity = %ld %%\r\n",
                    humidity / 1000);
            break;

          default:
            break;
        }
      break;
    }

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

/// Reset
static void factory_reset(void)
{
  app_log("factory reset\r\n");
  sl_btmesh_node_reset();
  delay_reset_ms(100);
}

static void app_reset_timer_cb(app_timer_t *handle, void *data)
{
  (void)handle;
  (void)data;
  sl_bt_system_reboot();
}

static app_timer_t app_reset_timer;
static void delay_reset_ms(uint32_t ms)
{
  if(ms < 10) {
      ms = 10;
  }
  app_timer_start(&app_reset_timer,
                  ms,
                  app_reset_timer_cb,
                  NULL,
                  false);
}

/**************************************************************************//**
 * Initialize server settings for the node.
 * This function is called both for newly provisioned nodes and already provisioned nodes.
 *****************************************************************************/
static void initialize_server_settings(void)
{
  sl_status_t sc;
  
  app_log("Setting up server functionality...\r\n");
  
  // Enable relay functionality
  sc = sl_btmesh_test_set_relay(1, 0, 0);
  app_assert_status_f(sc, "Failed to set relay\r\n");
  app_log("Relay enabled\r\n");
  
  // Set network transmission state
  sc = sl_btmesh_test_set_nettx(0, 0);
  app_assert_status_f(sc, "Failed to set network tx state\r\n");
  app_log("Network tx state set\r\n");
  
  // If my_address is not set yet (for already provisioned nodes), get it
  if (my_address == 0) {
    uint16_t node_address;
    sc = sl_btmesh_node_get_element_address(my_model.elem_index, &node_address);
    if (sc == SL_STATUS_OK) {
      my_address = node_address;
      app_log("Got node address: 0x%04x\r\n", my_address);
    } else {
      app_log("Failed to get node address, error: 0x%lx\r\n", sc);
    }
  }

  app_log("Server initialization complete\r\n");
}
