import React from "react";
import "./Admin_main.css";
import { FaUsers } from "react-icons/fa";
import { FaUserGear } from "react-icons/fa6";
import { FaMoneyBillWave } from "react-icons/fa6"; 
import { FaBroadcastTower } from "react-icons/fa";
import { FaSms } from "react-icons/fa";
import { MdOutlineLogout } from "react-icons/md";
import { auth } from "../firebaseConfig";
import { signOut } from "firebase/auth";
import { MdDashboardCustomize } from "react-icons/md";

import { NavLink, Outlet } from "react-router-dom";

import Admin_Top_bar from "./Admin_Top_bar";
import { useNavigate } from "react-router-dom";

const Sidebar = () => {
  const navigate = useNavigate();
  const handleLogoutAdmin = async () => {
    
    try {
      await signOut(auth);
      console.log("User logged out successfully");
      navigate("/newauth");  
    } catch (error) {
      console.error("Error logging out:", error.message);
    }
  };
  return (
    <div className="admin-sidebar">
      <div className="logo">
        <img src="https://i.ibb.co/TDd24C9X/Domestic.png" />
      </div>
      <nav>
        <ul>
          <NavLink
            to="/newadmin/admin_Dashboard"
            className={({ isActive }) => (isActive ? "active" : "")}
            style={{ textDecoration: "none" }}
          >
            <li>
              <MdDashboardCustomize size={25} /> <span>Dashboard</span>
            </li>
          </NavLink>
          <NavLink
            to="/newadmin/admin_view_user"
            className={({ isActive }) => (isActive ? "active" : "")}
            style={{ textDecoration: "none" }}
          >
            <li>
              <FaUsers size={25} /> <span>View User</span>
            </li>
          </NavLink>

          <NavLink
            to="/newadmin/admin_limit_user"
            className={({ isActive }) => (isActive ? "active" : "")}
            style={{ textDecoration: "none" }}
          >
            <li>
              <FaUserGear size={25} /> <span>Set Limit</span>
            </li>
          </NavLink>

          <NavLink
            to="/newadmin/admin_setPrice"
            className={({ isActive }) => (isActive ? "active" : "")}
            style={{ textDecoration: "none" }}
          >
            <li>
              <FaMoneyBillWave size={25} /> <span>Set Price</span>
            </li>
          </NavLink>
          <NavLink
            to="/newadmin/admin_sms_queue"
            className={({ isActive }) => (isActive ? "active" : "")}
            style={{ textDecoration: "none" }}
          >
            <li>
              <FaSms size={25} /> <span>SMS Status</span>
            </li>
          </NavLink>
          <NavLink
            to="/newadmin/admin_Brodcast"
            className={({ isActive }) => (isActive ? "active" : "")}
            style={{ textDecoration: "none" }}
          >
            <li>
              <FaBroadcastTower size={25} /> <span>Broadcast</span>
            </li>
          </NavLink>
        </ul>
      </nav>

      <div className="settings" onClick={handleLogoutAdmin}>
        <MdOutlineLogout size={20} /> <span>SignOut</span>
      </div>
    </div>
  );
};

const Admin_main = () => {
  return (
    <>
      <div className="admin_main-con">
        <Sidebar />
        <div className="main-admin">
          <Admin_Top_bar />
          <Outlet />
        </div>
      </div>
    </>
  );
};
export default Admin_main;
