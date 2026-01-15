# Smart NPK Sensor System (ESP32 + TFT Dashboard)

> **A fully automated, data-centric soil monitoring system designed for real-world agriculture, research, and ML-ready datasets.**  
Engineered with robustness, visual feedback, and long-term data intelligence in mind.

![overview](visuals\01_overview.png)

![overview](visuals\02_building_smart_system.png)

---

## Project Overview
This project is a **smart NPK (Nitrogen–Phosphorus–Potassium) soil monitoring system** built around an **ESP32** with a **dedicated TFT dashboard**.

It continuously:
- Reads **soil NPK, temperature, moisture, pH, and conductivity**
- Displays live, color-coded data on-screen
- Logs **system status, warnings, and errors visually**
- Uploads structured data to **Google Sheets** for long-term storage
- Automatically **reconnects WiFi** and resumes uploads
- Fetches configuration from **Google Apps Script at every boot**

The result is a **self-healing, ML/DL-ready data pipeline** with strong human-readable feedback.

![overview](visuals\03_smart_npk_sensor_system.png)
---

##  Key Features

###  Fully Autonomous Boot Flow
- Fetches **latest configuration from Google Apps Script** on every boot
- No hardcoded parameters required
- Remote tuning without reflashing firmware

### Real-Time Sensor Acquisition
- Nitrogen (N)
- Phosphorus (P)
- Potassium (K)
- Soil temperature
- Soil moisture
- Soil pH
- Soil electrical conductivity

All readings are timestamped using **NTP-synchronized RTC logic**.

---

###  Advanced TFT Dashboard (Core Highlight)
The screen is **not cosmetic**—it is a **system observability layer**.

Displays:
-  Time & Date (NTP synced)
- WiFi status (connected / disconnected / reconnecting)
- Live sensor values with bar graphs & gauges
- Errors, warnings, success logs
- Data upload confirmation
- System state awareness at a glance

This makes the device **field-debuggable without a laptop**.

![Dashboard](images\07_data_uploaded.png)
---

###  Intelligent Power Management (Deep Sleep)

- Device automatically enters **ESP32 deep sleep** outside configured working hours
- Working hours are **fetched dynamically from Google Apps Script** at every boot
- No firmware change required to update active time window
- Ensures:
  -  Massive power savings for field deployment
  -  Operation only during meaningful sampling periods
  -  Zero energy waste during inactive hours

  ![Deepsleep Ui](images\08_sleep.png)

**Wake-up logic:**
- Wakes automatically at next valid working window
- Resynchronizes time via NTP
- Refetches configuration
- Resumes sensing, display, and uploads seamlessly

This makes the system ideal for **battery-powered & long-term outdoor deployments**.

---

###  Resilient WiFi Handling
- Automatic WiFi reconnect if signal drops
- Visual indication on screen when:
  - WiFi disconnects mid-operation
  - Reconnection attempts are ongoing
  - Connection is restored
- Upload resumes automatically

No data loss, no silent failures.


 ![Wifi Ui](visuals\04_wifi.png)

---

###  Google Sheets Data Pipeline
- Sensor data uploaded periodically to Google Sheets
- Structured, clean tabular format
- Time-indexed entries
- Ideal for:
  - Machine Learning (ML)
  - Deep Learning (DL)
  - Soil health analytics
  - Predictive agriculture models

This turns the device into a **data generator, not just a sensor**.

---

###  ML / DL Ready by Design
The dataset generated is:
- Consistent
- Timestamped
- Labeled
- Noise-aware (via logs & status)

Perfect for:
- Crop recommendation models
- Fertilizer optimization
- Soil degradation prediction
- Anomaly detection

##### Sample Dataset (Pipeline Test)
Automatically logged, structured data stored in Google Sheets for downstream ML/DL workflows. ( Caution : Not original sensor data / random values )  

 ![Sample csvsheet](visuals\05_sample_csvsheet.png)


##### Live Field Dataset (Production Logging)
Real-time soil sensor data continuously logged and synchronized to Google Sheets during scheduled working hours.

 ![Live Field csvsheet](visuals\06_field_csvsheet.png)

---

##  System Architecture

```
Soil Sensors
    ↓
ESP32 (Sensor + Logic Layer)
    ↓            ↘
TFT Dashboard   WiFi Manager
                    ↓
             Google Apps Script
                    ↓
              Google Sheets
```

---

##  Hardware Used
- ESP32 Dev Module
- NPK Soil Sensor (RS485 / UART)
- TFT Display (MCUFRIEND / ILI-based)
- Soil Moisture Sensor
- Temperature Sensor
- Isolated power & signal conditioning
- Custom acrylic enclosure

---

##  Software Stack
- Arduino Framework (ESP32)
- Google Apps Script (config + ingestion)
- Google Sheets (dataset storage)

---

## Repository Structure 

```
📦 smart-npk-sensor
 ┣ 📂 docs
 ┃ ┗ google_apps_script.md
 ┣ 📂 firmware
 ┃ ┣ esp32_soil_npk_cloud_logger.ino
 ┃ ┗ soil_monitor_display.ino
 ┣ 📂 google_apps_script
 ┃ ┣ NPK_data_logger.gs
 ┃ ┗ sensor_config.gs
 ┣ 📂 images
 ┣ 📂 working_videos
 ┣ 📂 visuals 
 ┣ LICENSE
 ┗ README.md
```

---

## Logging Philosophy
Logs are treated as **first-class system signals**:
- Success 
- Warning 
- Error 

Shown:
- On TFT (human-readable)
- In serial logs
- Implicitly in dataset integrity

---

## Real-World Use Cases
- Smart farming
- Precision agriculture
- Research labs
- AI/ML dataset generation
- Educational embedded systems

---

## Future Enhancements
- Edge ML on ESP32
- Fertilizer recommendation engine
- Mobile dashboard
- Cloud ML integration
- Multi-node deployment

 ![Applications](visuals\07_applications.png)


---

##  Author
**Tanishq C. Saha**  
Electronics & Computer Science | Embedded Systems | AI/ML

---

##  Final Note
This project is **not just a sensor**.
It is a **self-aware, data-driven embedded system** built for scalability, intelligence, and real-world reliability.

