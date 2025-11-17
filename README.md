# Domestic Water Wastage Prevention System

An IoT-based solution for intelligent water management with real-time monitoring, automated control, and cloud integration. The system operates seamlessly in both online and offline modes while maintaining data integrity.

---

🎥 **[Watch Project Demo Video](https://drive.google.com/file/d/1bc1kWJXzAJOv_yFu3mCMb4uNGVxG7l2v/view?usp=sharing)**  
🌐 **[Goto Live Website](https://dwwp-20-git-main-akash-beras-projects-53418e47.vercel.app/)**

---

## Table of Contents
- [Key Features](#key-features)
- [System Architecture](#system-architecture)
- [Core Technologies](#core-technologies)
- [Hardware Components](#hardware-components)
- [Firebase Data Structure](#firebase-data-structure)
- [Installation & Setup](#installation--setup)
- [Operational Workflow](#operational-workflow)
- [License](#license)
- [Contributors](#contributors)

---

## Key Features

- **Role-Based Access Control**  
  **Admin**:  
  - Configure system-wide water limits and pricing  
  - Monitor all user activities and usage patterns  
  - Manage admin privileges and system settings  
  - Force emergency shutdown of water supply  
  
  **Users**:  
  - View real-time consumption metrics  
  - Receive personalized alerts and notifications  
  - Recharge water limits through integrated payments  
  - Access historical usage data  

- **Hybrid Operation Mode**  
  Seamless transition between online/offline modes with local data caching
- **Automated Water Control**  
  Servo-actuated valve management based on usage thresholds
- **Time-Synchronized Logging**  
  NTP server synchronization for accurate daily record-keeping
- **Smart Notifications**  
  **OLED Display**: Real-time system status and alerts  
  Audio/visual alerts for usage limits and system status
- **Payment Integration**  
  Razorpay-powered recharge system with transaction tracking

---


## System Architecture

### Hardware Subsystem
- **Sensing Layer**: Flow sensors for real-time water monitoring
- **Control Layer**: ESP32-based actuator management
- **Visual Interface**: 0.96" OLED display for status updates
- **Power Management**: Battery backup with TP4056 protection
- **User Interface**: Audio alerts via DF Mini MP3 module

---

## Hardware Components

| Component               | Specification                  |
|-------------------------|--------------------------------|
| Microcontroller         | ESP32-WROOM-32D               |
| Flow Sensor             | YF-S201 Hall Effect Sensor    |
| Display Module          | SSD1306 0.96" OLED            |
| Actuator                | SG90 Servo Motor              |
| Audio Module            | DF Mini MP3 Player            |
| Power Management        | TP4056 + 1200mAh Li-ion       |
| Storage                 | MicroSD Card (16GB)           |

---

### Software Subsystem
- **Edge Computing**: Local decision-making using Preferences storage
- **Cloud Integration**: Firebase Firestore for centralized data management
- **Time Synchronization**: NTP client for accurate timestamping
- **Dashboard**: React-based visualization and control interface

---

## Core Technologies

- **Frontend**: React 18 + Chart.js + Vite
- **Backend**: Firebase Firestore + Authentication
- **IoT Platform**: ESP32 (Arduino Core)
- **Payment Gateway**: Razorpay API
- **Time Service**: NTP Client Library


    

---

## Firebase Data Structure

```plaintext
  users/ [Collection]
  └── {userEmail}/ [Document]
      ├── servoState: boolean
      ├── lastSeen: timestamp
      ├── notification: string
      ├── userDetails: <map> {
      │   ├── fullName: string
      │   ├── mobileNo: string
      │   ├── emailId: string (optional)
      │   ├── address: string
      │   ├── accountNumber: string
      │   ├── consumerNumber: string
      │   ├── meterNumber: string
      │   └── supplyZone: string
      │   }
      ├── wifi_pass: string
      ├── wifi_ssid: string
      └── monthlyUsages/ [Subcollection]
          └── {YYYY-MM}/ [Document]
              ├── {YYYY-MM-DD}: number (daily water usage in liters)
              ├── {YYYY-MM-DD}: number
              ├── limit: number (user's monthly limit)
              ├── isMonthFinish: boolean
              ├── limitExceeded: boolean
              │
              ├── payment/ [Subcollection]
              │   └── payment_details/ [Document]
              │       ├── amount: number
              │       ├── date: timestamp (ISO string)
              │       ├── forMonth: string (YYYY-MM)
              │       ├── razor_pay_id: string
              │       ├── status: string ("pending" | "Completed")
              │       └── timeStamp: timestamp (ISO string)
              │
              └── addon/ [Subcollection]
                  └── {addon_id}/ [Document] (auto-generated or razor_pay_id)
                      ├── addon_date: timestamp (ISO string)
                      ├── amount: number
                      ├── quantityDone: number (liters purchased)
                      ├── refill: number (how many times refilled)
                      ├── razor_pay_id: string
                      └── status: string (always "Completed")

  admin/ [Collection]
  └── {adminDoc}/ [Document]
   ├── 01ListOfAdmin: array
   │ ├── 0: "admin@gmail.com"
   │ └── 1: "admin2@gmail.com"
   │
   ├── broadcast: map {
   │ ├── msg: array [
   │ │ 0: map {
   │ │   icon: "💳"
   │ │   message: "Friendly reminder: Your water bill payment is due in 3 days."
   │ │   timestamp: "2025-04-07T19:13:13.088Z"
   │ │   }
   │ │ ]
   │ └── timestamp: "2025-04-08T00:46:29.000Z" (April 8, 2025 at 12:46:29 AM UTC+5:30)
   │ }
   ├── limit: map {
   │ ├── max: number
   │ ├── penalty: number
   │ └── regular: number
   │ }
   │
   ├── price: map {
   │ ├── penaltyPrice: number
   │ └── regularPrice: number
   │ }
   │

  smsQueue/ [Collection]
    └── sms_ab_202511091059382/ [Document]  
        ├── attempts: 0
        ├── createdAt: "2025-11-09T10:59:38.258Z"
        ├── message: "Dear Akash Bera, addon of 80L purchased for Rs.50. Payment ID: pay_Rdck9DTiXfX5vm. Your water supply continues uninterrupted."
        ├── messageType: "addon"
        ├── metadata: {
        │     amount: 50,
        │     forMonth: null,
        │     paymentId: "pay_Rdck9DTiXfX5vm",
        │     refillAmount: 80,
        │     mobileNo: "9876543210",
        │     referenceId: "users/ab@gmail.com/monthlyUsages/2025-11/addon/addon_details"
        │ }
        ├── sentAt: "November 9, 2025 at 10:35:06 PM UTC+5:30"
        ├── status: "sent"
        └── userId: "ab@gmail.com"


```

---

## Installation & Setup

1. Clone repository:
```bash
git clone https://github.com/your-username/water-management-system.git
cd water-management-system
```

2. Configure Firebase:
```javascript
// src/firebase/config.js
const firebaseConfig = {
  apiKey: "YOUR_API_KEY",
  authDomain: "PROJECT_ID.firebaseapp.com",
  projectId: "PROJECT_ID",
  storageBucket: "PROJECT_ID.appspot.com",
  messagingSenderId: "SENDER_ID",
  appId: "APP_ID"
};
```

3. Install dependencies:
```bash
npm install
cd functions && npm install
```

---

## Operational Workflow

### Hardware Operation
1. **Data Acquisition**  
   - Flow sensors capture real-time usage data
   - ESP32 processes readings at 1Hz sampling rate

2. **Edge Processing**  
   - Local storage maintains last-known states (servo position/usage limits)
   - Offline mode uses cached values when internet unavailable

3. **Time Synchronization**  
   - Daily NTP server sync ensures accurate timestamps
   - Midnight flush of daily usage to Firestore

### Software Workflow
1. **State Management**  
![Alt Text](image.png)

   F -->|No| H[Maintain State]
   ```

2. **Data Synchronization**  
   - Periodic Firebase sync (15min intervals)
   - Conflict resolution prioritizes cloud data

3. **Payment Handling**  
   - Razorpay integration triggers Firestore updates
   - Successful payments increment usage limits

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---

## Contributors

- Akash Bera ([LinkedIn](https://linkedin.com/in/akash-bera))
- Subhayan Kapas ([LinkedIn](https://linkedin.com/in/subhayan-kapas))

---


pub sub sytstem for the message system (we have to add this in the readme file )

Classic Pub/Sub Pattern:
Publisher → Message Queue → Subscriber(s)
   ↓             ↓              ↓
Payment      smsQueue       ESP32
System       (Firebase)     (Worker)

Your System:
Payment Event → smsQueue Collection → ESP32 reads & sends SMS
   (Publisher)     (Message Queue)        (Subscriber/Consumer)

