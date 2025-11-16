# IoT_BLEMESH
Below is a **clean, concise, Markdown-formatted English description** of **Lab 6: BLE-Mesh** based strictly on the provided PDF.
Source: 

---

# **Lab 6 – Bluetooth Low Energy Mesh (BLE-Mesh)**

## **1. Objectives**

* Build a complete BLE-Mesh system.
* Use Silicon Labs Bluetooth Mesh API.
* Practice with three roles: **Provisioner**, **Client**, **Server** (optional Relay).

**Hardware**: EFR32xG21 BRD4180B + WSTK BRD4001A
**Software**: Simplicity Studio 5 + Simplicity SDK Suite 2024.6.0

---

# **2. BLE-Mesh Overview**

## **2.1 What is BLE-Mesh?**

BLE-Mesh is a many-to-many communication protocol built on top of Bluetooth Low Energy.
It supports:

* **Wide-area device networks**
* **Relay-based packet forwarding**
* **Low-power nodes**
* **Vendor-specific models**

A Mesh system consists of two main device types:

### **Unprovisioned Device**

* Broadcasts its identity.
* Not yet part of the mesh.

### **Provisioner**

* Adds devices to the mesh network.
* Assigns unicast address, network keys, application keys.
* Configures each node (Server, Client, Relay, LPN, Proxy).

Once provisioned, devices can take specialized roles:

* **Relay Node** – forwards packets.
* **Proxy Node** – allows GATT-based mesh interaction.
* **Friend / LPN** – low-power pairing.
* **Vendor Model Nodes** – custom message types.

---

# **3. Lab Architecture**

The lab requires **three independent projects**:

1. **Vendor Server** – receives sensor data.
2. **Vendor Client** – reads sensor data and publishes it.
3. **Provisioner** – provisions and configures both Server and Client.

(Optional) a **Relay** device can be added.

Each project starts from **“Bluetooth Mesh – SoC Empty”** template, after which you:

* Install dependencies (Log, Vendor Model, Test, Buttons, WSTK LCD …)
* Configure Vendor Model (Model ID, Company ID)
* Add to Mesh Configuration
* Replace project files with those from provided links

---

# **4. Vendor Model IDs**

```c
#define VENDOR_ID              0x1221
#define MY_VENDOR_SERVER_ID    0x1111
#define MY_VENDOR_CLIENT_ID    0x2222
#define MY_VENDOR_RELAY_ID     0x3333
```

Publication and subscription group addresses:

```c
#define CUSTOM_STATUS_GRP_ADDR   0xC001   // Server publish address
#define CUSTOM_CTRL_GRP_ADDR     0xC002   // Server subscribe address
```

Fixed Netkey & Appkey (shared across the mesh network).

---

# **5. Device Roles**

## **5.1 Provisioner**

Functions:

* Performs factory reset
* Scans for unprovisioned nodes
* Assigns unicast addresses
* Distributes netkey/appkey
* Configures publication/subscription for all nodes

**User interaction**

* Hold **Button 0**, then press **Reset** → enter Factory Reset
* Hold **Button 0** to scan for new devices
* Short press **Button 0** to provision the detected device

---

## **5.2 Client**

Role:

* Reads humidity and temperature (mock sensor)
* Publishes sensor data via Vendor Model (opcode 0x01)

User actions:

* **PB0**: read sensor & send immediately
* **PB1**: enable periodic sending

Data layout (8 bytes):

* 4B humidity
* 4B temperature

Publishing uses:

```c
sl_btmesh_vendor_model_set_publication();
sl_btmesh_vendor_model_publish();
```

---

## **5.3 Server**

Role:

* Receives Vendor Model messages.
* Parses sensor data based on opcode `sensor_status = 0x01`.
* Displays Celsius, Fahrenheit, and humidity via **Log** and **LCD**.

Data decoding example:

```c
case sensor_status:
    // decode 4 bytes humidity + 4 bytes temperature
```

---

## **5.4 Relay (optional)**

* Receives Vendor data from Client.
* Republishes the same payload using vendor model publication.
* Extends communication range.

---

# **6. Practical Tasks**

## **Preparation**

1. Study BLE-Mesh architecture.
2. Understand provisioning, publishing, subscribing.

## **Lab Exercises**

### **1. Transmit student IDs**

Each Client sends an encoded student ID.
Server decodes and prints them.

### **2. Custom message: device uptime & LED state**

* **Button 0** → send device uptime
* **Button 1** → send 2-bit LED state

  * LED bits toggle every 5 seconds
  * Server decodes and displays LED status on LCD:

| Bits | LED1 | LED2 |
| ---- | ---- | ---- |
| 00   | OFF  | OFF  |
| 01   | OFF  | ON   |
| 10   | ON   | OFF  |
| 11   | ON   | ON   |

---

# **7. Setup Notes**

If logs or LCD do not work:

* Flash a **Mesh demo** once (`Bluetooth Mesh – SoC Light CTL`)
* Then flash your custom project again
* Same for Bluetooth-only projects (flash "SoC Blinky" first)

---

# **8. Appendix Features**

The appendix describes how to:

* Enable USART logging
* Install Vendor Model & Test frameworks
* Add Button Press utility
* Add Sensor mock library (Client only)
* Install WSTK LCD driver
* Add Factory Reset (Provisioner only)
* Add Provisioner & Configuration Client roles
* Configure mesh stack parameters
* Add models to the Main element through Mesh Configurator
* Flash recovery demo projects (Mesh/Bluetooth)

---

If you want, I can also create:

* A **one-page summary**,
* A **full English lab report template**,
* A **diagram of the mesh system**,
* Simplified **flowcharts for Client/Server/Provisioner**.

Just tell me what you need.
