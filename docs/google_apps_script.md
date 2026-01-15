# Google Apps Script Integration

This project uses **Google Apps Script (GAS)** as a lightweight backend for device configuration management and cloud-based data ingestion. It enables remote control of device behavior and structured dataset storage without requiring a dedicated server.

---

## Purpose of Google Apps Script

Google Apps Script is used to:

- Provide runtime configuration to the ESP32 (working hours, intervals, thresholds)
- Receive and store sensor data in Google Sheets
- Act as a stateless HTTP endpoint for embedded devices
- Enable remote updates without firmware reflashing

---

## Script Responsibilities

The system is divided into two logical scripts:

### 1. Configuration Service (`sensor_config.gs`)

- Exposes an HTTP endpoint for configuration requests
- Returns device parameters such as:
  - Working hours
  - Sampling intervals
  - Feature flags
- Allows behavior changes without modifying firmware
- Configuration is fetched by the ESP32 at every boot

### 2. Data Ingestion Service (`soil_data_logger.gs`)

- Accepts HTTP POST requests from the ESP32
- Validates incoming sensor payloads
- Appends structured, timestamped rows to Google Sheets
- Ensures consistent schema for long-term data collection

---

## Setup Procedure

1. Create a new Google Apps Script project
2. Add the required `.gs` files
3. Link the script to a Google Sheet
4. Deploy the project as a Web App  
   - Execution as: **Me**  
   - Access: **Anyone with the link**
5. Copy the generated Web App URL
6. Configure the ESP32 firmware to use this endpoint

---

## Data Flow Overview

1. ESP32 boots and synchronizes time via NTP
2. Configuration is fetched from the Apps Script endpoint
3. Device operates only within configured working hours
4. Sensor data is collected and packaged
5. Data is uploaded to Google Sheets via HTTP
6. ESP32 enters deep sleep outside active hours

---

## Advantages of This Approach

- No dedicated cloud server required
- High reliability using Google’s infrastructure
- Easy inspection and export of datasets
- Scales well for low-to-medium device counts
- Suitable for research, prototyping, and ML dataset generation

---

## Notes on Security and Scaling

- For production use, additional validation or authentication is recommended
- Request rates should be controlled to avoid quota limits
- Schema consistency is critical for downstream ML workflows
