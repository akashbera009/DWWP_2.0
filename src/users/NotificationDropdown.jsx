import React, { useEffect, useState } from "react";
import "./NotificationDropdown.css";
import { db } from "../firebaseConfig"; // Adjust path as needed
import { doc, getDoc } from "firebase/firestore";

const NotificationDropdown = ({ isOpen, onClose }) => {
  const [notifications, setNotifications] = useState([]);

  useEffect(() => {
    const fetchNotifications = async () => {
      try {
        const docRef = doc(db, "admin", "broadcast");
        const docSnap = await getDoc(docRef);
        if (docSnap.exists()) {
          const msgArray = docSnap.data().msg || [];
          // Sort messages by timestamp (latest first)
          const sorted = msgArray.sort(
            (a, b) => b.timestamp?.seconds - a.timestamp?.seconds
          );
          setNotifications(sorted);
        } else {
          console.log("No broadcast document found.");
        }
      } catch (error) {
        console.error("Error fetching notifications:", error);
      }
    };

    fetchNotifications();
  }, []);

  useEffect(() => {
    const handleClickOutside = (e) => {
      if (
        isOpen &&
        !e.target.closest(".notification-dropdown") &&
        !e.target.closest(".notification-icon")
      ) {
        onClose();
      }
    };
    document.addEventListener("click", handleClickOutside);
    return () => document.removeEventListener("click", handleClickOutside);
  }, [isOpen, onClose]);

  if (!isOpen) return null;

  return (
    <div className="user-notification-dropdown">
      <div className="user-notification-header">
        <h4>Notifications</h4>
        <button className="user-mark-all-read">Mark all as read</button>
      </div>
      <div className="user-notification-list">
        {notifications.length > 0 ? (
          notifications.map((notification, index) => (
            <div key={index} className="user-notification-item">
              <div className="user-notification-icon">
                <span>{notification.icon || "🔔"}</span> 
              </div>
              <div className="user-notification-content">
                <p>{notification.message || "No message"}</p>
                <span className="user-notification-time">
                  {notification.timestamp?.seconds
                    ? new Date(
                        notification.timestamp.seconds * 1000
                      ).toLocaleString()
                    : "Just now"}
                </span>
              </div>
            </div>
          ))
        ) : (
          <div className="user-no-notifications">No notifications</div>
        )}
      </div>
    </div>
  );
};


export default NotificationDropdown;
