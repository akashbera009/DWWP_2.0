import React from "react";
import { db } from "/src/firebaseConfig.js";
import { collection, doc, setDoc, Timestamp } from "firebase/firestore";
import { getAuth } from "firebase/auth";

const DirectChangeDB = () => {
  const handleCreateUserData = async () => {
    try {
      const auth = getAuth();
      const user = auth.currentUser;

      if (!user) {
        alert("❌ Please log in first.");
        return;
      }

      const userId = user.email; // or user.uid
      const usersRef = collection(db, "users");
      const userDocRef = doc(usersRef, userId);

      // 1️⃣ Create the user document
      await setDoc(userDocRef, {
        servoState: true,
        lastSeen: Timestamp.now(),
        notification: "Welcome back!",
        wifi_ssid: "MyHomeWiFi",
        wifi_pass: "securepassword",
        userDetails: {
          fullName: user.displayName || "John Doe",
          mobileNo: "9876543210",
          emailId: userId,
          address: "123 Main Street",
          accountNumber: "AC0001",
          consumerNumber: "CN0001",
          meterNumber: "MN0001",
          supplyZone: "Zone A",
        },
      });

      // 2️⃣ Create monthlyUsages/{YYYY-MM} document
      const now = new Date();
      const monthId = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, "0")}`;
      const monthlyUsagesRef = collection(userDocRef, "monthlyUsages");
      const monthDocRef = doc(monthlyUsagesRef, monthId);

      // Generate daily usage
      const daysInMonth = new Date(now.getFullYear(), now.getMonth() + 1, 0).getDate();
      const dailyUsage = {};
      for (let day = 1; day <= daysInMonth; day++) {
        const dateKey = `${monthId}-${String(day).padStart(2, "0")}`;
        dailyUsage[dateKey] = Math.floor(Math.random() * 50) + 10; // 10-59 L/day
      }

      await setDoc(monthDocRef, {
        ...dailyUsage,
        limit: 1200,
        isMonthFinish: false,
        limitExceeded: false,
      });

      // 3️⃣ Create payment/payment_details subcollection
      const paymentRef = collection(monthDocRef, "payment");
      const paymentDetailsRef = doc(paymentRef, "payment_details");
      await setDoc(paymentDetailsRef, {
        amount: 500,
        date: Timestamp.fromDate(new Date()),
        forMonth: monthId,
        razor_pay_id: "pay_ABC123",
        status: "Completed",
        timeStamp: Timestamp.fromDate(new Date()),
      });

      // 4️⃣ Create addon/{addon_id} subcollection
      const addonRef = collection(monthDocRef, "addon");
      const addonId = "addon_001";
      const addonDocRef = doc(addonRef, addonId);
      await setDoc(addonDocRef, {
        addon_date: Timestamp.fromDate(new Date()),
        amount: 50,
        quantityDone: 80,
        refill: 1,
        razor_pay_id: "pay_ADDON001",
        status: "Completed",
      });

      alert(`✅ User, monthly usage, payment & addon created successfully!`);
    } catch (error) {
      console.error("❌ Error creating user data:", error);
      alert("❌ Failed to create/update user data. Check console for details.");
    }
  };

  return (
    <button
      onClick={handleCreateUserData}
      style={{
        marginTop: "20px",
        padding: "10px 15px",
        borderRadius: "6px",
        backgroundColor: "#4caf50",
        color: "#fff",
        border: "none",
        cursor: "pointer",
      }}
    >
      Create/Update My User Data
    </button>
  );
};

export default DirectChangeDB;
