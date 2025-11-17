import React, { useState, useEffect } from "react";
import { auth, db } from "../firebaseConfig";
import { doc, getDoc, updateDoc } from "firebase/firestore";
import { onAuthStateChanged } from "firebase/auth";
import md5 from "md5";
import {
  Bell,
  Edit2,
  X,
  Check,
  User,
  Mail,
  Phone,
  MapPin,
  CreditCard,
  Zap,
  Wifi,
   HelpCircle
} from "lucide-react";
import "./TopBar.css";
import "./NotificationDropdown.css";
import "./ProfileModal.css";
import NotificationDropdown from "./NotificationDropdown.jsx";
import toast, { Toaster } from "react-hot-toast";

const TopBar = () => {
  const [isNotificationsOpen, setIsNotificationsOpen] = useState(false);
  const [isProfileModalOpen, setIsProfileModalOpen] = useState(false);
  const [isEditModalOpen, setIsEditModalOpen] = useState(false);
  const [showConfirmation, setShowConfirmation] = useState(false);
  const [username, setUsername] = useState("");
  const [userEmail, setUserEmail] = useState("");
  const [profileImageurl, setProfileImageurl] = useState("");
  const [userDetails, setUserDetails] = useState({
    fullName: "",
    mobileNo: "",
    emailId: "",
    address: "",
    accountNumber: "",
    consumerNumber: "",
    meterNumber: "",
    supplyZone: "",
  });
  const [editedDetails, setEditedDetails] = useState({});

  const getGravatarUrl = (userEmail) => {
    const hash = md5(userEmail.trim().toLowerCase());
    return `https://www.gravatar.com/avatar/${hash}?d=identicon`;
  };

  useEffect(() => {
    const unsubscribe = onAuthStateChanged(auth, async (user) => {
      if (user) {
        setUserEmail(user.email);

        try {
          const userDocRef = doc(db, "users", user.email);
          const userDoc = await getDoc(userDocRef);
          if (userDoc.exists()) {
            const userData = userDoc.data();
            setProfileImageurl(userData.profileImageUrl || "");

            const details = userData.userDetails || {};
            setUserDetails({
              fullName: details.fullName || "",
              mobileNo: details.mobileNo || "",
              emailId: details.emailId || user.email,
              address: details.address || "",
              accountNumber: details.accountNumber || "",
              consumerNumber: details.consumerNumber || "",
              meterNumber: details.meterNumber || "",
              supplyZone: details.supplyZone || "",
            });
            setUsername(details.fullName || "User");
          }
        } catch (error) {
          console.error("Error fetching user details:", error);
        }
      }
    });
    return () => unsubscribe();
  }, []);

  const handleEditClick = () => {
    setEditedDetails({ ...userDetails });
    setIsProfileModalOpen(false);
    setIsEditModalOpen(true);
  };

  const handleInputChange = (e) => {
    setEditedDetails({
      ...editedDetails,
      [e.target.name]: e.target.value,
    });
  };

  const handleSaveClick = () => {
    setShowConfirmation(true);
  };

  const handleConfirmSave = async () => {
    try {
      const userDocRef = doc(db, "users", userEmail);
      await updateDoc(userDocRef, {
        userDetails: editedDetails,
      });
      setUserDetails(editedDetails);
      setUsername(editedDetails.fullName);
      setShowConfirmation(false);
      setIsEditModalOpen(false);
      toast.success("Profile updated successfully!");
    } catch (error) {
      console.error("Error updating user details:", error);
      toast.error("Failed to update profile. Please try again.");
    }
  };

  const handleCancelConfirmation = () => {
    setShowConfirmation(false);
  };

  const InfoField = ({ icon: Icon, label, value }) => (
    <div className="info-field">
      <div className="info-label">
        <Icon size={16} className="info-icon" />
        <span>{label}</span>
      </div>
      <div className="info-value">{value || "Not provided"}</div>
    </div>
  );

  return (
    <div className="top-bar">
      <Toaster
        position="top-right"
        toastOptions={{
          duration: 3000,
          style: {
            background: "linear-gradient(135deg, #488aec, #5b9ff5)",
            color: "#fff",
            borderRadius: "10px",
            padding: "16px",
          },
          success: {
            iconTheme: {
              primary: "#50fa7b",
              secondary: "#fff",
            },
          },
        }}
      />
      <div className="notificatio-profile">
        <div
          className="notification-icon"
          onClick={() => setIsNotificationsOpen(!isNotificationsOpen)}
        >
          <Bell size={20} />
        </div>

        <NotificationDropdown
          isOpen={isNotificationsOpen}
          onClose={() => setIsNotificationsOpen(false)}
        />

        <div
          className="user-profile"
          onClick={() => setIsProfileModalOpen(true)}
        >
          <img
            src={profileImageurl || getGravatarUrl(userEmail)}
            alt="User"
            className="user-icon"
            onError={(e) => {
              e.target.src = "https://i.ibb.co/93vpqhDS/profile-pic.png";
            }}
          />
          <span>{username}</span>
        </div>

        {/* View Profile Modal */}
        {isProfileModalOpen && (
          <div
            className="modal-overlay"
            onClick={() => setIsProfileModalOpen(false)}
          >
            <div
              className="profile-modal modern"
              onClick={(e) => e.stopPropagation()}
            >
              <div className="modal-header">
                <div className="header-content">
                  <img
                    src={profileImageurl || getGravatarUrl(userEmail)}
                    alt="User"
                    className="modal-user-avatar"
                    onError={(e) => {
                      e.target.src =
                        "https://i.ibb.co/93vpqhDS/profile-pic.png";
                    }}
                  />
                  <div>
                    <h2>{userDetails.fullName || "User Profile"}</h2>
                    <p className="user-email-subtitle">{userEmail}</p>
                  </div>
                </div>
                <button
                  className="close-button"
                  onClick={() => setIsProfileModalOpen(false)}
                >
                  <X size={20} />
                </button>
              </div>

              <div className="profile-content">
                <div className="info-section">
                  <h3 className="section-title">Personal Information</h3>
                  <InfoField
                    icon={User}
                    label="Full Name"
                    value={userDetails.fullName}
                  />
                  <InfoField
                    icon={Phone}
                    label="Mobile Number"
                    value={userDetails.mobileNo}
                  />
                  <InfoField
                    icon={Mail}
                    label="Email ID"
                    value={userDetails.emailId}
                  />
                  <InfoField
                    icon={MapPin}
                    label="Address"
                    value={userDetails.address}
                  />
                </div>

                <div className="info-section">
                  <h3 className="section-title">Account Details</h3>
                  <InfoField
                    icon={CreditCard}
                    label="Account Number"
                    value={userDetails.accountNumber}
                  />
                  <InfoField
                    icon={User}
                    label="Consumer Number"
                    value={userDetails.consumerNumber}
                  />
                  <InfoField
                    icon={Zap}
                    label="Meter Number"
                    value={userDetails.meterNumber}
                  />
                  <InfoField
                    icon={MapPin}
                    label="Supply Zone"
                    value={userDetails.supplyZone}
                  />
                </div>
              </div>

              <div className="modal-actions">
                <button
                  type="button"
                  className="btn-secondary"
                  onClick={() => setIsProfileModalOpen(false)}
                >
                  Close
                </button>
                <button
                  type="button"
                  className="btn-primary"
                  onClick={handleEditClick}
                >
                  <Edit2 size={16} />
                  Edit Profile
                </button>
              </div>
            </div>
          </div>
        )}

        {/* Edit Profile Modal */}
        {isEditModalOpen && (
          <div
            className="modal-overlay"
            onClick={() => setIsEditModalOpen(false)}
          >
            <div
              className="profile-modal modern"
              onClick={(e) => e.stopPropagation()}
            >
              <div className="modal-header">
                <h2>Edit Profile</h2>
                <button
                  className="close-button"
                  onClick={() => setIsEditModalOpen(false)}
                >
                  <X size={20} />
                </button>
              </div>

              <form
                className="profile-form"
                onSubmit={(e) => {
                  e.preventDefault();
                  handleSaveClick();
                }}
              >
                <div className="info-section">
                  <h3 className="section-title">Personal Information</h3>
                  <div className="form-group">
                    <label>Full Name</label>
                    <input
                      type="text"
                      name="fullName"
                      value={editedDetails.fullName}
                      onChange={handleInputChange}
                    />
                  </div>
                  <div className="form-group">
                    <label>Mobile Number</label>
                    <input
                      type="tel"
                      name="mobileNo"
                      disabled
                      className="readonly-input"
                      value={editedDetails.mobileNo}
                      onChange={handleInputChange}
                    />
                  </div>
                  <div className="form-group">
                    <label>Email ID (Optional)</label>
                    <input
                      type="email"
                      name="emailId"
                      value={editedDetails.emailId}
                      onChange={handleInputChange}
                    />
                  </div>
                  <div className="form-group">
                    <label>Address</label>
                    <textarea
                      name="address"
                      rows="3"
                      value={editedDetails.address}
                      onChange={handleInputChange}
                    />
                  </div>
                </div>

                <div className="info-section">
                  <h3 className="section-title">Account Details (Read Only)</h3>
                  <div className="form-group">
                    <label>Account Number</label>
                    <input
                      type="text"
                      name="accountNumber"
                      value={editedDetails.accountNumber}
                      disabled
                      className="readonly-input"
                    />
                  </div>
                  <div className="form-group">
                    <label>Consumer Number</label>
                    <input
                      type="text"
                      name="consumerNumber"
                      value={editedDetails.consumerNumber}
                      disabled
                      className="readonly-input"
                    />
                  </div>
                  <div className="form-group">
                    <label>Meter Number</label>
                    <input
                      type="text"
                      name="meterNumber"
                      value={editedDetails.meterNumber}
                      disabled
                      className="readonly-input"
                    />
                  </div>
                  <div className="form-group">
                    <label>Supply Zone</label>
                    <input
                      type="text"
                      name="supplyZone"
                      value={editedDetails.supplyZone}
                      disabled
                      className="readonly-input"
                    />
                  </div>
                </div>

                <div className="modal-actions">
                  <button
                    type="button"
                    className="btn-secondary"
                    onClick={() => setIsEditModalOpen(false)}
                  >
                    Cancel
                  </button>
                  <button type="submit" className="btn-primary">
                    <Check size={16} />
                    Save Changes
                  </button>
                </div>
              </form>
            </div>
          </div>
        )}

        {/* Confirmation Modal */}
        {showConfirmation && (
          <div className="modal-overlay">
            <div className="confirmation-modal">
              <div className="confirmation-icon">
                < HelpCircle size={85} />
              </div>
              <h3>Confirm Changes</h3>
              <p>
                Are you sure you want to save these changes to your profile?
              </p>
              <div className="confirmation-actions">
                <button
                  className="btn-secondary"
                  onClick={handleCancelConfirmation}
                >
                  Cancel
                </button>
                <button className="btn-primary" onClick={handleConfirmSave}>
                  Confirm
                </button>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default TopBar;
