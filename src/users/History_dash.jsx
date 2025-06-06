
import React, { useEffect, useState } from "react";
import "./History_dash.css";
import { useNavigate } from "react-router-dom";
import { MdAutoGraph } from "react-icons/md";
import { doc, getDoc } from "firebase/firestore";
import { db } from "../firebaseConfig"; // update this import as per your project
// import { useAuthState } from "react-firebase-hooks/auth";
import { auth } from "../firebaseConfig"; // update this import too
import { use } from "react";

const Rechargecard = ({userId}) => {
  const navigate = useNavigate();
  // const [user] = useAuthState(auth);
  const [percentageChange, setPercentageChange] = useState(null);
  const [trendType, setTrendType] = useState(""); // "increase" or "decrease"

  useEffect(() => {
    if (userId) {
      fetchWaterUsageComparison();
    }
  }, [userId]);

  const fetchWaterUsageComparison = async () => {
    const today = new Date();
    const currentMonth = today.toISOString().slice(0, 7); // e.g., "2025-04"
    // const fallbackPreviousMonth = "2025-03"; // 👈 your actual previous month with data
  
    try {
      const currDocRef = doc(db, "users", userId, "monthlyUsages", currentMonth);
      const currSnap = await getDoc(currDocRef);
  
      if (!currSnap.exists()) {
        console.log("No data for current month");
        return;
      }
  
      const currData = currSnap.data();
  
      // Try actual previous month first
      const previousMonthDate = new Date(Date.UTC(today.getFullYear(), today.getMonth() - 1, 1));
      const previousMonth = previousMonthDate.toISOString().slice(0, 7);      
      
      let prevDocRef = doc(db, "users", userId, "monthlyUsages", previousMonth);
      let prevSnap = await getDoc(prevDocRef);
  
      // // Fallback if previous month data not found
      // if (!prevSnap.exists()) {
      //   console.log(`No data for ${previousMonth}, trying fallback month ${fallbackPreviousMonth}`);
      //   prevDocRef = doc(db, "users", user.email, "monthlyUsages", fallbackPreviousMonth);
      //   prevSnap = await getDoc(prevDocRef);
      // }
  
      // if (!prevSnap.exists()) {
      //   console.log("No data for previous or fallback month");
      //   return;
      // }
  
      const prevData = prevSnap.data();
  
      const sumValues = (data) =>
        Object.entries(data)
          .filter(([key]) => /^\d{4}-\d{2}-\d{2}$/.test(key))
          .reduce((acc, [, value]) => acc + Number(value), 0);
  
      const currentTotal = sumValues(currData);
      const previousTotal = sumValues(prevData);
  
  
      if (previousTotal === 0) return;
  
      const change = ((currentTotal - previousTotal) / previousTotal) * 100;
      setPercentageChange(change.toFixed(2));
      setTrendType(change > 0 ? "increase" : "decrease");
    } catch (err) {
      console.error("Error fetching usage data:", err);
    }
  };
  

  const handleNavigation = () => {
    navigate("/user/graph");
  };

  return (
    <div className="card">
      <span className="title">
        <img src="https://i.ibb.co/GQ6fTbLf/graph.png" alt="Graph Icon" />
      </span>
      <span className="desc">
        {percentageChange !== null ? (
          <>This month water usage {trendType}d by</>
        ) : (
          <>Not enough data to compare</>
        )}
      </span>

      <div className="button-container">
        <button className="water-recharge-btn">
          <MdAutoGraph size={"25px"} />
          <span style={{ fontSize: "1rem", position: "relative", bottom: "5px", left: "2px" }}>
            {percentageChange !== null ? `${trendType === "increase" ? "+" : ""}${percentageChange}%` : "--"}
          </span>
        </button>
      </div>

      <div className="view-full-history-btn" onClick={handleNavigation}>
        <span>View Full History</span>
      </div>
    </div>
  );
};

export default Rechargecard;