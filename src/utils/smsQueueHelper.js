import { doc, setDoc, getDoc } from "firebase/firestore";
import { db } from "../firebaseConfig"; 

/**
 * Creates an SMS queue entry in Firestore
 * @param {string} userId - User email/ID
 * @param {object} options - SMS details
 */
export const createSmsQueueEntry = async (userId, options) => {
  try {
    // Get user's mobile number
    const userDoc = await getDoc(doc(db, "users", userId));
    
    if (!userDoc.exists()) {
      console.error("❌ User not found:", userId);
      return null;
    }

    const userData = userDoc.data();
    const mobileNo = userData.userDetails?.mobileNo;

    if (!mobileNo) {
      console.error("❌ Mobile number not found for user:", userId);
      return null;
    }

    // Generate unique SMS queue ID
    const timestamp = new Date().toISOString().replace(/[-:T.]/g, '').slice(0, 15);
    const smsId = `sms_${userId.split('@')[0]}_${timestamp}`;

    // Prepare SMS message based on type
    const message = generateSmsMessage(options.messageType, {
      ...options,
      userName: userData.userDetails?.fullName || "Customer"
    });

    // Create SMS queue document
    const smsQueueRef = doc(db, "smsQueue", smsId);
    
    await setDoc(smsQueueRef, {
      userId: userId,
      mobileNo: mobileNo,
      messageType: options.messageType,
      referenceId: options.referenceId || null,
      message: message,
      status: "pending",
      attempts: 0,
      createdAt: new Date().toISOString(),
      sentAt: null,
      lastError: null,
      // Additional metadata
      metadata: {
        paymentId: options.paymentId || null,
        amount: options.paymentAmount || null,
        forMonth: options.forMonth || null,
        refillAmount: options.refillAmount || null
      }
    });

    console.log("✅ SMS queue entry created:", smsId);
    return smsId;

  } catch (error) {
    console.error("❌ Error creating SMS queue entry:", error);
    return null;
  }
};

/**
 * Generates SMS message based on type
 */
const generateSmsMessage = (messageType, data) => {
  const { userName, paymentAmount, forMonth, refillAmount, paymentId } = data;

  switch (messageType) {
    case "payment":
      return `Dear ${userName}, your water bill payment of Rs.${paymentAmount} for ${forMonth} has been received successfully. Payment ID: ${paymentId}. Thank you!`;

    case "addon":
      return `Dear ${userName}, addon of ${refillAmount}L purchased for Rs.${paymentAmount}. Payment ID: ${paymentId}. Your water supply continues uninterrupted.`;

    case "limit_exceeded":
      return `Dear ${userName}, your monthly water limit has been exceeded. Please recharge to avoid service disruption. Visit our app to add more balance.`;

    case "reminder":
      return `Dear ${userName}, your water bill payment is due in 3 days. Please pay to avoid disconnection. Amount: Rs.${paymentAmount}`;

    default:
      return `Dear ${userName}, thank you for using our service.`;
  }
};