import React, { useState ,useEffect } from "react";
import { handlePayment } from "../utils/razorpayPayment"; 
import { db } from "../firebaseConfig"; // Adjust the path as necessary
import { doc, updateDoc, getDoc , setDoc } from "firebase/firestore";
import "./TopUpCard.css";
import Loader from '../Loader/Loader'

const TopUpCard = ({ userId, addon, price, Refill, url , msg }) => {
  const [quantity, setQuantity] = useState(1); // Default quantity set to 1
  const [paymentSuccess, setPaymentSuccess] = useState(null);
  const [loading, setLoading] = useState(false);


  const now = new Date();
  const year = now.getFullYear();
  const month = String(now.getMonth() + 1).padStart(2, "0"); // Ensure two-digit format
  const yearMonth = `${year}-${month}`; // Format: "YYYY-MM"

  
  const addonRef = doc(db, "users", userId, "monthlyUsages", yearMonth, "addon", "addon_details");

  const handleTopUp = async (price, quantity, Refill) => {
    try {
      const paymentId = await handlePayment(userId, price * quantity, quantity, Refill, "addon");
  
      if (paymentId) {
        let totalPrice = price * quantity || 0;
        let flag = "addon";
        let timeStamp = new Date().toISOString();
        
        console.log("✅ Payment Successful! Updating Firestore...");
  
        setLoading(true) ; 
        const totalRefill = Number(Refill) * Number(quantity);
  
        // 🔹 Fetch the current document OR create a new one if missing
        const docSnap = await getDoc(addonRef);
  
        let currentLimit = 0;
        let razorPayIds = [];
        let timeStamps = [];
        let amount = [];
        let quantityDone = [];
        let refill = [];
  
        if (docSnap.exists()) {
          // ✅ Document exists, extract existing data
          let currentData = docSnap.data();
          currentLimit = currentData.added_limit || 0;
          razorPayIds = currentData.razor_pay_id || [];
          timeStamps = currentData.timeStamp || [];
          amount = currentData.amount || [];
          quantityDone = currentData.quantityDone || [];
          refill = currentData.refill || [];
        } else {
          // ❌ Document does NOT exist, initialize it with default values
          // console.log("🚀 No existing document found, creating a new one...");
          
          await setDoc(addonRef, {
            added_limit: 0,
            razor_pay_id: [],
            timeStamp: [],
            amount: [],
            quantityDone: [],
            refill: []
          });

          // console.log("📄 New document created successfully!");
        }
  
        // ✅ Append new values
        razorPayIds.push(paymentId);
        timeStamps.push(timeStamp);
        amount.push(price);
        quantityDone.push(quantity);
        refill.push(Refill);
  
        // ✅ Update Firestore with all the data
        await updateDoc(addonRef, {
          added_limit: currentLimit + totalRefill,
          razor_pay_id: razorPayIds,
          timeStamp: timeStamps,
          amount: amount,
          quantityDone: quantityDone,
          refill: refill
        });
  
        // console.log(`✅ Firestore updated! New added_limit = ${currentLimit + totalRefill}`);
        // console.log(`Added Payment ID: ${paymentId}, Timestamp: ${timeStamp}, Amount: ₹${totalPrice}, Quantity: ${quantity}, Refill: ${totalRefill}L`);

        setLoading(false) ;
        setPaymentSuccess({ totalPrice, paymentId, timeStamp, flag });
        
        setTimeout(() => {
          setPaymentSuccess(null);
        }, 10000000);
      } else {
        console.log("❌ Payment Failed. Firestore update skipped.");
      }
    } catch (error) {
      console.error("❌ Error processing payment:", error);
    }
  };
  


  
  return (
    <>
         <div className="top-up_card festive-theme">
      <span className="festive-label">{addon}</span>
       
        <div className="water-icon">
       
        
      
        {/* <img src="https://i.ibb.co/LXmxY1gd/water-1.png"/> */}
        <img src={url}/>
        </div>
        <div className="subscription-title"> {msg}{Refill}L Water </div>
        <div className="quantity-selector">
          <span className="label">Quantity</span>
          <div className="controls">
            <button
              onClick={() => setQuantity(Math.max(quantity - 1, 1))}
              className="btn"
            >
              -
            </button>
            <span className="count">{quantity}</span>
            <button
              onClick={() => setQuantity(quantity + 1)}
              className="btn"
            >
              +
            </button>
          </div>
        </div>
        <br></br>
        <div className="price">₹ {price} per Pack</div>
        <button className="add-btn" onClick={() => handleTopUp(price, quantity, Refill)}>
          Add
        </button>
      </div>
    {paymentSuccess &&
     <div className="success-card-container">
      <div className="success-card-payment">
     

          <div className="success-icon">
              <svg className="checkmark" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 52 52">
                  <circle className="checkmark__circle" cx="26" cy="26" r="25" fill="none" />
                  <path className="checkmark__check" fill="none" d="M14.1 27.2l7.1 7.2 16.7-16.8" />
              </svg>
          </div>
          <h2>Successfully Paid {paymentSuccess.totalPrice}</h2>
          <p>Your payment for {paymentSuccess.flag} has been received.</p>
          <p>Pay ID : {paymentSuccess.paymentId} </p>
          <p>Time: {paymentSuccess.timeStamp} </p>
          <div className="payment-success-button-group">
          <button className="view-dialog-payment" onClick={() => window.location.href = "./pay"}>View</button>
          <button className="cancel-dialog-payment"  onClick={() => setPaymentSuccess(null)}> Close </button>
          </div>
      </div>
      </div>
    }
    { loading && 
    <div>
      <Loader/>
      </div>}
    </>
  );
};

export default TopUpCard;
