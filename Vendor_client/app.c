/***************************************************************************//**
 * @file app.c
 * @brief Core application logic for the vendor client node.
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "sl_sensor_rht.h"

#include "em_cmu.h"
#include "em_gpio.h"

#include "my_model_def.h"

#include "app_button_press.h"
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"

#define EX_B0_PRESS                                 ((1) << 5)
#define EX_B0_LONG_PRESS                            ((1) << 6)
#define EX_B1_PRESS                                 ((1) << 7)
#define EX_B1_LONG_PRESS                            ((1) << 8)

// Timing
// Check section 4.2.2.2 of Mesh Profile Specification 1.0 for format
#define STEP_RES_100_MILLI                          0
#define STEP_RES_1_SEC                              ((1) << 6)
#define STEP_RES_10_SEC                             ((2) << 6)
#define STEP_RES_10_MIN                             ((3) << 6)

#define STEP_RES_BIT_MASK                           0xC0

// Max x is 63
#define SET_100_MILLI(x)                            (uint8_t)(STEP_RES_100_MILLI | ((x) & (0x3F)))
#define SET_1_SEC(x)                                (uint8_t)(STEP_RES_1_SEC | ((x) & (0x3F)))
#define SET_10_SEC(x)                               (uint8_t)(STEP_RES_10_SEC | ((x) & (0x3F)))
#define SET_10_MIN(x)                               (uint8_t)(STEP_RES_10_MIN | ((x) & (0x3F)))

/// Advertising Provisioning Bearer
#define PB_ADV                                      0x1
/// GATT Provisioning Bearer
#define PB_GATT                                     0x2

// Used button indexes
#define BUTTON_PRESS_BUTTON_0                       0
#define BUTTON_PRESS_BUTTON_1                       1

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

static uint8_t temperature[4] = {0, 0, 0, 0};
static uint8_t humidity[4] = {0, 0, 0, 0};
static uint8_t sensor_data[8];
static uint16_t my_address = 0;

static uint32_t periodic_timer_ms = 0;
bool select_update_mode = false;

static uint8_t period_idx = 0;
static uint8_t periods[] = {
  SET_100_MILLI(10),        /* 1s */
  SET_1_SEC(10),           /* 10s   */
  SET_10_SEC(6),          /* 1min  */
  SET_10_MIN(1),           /* 10min */
  0
};

static my_model_t my_model = {
  .elem_index = PRIMARY_ELEMENT,
  .vendor_id = VENDOR_ID,
  .model_id = MY_VENDOR_CLIENT_ID,
  .publish = 1,
  .opcodes_len = 1,
  .opcodes_data[0] = sensor_status
};

static void factory_reset(void);
static void read_sensor_data(void);
static void setup_periodcal_update(uint8_t interval);
static void delay_reset_ms(uint32_t ms);
static void choose_period(uint8_t update_interval);
static void initialize_client_settings(void);
void app_button_press_select_period_update_cb(uint8_t button, uint8_t duration);

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  app_log("=================\r\n");
  app_log("Client Device\r\n");
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
  snprintf(name, NAME_BUF_LEN, "Client %02x:%02x",
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
      uint8_t *data = NULL;
      // check if external signal triggered by button 0 press
      if(evt->data.evt_system_external_signal.extsignals & EX_B0_PRESS) {
          read_sensor_data();
          data = sensor_data;
          app_log("B0 Pressed. Data is sent once.\r\n");
          // set the vendor model publication message
          sc = sl_btmesh_vendor_model_set_publication(my_model.elem_index,
                                                      my_model.vendor_id,
                                                      my_model.model_id,
                                                      my_model.opcodes_data[0],
                                                      1,
                                                      DATA_LENGTH,
                                                      data);
          if(sc != SL_STATUS_OK) {
              app_log("Set publication error: 0x%04lX\r\n", sc);
          } else {
              app_log("Set publication done. Publishing...\r\n");
              // publish the vendor model publication message
              sc = sl_btmesh_vendor_model_publish(my_model.elem_index,
                                                  my_model.vendor_id,
                                                  my_model.model_id);
              if(sc != SL_STATUS_OK) {
                  app_log("Publish error: 0x%04lX\r\n", sc);
              } else {
                  app_log("Publish done.\r\n");
              }
          }
      }
      // check if external signal triggered by button 1 press
      if(evt->data.evt_system_external_signal.extsignals & EX_B1_PRESS) {
          read_sensor_data();
          select_update_mode = true;
          period_idx = 0;
          choose_period(period_idx);
      }
      break;
    }

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
        initialize_client_settings();
        lcd_print("Node ready", SL_BTMESH_WSTK_LCD_ROW_STATUS_CFG_VAL);
      } else {
        app_log("Node unprovisioned\r\n");
        // Start unprovisioned Beaconing using PB-ADV and PB-GATT Bearers (done automatically now)
        app_log("Send unprovisioned beacons.\r\n");
        lcd_print("Node unprovisioned", SL_BTMESH_WSTK_LCD_ROW_STATUS_CFG_VAL);
      }
      break;

    // -------------------------------
    // Provisioning Events
    case sl_btmesh_evt_node_provisioned_id:
      app_log("Provisioning done. Address: 0x%04x, IV Index: 0x%lx\r\n",
              evt->data.evt_node_provisioned.address,
              evt->data.evt_node_provisioned.iv_index);
      initialize_client_settings();
      lcd_print("Provisioning done.", SL_BTMESH_WSTK_LCD_ROW_STATUS_CFG_VAL);
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
              evt->data.evt_node_key_added.type == 0 ? "Network" : "Application",
              evt->data.evt_node_key_added.index);
      break;

    case sl_btmesh_evt_node_config_set_id:
      app_log("Evt_node_config_set_id\r\n\t");
      break;

    case sl_btmesh_evt_node_model_config_changed_id:
      app_log("Model config changed, type: %d, elem_addr: %x, model_id: %x, vendor_id: %x\r\n",
              evt->data.evt_node_model_config_changed.node_config_state,
              evt->data.evt_node_model_config_changed.element_address,
              evt->data.evt_node_model_config_changed.model_id,
              evt->data.evt_node_model_config_changed.vendor_id);
      break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

void app_button_press_cb(uint8_t button, uint8_t duration)
{
  if(select_update_mode) {
      app_button_press_select_period_update_cb(button, duration);
      return;
  }
  // Selecting action by duration
  switch (duration) {
    case APP_BUTTON_PRESS_DURATION_SHORT:
      // Handling of button press less than 0.25s
    case APP_BUTTON_PRESS_DURATION_MEDIUM:
      // Handling of button press greater than 0.25s and less than 1s
      if (button == BUTTON_PRESS_BUTTON_0) {
        sl_bt_external_signal(EX_B0_PRESS);
      } else {
        sl_bt_external_signal(EX_B1_PRESS);
      }
      break;
    case APP_BUTTON_PRESS_DURATION_LONG:
      // Handling of button press greater than 1s and less than 5s
      if (button == BUTTON_PRESS_BUTTON_0) {
        sl_bt_external_signal(EX_B0_LONG_PRESS);
      } else {
        sl_bt_external_signal(EX_B1_LONG_PRESS);
      }
      break;
    case APP_BUTTON_PRESS_DURATION_VERYLONG:
      break;
    default:
      break;
  }
}

/// Temperature and Humidity
static void read_sensor_data(void)
{
//  float temp;
  if(sl_sensor_rht_get((uint32_t *)humidity, (int32_t *)temperature) != SL_STATUS_OK) {
    app_log("Error while reading temperature and humidity sensor. Clear the buffer.\r\n");
    memset(temperature, 0, sizeof(temperature));
    memset(humidity, 0, sizeof(humidity));
  }
//  app_log("Temp: %ld.%ld Celsius\r\n", (*(int32_t *)temperature) / 1000,(*(int32_t *)temperature) % 1000);
//  app_log("Hum: %ld %%\r\n", (*(uint32_t *)humidity) / 1000);

  for (int i = 0; i < 4; i ++) {
      sensor_data[i] = humidity[i];
      sensor_data[i + 4] = temperature[i];
  }
}


/// Reset
static void factory_reset(void)
{
  app_log("Factory reset\r\n");
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

/// Update Interval
static void periodic_update_timer_cb(app_timer_t *handle, void *data)
{
  (void)handle;
  (void)data;
  sl_status_t sc;
  app_log("New data update\r\n");
  read_sensor_data();
  sc = sl_btmesh_vendor_model_set_publication(my_model.elem_index,
                                              my_model.vendor_id,
                                              my_model.model_id,
                                              my_model.opcodes_data[0],
                                              1,
                                              DATA_LENGTH,
                                              sensor_data);
  if(sc != SL_STATUS_OK) {
    app_log("Set publication error: 0x%04lX\r\n", sc);
  } else {
    app_log("Set publication done. Publishing...\r\n");
    sc = sl_btmesh_vendor_model_publish(my_model.elem_index,
                                        my_model.vendor_id,
                                        my_model.model_id);
    if (sc != SL_STATUS_OK) {
      app_log("Publish error: 0x%04lX\r\n", sc);
    } else {
      app_log("Publish done.\r\n");
    }
  }
}

static void parse_period(uint8_t interval)
{
  switch (interval & STEP_RES_BIT_MASK) {
    case STEP_RES_100_MILLI:
      periodic_timer_ms = 100 * (interval & (~STEP_RES_BIT_MASK));
      break;
    case STEP_RES_1_SEC:
      periodic_timer_ms = 1000 * (interval & (~STEP_RES_BIT_MASK));
      break;
    case STEP_RES_10_SEC:
      periodic_timer_ms = 10000 * (interval & (~STEP_RES_BIT_MASK));
      break;
    case STEP_RES_10_MIN:
      // 10 min = 600000ms
      periodic_timer_ms = 600000 * (interval & (~STEP_RES_BIT_MASK));
      break;
    default:
      // Set to 0 for "No update" case or invalid interval
      periodic_timer_ms = 0;
      break;
  }
}

static app_timer_t periodic_update_timer;
static void setup_periodcal_update(uint8_t interval)
{
  // Stop existing timer first
  app_timer_stop(&periodic_update_timer);
  
  parse_period(interval);
  
  // Only start timer if periodic_timer_ms is not 0
  if (periodic_timer_ms > 0) {
    app_timer_start(&periodic_update_timer,
                    periodic_timer_ms,
                    periodic_update_timer_cb,
                    NULL,
                    true);
  } else {
    app_log("Periodic update stopped.\r\n");
  }
}
void choose_period(uint8_t choose)
{
  lcd_print("Hold PB0 to choose", 3);
  lcd_print("Choose your period update: ", 4);
  switch (choose) {
    case 0:
      lcd_print("1 second", 5);
      break;
    case 1:
      lcd_print("10 seconds", 5);
      break;
    case 2:
      lcd_print("1 minute", 5);
      break;
    case 3:
      lcd_print("10 minutes", 5);
      break;
    default:
      lcd_print("No update", 5);
      break;
  }
}

void print_update_time(uint8_t choose)
{
  switch (choose) {
    case 0:
      app_log("1s\r\n");
      break;
    case 1:
      app_log("10s\r\n");
      break;
    case 2:
      app_log("1m\r\n");
      break;
    case 3:
      app_log("10m\r\n");
      break;
    default:
      app_log("No update\r\n");
      break;
  }
}

void app_button_press_select_period_update_cb(uint8_t button, uint8_t duration)
{
  // Selecting action by duration
  switch (duration) {
    case APP_BUTTON_PRESS_DURATION_SHORT:
      // Handling of button press less than 0.25s
      if(button == BUTTON_PRESS_BUTTON_0) {
        if(period_idx < sizeof(periods) - 1)
          period_idx++;
        else
          period_idx = 0;
      } else {
        if(period_idx > 0)
          period_idx--;
        else
          period_idx = sizeof(periods) - 1;
      }
      choose_period(period_idx);

      break;
    case APP_BUTTON_PRESS_DURATION_LONG:
      // Handling of button press greater than 1s and less than 5s
      if (button == BUTTON_PRESS_BUTTON_0) {
        select_update_mode = false;
        app_log("Mode %1d selected.\r\n", period_idx);
        app_log("Period update time: ");
        print_update_time(period_idx);
        setup_periodcal_update(periods[period_idx]);
        lcd_print("PB0: Public data", 3);
        lcd_print("PB1: Set period", 4);
        app_log("B1 Pressed. Set periodic update done.\r\n");
      }
      break;
    default:
      break;
  }
}

/**************************************************************************//**
 * Initialize client settings for the node.
 * This function is called both for newly provisioned nodes and already provisioned nodes.
 *****************************************************************************/
static void initialize_client_settings(void)
{
  sl_status_t sc;
  
  app_log("Setting up client functionality...\r\n");
  
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


  app_log("Client initialization complete\r\n");
  lcd_print("PB0: Public data", 3);
  lcd_print("PB1: Set period", 4);
}
