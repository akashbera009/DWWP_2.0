// fetchUsers.js
import { collection, getDocs, doc, getDoc } from "firebase/firestore";
import { db } from "../firebaseConfig";

// ----------------------
// Fetch Monthly Usage + Bill Calculation
// ----------------------
const fetchMonthUsage = async (userEmail) => {
  try {
    const now = new Date();
    const yearMonth = `${now.getFullYear()}-${String(
      now.getMonth() + 1
    ).padStart(2, "0")}`;

    const monthDocRef = doc(db, "users", userEmail, "monthlyUsages", yearMonth);
    const monthDocSnap = await getDoc(monthDocRef);

    let totalUsage = 0;
    let paidStatus = false;

    if (monthDocSnap.exists()) {
      const monthData = monthDocSnap.data();

      // 1️⃣ Sum all daily usage
      for (const key in monthData) {
        if (/^\d{4}-\d{2}-\d{2}$/.test(key)) {
          totalUsage += monthData[key] || 0;
        }
      }

      // 2️⃣ Read limit from monthlyUsages
      const regularLimit = Number(monthData.limit || 0);

      // 3️⃣ Read Admin Prices
      const priceSnap = await getDoc(doc(db, "admin", "price"));
      const regularPrice = priceSnap.exists()
        ? Number(priceSnap.data().regularPrice)
        : 0;
      const penaltyPrice = priceSnap.exists()
        ? Number(priceSnap.data().penaltyPrice)
        : 0;

      // 4️⃣ Calculate pricing
      const regularUsage = Math.min(totalUsage, regularLimit);
      const penaltyUsage = Math.max(totalUsage - regularLimit, 0);

      const regularCost = regularUsage * regularPrice;
      const penaltyCost = penaltyUsage * penaltyPrice;

      const totalPrice = regularCost + penaltyCost;

      // 5️⃣ Check payment/payment_details
      const paymentRef = collection(
        db,
        "users",
        userEmail,
        "monthlyUsages",
        yearMonth,
        "payment"
      );
      const paymentDocs = await getDocs(paymentRef);
      paymentDocs.forEach((docSnap) => {
        const payment = docSnap.data();
        if (payment.status === "Completed") paidStatus = true;
      });

      return {
        totalUsage,
        regularUsage,
        penaltyUsage,
        totalPrice,
        regularCost,
        penaltyCost,
        paidStatus,
      };
    }

    return {
      totalUsage: 0,
      regularUsage: 0,
      penaltyUsage: 0,
      totalPrice: 0,
      paidStatus: false,
    };
  } catch (error) {
    console.error(`Error fetching month usage for ${userEmail}:`, error);
    return {
      totalUsage: 0,
      regularUsage: 0,
      penaltyUsage: 0,
      totalPrice: 0,
      paidStatus: false,
    };
  }
};

// ----------------------
// Fetch ALL users
// ----------------------
export const fetchAllUsers = async () => {
  try {
    const usersRef = collection(db, "users");
    const userDocs = await getDocs(usersRef);
    const users = [];

    for (const userDoc of userDocs.docs) {
      const userEmail = userDoc.id;
      const userData = userDoc.data();

      const {
        totalUsage,
        totalPrice,
        paidStatus,
      } = await fetchMonthUsage(userEmail);

      const now = new Date();
      const monthNames = [
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
      ];

      const userDetails = userData.userDetails || {};

      users.push({
        id: userEmail,
        name: userDetails.fullName || "Unknown",
        emailId: userDetails.emailId || userEmail,
        phone: userDetails.mobileNo || "N/A",
        accountNumber: userDetails.accountNumber || "N/A",
        consumerNumber: userDetails.consumerNumber || "N/A",
        meterNumber: userDetails.meterNumber || "N/A",
        address: userDetails.address || "N/A",
        supplyZone: userDetails.supplyZone || "N/A",

        wifi_ssid: userData.wifi_ssid || "N/A",
        wifi_pass: userData.wifi_pass || "N/A",
        servoState: userData.servoState ? "Active" : "Inactive",

        currentUsage: totalUsage,
        currentUsageFormatted: `${totalUsage.toFixed(2)} L`,

        // ⭐ REAL BILL (matches dashboard)
        dueBill: paidStatus ? "₹ 0" : `₹ ${totalPrice.toFixed(2)}`,

        currentMonthName: monthNames[now.getMonth()],
        currentYear: now.getFullYear(),
        userDetails,
      });
    }

    return users;
  } catch (err) {
    console.error("Error loading users:", err);
    return [];
  }
};
