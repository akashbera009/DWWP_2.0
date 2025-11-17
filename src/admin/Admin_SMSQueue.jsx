import React, { useEffect, useState } from "react";
import {
  collection,
  query,
  orderBy,
  onSnapshot,
  addDoc,
  serverTimestamp,
  doc,
  setDoc, 
  updateDoc,
  deleteDoc,
} from "firebase/firestore";
import { db } from "../firebaseConfig";

const STATUS_STYLES = {
  pending: { background: "#FFC107", color: "#000" },
  sent: { background: "#4CAF50", color: "#fff" },
  failed: { background: "#F44336", color: "#fff" },
};

const Admin_SMSQueue = () => {
  const [tasks, setTasks] = useState([]);
  const [loading, setLoading] = useState(true);

  const [form, setForm] = useState({
    userId: "",
    mobileNo: "",
    messageType: "payment",
    referenceId: "",
    message: "",
  });

  const [isBottomOpen, setBottomOpen] = useState(false);
  const [bottomFilter, setBottomFilter] = useState("all");
  const [isCreateOpen, setCreateOpen] = useState(false);

  useEffect(() => {
    const smsRef = collection(db, "smsQueue");
    const q = query(smsRef, orderBy("createdAt", "desc"));
    const unsub = onSnapshot(
      q,
      (snapshot) => {
        const out = snapshot.docs.map((d) => ({ id: d.id, ...d.data() }));
        setTasks(out);
        setLoading(false);
      },
      (err) => {
        console.error("Failed to subscribe to smsQueue:", err);
        setLoading(false);
      }
    );

    return () => unsub();
  }, []);

  const formatTs = (ts) => {
    if (!ts) return "-";
    if (typeof ts?.toDate === "function") {
      return ts.toDate().toLocaleString();
    }
    try {
      const d = new Date(ts);
      if (isNaN(d)) return String(ts);
      return d.toLocaleString();
    } catch {
      return String(ts);
    }
  };

  const onFormChange = (e) => {
    const { name, value } = e.target;
    setForm((p) => ({ ...p, [name]: value }));
  };

const createTask = async () => {
  if (!form.userId || !form.mobileNo || !form.message) {
    return alert("Please fill userId, mobileNo and message.");
  }

  try {
    const now = new Date();
    const pad = (n) => String(n).padStart(2, "0");
    const dateTimeStr = `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(
      now.getDate()
    )}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;

    const docId = `sms_${form.userId.split('@')[0]}_${dateTimeStr}`;
    // /smsQueue/sms_akashbera102003_gmail_com_20251109_153718
    const smsRef = doc(db, "smsQueue", docId); // custom document ID

    await setDoc(smsRef, {
      userId: form.userId,
      mobileNo: form.mobileNo,
      messageType: form.messageType,
      referenceId: form.referenceId || null,
      message: form.message,
      status: "pending",
      attempts: 0,
      createdAt: serverTimestamp(),
      sentAt: null,
      lastError: null,
    });

    setForm({
      userId: "",
      mobileNo: "",
      messageType: "payment",
      referenceId: "",
      message: "",
    });

    alert("SMS task created!");
  } catch (err) {
    console.error("Error creating SMS task:", err);
    alert("Failed to create SMS task (see console).");
  }
};


  const markSent = async (task) => {
    try {
      const d = doc(db, "smsQueue", task.id);
      await updateDoc(d, {
        status: "sent",
        sentAt: serverTimestamp(),
        lastError: null,
      });
    } catch (err) {
      console.error("markSent error:", err);
    }
  };

  const markFailed = async (task, errorMessage = "Failed to send") => {
    try {
      const d = doc(db, "smsQueue", task.id);
      await updateDoc(d, {
        status: "failed",
        sentAt: null,
        lastError: errorMessage,
      });
    } catch (err) {
      console.error("markFailed error:", err);
    }
  };

  const retryTask = async (task) => {
    try {
      const d = doc(db, "smsQueue", task.id);
      await updateDoc(d, {
        attempts: (task.attempts || 0) + 1,
        status: "pending",
        lastError: null,
      });
    } catch (err) {
      console.error("retryTask error:", err);
    }
  };

  const removeTask = async (task) => {
    if (!window.confirm("Delete this SMS task?")) return;
    try {
      await deleteDoc(doc(db, "smsQueue", task.id));
    } catch (err) {
      console.error("deleteTask error:", err);
    }
  };

  const bottomTasks = tasks.filter((t) => {
    if (bottomFilter === "all") return true;
    if (!t.status) return false;
    return t.status === bottomFilter;
  });

  return (
    <div style={styles.container}>
      <div
        style={{
          ...styles.contentWrapper,
          paddingBottom: isBottomOpen ? "45vh" : "2rem",
        }}
      >
        <div style={styles.headerRow}>
          <h1 style={styles.title}>SMS Queue (Admin)</h1>

          <div style={styles.headerActions}>
            <button
              style={styles.toggleBtn}
              onClick={() => setCreateOpen((v) => !v)}
            >
              {isCreateOpen ? "Hide Manual Message" : "Create Manual Message"}
            </button>
          </div>
        </div>

        {isCreateOpen && (
          <div style={styles.card}>
            <div style={{ marginBottom: 20 }}>
              <h3 style={{ color: "#fff", marginBottom: 10 }}>
                Create SMS Task
              </h3>

              <div style={{ display: "grid", gap: 10 }}>
                <label style={{ color: "#a0a0a0" }}>
                  User ID (email or uid)
                </label>
                <input
                  name="userId"
                  value={form.userId}
                  onChange={onFormChange}
                  placeholder="user@example.com"
                  style={styles.input}
                />

                <label style={{ color: "#a0a0a0" }}>Mobile No</label>
                <input
                  name="mobileNo"
                  value={form.mobileNo}
                  onChange={onFormChange}
                  placeholder="9876543210"
                  style={styles.input}
                />

                <label style={{ color: "#a0a0a0" }}>Message Type</label>
                <select
                  name="messageType"
                  value={form.messageType}
                  onChange={onFormChange}
                  style={styles.select}
                >
                  <option value="payment">Payment</option>
                  <option value="addon">Addon</option>
                  <option value="limit_exceeded">Limit Exceeded</option>
                  <option value="reminder">Reminder</option>
                </select>

                <label style={{ color: "#a0a0a0" }}>
                  Reference ID (optional)
                </label>
                <input
                  name="referenceId"
                  value={form.referenceId}
                  onChange={onFormChange}
                  placeholder="users/user@example.com/monthlyUsages/2025-11/..."
                  style={styles.input}
                />

                <label style={{ color: "#a0a0a0" }}>Message</label>
                <textarea
                  name="message"
                  value={form.message}
                  onChange={onFormChange}
                  placeholder="Compose your message..."
                  rows={4}
                  style={styles.textarea}
                />

                <button
                  style={{
                    ...styles.sendBtn,
                    opacity:
                      !form.userId || !form.mobileNo || !form.message ? 0.5 : 1,
                    cursor:
                      !form.userId || !form.mobileNo || !form.message
                        ? "not-allowed"
                        : "pointer",
                  }}
                  onClick={createTask}
                  disabled={!form.userId || !form.mobileNo || !form.message}
                >
                  ➕ Create SMS Task
                </button>
              </div>
            </div>
          </div> 
        )}

        <hr style={styles.divider} />

        <h3 style={{ color: "#fff", marginBottom: 12 }}>Queued SMS Tasks</h3>

        {loading ? (
          <p style={{ color: "#a0a0a0" }}>Loading...</p>
        ) : tasks.length === 0 ? (
          <p style={{ color: "#a0a0a0" }}>No tasks in smsQueue.</p>
        ) : (
          <div style={{ display: "grid", gap: 12 }}>
            {tasks.map((t) => (
              <div key={t.id} style={styles.taskCard}>
                <div style={styles.taskLeft}>
                  <div style={styles.taskTopline}>
                    <strong style={{ color: "#fff", fontSize: 16 }}>
                      {t.userId || "-"}
                    </strong>
                    <span style={{ color: "#a0a0a0" }}>
                      {t.mobileNo || "-"}
                    </span>
                    <span style={styles.badge}>{t.messageType}</span>
                    <span
                      style={{
                        ...styles.statusBadge,
                        ...(STATUS_STYLES[t.status] || STATUS_STYLES.pending),
                      }}
                    >
                      {t.status || "pending"}
                    </span>
                  </div>

                  <div style={{ color: "#d0d0d0", marginBottom: 8 }}>
                    {t.message}
                  </div>

                  <div style={styles.taskMeta}>
                    <span>Attempts: {t.attempts ?? 0}</span>
                    <span>Created: {formatTs(t.createdAt)}</span>
                    <span>Sent: {formatTs(t.sentAt)}</span>
                    <span>LastError: {t.lastError ?? "-"}</span>
                    {t.referenceId ? <span>Ref: {t.referenceId}</span> : null}
                  </div>
                </div>

                <div style={styles.taskActions}>
                  <div>

                  <button
                    style={{ ...styles.actionBtn, ...styles.greenBtn }}
                    onClick={() => markSent(t)}
                    title="Mark as sent"
                    >
                    ✅
                  </button>
                  <button
                    style={{ ...styles.actionBtn, ...styles.redBtn }}
                    onClick={() => {
                      const err = prompt(
                        "Optional: enter failure reason",
                        "Network timeout"
                      );
                      markFailed(t, err || "Failed to send");
                    }}
                    >
                    ❌
                  </button>
                    </div>
                  <button
                    style={{ ...styles.actionBtn, ...styles.blueBtn }}
                    onClick={() => retryTask(t)}
                    title="Retry"
                  >
                    ⟳
                  </button>
                  <button
                    style={{ ...styles.actionBtn, ...styles.grayBtn }}
                    onClick={() => removeTask(t)}
                    title="Delete"
                  >
                    🗑
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

const styles = {
  container: {
    padding: "2rem",
    minHeight: "100vh",
    background: "#0a0f1f",
    marginLeft: "20vw" ,
    position: "relative",
    zIndex:0
  },
  contentWrapper: {
    transition: "padding-bottom 300ms ease",
  },
  headerRow: {
    display: "flex",
    alignItems: "center",
    justifyContent: "space-between",
    gap: 12,
    marginBottom: "1rem",
  },
  title: {
    color: "#ffffff",
    fontSize: "2.2rem",
    letterSpacing: 1,
    margin: 0,
  },
  headerActions: {
    display: "flex",
    gap: 8,
    alignItems: "center",
  },
  toggleBtn: {
    background: "linear-gradient(45deg, #2196f3, #1976d2)",
    color: "#fff",
    border: "none",
    padding: "0.6rem 1rem",
    borderRadius: 8,
    cursor: "pointer",
    fontWeight: 600,
  },
  card: {
    background: "linear-gradient(145deg, rgb(7, 16, 45), rgb(58, 60, 84))",
    maxWidth: 1100,
    margin: "0 auto",
    padding: "2rem",
    borderRadius: 15,
    boxShadow: "0 10px 30px rgba(0, 0, 0, 0.3)",
  },
  input: {
    padding: 10,
    borderRadius: 6,
    border: "1px solid rgba(255, 255, 255, 0.08)",
    background: "#0a0f1f",
    color: "#fff",
  },
  select: {
    width: "100%",
    padding: "1rem",
    background: "#0a0f1f",
    border: "1px solid rgba(255, 255, 255, 0.1)",
    borderRadius: 8,
    color: "white",
    fontSize: "1rem",
  },
  textarea: {
    width: "100%",
    padding: "1rem",
    background: "rgba(255, 255, 255, 0.05)",
    borderRadius: 8,
    border: "none",
    color: "white",
    fontSize: "1rem",
    lineHeight: 1.5,
    resize: "vertical",
    minHeight: 80,
  },
  sendBtn: {
    background: "linear-gradient(45deg, #4caf50, #388e3c)",
    color: "#fff",
    border: "none",
    padding: "0.8rem",
    borderRadius: 8,
    fontWeight: 600,
    fontSize: "1rem",
  },
  divider: {
    border: "none",
    borderTop: "1px solid rgba(255, 255, 255, 0.06)",
    margin: "1.5rem 0",
  },
  taskCard: {
    padding: 14,
    borderRadius: 12,
    background: "linear-gradient(145deg, rgb(7, 16, 45), rgb(58, 60, 84))",
    display: "flex",
    gap: 14,
    alignItems: "flex-start",
    justifyContent: "space-between",
  },
  taskLeft: {
    flex: 1,
  },
  taskTopline: {
    display: "flex",
    gap: 12,
    alignItems: "center",
    marginBottom: 6,
    flexWrap: "wrap",
  },
  badge: {
    marginLeft: 8,
    padding: "4px 8px",
    borderRadius: 8,
    background: "rgba(255, 255, 255, 0.03)",
    color: "#fff",
    fontSize: 12,
  },
  statusBadge: {
    marginLeft: 8,
    padding: "4px 8px",
    borderRadius: 8,
    fontSize: 12,
    fontWeight: 700,
  },
  taskMeta: {
    color: "#9aa0b2",
    fontSize: 13,
    display: "flex",
    gap: 16,
    flexWrap: "wrap",
  },
  taskActions: {
    display: "flex",
    flexDirection: "column",
    gap: 8,
    minWidth: 120,
  },
  actionBtn: {
    padding: 8,
    borderRadius: 8,
    border: "none",
    cursor: "pointer",
    fontSize: 14,
    color: "#fff",
  },
  greenBtn: {
    background: "linear-gradient(45deg, #4caf50, #388e3c)",
  },
  redBtn: {
    background: "linear-gradient(45deg, #f44336, #d32f2f)",
  },
  blueBtn: {
    background: "linear-gradient(45deg, #2196f3, #1976d2)",
  },
  grayBtn: {
    background: "linear-gradient(45deg, #9e9e9e, #757575)",
  },
  sentPanel: {
    position: "fixed",
    left: "15vw",
    right: "2rem",
    height: "40vh",
    maxHeight: "60vh",
    background:
      "linear-gradient(180deg, rgba(7,12,30,0.95), rgba(20,26,46,0.98))",
    borderTopLeftRadius: 12,
    borderTopRightRadius: 12,
    boxShadow: "0 -8px 30px rgba(0,0,0,0.6)",
    transition: "bottom 300ms ease",
    display: "flex",
    flexDirection: "column",
    overflow: "hidden",
    zIndex: 1000,
  },
  sentPanelHeader: {
    display: "flex",
    alignItems: "center",
    justifyContent: "space-between",
    padding: "12px 16px",
    borderBottom: "1px solid rgba(255,255,255,0.04)",
  },
  sentPanelBody: {
    overflow: "auto",
    padding: "12px 16px",
    display: "flex",
    flexDirection: "column",
    gap: 10,
  },
  sentItem: {
    display: "flex",
    justifyContent: "space-between",
    gap: 12,
    padding: 10,
    borderRadius: 8,
    background: "rgba(255,255,255,0.02)",
    border: "1px solid rgba(255,255,255,0.02)",
  },
};

export default Admin_SMSQueue;
