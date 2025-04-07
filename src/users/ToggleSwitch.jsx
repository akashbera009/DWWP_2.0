import React, { useState , useEffect } from "react";
import { db } from '../firebaseConfig';  // Adjust the path as necessary
import { doc, getDoc, setDoc, onSnapshot, updateDoc } from 'firebase/firestore';  // Import updateDoc for updating fields
import "./ToggleSwitch.css"; // Assuming you will add the CSS separately or inline.

const ToggleSwitch = ({userId}) => {
  if (!userId) {
    console.error("userId is undefined in ToggleSwitch");
    return null; // Don't render the component if userId is missing
  }

  const [servoState, setServoState] = useState(null);
  const [loading, setLoading] = useState(true);
  const [isChecked, setIsChecked] = useState(true); // Default is checked
  
    const [waterData, setWaterData] = useState({
      totalUsage: 0,
      regularPrice: 0,
      penalty: 0,
      totalPrice: 0,
      usagePercentage: 0,
      safeLimit: 70,
    });
    const [todayUsage, setTodayUsage] = useState(  parseFloat(sessionStorage.getItem("todayUsage")) || 0);
  
    const [totalUsage, setTotalUsage] = useState(  parseFloat(sessionStorage.getItem("totalUsage")) || 0);
    // const [penaltyPrice, setPenaltyPrice] = useState(  parseFloat(sessionStorage.getItem("penaltyPrice")) || 0);
    // const [regularPrice, setRegularPrice] = useState( parseFloat(sessionStorage.getItem("regularPrice")) || 0);
    let [regularLimit, setRegularLimit] = useState( parseFloat(sessionStorage.getItem("regularLimit")) || 0);
    const [penaltyLimit, setPenaltyLimit] = useState( parseFloat(sessionStorage.getItem("penaltyLimit")) || 0); 
    const [addedLimit, setAddedLimit] = useState( parseFloat(sessionStorage.getItem("addedLimit")) || 0); 
  
    const [limitBYUser,  setLimitByUser] = useState( parseFloat(sessionStorage.getItem("limitBYUser")) || 0); // limit by user 
  
    const [maxLimit, setMaxLimit] = useState( parseFloat(sessionStorage.getItem("maxLimit")) || 0);
    const [isWaterFlowFetched, setIsWaterFlowFetched] = useState(false);
    // const [isPriceFetched, setIsPriceFetched] = useState(false);
    const [isLimitFetched, setIsLimitFetched] = useState(false);
    const [isLoading, setIsLoading] = useState(true);

  const servoControlDocRef = doc(db, 'users', userId );

    useEffect(() => {
    const fetchInitialData = async () => {
      try {
        const [servoSnap] = await Promise.all([
          getDoc(servoControlDocRef),
        ]);

        if (servoSnap.exists()) {
          setServoState(servoSnap.data().servoState);
          // console.log(servoState);
          
        } else {
          console.log("No such servoControl document!");
        }
      } catch (error) {
        console.error("Error fetching initial data: ", error);
      } finally {
        setLoading(false);
      }
    };

    fetchInitialData();

    const unsubscribeServoControl = onSnapshot(servoControlDocRef, (docSnap) => {
      if (docSnap.exists()) {
        setServoState(docSnap.data().servoState);
      } else {
        console.log("No such servoControl document!");
      }
    });
      return () => {
        unsubscribeServoControl();
      };
    }, [userId, servoControlDocRef]);


    // for the added limit 
    const now = new Date();
    const year = now.getFullYear();
    const month = String(now.getMonth() + 1).padStart(2, "0");
    const yearMonth = `${year}-${month}`;

    const addonDocRef = doc(db, `users/${userId}/monthlyUsages/${yearMonth}/addon/addon_details`);
    // fetch added limit 
    useEffect(() => {
      if (!userId) return;
    
      const fetchAddedLimit = async () => {
        try {
        const addonSnap = await getDoc(addonDocRef);
          if (addonSnap.exists()) {
            const addonData = addonSnap.data();
            setAddedLimit(addonData.added_limit)
          }else {
            console.log("No added_limit document found.");
          }
        } catch (error) {
          console.error("Error fetching addedLimit:", error);
        }
      };
    
      fetchAddedLimit();
    }, [userId,limitBYUser]);

    const cutoffLimit = limitBYUser + penaltyLimit + addedLimit;
    
    // console.log(limitBYUser ,  penaltyLimit , addedLimit);
    
    
    const handleToggle = async () => {
      if (totalUsage >= cutoffLimit) {
        console.warn("Usage limit exceeded. Manual control disabled.");
        return; // prevent toggle
      }
      const newState = !servoState;
      try {
            await updateDoc(servoControlDocRef, { servoState: newState });
            setServoState(newState);
          } catch (error) {
            console.error("Error updating servoControl document: ", error);
          }
    };
    // auto disable 
    useEffect(() => {
      // const cutoffLimit =  + penaltyLimit;
    
      if (totalUsage >= cutoffLimit && servoState !== false) {
        // Automatically turn off the servo
        updateDoc(servoControlDocRef, { servoState: false })
          .then(() => {
            setServoState(false);
            console.log("Servo auto-disabled due to usage limit.");
          })
          .catch((err) => {
            console.error("Failed to auto-disable servo:", err);
          });
      }
    }, [totalUsage, limitBYUser, penaltyLimit]);
    


    // limits and totalusages getign 
      useEffect(() => {
        if (!userId) return; 
      
            // Get the current year and month dynamically
            const now = new Date();
            const year = now.getFullYear();
            const month = String(now.getMonth() + 1).padStart(2, "0"); // Ensure two-digit format
            // const day = String(now.getDate()); // Ensure two-digit format
            const day = String(now.getDate()).padStart(2, "0");
            const yearMonth = `${year}-${month}`; // Format: "YYYY-MM"
            let today = `${year}-${month}-${day}`; // Format: "YYYY-MM"
        
            const usageDocRef = doc(db, "users", userId, "monthlyUsages", yearMonth);
    
        const unsubscribe = onSnapshot(usageDocRef, (docSnap) => {
          if (docSnap.exists()) {
            const data = docSnap.data();
            today = today.toString()
            setTodayUsage(data[today] || 0 );
      
            // Sum all values from the document (each field is a date with a number)
            const total = Object.entries(data)
              .filter(([key]) => key.startsWith(yearMonth)) // Only include keys with "YYYY-MM"
              .reduce((sum, [, usage]) => sum + (usage || 0), 0);
      
            const userLimit = data.limit || 0; // Fetch limit value directly
            
            setLimitByUser(userLimit);
            sessionStorage.setItem("limitBYUser", userLimit);
      
            setTotalUsage(total);
            sessionStorage.setItem("totalUsage", total);
          }
          setIsWaterFlowFetched(true);
        }, (error) => {
          console.error("Error fetching water usage:", error);
        });
      
        // Cleanup function to unsubscribe when component unmounts
        return () => unsubscribe();
      }, [userId]);

      // setLimitByUser and setTotalUsage
      useEffect(() => {
        if (!userId) return;
        // Listener for waterflowSensor
            const now = new Date();
            const year = now.getFullYear();
            const month = String(now.getMonth() + 1).padStart(2, "0"); // Ensure two-digit format
            const yearMonth = `${year}-${month}`; // Format: "YYYY-MM"
          const unsubscribeWaterFlow = onSnapshot(
          doc(db, "users", userId, "monthlyUsages", yearMonth) , 
          (docSnap) => {
            if (docSnap.exists()) {
              const data = docSnap.data();

              const total = Object.entries(data)
              .filter(([key]) => key.startsWith(yearMonth)) // Only include keys with "YYYY-MM"
              .reduce((sum, [, usage]) => sum + (usage || 0), 0);

              const userLimit = Object.entries(data)
              .find(([key]) => key === "limit"); // Find the key-value pair directly
              
              const limitValue = userLimit ? Number(userLimit[1]) : 0; // Convert to number, default to 0
              
              setLimitByUser(limitValue);
              sessionStorage.setItem("limitBYUser", limitValue);
              
              setTotalUsage(total);
              sessionStorage.setItem("totalUsage", total);

            } else {
              console.log("No water usage data for this month.");
              setTotalUsage(0);
            }
            setIsWaterFlowFetched(true);
          },
          (error) => {
            console.error("Error fetching water usage:", error);
            setIsWaterFlowFetched(true);
          }
        );

        // Listener for price
        const unsubscribePrice = onSnapshot(
          doc(db, "admin", "price"),
          (priceDocSnap) => {
            if (priceDocSnap.exists()) {
              const data = priceDocSnap.data();
              // setPenaltyPrice(data.penaltyPrice || 0);
              // setRegularPrice(data.regularPrice || 0);
              // setIsPriceFetched(true);
              // sessionStorage.setItem("penaltyPrice", data.penaltyPrice);
              // sessionStorage.setItem("regularPrice", data.regularPrice);
              // console.log("Prices fetched: 😒😒", data);
            } else {
              console.log("No such price document!");
              // setIsPriceFetched(true); // Even if document doesn't exist, consider it fetched
            }
          },
          (error) => {
            console.error("Error fetching price document: ", error);
            // setIsPriceFetched(true); // Prevent indefinite loading on error
          }
        );

        // Listener for limit
        const unsubscribeLimit = onSnapshot(
          doc(db, "admin", "limit"),
          (limitDocSnap) => {
            if (limitDocSnap.exists()) {
              const data = limitDocSnap.data();
              setPenaltyLimit(data.penalty || 0);
              setRegularLimit(data.regular || 100);
              setMaxLimit(data.max) ; 

              sessionStorage.setItem("regularLimit", data.regular);
              sessionStorage.setItem("penaltyLimit", data.penalty);
              sessionStorage.setItem("maxLimit", data.max);


              setIsLimitFetched(true);
              // console.log("Limits fetched: ", data);
            } else {
              console.log("No such limit document!");
              setIsLimitFetched(true); // Even if document doesn't exist, consider it fetched
            }
          },
          (error) => {
            console.error("Error fetching limit document: ", error);
            setIsLimitFetched(true); // Prevent indefinite loading on error
          }
        );

        // Cleanup the listeners when the component unmounts
        return () => {
          unsubscribeWaterFlow();
          unsubscribePrice();
          unsubscribeLimit();
        };
      }, [userId]);
    
      useEffect(() => {
        const fetchData = async () => {
          await new Promise((resolve) => setTimeout(resolve, 1500));
    
          setWaterData({
            totalUsage: 0,
            regularPrice: 0,
            penalty: 0,
            totalPrice: 0,
            usagePercentage: 0,
            safeLimit: 0,
          });
          setIsLoading(false);
        };
    
        fetchData();
      }, []);

  return (
  <>

    <div className="container"  >
      <label className="switch" >
        <input
          type="checkbox"
          className="togglesw"
          checked={!!servoState} 
          onChange={handleToggle} 
        />
        <div className={`indicator left ${servoState? 'active' : ''}`}></div>
        <div className={`indicator right ${!servoState ? 'active' : ''}`}></div>
        <div className={`button ${servoState ? 'active' : ''}`}></div>
      </label>
    </div>

    <div className="message-section ">
       {/* <p className="servo-message">{servoState ? ( <>On</>):(<>off</>)} </p> */}
       {totalUsage >= cutoffLimit && (
          <p className="limit-message">Water usage limit reached. Control disabled.</p>
        )}

    </div>
    
  </>);
};

export default ToggleSwitch;