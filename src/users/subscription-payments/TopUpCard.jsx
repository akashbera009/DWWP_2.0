import React, { useState } from "react";
import { handlePayment } from "../../utils/razorpayPayment";
import { db } from "../../firebaseConfig";
import { doc, updateDoc, getDoc, setDoc } from "firebase/firestore";
import "./TopUpCard.css";
import Loader from "../../Loader/Loader";
import { Droplet, Plus, Minus, CheckCircle2, X } from "lucide-react";
import { createSmsQueueEntry } from "../../utils/smsQueueHelper";

const TopUpCard = ({ userId, addon, price, Refill, url, msg }) => {
  const [quantity, setQuantity] = useState(1);
  const [paymentSuccess, setPaymentSuccess] = useState(null);
  const [loading, setLoading] = useState(false);

  const now = new Date();
  const year = now.getFullYear();
  const month = String(now.getMonth() + 1).padStart(2, "0");
  const yearMonth = `${year}-${month}`;

  const addonRef = doc(
    db,
    "users",
    userId,
    "monthlyUsages",
    yearMonth,
    "addon",
    "addon_details"
  );

  const handleTopUp = async () => {
    try {
      setLoading(true);

      const paymentId = await handlePayment(
        userId,
        price * quantity,
        quantity,
        Refill,
        "addon"
      );

      if (paymentId) {
        const totalRefill = Number(Refill) * Number(quantity);
        const timeStamp = new Date().toISOString();
        const docSnap = await getDoc(addonRef);

        let data = {
          added_limit: 0,
          razor_pay_id: [],
          timeStamp: [],
          amount: [],
          quantityDone: [],
          refill: [],
        };

        if (docSnap.exists()) {
          data = docSnap.data();
        } else {
          await setDoc(addonRef, data);
        }

        data.razor_pay_id.push(paymentId);
        data.timeStamp.push(timeStamp);
        data.amount.push(price);
        data.quantityDone.push(quantity);
        data.refill.push(Refill);

        await updateDoc(addonRef, {
          added_limit: data.added_limit + totalRefill,
          razor_pay_id: data.razor_pay_id,
          timeStamp: data.timeStamp,
          amount: data.amount,
          quantityDone: data.quantityDone,
          refill: data.refill,
        });

        await createSmsQueueEntry(userId, {
          messageType: "addon",
          paymentAmount: price * quantity,
          refillAmount: totalRefill,
          paymentId: paymentId,
          referenceId: `users/${userId}/monthlyUsages/${yearMonth}/addon/addon_details`,
        });

        setPaymentSuccess({
          totalPrice: price * quantity,
          paymentId,
          timeStamp,
          flag: "addon",
        });
      }
    } catch (err) {
      console.error("❌ Payment failed:", err);
    } finally {
      setLoading(false);
    }
  };

  return (
    <>
      {/* 🌊 Modern Top-Up Card */}
      <div className="topup-card-modern">
        <div className="topup-card-header">
          <span className="topup-badge">{addon}</span>
          <h4 className="topup-title">
            {/* <Droplet className="droplet-icon" size={20} /> */}
          </h4>
        </div>

        <div className="topup-icon-container">
          <img src={url} alt="Water" className="topup-water-icon" />
          <div className="topup-text">
            <div>{msg}</div>
            <div>{Refill}L Water</div>
          </div>
        </div>

        <div className="topup-content">
          {/* Price Section */}
          <div className="topup-price-section">
            <span className="topup-price-label">Price per Pack</span>
            <span className="topup-total-amount">₹ {price}</span>
            {/* Quantity Section */}
            <div className="topup-quantity-section">
              {/* <label className="topup-label">Quantity</label> */}
              <div className="topup-quantity-controls">
                <button
                  className="topup-qty-btn"
                  onClick={() => setQuantity(Math.max(quantity - 1, 1))}
                  disabled={quantity <= 1}
                >
                  <Minus size={18} />
                </button>
                <span className="topup-qty-display">{quantity}</span>
                <button
                  className="topup-qty-btn"
                  onClick={() => setQuantity(quantity + 1)}
                >
                  <Plus size={18} />
                </button>
              </div>
            </div>
          </div>

          {quantity > 1 && (
            <div className="topup-total-section">
              <span>Total</span>

              <span className="topup-total-amount-blue">
                {Refill * quantity} Liters
                <span className="topup-total-nonamount">for </span>
                <span className="topup-total-amount">₹ {price * quantity}</span>
              </span>
            </div>
          )}

          <button className="topup-add-btn" onClick={handleTopUp}>
            <Plus size={18} />
            Add ₹ {price * quantity}
          </button>
        </div>
      </div>

      {/* 💸 Payment Success Modal */}
      {paymentSuccess && (
        <div className="topup-modal-overlay">
          <div className="topup-success-modal">
            <button
              className="topup-modal-close"
              onClick={() => setPaymentSuccess(null)}
            >
              <X size={18} />
            </button>

            <div className="topup-success-icon">
              <div className="topup-checkmark-circle">
                <CheckCircle2 size={56} className="topup-checkmark" />
              </div>
            </div>

            <h2 className="topup-success-title">Payment Successful!</h2>
            <p className="topup-success-subtitle">
              Your payment for <b>{paymentSuccess.flag}</b> has been received.
            </p>

            <div className="topup-payment-details">
              <div className="topup-detail-row">
                <span className="topup-detail-label">Amount Paid</span>
                <span className="topup-detail-value">
                  ₹ {paymentSuccess.totalPrice}
                </span>
              </div>
              <div className="topup-detail-row">
                <span className="topup-detail-label">Payment ID</span>
                <span className="topup-payment-id">
                  {paymentSuccess.paymentId}
                </span>
              </div>
              <div className="topup-detail-row">
                <span className="topup-detail-label">Timestamp</span>
                <span className="topup-detail-value">
                  {new Date(paymentSuccess.timeStamp).toLocaleString()}
                </span>
              </div>
            </div>

            <div className="topup-modal-actions">
              <button
                className="topup-btn-secondary"
                onClick={() => (window.location.href = "./pay")}
              >
                View Payments
              </button>
              <button
                className="topup-btn-primary"
                onClick={() => setPaymentSuccess(null)}
              >
                Close
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Loader Overlay */}
      {loading && (
        <div className="topup-loader-overlay">
          <Loader />
        </div>
      )}
    </>
  );
};

export default TopUpCard;
