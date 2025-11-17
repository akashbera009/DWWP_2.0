import React, { useState, useEffect, useCallback } from "react";
import { Outlet } from "react-router-dom";
import { motion } from "framer-motion";
import {
  collection,
  getDocs,
  doc,
  getDoc,
  setDoc,
  onSnapshot,
} from "firebase/firestore";
import { db } from "../../firebaseConfig";
import { handlePayment } from "../../utils/razorpayPayment";
import { downloadPDF } from "../../utils/downloadPDF";
import "./PaymentsDashboard.css";
import { createSmsQueueEntry } from "../../utils/smsQueueHelper";
import Loader from "../../Loader/Loader";


const PaymentsDashboard = ({ userId }) => {
  const [transactions, setTransactions] = useState([]);
  const [expandedMonths, setExpandedMonths] = useState({});
  const [searchTerm, setSearchTerm] = useState("");
  const [filterStatus, setFilterStatus] = useState("All");
  const [activeDropdown, setActiveDropdown] = useState(null);
  const [selectedTransaction, setSelectedTransaction] = useState(null);
  const [loading, setLoading] = useState(true);
  const [refreshTrigger, setRefreshTrigger] = useState(0); // ✅ Trigger for manual refresh

  // ✅ Optimized fetchTransactions - removed unnecessary callback wrapper
  const fetchTransactions = useCallback(async (userEmail) => {
    try {
      setLoading(true);
      const monthlyUsagesRef = collection(
        db,
        `users/${userEmail}/monthlyUsages`
      );
      const monthlyUsagesSnap = await getDocs(monthlyUsagesRef);
      const fetchedTransactions = [];

      for (const monthDoc of monthlyUsagesSnap.docs) {
        const yearMonth = monthDoc.id;

        // Fetch payment details
        const paymentDocRef = doc(
          db,
          `users/${userEmail}/monthlyUsages/${yearMonth}/payment/payment_details`
        );
        const paymentSnap = await getDoc(paymentDocRef);

        if (paymentSnap.exists()) {
          const paymentData = paymentSnap.data();
          fetchedTransactions.push({
            id: paymentData.razor_pay_id || "N/A",
            date: paymentData.date || new Date().toISOString(),
            month: yearMonth,
            type: "Regular",
            amount: `₹${paymentData.amount || 0}`,
            status: paymentData.status || "Pending",
          });
        }

        // Fetch addon details
        const addonCollectionRef = collection(
          db,
          `users/${userEmail}/monthlyUsages/${yearMonth}/addon`
        );
        const addonSnap = await getDocs(addonCollectionRef);

        addonSnap.forEach((addonDoc) => {
          const addonData = addonDoc.data();
          fetchedTransactions.push({
            id: addonData.razor_pay_id || addonDoc.id,
            date: addonData.addon_date || new Date().toISOString(),
            month: yearMonth,
            type: "Addon",
            amount: `₹${addonData.amount || 0}`,
            qty: addonData.quantityDone || 0,
            refill: addonData.refill || 0,
            status: "Completed",
          });
        });
      }

      // Sort transactions by date descending
      const sortedTransactions = fetchedTransactions.sort(
        (a, b) => new Date(b.date) - new Date(a.date)
      );

      setTransactions(sortedTransactions);
    } catch (error) {
      console.error("Error fetching transactions:", error);
    } finally {
      setLoading(false);
    }
  }, []); // ✅ No dependencies - stable function

  // ✅ Fetch on mount and when refreshTrigger changes
  useEffect(() => {
    if (userId) {
      fetchTransactions(userId);
    }
  }, [userId, refreshTrigger, fetchTransactions]);

  const closeDropdown = () => setActiveDropdown(null);

  const viewDetails = (transaction) => {
    setSelectedTransaction(transaction);
  };

  // ✅ Memoized filtered transactions
  const filteredTransactions = React.useMemo(() => {
    const lowerSearch = searchTerm.toLowerCase();
    const lowerFilterStatus = filterStatus.toLowerCase();

    return transactions.filter((transaction) => {
      const matchesStatus =
        lowerFilterStatus === "all" ||
        transaction.status.toLowerCase() === lowerFilterStatus ||
        transaction.type.toLowerCase() === lowerFilterStatus;

      const matchesSearch = Object.values(transaction).some((value) =>
        String(value).toLowerCase().includes(lowerSearch)
      );

      return matchesStatus && matchesSearch;
    });
  }, [transactions, searchTerm, filterStatus]);

  // Auto-expand months with matching transactions
  useEffect(() => {
    if (searchTerm.trim() === "") return;

    const matchingMonths = {};
    filteredTransactions.forEach((transaction) => {
      matchingMonths[transaction.month] = true;
    });

    setExpandedMonths(matchingMonths);
  }, [filteredTransactions, searchTerm]);

  // ✅ Memoized grouped transactions
  const groupedTransactions = React.useMemo(
    () =>
      filteredTransactions.reduce((acc, transaction) => {
        const { month } = transaction;
        acc[month] = acc[month] || [];
        acc[month].push(transaction);
        return acc;
      }, {}),
    [filteredTransactions]
  );

  const sortedMonths = Object.keys(groupedTransactions).sort((a, b) =>
    b.localeCompare(a)
  );

  const toggleMonth = (month) => {
    setExpandedMonths((prev) => ({ ...prev, [month]: !prev[month] }));
  };

  const toggleDropdown = (transactionId) => {
    setActiveDropdown((prev) =>
      prev === transactionId ? null : transactionId
    );
  };

  const ChevronDown = () => (
    <svg width="25" height="25" viewBox="0 2 24 24" fill="none">
      <path
        d="M6 10L12 16L18 10"
        stroke="#488AEC"
        strokeWidth="3"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    </svg>
  );

  return (
    <>
      {/* ✅ Pass refresh function to RechargeCard */}
      <RechargeCard
        userId={userId}
        onPaymentSuccess={() => setRefreshTrigger((prev) => prev + 1)}
      />

      <motion.div
        className="pay-con"
        initial={{ y: "-100vh", opacity: 0 }}
        animate={{ y: 0, opacity: 1 }}
        transition={{
          duration: 0.8,
          ease: [0.25, 1, 0.5, 1],
          type: "spring",
          stiffness: 100,
          damping: 15,
        }}
      >
        <div className="paymentDashboard-container" onClick={closeDropdown}>
          <div className="paymentDashboard-header">
            <div className="paymentDashboard-header-left">
              <img
                src="https://i.ibb.co/JFp06q4R/payment-history.png"
                alt="Payment History"
              />
              <h2>Transactions History</h2>
            </div>
            <div className="paymentDashboard-search-container">
              <input
                type="text"
                placeholder="Search transactions..."
                className="paymentDashboard-search-bar"
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
              />
              <select
                className="paymentDashboard-filter-btn"
                value={filterStatus}
                onChange={(e) => setFilterStatus(e.target.value)}
              >
                <option value="All">All Statuses</option>
                <option value="pending">Pending</option>
                <option value="Completed">Completed</option>
                <option value="regular">Regular</option>
                <option value="addon">Addon</option>
              </select>
            </div>
          </div>

          {loading ? (
            <Loader />
          ) : filteredTransactions.length === 0 ? (
            <div className="no-results">No matching transactions found.</div>
          ) : (
            <table className="paymentDashboard-table">
              <thead>
                <tr>
                  <th>Transaction ID</th>
                  <th>Date</th>
                  <th>Type</th>
                  <th>Amount</th>
                  <th>Qty</th>
                  <th>Status</th>
                  <th>Actions</th>
                </tr>
              </thead>
              <tbody>
                {sortedMonths.map((month) => (
                  <React.Fragment key={month}>
                    <tr
                      className="month-header"
                      onClick={() => toggleMonth(month)}
                    >
                      <td colSpan="7" className="month-header-content">
                        <span
                          className={`month-chevron ${
                            expandedMonths[month] ? "expanded" : ""
                          }`}
                        >
                          <ChevronDown />
                        </span>
                        {month} (
                        {
                          groupedTransactions[month].filter(
                            (transaction) => transaction.status !== "pending"
                          ).length
                        }{" "}
                        transactions)
                      </td>
                    </tr>

                    {expandedMonths[month] &&
                      groupedTransactions[month]
                        .filter(
                          (transaction) => transaction.status !== "pending"
                        )
                        .map((transaction) => (
                          <tr key={transaction.id}>
                            <td>{transaction.id}</td>
                            <td>
                              {new Date(transaction.date).toLocaleDateString()}
                            </td>
                            <td>{transaction.type}</td>
                            <td>{transaction.amount}</td>
                            <td>{transaction.qty}</td>
                            <td
                              className={`status-${transaction.status.toLowerCase()}`}
                            >
                              {transaction.status}
                            </td>
                            <td
                              className="action-cell"
                              onClick={(e) => e.stopPropagation()}
                            >
                              <button
                                className="action-btn"
                                onClick={(e) => {
                                  e.stopPropagation();
                                  toggleDropdown(transaction.id);
                                }}
                              >
                                ⋮
                              </button>
                              <div
                                className={`dropdown ${
                                  activeDropdown === transaction.id
                                    ? "active"
                                    : ""
                                }`}
                              >
                                <button
                                  onClick={() => viewDetails(transaction)}
                                >
                                  View Details
                                </button>
                                <button
                                  onClick={() => downloadPDF(transaction)}
                                >
                                  Download Receipt
                                </button>
                              </div>
                            </td>
                          </tr>
                        ))}
                  </React.Fragment>
                ))}
              </tbody>
            </table>
          )}
        </div>
        <Outlet />
      </motion.div>

      {selectedTransaction && (
        <TransactionModal
          transaction={selectedTransaction}
          onClose={() => setSelectedTransaction(null)}
        />
      )}
    </>
  );
};

const TransactionModal = ({ transaction, onClose }) => {
  if (!transaction) return null;

  return (
    <div className="transaction-modal-overlay">
      <div className="transaction-modal">
        <div className="transaction-modal-header">
          <h3>Transaction Details</h3>
          <button className="transaction-close-button" onClick={onClose}>
            &times;
          </button>
        </div>

        <div className="transaction-modal-content">
          <div className="detail-row">
            <span className="detail-label">Transaction ID:</span>
            <span className="detail-value">{transaction.id}</span>
          </div>
          <div className="detail-row">
            <span className="detail-label">Date:</span>
            <span className="detail-value">
              {new Date(transaction.date).toLocaleDateString()}
            </span>
          </div>
          <div className="detail-row">
            <span className="detail-label">Month:</span>
            <span className="detail-value">{transaction.month}</span>
          </div>
          <div className="detail-row">
            <span className="detail-label">Type:</span>
            <span className="detail-value">{transaction.type}</span>
          </div>
          <div className="detail-row">
            <span className="detail-label">Amount:</span>
            <span className="detail-value">{transaction.amount}</span>
          </div>
          <div className="detail-row">
            <span className="detail-label">Qty:</span>
            <span className="detail-value">{transaction.qty}</span>
          </div>
          <div className="detail-row">
            <span className="detail-label">Status:</span>
            <span className={`status-${transaction.status.toLowerCase()}`}>
              {transaction.status}
            </span>
          </div>
        </div>

        <div className="transaction-modal-actions">
          <button
            className="download-button"
            onClick={() => downloadPDF(transaction)}
          >
            Download Receipt
          </button>
          <button className="transaction-close-modal-button" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
};

// ✅ Optimized RechargeCard (from previous optimization)
const RechargeCard = ({ userId, onPaymentSuccess }) => {
  const [totalUsage, setTotalUsage] = useState(0);
  const [penaltyPrice, setPenaltyPrice] = useState(0);
  const [regularPrice, setRegularPrice] = useState(0);
  const [limitBYUser, setLimitByUser] = useState(0);
  const [rechargeDetails, setRechargeDetails] = useState({
    amount: 0,
    usage: 0,
    status: "...",
    dueDate: "",
    isLoading: true,
  });

  // ✅ Single useEffect for ALL real-time data
  useEffect(() => {
    if (!userId) return;

    const now = new Date();
    const year = now.getFullYear();
    const month = String(now.getMonth() + 1).padStart(2, "0");
    const yearMonth = `${year}-${month}`;

    // Real-time listener for water usage
    const unsubscribeWaterFlow = onSnapshot(
      doc(db, "users", userId, "monthlyUsages", yearMonth),
      (docSnap) => {
        if (docSnap.exists()) {
          const data = docSnap.data();

          const total = Object.entries(data)
            .filter(([key]) => key.startsWith(yearMonth))
            .reduce((sum, [, usage]) => sum + (usage || 0), 0);

          const userLimit = data.limit || 0;

          setLimitByUser(userLimit);
          setTotalUsage(total);
        } else {
          setTotalUsage(0);
          setLimitByUser(0);
        }
      },
      (error) => console.error("Error fetching water usage:", error)
    );

    // Real-time listener for price
    const unsubscribePrice = onSnapshot(
      doc(db, "admin", "price"),
      (priceDocSnap) => {
        if (priceDocSnap.exists()) {
          const data = priceDocSnap.data();
          setPenaltyPrice(data.penaltyPrice || 0);
          setRegularPrice(data.regularPrice || 0);
        }
      },
      (error) => console.error("Error fetching price:", error)
    );

    // Real-time listener for payment status
    const unsubscribePayment = onSnapshot(
      doc(
        db,
        `users/${userId}/monthlyUsages/${yearMonth}/payment/payment_details`
      ),
      (docSnap) => {
        const dueDate = new Date(now.getFullYear(), now.getMonth() + 1, 5);

        if (docSnap.exists()) {
          const data = docSnap.data();
          setRechargeDetails((prev) => ({
            ...prev,
            status: data.status || "pending",
            dueDate: dueDate.toLocaleDateString(),
            isLoading: false,
          }));
        } else {
          setRechargeDetails((prev) => ({
            ...prev,
            status: "pending",
            dueDate: dueDate.toLocaleDateString(),
            isLoading: false,
          }));
        }
      },
      (error) => {
        console.error("Error fetching payment status:", error);
        setRechargeDetails((prev) => ({ ...prev, isLoading: false }));
      }
    );

    return () => {
      unsubscribeWaterFlow();
      unsubscribePrice();
      unsubscribePayment();
    };
  }, [userId]);

  // Calculate derived values
  const regularLimit = limitBYUser;
  const regularUsage = Math.min(totalUsage, regularLimit);
  const penaltyUsage =
    totalUsage > regularLimit ? totalUsage - regularLimit : 0;
  const regularPriceTotal = regularUsage * regularPrice;
  const penaltyPriceTotal = penaltyUsage * penaltyPrice;
  const totalPrice = (regularPriceTotal + penaltyPriceTotal).toFixed(0);

  // Update rechargeDetails when calculations change
  useEffect(() => {
    if (rechargeDetails.isLoading) return;

    setRechargeDetails((prev) => ({
      ...prev,
      amount: totalPrice,
      usage: totalUsage.toFixed(2),
    }));
  }, [totalPrice, totalUsage]);

  const handleRechargePayment = async () => {
    // console.log("Initiating recharge payment...");
    try {
      const paymentId = await handlePayment(
        userId,
        rechargeDetails.amount,
        1,
        0,
        "recharge"
      );

      if (paymentId) {
        const now = new Date();
        const year = now.getFullYear();
        const month = String(now.getMonth() + 1).padStart(2, "0");
        const yearMonth = `${year}-${month}`;

        const docRef = doc(
          db,
          `users/${userId}/monthlyUsages/${yearMonth}/payment/payment_details`
        );

        await setDoc(
          docRef,
          {
            amount: rechargeDetails.amount,
            date: now.toISOString(),
            forMonth: yearMonth,
            razor_pay_id: paymentId,
            status: "Completed",
            timeStamp: now.toISOString(),
          },
          { merge: true }
        );

        console.log("Payment success! Firestore updated.");

        await createSmsQueueEntry(userId, {
          messageType: "payment",
          paymentAmount: rechargeDetails.amount,
          forMonth: yearMonth,
          paymentId: paymentId,
          referenceId: `users/${userId}/monthlyUsages/${yearMonth}/payment/payment_details`,
        });

        if (onPaymentSuccess) {
          onPaymentSuccess();
        }
      }
    } catch (error) {
      console.error("Payment failed:", error);
    }
  };

  if (rechargeDetails.isLoading) {
    return (
      <div className="loading-recharge">Checking for pending Recharge...</div>
    );
  }

  return (
    <div className="recharge-card">
      <div className="rc-header">
        <h3>Current Recharge</h3>
        <span className="rc-status">
          <span
            className={`status-dot ${rechargeDetails.status.toLowerCase()}`}
          ></span>
          {rechargeDetails.status}
        </span>
      </div>

      <div className="rc-details">
        <div className="rc-amount">
          <span className="rc-label">Amount:</span>
          <span className="rc-value">₹{rechargeDetails.amount}</span>
        </div>
        <div className="rc-usage">
          <span className="rc-label">Usages (in Liters):</span>
          <span className="rc-value">{rechargeDetails.usage} L</span>
        </div>
        <div className="rc-due-date">
          <span className="rc-label">Due Date:</span>
          <span className="rc-value">{rechargeDetails.dueDate}</span>
        </div>
      </div>

      {rechargeDetails.status === "pending" && (
        <button className="rc-pay-button" onClick={handleRechargePayment}>
          Pay Now
        </button>
      )}
    </div>
  );
};

export default PaymentsDashboard;
