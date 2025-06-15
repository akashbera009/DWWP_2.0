

import { initializeApp } from 'firebase/app';
import { getAuth } from 'firebase/auth'; // Import getAuth
import { getFirestore } from 'firebase/firestore';

// Your web app's Firebase configuration
const firebaseConfig = {
  apiKey: "AIzaSyCaQ4-CXMn-2HB0RdMKZvDmVm6SGtXPnk8",
  authDomain: "waterflow-dashboard.firebaseapp.com",
  projectId: "waterflow-dashboard",
  storageBucket: "waterflow-dashboard.appspot.com",
  messagingSenderId: "63673543015",
  appId: "1:63673543015:web:94eb493ecc8adfa41f1391"

  // subhayan
  // apiKey: "AIzaSyDtB7SHnpIWOU0NSt_jiRe_OS9g6gp_LzE",
  // authDomain: "dwp-5th-sem-project.firebaseapp.com",
  // projectId: "dwp-5th-sem-project",
  // storageBucket: "dwp-5th-sem-project.firebasestorage.app",
  // messagingSenderId: "446965936454",
  // appId: "1:446965936454:web:cc584d92bf7c1b39d37547",
  // measurementId: "G-5Z2GXCVQTZ"


  // apiKey:import.meta.env.VITE_FIREBASE_API_KEY ,
  // authDomain: import.meta.env.VITE_AUTHDOMAIN,
  // projectId: import.meta.env.VITE_PROJECTID,
  // storageBucket: import.meta.env.VITE_STORAGEBUCKET,
  // messagingSenderId: import.meta.env.VITE_MESSAGING_SENDER ,
  // appId: import.meta.env.VITE_APPID,
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);

// Initialize Firebase Authentication and Firestore
const auth = getAuth(app); // Initialize auth
const db = getFirestore(app);
const firestore = getFirestore(app);

export { firestore  , auth, db };
