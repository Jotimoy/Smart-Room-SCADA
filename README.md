# Smart Room SCADA Controller

An IoT-based embedded room monitoring and automation system built using ESP8266 (NodeMCU).

## 🔧 Hardware Used
- ESP8266 NodeMCU
- BMP280 Temperature & Pressure Sensor
- DS3231 RTC Module
- 16x2 I2C LCD
- Relay Modules (Fan, Light, Lamp)

## 🌐 Network Configuration
- Static IP: 192.168.0.140
- Local Web Server Architecture
- No Cloud Dependency

## 🚀 Features

### 📊 Monitoring
- Real-time Temperature & Pressure
- RTC-based Time & Date
- LCD Display (12-hour format + Day + Date)
- SCADA-style Web Dashboard
- Temperature & Pressure Graph
- Heap & WiFi RSSI Graph

### 🤖 Automation
- Automatic Fan Control (Temperature Threshold)
- RTC-based Fan Scheduling
- Manual Device Control via Web

### 🧠 System Diagnostics
- Free Heap Monitoring
- WiFi Signal Strength (RSSI)
- CPU Frequency
- Flash Memory
- Uptime Tracking
- Alert System for Critical Conditions

### 💾 Data Logging
- CSV Data Export

---

## 🖥 Dashboard Preview

![Dashboard](images/dashboard.png)

---

## 📂 Project Structure

- `Smart_Room_SCADA.ino` → Main firmware
- `images/` → Project screenshots

---

## ⚙ Technologies Used
- C++
- ESP8266WebServer
- HTML / CSS / JavaScript
- Chart.js
- REST-style API endpoints

---

## 📈 Future Improvements
- Multi-room expansion
- Cloud integration
- Mobile App control
- OTA firmware update

---

## 👨‍💻 Author
Jotirmoy Mollick  
CSE Student | Robotics & IoT Enthusiast
