import React, { useState, useEffect } from "react";
import { motion } from "framer-motion";
import "./Servo_Control.css";
import { db } from "/src/firebaseConfig.js";
import { doc, onSnapshot } from "firebase/firestore";
import ToggleSwitch from "./ToggleSwitch";

const Servo_Control = ({ userId }) => {
  const [lastSeen, setLastSeen] = useState(null);
  const [status, setStatus] = useState("Loading...");
  const [days, setDays] = useState(0);

  useEffect(() => {
    if (!userId) return;

    const userDocRef = doc(db, "users", userId);
    const unsubscribe = onSnapshot(userDocRef, (docSnap) => {
      if (docSnap.exists()) {
        const timestamp = docSnap.data().lastSeen;
        setLastSeen(timestamp);
      } else {
        setLastSeen(null);
      }
    });

    return () => unsubscribe();
  }, [userId]);

  useEffect(() => {
    if (!lastSeen) return;

    const updateStatus = () => {
      const now = Date.now();
      const diffSeconds = Math.floor((now - lastSeen) / 1000);
      const diffMinutes = Math.floor((now - lastSeen) / (1000 * 60));
      const diffDays = Math.floor(diffMinutes / (60 * 24));
      setDays(diffDays);

      if (diffSeconds < 15) {
        setStatus("🟢 Online");
      } else if (diffSeconds < 60) {
        setStatus(`🟡 Last online ${diffSeconds} sec ago`);
      } else if (diffSeconds < 3600) {
        setStatus(`🟡 Last online ${Math.floor(diffSeconds / 60)} min ago`);
      } else if (diffSeconds < 86400) {
        setStatus(`🟡 Last online ${Math.floor(diffSeconds / 3600)} hrs ago`);
      } else {
        setStatus(`🔴 Last online ${Math.floor(diffSeconds / 86400)} days ago`);
      }
    };

    updateStatus();
    const interval = setInterval(updateStatus, 5000);

    return () => clearInterval(interval);
  }, [lastSeen]);

  // inside Servo_Control component (before return)
  const computePct = () => {
    if (!lastSeen) return 0;
    const diffSeconds = Math.floor((Date.now() - lastSeen) / 1000);
    // normalized inverse percentage across 24h:
    const ratio = 1 - Math.min(diffSeconds, 86400) / 86400; // 1 -> very recent, 0 -> >24h
    const raw = Math.round(ratio * 100);
    // minimum small visible ring (so not invisible)
    return Math.max(6, raw);
  };
  const pct = computePct();

  return (
    <motion.div
      className="gate-con"
      initial={{ opacity: 0, y: -20 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ duration: 0.5 }}
    >
      <motion.div
        className="servo-card"
        initial={{ scale: 0.9 }}
        animate={{ scale: 1 }}
        transition={{ duration: 0.3 }}
      >
        <motion.span
          className="servo-small-text"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          transition={{ duration: 0.7 }}
        >
          Water Supply Control
        </motion.span>
        <div className="fixed-amount">
          <ToggleSwitch userId={userId} disabled={status !== "🟢 Online"} />
        </div>

        <div
          className={`device-status ${
            status.includes("🟢")
              ? "online"
              : status.includes("🟡")
              ? "idle"
              : "offline"
          }`}
        >
          {status}
        </div>

        {status !== "🟢 Online" && (
          <motion.div
            className="offline-warning"
            initial={{ opacity: 0, y: -5 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.4 }}
          >
            ⚠️ Device is currently offline. Water control switch is disabled.
          </motion.div>
        )}
      </motion.div>
    </motion.div>
  );
};

export default Servo_Control;
